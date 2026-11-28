// Copyright 2012 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Log viewer window implementation.
#include "sawbuck/viewer/viewer_window.h"

#include <string.h>

// Named because WIN32_LEAN_AND_MEAN, which build/common.gypi sets for
// the whole tree, is what keeps shellapi.h out of windows.h. Without
// this, ShellExecute below is undeclared.
#include <shellapi.h>

#include "pcrecpp.h"  // NOLINT
#include "base/bind.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/win/event_trace_consumer.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "sawbuck/viewer/const_config.h"
#include "sawbuck/viewer/preferences.h"
#include "sawbuck/viewer/provider_dialog.h"
#include "sawbuck/viewer/viewer_constants.h"
#include "sawbuck/viewer/viewer_module.h"
#include <initguid.h>  // NOLINT

namespace {

// A regular expression that matches "[<stuff>:<file>(<line>)].message"
// and extracts the file/line/message parts.
const pcrecpp::RE kFileRe("\\[[^\\]]*\\:([^:]+)\\((\\d+)\\)\\].(.*\\w).*",
                          PCRE_NEWLINE_ANYCRLF | PCRE_DOTALL | PCRE_UTF8);

// True if this machine looks configured to negotiate TLS 1.2 through WinHTTP,
// which is what symsrv.dll fetches symbols with.
//
// Only meaningful below Windows 8.1, where the answer is yes without asking.
// Below that it is a yes only if someone has done the work -- the update for
// their version, plus these registry values -- and this is how to find out
// without attempting a fetch and waiting for it to fail. See
// assets/enable_tls12.reg, which is the other half of this.
//
// Read-only and unprivileged: both keys are world-readable, so this costs two
// RegOpenKeyEx calls and asks for nothing a normal user does not have.
//
// What it cannot establish, and the reason the caller treats a yes as "worth
// trying" rather than a guarantee: whether this version's WinHTTP honours
// DefaultSecureProtocols at all. KB3140245 is what adds that, and it is
// offered for Windows 8 and Server 2012 as well as for 7 -- so a stock 8 is
// in the same position as a stock 7. On XP and Vista the schannel side is
// reachable through KB4019276, but whether WinHTTP reads this value there
// has not been confirmed on a real machine. A false yes costs symbol
// resolution and nothing else, and is correctable from the Symbol Path
// dialog.
bool IsTls12EnabledForWinHttp() {
  CRegKey schannel;
  if (schannel.Open(HKEY_LOCAL_MACHINE, kTls12ClientKey, KEY_READ) !=
      ERROR_SUCCESS) {
    return false;
  }

  // The two values mean opposite things by their absence, which is worth
  // being careful about rather than treating them as a pair.
  //
  // DisabledByDefault absent means "use the system default", and below
  // Windows 8.1 that default is 1 for TLS 1.2 -- available, but only to a
  // caller that asks for it by name. symsrv does not, so absent is a no.
  DWORD disabled_by_default = 1;
  if (schannel.QueryDWORDValue(kDisabledByDefaultValue,
                               disabled_by_default) != ERROR_SUCCESS ||
      disabled_by_default != 0) {
    return false;
  }

  // Enabled absent means the protocol is available. Only an explicit zero
  // turns it off, so this check is for someone having disabled it on purpose
  // after enabling it above -- contradictory, but the registry permits it and
  // zero is the one that wins.
  DWORD enabled = 1;
  if (schannel.QueryDWORDValue(kEnabledValue, enabled) == ERROR_SUCCESS &&
      enabled == 0) {
    return false;
  }

  // And what WinHTTP offers when the caller names no protocol, which is
  // symsrv's case. Opened in this process's own registry view deliberately:
  // a 32-bit Sawbuck is redirected to Wow6432Node, which is the copy its own
  // WinHTTP reads, so being redirected gives the right answer rather than a
  // wrong one.
  CRegKey winhttp;
  if (winhttp.Open(HKEY_LOCAL_MACHINE, kWinHttpKey, KEY_READ) !=
      ERROR_SUCCESS) {
    return false;
  }

  DWORD protocols = 0;
  if (winhttp.QueryDWORDValue(kDefaultSecureProtocolsValue, protocols) !=
      ERROR_SUCCESS) {
    return false;
  }

  return (protocols & kTls12Protocol) != 0;
}

bool Is64BitSystem() {
  if (sizeof(void*) == 8)  // NOLINT
    return true;

  HMODULE kernel32 = ::GetModuleHandle(L"kernel32.dll");
  if (!kernel32)
    return false;

  typedef BOOL (WINAPI* IsWow64ProcessProc)(HANDLE process, PBOOL is_wow64);
  IsWow64ProcessProc is_wow_64_process =
      reinterpret_cast<IsWow64ProcessProc>(
          ::GetProcAddress(kernel32, "IsWow64Process"));

  if (is_wow_64_process == NULL)
    return false;

  BOOL is_wow_64 = FALSE;
  CHECK(IsWow64Process(::GetCurrentProcess(), &is_wow_64));

  return is_wow_64 != FALSE;
}

}  // namespace

bool operator < (const GUID& a, const GUID& b) {
  return 0 < memcmp(&a, &b, sizeof(a));
}

void ViewerWindow::CompileAsserts() {
}

