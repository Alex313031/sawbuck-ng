// Copyright 2009 Google Inc.
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
// Log viewer module declaration.
#include "sawbuck/viewer/viewer_module.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/logging_win.h"
#include "base/run_loop.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_win.h"
#include "sawbuck/viewer/viewer_window.h"
#include "unicode/uclean.h"
#include "unicode/utypes.h"

#include <initguid.h>  // NOLINT
#include "sawbuck/viewer/sawbuck_guids.h"  // NOLINT

HRESULT SawbuckAppModule::Init(ATL::_ATL_OBJMAP_ENTRY* obj_map,
                               HINSTANCE instance,
                               const GUID* lib_id) {
  HRESULT hr = ::OleInitialize(NULL);
  if (FAILED(hr))
    return hr;

  return CAppModule::Init(obj_map, instance, lib_id);
}

void SawbuckAppModule::Term() {
  CAppModule::Term();
  ::OleUninitialize();
}

SawbuckAppModule g_sawbuck_app_module;

// This class makes a Frankenstein wedding between the WTL and the Chrome
// base message loop classes. Dispatching events through the Chrome base
// message loop from the main UI thread allows us to use the very nice
// task primitives to dispatch work back to the UI thread. WTL on
// the other hand, requires a CMessageLoop (derivative) to dispatch
// Window events, which is how we get this ... well ... abomination.
class HybridMessageLoopObserver
    : public CMessageLoop,
      public base::MessagePumpDispatcher,
      public base::MessageLoop::TaskObserver {
 public:
  HybridMessageLoopObserver() : idle_scheduled_(false) {
  }

  // @name base::MessagePumpDispatcher implementation.
  // Implemented to hook in WTL PreTranslateMessage.
  // @{
  virtual uint32_t Dispatch(const base::NativeEvent& event) override;
  // @}

  // @name base::MessageLoop::TaskObserver implementation.
  // Implemented to keep a task out for WTL idle processing.
  // @{
  virtual void WillProcessTask(const base::PendingTask& pending_task) override;
  virtual void DidProcessTask(const base::PendingTask& pending_task) override;
  // @}

 private:
  void MaybeScheduleIdleTask();
  void OnIdleTask();

  bool idle_scheduled_;
};

uint32_t HybridMessageLoopObserver::Dispatch(const base::NativeEvent& event) {
  // Make sure menus, toolbars and such are updated after event is handled.
  MaybeScheduleIdleTask();

  // Offer the message to WTL's filters before the pump dispatches it. This is
  // the "hook in WTL PreTranslateMessage" the class comment above describes,
  // and it had been missing: CMessageLoop::PreTranslateMessage walks the
  // registered CMessageFilters, which is how a message reaches
  // ViewerWindow::PreTranslateMessage and from there CFrameWindowImpl's, the
  // one that finally calls TranslateAccelerator on the IDR_MAIN_FRAME table.
  //
  // Without it no accelerator did anything, while the matching menu items all
  // worked -- a menu click arrives as a plain WM_COMMAND and needs no
  // translation, whereas Ctrl+C arrives as WM_KEYDOWN and is only turned into
  // WM_COMMAND by TranslateAccelerator.
  //
  // When a filter reports the message handled, return POST_DISPATCH_NONE so
  // the pump does not also TranslateMessage/DispatchMessage it: see
  // MessagePumpForUI::ProcessMessageHelper, which performs the default action
  // only when POST_DISPATCH_PERFORM_DEFAULT comes back. Dispatching as well
  // would deliver the original keystroke on top of the command it generated.
  MSG msg = event;
  if (CMessageLoop::PreTranslateMessage(&msg))
    return POST_DISPATCH_NONE;

  return POST_DISPATCH_PERFORM_DEFAULT;
}

void HybridMessageLoopObserver::WillProcessTask(
    const base::PendingTask& pending_task) {
  // Make sure we idle to update menus and such after each task or batch of
  // tasks has been handled.
  MaybeScheduleIdleTask();
}