ViewerWindow::ViewerWindow()
     : symbol_lookup_worker_("Symbol Lookup Worker"),
       next_sink_cookie_(1),
       log_viewer_(this),
       ui_loop_(NULL),
       notify_log_view_new_items_(
          base::Bind(&ViewerWindow::NotifyLogViewNewItems,
                     base::Unretained(this))),
       notify_log_view_new_items_pending_(false),
       update_status_task_(base::Bind(&ViewerWindow::UpdateStatus,
                                      base::Unretained(this))),
       update_status_task_pending_(false),
       log_consumer_thread_("Event log consumer"),
       kernel_consumer_thread_("Kernel log consumer"),
       reset_settings_on_exit_(false) {
  ui_loop_ = base::MessageLoop::current();
  DCHECK(ui_loop_ != NULL);

  symbol_lookup_worker_.Start();
  DCHECK(symbol_lookup_worker_.message_loop() != NULL);

  status_callback_ = base::Bind(&ViewerWindow::OnStatusUpdate,
                                base::Unretained(this));
  symbol_lookup_service_.set_status_callback(status_callback_);

  symbol_lookup_service_.set_background_thread(
      symbol_lookup_worker_.message_loop());

  InitSymbolPath();
  symbol_lookup_service_.SetSymbolPath(symbol_path_.c_str());

  settings_.ReadProviders();
  settings_.ReadSettings();
}

ViewerWindow::~ViewerWindow() {
  // Last resort..
  StopCapturing();

  // Now, and not when the command was given: see reset_settings_on_exit_.
  // Everything that writes to the key has finished by the time a window
  // object is being destroyed.
  if (reset_settings_on_exit_ && !DeleteSavedSettings())
    LOG(ERROR) << "Could not delete the saved settings on exit.";

  symbol_lookup_worker_.Stop();

  notify_log_view_new_items_.Cancel();
  update_status_task_.Cancel();
}

namespace {

class ImportLogConsumer
    : public base::win::EtwTraceConsumerBase<ImportLogConsumer>,
      public LogParser,
      public KernelLogParser {
 public:
  ImportLogConsumer();
  ~ImportLogConsumer();

  static void ProcessEvent(PEVENT_TRACE event);

 private:
  static ImportLogConsumer* current_;
};

ImportLogConsumer* ImportLogConsumer::current_ = NULL;

ImportLogConsumer::ImportLogConsumer() {
  DCHECK(current_ == NULL);
  current_ = this;
}

ImportLogConsumer::~ImportLogConsumer() {
  DCHECK(current_ == this);
  current_ = NULL;
}

void ImportLogConsumer::ProcessEvent(PEVENT_TRACE event) {
  DCHECK(current_ != NULL);

  if (!current_->LogParser::ProcessOneEvent(event) &&
      !current_->KernelLogParser::ProcessOneEvent(event)) {
    LOG(INFO) << "Unknown event";
  }
}

}  // namespace

void ViewerWindow::ImportLogFiles(const std::vector<base::FilePath>& paths) {
  UISetText(0, L"Importing");
  UIUpdateStatusBar();

  ImportLogConsumer import_consumer;

  // Open all the log files.
  for (size_t i = 0; i < paths.size(); ++i) {
    HRESULT hr = import_consumer.OpenFileSession(paths[i].value().c_str());

    if (FAILED(hr)) {
      std::wstring msg =
          base::StringPrintf(L"Failed to open log file \"%ls\", error 0x%08X",
                             paths[i].value().c_str(),
                             hr);

      ::MessageBox(m_hWnd, msg.c_str(), L"Error Importing Logs", MB_OK);
      return;
    }
  }

  // Attach our event sinks to the consumer.
  import_consumer.set_event_sink(this);
  import_consumer.set_trace_sink(this);
  import_consumer.set_process_event_sink(&process_info_service_);
  import_consumer.set_module_event_sink(&symbol_lookup_service_);

  // Consume the files.
  // TODO(siggi): Report progress here.
  HRESULT hr = import_consumer.Consume();
  if (FAILED(hr)) {
    std::wstring msg =
        base::StringPrintf(L"Import failed with error 0x%08X", hr);
    ::MessageBox(m_hWnd, msg.c_str(), L"Error Importing Logs", MB_OK);
  }

  // Not a hardcoded "Ready": importing is only possible while not capturing
  // today, but deriving it means this cannot go stale if that changes.
  SetIdleStatus();
}

const wchar_t kLogFileFilter[] =
    L"Event Trace Files\0*.etl\0"
    L"All Files\n\0*.*\0";

const wchar_t* ViewerWindow::GetIdleStatusText() const {
  // log_controller_.session() is the authority on whether a trace session is
  // live; SetCapture, StartCapturing and StopCapturing all derive from it
  // rather than from a separate flag, so there is nothing to keep in step.
  return log_controller_.session() != NULL ? L"Capturing" : L"Ready";
}

void ViewerWindow::SetIdleStatus() {
  UISetText(0, GetIdleStatusText());
  UIUpdateStatusBar();
}

void ViewerWindow::SetCapture(bool capture) {
  bool capturing = (log_controller_.session() != NULL);
  if (capturing != capture) {
    if (capture) {
      if (!StartCapturing()) {
        capture = false;
        StopCapturing();
      }
    } else {
      StopCapturing();
    }
  }

  // Only allow import when not capturing.
  UIEnable(ID_FILE_IMPORT, !capture);
  UISetCheck(ID_LOG_CAPTURE, capture);

  // Reflect the new state. This is the single place capture is turned on or
  // off -- OnToggleCapture and the --start-capture switch both come through
  // here -- so it is the only place that needs to.
  SetIdleStatus();
}

LRESULT ViewerWindow::OnImport(
    WORD code, LPARAM lparam, HWND wnd, BOOL& handled) {
  CMultiFileDialog dialog(NULL, NULL, 0, kLogFileFilter, m_hWnd);

  if (dialog.DoModal() == IDOK) {
    std::vector<base::FilePath> paths;

    std::wstring path;
    int len = dialog.GetFirstPathName(NULL, 0);
    DCHECK(len != 0);
    path.resize(len);
    len = dialog.GetFirstPathName(&path[0], path.size());
    DCHECK(len != 0);

    do {
      paths.push_back(base::FilePath(path));

      len = dialog.GetNextPathName(NULL, 0);
      if (len != 0) {
        path.resize(len);
        len = dialog.GetNextPathName(&path[0], path.size());
      }
    } while (len != 0);

    ImportLogFiles(paths);
  }

  return 0;
}

LRESULT ViewerWindow::OnExit(
    WORD code, LPARAM lparam, HWND wnd, BOOL& handled) {
  PostMessage(WM_CLOSE);
  return 0;
}

LRESULT ViewerWindow::OnAbout(
    WORD code, LPARAM lparam, HWND wnd, BOOL& handled) {
  CSimpleDialog<IDD_ABOUT> dialog;
  dialog.DoModal(m_hWnd);
  return 0;
}

void ViewerWindow::StopCapturing() {
  log_controller_.Stop(NULL);
  kernel_controller_.Stop(NULL);
  log_consumer_thread_.Stop();
  log_consumer_.reset();

  kernel_consumer_thread_.Stop();
  kernel_consumer_.reset();
}

static bool TestAndOfferToStopSession(HWND parent,
                                      const wchar_t* session_name) {
  // Try and query the session properties.
  // This can only succeed if the session exists.
  base::win::EtwTraceProperties props;
  HRESULT hr = base::win::EtwTraceController::Query(session_name, &props);
  if (SUCCEEDED(hr)) {
    std::wstring str;
    str = base::StringPrintf(
        L"The log trace session \"%ls\" is already in use. "
        L"You may have another copy of Sawbuck running already, or some other "
        L"application may be using the session, or (shudder) Sawbuck may have "
        L"crashed previously.\n"
        L"Press OK to close the session and start capturing.",
            session_name);

    int result = ::MessageBox(parent,
                              str.c_str(),
                              L"Trace Session in use",
                              MB_OKCANCEL);

    if (result == IDOK) {
      // User pressed OK, attempt to stop the session.
      hr = base::win::EtwTraceController::Stop(session_name, &props);
      if (FAILED(hr)) {
        str = base::StringPrintf(L"Failed to stop trace session \"%ls\".",
                                 session_name);
        ::MessageBox(parent, str.c_str(), L"Error", MB_OK);
        return false;
      }
    } else {
      // User cancelled.
      return false;
    }
  }

  return true;
}

// Reports why capturing could not start, and returns false so each failure
// path below can stay a single line.
//
// Worth a message box rather than a log line nobody sees: every one of those
// paths used to 'return false' silently, and SetCapture then quietly undid the
// menu check -- so the entire symptom of running Sawbuck unelevated was that
// Capture appeared to do nothing at all. Starting the NT Kernel Logger needs
// administrator rights, so that is the common case by a wide margin.
//
// The preflight above deliberately does not come through here. It has already
// told the user the session was in use and been declined, and a second box
// saying the same thing would be worse than none.
static bool ReportCaptureFailure(HWND parent,
                                 const wchar_t* what,
                                 HRESULT hr) {
  std::wstring message = base::StringPrintf(
      L"%ls\n\n"
      L"Error 0x%08X.\n\n"
      L"Capturing starts an ETW session and the NT Kernel Logger, and the "
      L"kernel logger cannot be started without administrator rights. If "
      L"Sawbuck is not running elevated, that is almost certainly the "
      L"reason -- 0x%08X is access denied.",
      what,
      // Cast because %X takes an unsigned int and HRESULT is a long. MSVC
      // does not check this (PRINTF_FORMAT is a no-op there) but clang-cl
      // does, and gyp_sawbuck.py can select it.
      static_cast<unsigned int>(hr),
      static_cast<unsigned int>(HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED)));

  ::MessageBox(parent, message.c_str(), L"Cannot start capturing",
               MB_OK | MB_ICONERROR);
  return false;
}

bool ViewerWindow::StartCapturing() {
  DCHECK(NULL == log_controller_.session());
  DCHECK(NULL == kernel_controller_.session());
  DCHECK(NULL == log_consumer_.get());
  DCHECK(NULL == kernel_consumer_.get());

  // Preflight the start operation by seeing whether one of the log sessions
  // we're going to establish are already in use, and offer to stop them if so.
  if (!TestAndOfferToStopSession(m_hWnd, kSessionName) ||
      !TestAndOfferToStopSession(m_hWnd, KERNEL_LOGGER_NAME)) {
    // One or both log sessions still in use.
    return false;
  }

  // Create a session for our log message capturing.
  base::win::EtwTraceProperties log_props;
  EVENT_TRACE_PROPERTIES* p = log_props.get();
  // Use the QPC timer, see
  // http://msdn.microsoft.com/en-us/library/aa364160(v=vs.85).aspx.
  p->Wnode.ClientContext = 1;
  p->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
  p->MaximumFileSize = 100;  // 100 M file size.
  // Get image load and process events.
  p->FlushTimer = 1;  // flush every second.
  p->BufferSize = 16;  // 16 K buffers.
  HRESULT hr = log_controller_.Start(kSessionName, &log_props);
  if (FAILED(hr)) {
    return ReportCaptureFailure(
        m_hWnd, L"Could not start the Sawbuck log trace session.", hr);
  }

  // And open a consumer on it.
  log_consumer_.reset(new LogConsumer());
  log_consumer_->set_event_sink(this);
  log_consumer_->set_trace_sink(this);
  hr = log_consumer_->OpenRealtimeSession(kSessionName);
  if (FAILED(hr)) {
    return ReportCaptureFailure(
        m_hWnd, L"Could not open the Sawbuck log trace session.", hr);
  }

  // Consume it in a new thread.
  CHECK(log_consumer_thread_.Start());
  log_consumer_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(base::IgnoreResult(&LogConsumer::Consume),
                 base::Unretained(log_consumer_.get())));

  // Start the kernel logger session.
  base::win::EtwTraceProperties kernel_props;
  p = kernel_props.get();
  p->Wnode.Guid = SystemTraceControlGuid;
  // Use the QPC timer, see
  // http://msdn.microsoft.com/en-us/library/aa364160(v=vs.85).aspx.
  p->Wnode.ClientContext = 1;
  p->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
  p->MaximumFileSize = 100;  // 100 M file size.
  // Get image load and process events.
  p->EnableFlags = EVENT_TRACE_FLAG_IMAGE_LOAD | EVENT_TRACE_FLAG_PROCESS;
  p->FlushTimer = 1;  // flush every second.
  p->BufferSize = 16;  // 16 K buffers.
  hr = kernel_controller_.Start(KERNEL_LOGGER_NAME, &kernel_props);
  if (FAILED(hr)) {
    return ReportCaptureFailure(
        m_hWnd, L"Could not start the NT Kernel Logger session.", hr);
  }

  // And open a consumer on it.
  kernel_consumer_.reset(new KernelLogConsumer());
  DCHECK(NULL != kernel_consumer_.get());
  kernel_consumer_->set_module_event_sink(&symbol_lookup_service_);
  kernel_consumer_->set_process_event_sink(&process_info_service_);
  kernel_consumer_->set_is_64_bit_log(Is64BitSystem());
  hr = kernel_consumer_->OpenRealtimeSession(KERNEL_LOGGER_NAME);
  if (FAILED(hr)) {
    return ReportCaptureFailure(
        m_hWnd, L"Could not open the NT Kernel Logger session.", hr);
  }

  // Consume it in a new thread.
  CHECK(kernel_consumer_thread_.Start());
  kernel_consumer_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(base::IgnoreResult(&KernelLogConsumer::Consume),
                 base::Unretained(kernel_consumer_.get())));

  if (SUCCEEDED(hr))
    EnableProviders(settings_);

  return SUCCEEDED(hr);
}