void HybridMessageLoopObserver::DidProcessTask(
    const base::PendingTask& pending_task) {
  // Intentionally empty.
}

void HybridMessageLoopObserver::MaybeScheduleIdleTask() {
  // Keep zero or one task outstanding at any time.
  if (idle_scheduled_)
    return;

  idle_scheduled_ = true;
  base::MessageLoop::current()->PostNonNestableTask(
      FROM_HERE, base::Bind(&HybridMessageLoopObserver::OnIdleTask,
                            base::Unretained(this)));
}

void HybridMessageLoopObserver::OnIdleTask() {
  idle_scheduled_ = false;

  // Perform idle processing and re-schedule if we had some work.
  if (OnIdle(0))
    MaybeScheduleIdleTask();
}

int APIENTRY wWinMain(HINSTANCE instance,
                      HINSTANCE prev_instance,
                      LPTSTR /*cmd_line*/,
                      int show) {
  base::CommandLine::Init(0, NULL);
  base::AtExitManager at_exit;

  // Initialize ICU.
  CHECK(base::i18n::InitializeICU());

  // InitializeICU succeeding is not the same as the data being usable. All it
  // does on this path is mmap icudtl.dat and hand the pointer to
  // udata_setCommonData, which registers it without parsing the package,
  // checking the table of contents or comparing versions. A stale or
  // mismatched data file therefore passes the CHECK above and only shows up
  // later, item by item, as ICU lookups miss and fall back -- which renders as
  // corrupted text rather than any kind of error.
  //
  // Two things make that easy to hit. The file name carries no version, on
  // purpose: base/i18n/icu_util.cc says "Use an unversioned file name to
  // simplify a icu version update down the road", so a 4.6 and a 54.1 data
  // file are indistinguishable on disk. And it arrives in the output
  // directory by way of a copy step, so an incremental build that decides the
  // copy is up to date leaves the previous version in place -- which is
  // exactly how this was found, on Build Solution rather than Rebuild
  // Solution, after an ICU version bump.
  //
  // u_init forces the common data to be loaded and validated now, turning all
  // of that into one loud failure at startup.
  UErrorCode icu_status = U_ZERO_ERROR;
  u_init(&icu_status);
  CHECK(U_SUCCESS(icu_status))
      << "ICU data is unusable: " << u_errorName(icu_status)
      << ". icudtl.dat next to the executable is most likely stale or from a "
      << "different ICU version -- this build expects " << U_ICU_VERSION
      << ". A full rebuild, or deleting it and rebuilding, should replace it.";

  // Init logging to no file logging.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_NONE;
  logging::InitLogging(settings);
  logging::LogEventProvider::Initialize(kSawbuckLogProvider);

  ::OleInitialize(NULL);
  ::InitCommonControls();

  g_sawbuck_app_module.Init(NULL, instance, NULL);

  // Initialize the WTL message loop hookup.
  HybridMessageLoopObserver observer;
  g_sawbuck_app_module.AddMessageLoop(&observer);

  // Instantiate the base message loop, and plumb the WTL hookup.
  base::MessageLoopForUI message_loop;
  message_loop.AddTaskObserver(&observer);

  ViewerWindow window;
  window.CreateEx();
  window.ShowWindow(show);
  window.UpdateWindow();

  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch("import")) {
    std::vector<std::wstring> files(cmd_line->GetArgs());
    std::vector<base::FilePath> paths;
    for (size_t i = 0; i < files.size(); ++i) {
      paths.push_back(base::FilePath(files[i]));
    }
    window.ImportLogFiles(paths);
  } else if (cmd_line->HasSwitch("start-capture")) {
    window.SetCapture(true);
  }

  // Run the ugly, hybrid message loop with the observer/dispatcher.
  base::RunLoop run_loop(&observer);
  run_loop.Run();

  g_sawbuck_app_module.RemoveMessageLoop();

  g_sawbuck_app_module.Term();

  return 0;
}