void ViewerWindow::EnableProviders(
    const ProviderConfiguration& settings) {
  for (size_t i = 0; i < settings.settings().size(); ++i) {
    log_controller_.EnableProvider(
        settings.settings()[i].provider_guid,
        settings.settings()[i].log_level,
        settings.settings()[i].enable_flags);
  }
}

void ViewerWindow::OnLogMessage(const LogEvents::LogMessage& log_message) {
  ViewerWindow::LogMessage msg;
  msg.level = log_message.level;
  msg.process_id = log_message.process_id;
  msg.thread_id = log_message.thread_id;
  msg.time_stamp = log_message.time;

  // Use regular expression matching to extract the
  // file/line/message from the log string, which is of
  // format "[<stuff>:<file>(<line>)] <message><ws>".
  if (!kFileRe.FullMatch(
      pcrecpp::StringPiece(log_message.message, log_message.message_len),
                           &msg.file, &msg.line, &msg.message)) {
    // As fallback, just slurp the entire string.
    msg.message.assign(log_message.message, log_message.message_len);
  }

  // If the message carried file information, use that
  // in preference to the above.
  if (log_message.file_len != 0) {
    msg.file.assign(log_message.file, log_message.file_len);
    msg.line = log_message.line;
  }

  if (log_message.trace_depth > 0) {
    msg.trace.insert(msg.trace.begin(),
                     &log_message.traces[0],
                     &log_message.traces[log_message.trace_depth - 1]);
  }

  base::AutoLock lock(list_lock_);
  log_messages_.push_back(msg);

  ScheduleNewItemsNotification();
}

void ViewerWindow::OnStatusUpdate(const wchar_t* status) {
  base::AutoLock lock(status_lock_);
  const wchar_t kBackSpace = 0x08;
  if (status_.find_first_of(L"\r\n") == std::wstring::npos) {
    // No EOL in current status, backup for every backspace char.
    for (; *status != L'\0'; ++status) {
      if (*status == kBackSpace) {
        if (status_.length() > 0)
          status_.resize(status_.length() - 1);
      } else {
        status_ += *status;
      }
    }
  } else {
    // EOL in current status, just replace it.
    status_ = status;
  }

  // Post a task to update the status on the UI thread, unless
  // there's a task already pending.
  if (!update_status_task_pending_) {
    update_status_task_pending_ = true;
    ui_loop_->PostTask(FROM_HERE, update_status_task_.callback());
  }
}

void ViewerWindow::UpdateStatus() {
  DCHECK_EQ(base::MessageLoop::current(), ui_loop_);

  std::wstring status;
  {
    base::AutoLock lock(status_lock_);
    update_status_task_pending_ = false;
    status = status_;
  }

  // Symbol lookup drives this, and reports an empty string when it has
  // nothing in flight. Blanking the bar then loses the one piece of state
  // worth showing at rest, so fall back to the idle text -- which is how
  // "Capturing" appears when nothing else is competing for the space.
  if (status.empty()) {
    SetIdleStatus();
    return;
  }

  UISetText(0, status.c_str());
}

void ViewerWindow::OnTraceEventBegin(
    const TraceEvents::TraceMessage& trace_message) {
  AddTraceEventToLog("BEGIN", trace_message);
}

void ViewerWindow::OnTraceEventEnd(
    const TraceEvents::TraceMessage& trace_message) {
  AddTraceEventToLog("END", trace_message);
}

void ViewerWindow::OnTraceEventInstant(
    const TraceEvents::TraceMessage& trace_message) {
  AddTraceEventToLog("INSTANT", trace_message);
}

void ViewerWindow::AddTraceEventToLog(const char* type,
    const TraceEvents::TraceMessage& trace_message) {
  ViewerWindow::LogMessage msg;
  msg.level = trace_message.level;
  msg.process_id = trace_message.process_id;
  msg.thread_id = trace_message.thread_id;
  msg.time_stamp = trace_message.time;

  // The message will be of form "{BEGIN|END|INSTANT}(<name>, 0x<id>): <extra>"
  msg.message = base::StringPrintf("%s(%*s, 0x%08X): %*s",
                                   type,
                                   trace_message.name_len,
                                   trace_message.name,
                                   trace_message.id,
                                   trace_message.extra_len,
                                   trace_message.extra);

  for (size_t i = 0; i < trace_message.trace_depth; ++i)
    msg.trace.push_back(trace_message.traces[i]);

  base::AutoLock lock(list_lock_);
  log_messages_.push_back(msg);

  ScheduleNewItemsNotification();
}

void ViewerWindow::ScheduleNewItemsNotification() {
  // The list lock must be held.
  list_lock_.AssertAcquired();

  if (!notify_log_view_new_items_pending_) {
    notify_log_view_new_items_pending_ = true;
    ui_loop_->PostTask(FROM_HERE, notify_log_view_new_items_.callback());
  }
}

void ViewerWindow::NotifyLogViewNewItems() {
  DCHECK_EQ(ui_loop_, base::MessageLoop::current());
  {
    base::AutoLock lock(list_lock_);

    // Notification no longer pending.
    notify_log_view_new_items_pending_ = false;
  }

  EventSinkMap::iterator it(event_sinks_.begin());
  for (; it != event_sinks_.end(); ++it) {
    it->second->LogViewNewItems();
  }
}

void ViewerWindow::NotifyLogViewCleared() {
  DCHECK_EQ(ui_loop_, base::MessageLoop::current());
  EventSinkMap::iterator it(event_sinks_.begin());
  for (; it != event_sinks_.end(); ++it) {
    it->second->LogViewCleared();
  }
}

LRESULT ViewerWindow::OnConfigureProviders(WORD code,
                                           LPARAM lparam,
                                           HWND wnd,
                                           BOOL& handled) {
  // Make a copy of our settings.
  ProviderConfiguration settings_copy;

  settings_copy.Copy(settings_);

  ProviderDialog dialog(&settings_copy);
  if (dialog.DoModal(m_hWnd) == IDOK) {
    settings_.Copy(settings_copy);
    EnableProviders(settings_);
    settings_.WriteSettings();
  }

  return 0;
}

LRESULT ViewerWindow::OnRegisterProviders(WORD code,
                                          LPARAM lparam,
                                          HWND wnd,
                                          BOOL& handled) {
  LONG err = settings_.RegisterDefaultProviders();

  if (err != ERROR_SUCCESS) {
    // Just the code. logging::SystemErrorCodeToString would give the text,
    // but it returns a std::string from FormatMessageA -- ANSI, not UTF-8 --
    // so widening it correctly needs more machinery than the message is
    // worth. Naming the one error anyone will actually hit is more use.
    std::wstring message = base::StringPrintf(
        L"Could not write the default log provider registrations.\n\n"
        L"Error %d.\n\n"
        L"These live under HKEY_LOCAL_MACHINE, so writing them needs "
        L"administrator rights -- error 5 is access denied.",
        err);
    ::MessageBox(m_hWnd, message.c_str(), L"Register Default Providers",
                 MB_OK | MB_ICONERROR);
    return 0;
  }

  // Pick up what was just written. ReadProviders replaces the provider list
  // wholesale and ReadSettings layers the saved per-provider levels back over
  // it, so this is the same pair the constructor runs. EnableProviders then
  // applies them to a capture session if one is running, which matches what
  // OnConfigureProviders does once the dialog is accepted -- without it a
  // provider registered mid-session would not start producing events until
  // capture was stopped and restarted.
  settings_.ReadProviders();
  settings_.ReadSettings();
  EnableProviders(settings_);

  std::wstring message = base::StringPrintf(
      L"Registered %d default log providers.\n\n"
      L"They are now listed under Configure Providers.",
      static_cast<int>(settings_.settings().size()));
  ::MessageBox(m_hWnd, message.c_str(), L"Register Default Providers",
               MB_OK | MB_ICONINFORMATION);

  return 0;
}

LRESULT ViewerWindow::OnToggleCapture(WORD code,
                                      LPARAM lparam,
                                      HWND wnd,
                                      BOOL& handled) {
  bool capturing = log_controller_.session() != NULL;
  DCHECK_EQ(capturing,
            ((UIGetState(ID_LOG_CAPTURE) & UPDUI_CHECKED) == UPDUI_CHECKED));
  SetCapture(!capturing);

  return 0;
}

namespace {

class SymbolPathDialog: public CDialogImpl<SymbolPathDialog> {
 public:
  BEGIN_MSG_MAP(SymbolPathDialog)
    MSG_WM_INITDIALOG(OnInitDialog)
    COMMAND_RANGE_HANDLER(IDOK, IDNO, OnCloseCmd)
  END_MSG_MAP()

  static const int IDD = IDD_SYMBOLPATH;

  explicit SymbolPathDialog(std::wstring* symbol_path)
      : symbol_path_(symbol_path) {
    DCHECK(symbol_path != NULL);
  }

 private:
  BOOL OnInitDialog(CWindow focus, LPARAM init_param) {
    SetDlgItemText(IDC_SYMBOLPATH, symbol_path_->c_str());
    CenterWindow(GetParent());
    return TRUE;
  }

  LRESULT OnCloseCmd(WORD code, WORD id, HWND ctl, BOOL& handled) {
    ::EndDialog(m_hWnd, id);

    // Stash the new symbol path to the string we were handed on IDOK.
    HWND item = GetDlgItem(IDC_SYMBOLPATH);
    if (id == IDOK && item != NULL) {
      int length = ::GetWindowTextLength(item);
      symbol_path_->resize(length);
      length = ::GetWindowText(item, &(*symbol_path_)[0], length + 1);
      symbol_path_->resize(length);
    }

    return 0;
  }

  std::wstring* symbol_path_;
};

}  // namespace

bool ViewerWindow::DeleteSavedSettings() {
  // Opened with an empty subkey, which gives a second handle to HKCU itself,
  // so that the whole of config::kSettingsKey can be named in one call rather
  // than split into a parent path and a leaf here.
  base::win::RegKey key;
  LONG result = key.Open(HKEY_CURRENT_USER, L"", KEY_WRITE);
  if (result != ERROR_SUCCESS)
    return false;

  // Recursive -- base::win::RegKey::DeleteKey walks the subkeys itself, which
  // matters because Levels sits underneath.
  result = key.DeleteKey(config::kSettingsKey);

  // Nothing there is the same outcome as having removed it. That is the usual
  // case for a second call: the one in the destructor runs after the handler
  // has already deleted the tree, and only has anything to do because the
  // windows wrote it back on their way out.
  return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
}

LRESULT ViewerWindow::OnResetSettings(WORD code,
                                      LPARAM lparam,
                                      HWND wnd,
                                      BOOL& handled) {
  const wchar_t kPrompt[] =
      L"This clears everything Sawbuck has remembered: the window position "
      L"and size, the column layouts for all three lists, the stack trace pane "
      L"height, the cell outline setting, the symbol path, the filters, and "
      L"the per-provider log levels.\n"
      L"\n"
      L"Provider registrations are left alone. Those live under "
      L"HKEY_LOCAL_MACHINE and belong to the installer, not to these "
      L"settings.\n"
      L"\n"
      L"Reset them now?";
  if (::MessageBox(m_hWnd, kPrompt, L"Reset All Settings",
                   MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
    return 0;
  }

  if (!DeleteSavedSettings()) {
    ::MessageBox(m_hWnd,
                 L"Could not delete the saved settings.",
                 L"Reset All Settings",
                 MB_OK | MB_ICONERROR);
    return 0;
  }

  // Again on the way out, because this window and the list views all write
  // their state back as they are destroyed -- see reset_settings_on_exit_.
  // Without the second pass, closing Sawbuck would immediately recreate most
  // of what was just removed.
  reset_settings_on_exit_ = true;

  ::MessageBox(m_hWnd,
               L"Settings have been reset.\n"
               L"\n"
               L"The window and columns keep their current layout for this "
               L"session, and start from the defaults the next time Sawbuck "
               L"runs.",
               L"Reset All Settings",
               MB_OK | MB_ICONINFORMATION);
  return 0;
}

LRESULT ViewerWindow::OnHelp(WORD code,
                             LPARAM lparam,
                             HWND wnd,
                             BOOL& handled) {
  // Handed to explorer.exe rather than opened directly, because Sawbuck runs
  // elevated: the manifest asks for requireAdministrator, since controlling
  // an ETW session needs it, and viewer.gyp sets UACExecutionLevel to match.
  //
  // A process started from an elevated parent inherits the elevated token,
  // and a browser handed a URL that way either refuses to run or opens
  // nothing -- while ShellExecute reports success either way, so the failure
  // is completely silent. Passing the URL as explorer.exe's argument gets it
  // opened by the shell instead, which is running as the logged-on user:
  // explorer sees an instance is already up and forwards the request to it
  // rather than starting a second one.
  //
  // A NULL verb, so whatever is registered as the default for the file type
  // is what runs, rather than hardcoding "open".
  //
  // The return is an HINSTANCE for historical reasons and is not a handle:
  // anything at or above 33 is success, and anything at or below 32 is an
  // error code wearing a pointer's type.
  HINSTANCE result = ::ShellExecute(m_hWnd, NULL, L"explorer.exe", kHelpUrl,
                                    NULL, SW_SHOWNORMAL);
  INT_PTR shell_result = reinterpret_cast<INT_PTR>(result);
  if (shell_result <= 32) {
    // The code and the address both go in the message: the first is the only
    // thing that says why, and the second is the only useful thing left when
    // there is no browser to hand it to.
    // Cast rather than %Id, which is an MSVC spelling nothing else in this
    // tree uses. Every value that reaches here is 0 to 32.
    std::wstring message = base::StringPrintf(
        L"Could not open a web browser (error %d).\n\n%ls",
        static_cast<int>(shell_result), kHelpUrl);
    ::MessageBox(m_hWnd, message.c_str(), L"Help", MB_OK | MB_ICONERROR);
  }

  return 0;
}

LRESULT ViewerWindow::OnSymbolPath(WORD code,
                                   LPARAM lparam,
                                   HWND wnd,
                                   BOOL& handled) {
  SymbolPathDialog dialog(&symbol_path_);

  if (IDOK == dialog.DoModal(m_hWnd)) {
    Preferences pref;
    pref.WriteStringValue(config::kSymPathValue, symbol_path_);

    symbol_lookup_service_.SetSymbolPath(symbol_path_.c_str());
  }

  return 0;
}

BOOL ViewerWindow::OnIdle() {
  // Nothing to export with no rows. This is the unfiltered count, so a
  // filter that hides everything still leaves the item enabled; the
  // handler says so rather than the menu going quiet.
  UIEnable(ID_LOG_EXPORT, GetNumRows() > 0);

  UIUpdateMenuBar();
  UIUpdateStatusBar();

  return TRUE;
}

BOOL ViewerWindow::PreTranslateMessage(MSG* msg) {
  return SuperFrame::PreTranslateMessage(msg);
}

int ViewerWindow::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  // TODO(siggi): Make the toolbar useful.
  // CreateSimpleToolBar();

  // Import is enabled, except when capturing.
  UIEnable(ID_FILE_IMPORT, true);

  // Edit menu is disabled by default.
  UIEnable(ID_EDIT_CUT, false);
  UIEnable(ID_EDIT_COPY, false);
  UIEnable(ID_EDIT_PASTE, false);
  UIEnable(ID_EDIT_CLEAR, false);
  UIEnable(ID_EDIT_CLEAR_ALL, false);
  UIEnable(ID_EDIT_SELECT_ALL, false);
  UIEnable(ID_EDIT_FIND, false);
  UIEnable(ID_EDIT_FIND_NEXT, false);

  CreateSimpleStatusBar();
  UIAddStatusBar(m_hWndStatusBar);

  // Set the main window title.
  SetWindowText(kWindowTitle);

  log_viewer_.SetLogView(this);
  log_viewer_.SetSymbolLookupService(&symbol_lookup_service_);
  log_viewer_.SetProcessInfoService(&process_info_service_);

  log_viewer_.Create(m_hWnd,
                     NULL,
                     NULL,
                     WS_VISIBLE | WS_CHILDWINDOW | WS_CLIPCHILDREN,
                     WS_EX_CLIENTEDGE);

  // Set the list view as the client view.
  m_hWndClient = log_viewer_;

  // Retrieve our placement from registry if available, and
  // place our window to the last saved placement if so.
  //
  // Note that the key existing is not the same as the value existing: the
  // installer writes 'installed' under this same key, so it is there from the
  // moment Sawbuck is installed. Whether to use the default size is decided by
  // the inner read, not by this open.
  bool restored = false;
  CRegKey key;
  ULONG err = key.Open(HKEY_CURRENT_USER, config::kSettingsKey);
  if (err == ERROR_SUCCESS) {
    WINDOWPLACEMENT placement = { 0 };
    ULONG size = sizeof(placement);
    err = key.QueryBinaryValue(config::kWindowPosValue, &placement, &size);
    if (err == ERROR_SUCCESS && size == sizeof(placement)) {
      // If we were closed invisible, minimized, or any other weird show state,
      // we don't want to get back in that state. Force normal or maximized.
      if (placement.showCmd != SW_SHOWNORMAL &&
          placement.showCmd != SW_SHOWMAXIMIZED) {
        placement.showCmd = SW_SHOWNORMAL;
      }
      SetWindowPlacement(&placement);
      restored = true;
    }
  }

  // Not fatal if it fails -- the window keeps the size Windows chose for it at
  // creation, which is a usable window and not a broken one -- but it means
  // either the monitor could not be measured or the move was refused, and
  // neither should happen.
  if (!restored && !SetDefaultPlacement())
    LOG(WARNING) << "Could not apply the default window placement.";

  UpdateLayout();
  UIAddMenuBar(m_hWnd);

  CMessageLoop* loop = g_sawbuck_app_module.GetMessageLoop();
  DCHECK(loop != NULL);
  loop->AddMessageFilter(this);
  loop->AddIdleHandler(this);

  return 0;
}

bool ViewerWindow::SetDefaultPlacement() {
  // The work area rather than the whole screen, so the taskbar is not counted
  // as space we can use. Of the monitor this window landed on, which with
  // nothing saved is the primary one, but asking the monitor is still the
  // right question to ask.
  CRect work_area;
  HMONITOR monitor = ::MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO info = { sizeof(info) };
  if (monitor != NULL && ::GetMonitorInfo(monitor, &info)) {
    work_area = info.rcWork;
  } else if (!::SystemParametersInfo(SPI_GETWORKAREA, 0, &work_area, 0)) {
    // Neither answered, which should not happen. Leave the window at whatever
    // size it was created with -- CW_USEDEFAULT, so Windows chose it -- rather
    // than guess at a screen we failed to measure.
    return false;
  }

  int width = MulDiv(work_area.Width(), kDefaultWorkAreaFraction, 100);
  int height = MulDiv(work_area.Height(), kDefaultWorkAreaFraction, 100);

  // The floor, then the ceiling, and in that order. Raising to the minimum
  // first and capping to the work area second means a display too small for
  // the minimum gets a window the size of its work area rather than one
  // hanging off the edge of the screen. That is not hypothetical: 60% of a
  // 1024x768 XP desktop is 614x443, which the floor lifts to 1180x600, which
  // is wider than the screen.
  if (width < kMinDefaultWidth)
    width = kMinDefaultWidth;
  if (height < kMinDefaultHeight)
    height = kMinDefaultHeight;
  if (width > work_area.Width())
    width = work_area.Width();
  if (height > work_area.Height())
    height = work_area.Height();

  // Centred in the work area, computed here rather than with CenterWindow()
  // so that it is the work area we centre in and not the whole screen -- a
  // window centred on the screen sits slightly low when the taskbar is at the
  // bottom.
  int x = work_area.left + (work_area.Width() - width) / 2;
  int y = work_area.top + (work_area.Height() - height) / 2;

  return SetWindowPos(NULL, x, y, width, height,
                      SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
}

void ViewerWindow::OnGetMinMaxInfo(LPMINMAXINFO min_max_info) {
  // A floor on the resize, not on the default size above. Set unconditionally
  // and not merged with whatever the default frame reported, because the
  // default is far smaller than this -- SM_CXMINTRACK by SM_CYMINTRACK, about
  // 136 by 39 -- so taking the larger would always pick ours anyway.
  //
  // ptMinTrackSize is the whole window including its frame and caption, which
  // is the measure the user is dragging.
  min_max_info->ptMinTrackSize.x = kMinTrackWidth;
  min_max_info->ptMinTrackSize.y = kMinTrackHeight;

  // Deliberately not calling SetMsgHandled(FALSE) to pass this on. Of the
  // classes ViewerWindow chains to -- CUpdateUI and CFrameWindowImpl -- neither
  // handles WM_GETMINMAXINFO, so there is nothing downstream to consult and
  // the values above are the whole answer. The one WTL class that does handle
  // it, CDialogResize, does exactly this: assigns ptMinTrackSize from a
  // stored point and returns 0.
}

void ViewerWindow::OnDestroy() {
  // Get our Window placement and stash it in registry.
  WINDOWPLACEMENT placement = { sizeof(placement) };
  if (GetWindowPlacement(&placement)) {
    CRegKey key;

    ULONG err = key.Create(HKEY_CURRENT_USER, config::kSettingsKey);
    if (err == ERROR_SUCCESS)
      key.SetBinaryValue(config::kWindowPosValue,
                         &placement,
                         sizeof(placement));
  }

  // Wind up this program.
  ::PostQuitMessage(1);
}

int ViewerWindow::GetNumRows() {
  base::AutoLock lock(list_lock_);
  return log_messages_.size();
}

void ViewerWindow::ClearAll() {
  {
    base::AutoLock lock(list_lock_);
    log_messages_.clear();
  }
  NotifyLogViewCleared();
}

int ViewerWindow::GetSeverity(int row) {
  base::AutoLock lock(list_lock_);
  return log_messages_[row].level;
}

DWORD ViewerWindow::GetProcessId(int row) {
  base::AutoLock lock(list_lock_);
  return log_messages_[row].process_id;
}

DWORD ViewerWindow::GetThreadId(int row) {
  base::AutoLock lock(list_lock_);
  return log_messages_[row].thread_id;
}

base::Time ViewerWindow::GetTime(int row) {
  base::AutoLock lock(list_lock_);
  return log_messages_[row].time_stamp;
}

std::string ViewerWindow::GetFileName(int row) {
  base::AutoLock lock(list_lock_);
  return log_messages_[row].file;
}

int ViewerWindow::GetLine(int row) {
  base::AutoLock lock(list_lock_);
  return log_messages_[row].line;
}

std::string ViewerWindow::GetMessage(int row) {
  base::AutoLock lock(list_lock_);
  return log_messages_[row].message;
}

void ViewerWindow::GetStackTrace(int row, std::vector<void*>* trace) {
  base::AutoLock lock(list_lock_);
  *trace = log_messages_[row].trace;
}

void ViewerWindow::Register(ILogViewEvents* event_sink,
                            int* registration_cookie) {
  int cookie = next_sink_cookie_++;

  event_sinks_.insert(std::make_pair(cookie, event_sink));
  *registration_cookie = cookie;
}

void ViewerWindow::Unregister(int registration_cookie) {
  event_sinks_.erase(registration_cookie);
}

void ViewerWindow::InitSymbolPath() {
  {
    // Attempt to read our current preference if one exists.
    Preferences pref;
    if (pref.ReadStringValue(config::kSymPathValue, &symbol_path_, NULL))
      return;
  }

  // No preference, see if there's a fallback in the environment.
  scoped_ptr<base::Environment> env(base::Environment::Create());
  std::string nt_symbol_path;
  if (!env.get() || !env->GetVar("_NT_SYMBOL_PATH", &nt_symbol_path)) {
    // We have no symbol path, make one up!
    base::FilePath temp_dir;
    if (!PathService::Get(base::DIR_TEMP, &temp_dir))
      return;

    base::FilePath sym_dir(temp_dir.Append(L"symbols"));
    if (!base::CreateDirectory(sym_dir))
      return;

    // https where TLS 1.2 will actually be negotiated, http otherwise. Both
    // of these servers require TLS 1.2 now, so this is about reachability
    // rather than preference.
    //
    // Windows 8.1 is where an untouched install negotiates TLS 1.2 for a
    // caller that does not ask for it by name, which symsrv does not. That is
    // a different line from where TLS 1.2 is *possible*: every Windows from
    // XP SP3 up can do it, given the update for its version -- KB4019276 for
    // the XP/POSReady and Server 2008 SP2 family, Vista SP2 behind KB4493471,
    // KB3140245 for 7, 8 and Server 2012 -- plus the registry values.
    //
    // 8.1 rather than 8 because KB3140245 is offered for Windows 8 too: its
    // WinHTTP needs the same update before DefaultSecureProtocols means
    // anything, so a stock 8 is in the same position as a stock 7. The line
    // has to be drawn at what a bare install does with nothing but its
    // service pack -- which is what a VM kept around for debugging on legacy
    // Windows actually is, and the only safe assumption about anyone else's
    // machine.
    //
    // So below 8.1, ask instead of assuming. The short-circuit means the
    // probe only runs where the answer is in doubt.
    //
    // A no is always safe. Both servers still answer over plain http with no
    // redirect, so http works on every version either way; it is a wrong yes
    // that costs something, and only symbol resolution.
    const bool use_https =
        base::win::GetVersion() >= base::win::VERSION_WIN8_1 ||
        IsTls12EnabledForWinHttp();

    // Pointers rather than strings: these feed %ls directly below, and
    // base::StringPrintf is variadic, so a std::wstring would have to be
    // .c_str()'d anyway and a std::string could not be used at all.
    const wchar_t* const microsoft_sym_srv =
        use_https ? kMicrosoftSymSrvHttps : kMicrosoftSymSrvHttp;
    const wchar_t* const chrome_sym_srv =
        use_https ? kChromeSymSrvHttps : kChromeSymSrvHttp;

    symbol_path_ =
        base::StringPrintf(L"SRV*%ls*%ls;SRV*%ls*%ls",
                           sym_dir.value().c_str(),
                           microsoft_sym_srv,
                           sym_dir.value().c_str(),
                           chrome_sym_srv);

    // Worth a line in the log, because this decision is made once and then
    // kept: the path is written to preferences immediately below, and the
    // read at the top of this function returns early ever after. So enabling
    // TLS 1.2 on an older Windows *after* a first run leaves the http path in
    // place until the preference is cleared or the Symbol Path dialog is used.
    LOG(INFO) << "Using symbol path over "
              << (use_https ? "HTTPS" : "HTTP") << ".";

    // Write the newly fabricated path to our preferences.
    Preferences pref;
    pref.WriteStringValue(config::kSymPathValue, symbol_path_);
  } else {
    symbol_path_ = base::UTF8ToWide(nt_symbol_path);
  }
}
