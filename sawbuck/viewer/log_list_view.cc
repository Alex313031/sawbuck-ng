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
// Log viewer window implementation.
#include "sawbuck/viewer/log_list_view.h"

#include <stdint.h>
#include <string.h>

#include <atlalloc.h>
#include <atldlgs.h>
#include <wmistr.h>
#include <evntrace.h>
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/i18n/time_formatting.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "pcrecpp.h"  // NOLINT
#include "sawbuck/log_lib/process_info_service.h"
#include "sawbuck/viewer/const_config.h"
#include "sawbuck/viewer/resource.h"
#include "sawbuck/viewer/stack_trace_list_view.h"

namespace {

std::string GetSeverityText(UCHAR severity) {
  switch (severity)  {
    case TRACE_LEVEL_NONE:
      return "NONE";
    case TRACE_LEVEL_FATAL:
      return "FATAL";
    case TRACE_LEVEL_ERROR:
      return "ERROR";
    case TRACE_LEVEL_WARNING:
      return "WARNING";
    case TRACE_LEVEL_INFORMATION:
      return "INFORMATION";
    case TRACE_LEVEL_VERBOSE:
      return "VERBOSE";
  }

  // Anything above VERBOSE is a VLOG verbosity level, not a reserved ETW
  // level. base/logging_win.cc encodes VLOG(N) as
  //     TRACE_LEVEL_INFORMATION - severity,  with severity == -N
  // so VLOG(1) arrives as TRACE_LEVEL_VERBOSE and VLOG(2) and up arrive as
  // 6, 7, 8 ... with no upper bound. Labelling those "RESERVED6".."RESERVED9"
  // and then "UNKNOWN", as this used to, was doubly wrong: they are not
  // reserved, and every VLOG above 5 became indistinguishable.
  if (severity > TRACE_LEVEL_VERBOSE)
    return base::StringPrintf("VERBOSE%d",
                              severity - TRACE_LEVEL_INFORMATION);

  return base::StringPrintf("LEVEL %d", severity);
}

// Returns true iff state indicates a selected listview item.
bool IsSelected(UINT state) {
  return (state & LVIS_SELECTED) == LVIS_SELECTED;
}

const int kNoItem = -1;

// Capacity reserved for LogListView::item_text_ so that OnGetDispInfo never
// reallocates it, which would free a buffer the list view is still holding.
// Generous on purpose: a log message longer than this would reallocate once,
// and the capacity then stays grown, so the window is a single paint at worst.
const size_t kItemTextCapacity = 64 * 1024;

}  // namespace

using base::StringPrintf;

const LogListView::ColumnInfo LogListView::kColumns[] = {
  { 24, L"Severity" },
  { 42, L"Process ID" },
  { 42, L"Thread ID" },
  { 80, L"Time" },
  { 180, L"File" },
  { 30, L"Line" },
  { 640, L"Message", }
};

const wchar_t* LogListView::kConfigKeyName =
    config::kSettingsKey;
const wchar_t* LogListView::kColumnOrderValueName =
    config::kLogViewColumnOrder;
const wchar_t* LogListView::kColumnWidthValueName =
    config::kLogViewColumnWidths;


LogViewFormatter::LogViewFormatter() {
}

bool LogViewFormatter::FormatColumn(ILogView* log_view,
                                    int row,
                                    Column col,
                                    std::string* str) {
  DCHECK(log_view != NULL);
  DCHECK(str != NULL);

  switch (col) {
    case SEVERITY:
      *str = GetSeverityText(log_view->GetSeverity(row));
      break;

    case PROCESS_ID:
      *str = StringPrintf("%d", log_view->GetProcessId(row));
      break;

    case THREAD_ID:
      *str = StringPrintf("%d", log_view->GetThreadId(row));
      break;

    case TIME:
      {
        if (!base_time_.is_null()) {
          base::Time row_time = log_view->GetTime(row);
          base::TimeDelta time_delta = row_time - base_time_;
          bool is_negative = false;

          if (time_delta.ToInternalValue() < 0) {
            is_negative = true;
            time_delta = -time_delta;
          }

          int64_t hours = time_delta.InHours();
          int64_t minutes = time_delta.InMinutes() % 60;
          int64_t seconds = time_delta.InSeconds() % 60;
          int64_t milliseconds = time_delta.InMilliseconds() % 1000;

          if (is_negative) {
            *str = StringPrintf("-%02lld:%02lld:%02lld-%03lld",
                                hours, minutes, seconds, milliseconds);
          } else {
            *str = StringPrintf("%02lld:%02lld:%02lld-%03lld",
                                hours, minutes, seconds, milliseconds);
          }
        } else {
          base::Time time = log_view->GetTime(row);
          base::Time::Exploded exploded;
          time.LocalExplode(&exploded);
          *str = StringPrintf("%02d:%02d:%02d-%03d",
                              exploded.hour,
                              exploded.minute,
                              exploded.second,
                              exploded.millisecond);
        }
      }
      break;

    case FILE:
      *str = log_view->GetFileName(row);
      break;

    case LINE:
      *str = StringPrintf("%d", log_view->GetLine(row));
      break;

    case MESSAGE:
      *str = log_view->GetMessage(row);
      break;

    default:
      return false;
      break;
  }

  return true;
}

LogListView::LogListView(CUpdateUIBase* update_ui)
    : log_view_(NULL), event_cookie_(0),
      update_ui_(update_ui), stack_trace_view_(NULL),
      process_info_service_(NULL), context_menu_row_(kNoItem) {
  ui_loop_ = base::MessageLoop::current();

  context_menu_bar_.LoadMenu(IDR_LIST_VIEW_CONTEXT_MENU);
  context_menu_ = context_menu_bar_.GetSubMenu(0);

  static_assert(arraysize(kColumns) == COL_MAX,
                "wrong number of column info");
}

void LogListView::SetLogView(ILogView* log_view) {
  if (log_view_ == log_view)
    return;

  // Unregister from old log view.
  if (log_view_ != NULL) {
    log_view_->Unregister(event_cookie_);
    event_cookie_ = 0;
  }

  // Store the new one.
  log_view_ = log_view;

  // Adjust our size if we've been created already.
  if (IsWindow()) {
    int num_rows = log_view_->GetNumRows();
    SetItemCountEx(num_rows, 0);  // Invalidate the whole list.
    // We initially want to show the latest items
    EnsureVisible(num_rows - 1, TRUE /* PartialOK */);
  }

  // Register for event notifications.
  if (log_view_ != NULL)
    log_view_->Register(this, &event_cookie_);
}

LRESULT LogListView::OnCreate(UINT msg,
                              WPARAM wparam,
                              LPARAM lparam,
                              BOOL& handled) {
  // Call through to the original window class first.
  LRESULT ret = DefWindowProc(msg, wparam, lparam);

  CImageList image_list;
  image_list.Create(16, 16, ILC_COLOR24 | ILC_MASK, 4, 0);

  // -1 means "no icon". Note this must not be 0-filled: 0 is a valid image
  // index -- the first icon added below -- so any level left at the default
  // would silently draw that icon. That is what used to happen to
  // TRACE_LEVEL_NONE and to every VLOG level above VERBOSE, which all showed
  // the error icon.
  image_indexes_.assign(TRACE_LEVEL_VERBOSE + 1, -1);
  // No MAKEINTRESOURCE here: the IDI_* constants are already the result of
  // one, so applying it a second time casts the pointer down to a WORD and
  // back (C4302). That happens to survive only because the values are small
  // integers in disguise.
  image_indexes_[TRACE_LEVEL_FATAL] =
      image_list.AddIcon(::LoadIcon(NULL, IDI_ERROR));
  image_indexes_[TRACE_LEVEL_ERROR] =
      image_list.AddIcon(::LoadIcon(NULL, IDI_ERROR));
  image_indexes_[TRACE_LEVEL_WARNING] =
      image_list.AddIcon(::LoadIcon(NULL, IDI_WARNING));
  image_indexes_[TRACE_LEVEL_INFORMATION] =
      image_list.AddIcon(::LoadIcon(NULL, IDI_INFORMATION));
  image_indexes_[TRACE_LEVEL_VERBOSE] =
      image_list.AddIcon(::LoadIcon(NULL, IDI_QUESTION));

  SetImageList(image_list, LVSIL_SMALL);
  AddColumns();

  // Reserve once so that assigning cell text in OnGetDispInfo never
  // reallocates -- see the note there. std::basic_string is guaranteed not to
  // reallocate while size() <= capacity().
  item_text_.reserve(kItemTextCapacity);

  // Tune our extended styles.
  SetExtendedListViewStyle(LVS_EX_HEADERDRAGDROP |
                           LVS_EX_FULLROWSELECT |
                           LVS_EX_INFOTIP |
                           LVS_EX_DOUBLEBUFFER);

  int num_rows = log_view_->GetNumRows();
  SetItemCountEx(num_rows, 0);
  // We initially want to show the latest items
  EnsureVisible(num_rows - 1, TRUE /* PartialOK */);

  return ret;
}

void LogListView::OnDestroy() {
  if (log_view_ != NULL) {
    log_view_->Unregister(event_cookie_);
  }
  SaveColumns();
}

LRESULT LogListView::OnGetDispInfo(NMHDR* pnmh) {
  NMLVDISPINFO* info = reinterpret_cast<NMLVDISPINFO*>(pnmh);
  int col = info->item.iSubItem;
  size_t row = info->item.iItem;

  if (col == COL_SEVERITY && info->item.mask & LVIF_IMAGE) {
    info->item.iImage =
        GetImageIndexForSeverity(log_view_->GetSeverity(row));
  }

  std::string temp_text;
  formatter_.FormatColumn(log_view_,
                          row,
                          static_cast<LogViewFormatter::Column>(col),
                          &temp_text);

  std::wstring text = base::UTF8ToWide(temp_text);
  base::TrimWhitespace(text, base::TRIM_TRAILING, &text);

  // Copy into item_text_'s existing buffer rather than assigning to it.
  //
  // This matters because the pointer handed to the control below has to stay
  // valid until the control has finished with it, which is after this handler
  // returns. The control asks for several cells of a row before painting
  // them, so a later call must not move or free the buffer an earlier call
  // handed out.
  //
  // The old code did `item_text_ = base::UTF8ToWide(...)`, a move-assignment
  // from a temporary, which swapped in the temporary's buffer and freed the
  // one already given to the control. Asking for the severity cell
  // ("INFORMATION") and then the message cell of the same row was enough to
  // do it whenever the message was the longer of the two, and the severity
  // cell then painted from freed memory -- a run of identical boxes, being
  // the debug heap's 0xDDDD fill pattern rendered as wide characters.
  //
  // assign() copies into the reserved capacity and cannot reallocate while
  // the text fits, so the address stays put. See kItemTextCapacity.
  item_text_.assign(text.data(), text.size());

  if (info->item.mask & LVIF_TEXT)
    info->item.pszText = const_cast<LPWSTR>(item_text_.c_str());

  return 0;
}

LRESULT LogListView::OnItemChanged(NMHDR* pnmh) {
  NMLISTVIEW* info = reinterpret_cast<NMLISTVIEW*>(pnmh);

  int row = info->iItem;

  if (stack_trace_view_ != NULL) {
    if (IsSelected(info->uNewState) && !IsSelected(info->uOldState)) {
      // Set the stack trace for a single row selection only.
      if (row != kNoItem) {
        std::vector<void*> trace;
        log_view_->GetStackTrace(row, &trace);

        DCHECK(stack_trace_view_ != NULL);
        stack_trace_view_->SetStackTrace(
            log_view_->GetProcessId(row),
            log_view_->GetTime(row),
            trace.size(),
            trace.size() ? &trace[0] : NULL);
      }
    } else if (!IsSelected(info->uNewState) && IsSelected(info->uOldState)) {
      // Clear the trace.
      DCHECK(stack_trace_view_ != NULL);
      stack_trace_view_->SetStackTrace(0, base::Time::Now(), 0, NULL);
    }
  }

  UpdateCommandStatus(true);

  return 0;
}

LRESULT LogListView::OnGetInfoTip(NMHDR* pnmh) {
  NMLVGETINFOTIP* info_tip = reinterpret_cast<NMLVGETINFOTIP*>(pnmh);
  size_t row = info_tip->iItem;

  if (process_info_service_ != NULL) {
    DWORD pid = log_view_->GetProcessId(row);
    base::Time time = log_view_->GetTime(row);

    IProcessInfoService::ProcessInfo info = {};
    if (process_info_service_->GetProcessInfo(pid, time, &info)) {
      std::wstringstream text;

      text << L"Process: " << info.command_line_ << std::endl;
      if (info.started_ != base::Time()) {
        text << L"Started: "
            << base::TimeFormatShortDateAndTime(info.started_) << std::endl;
      }
      if (info.ended_ != base::Time()) {
        text << L"Ended: "
            << base::TimeFormatShortDateAndTime(info.ended_) << std::endl
            << L"Exit code: " << info.exit_code_ << std::endl;
      }

      wcscpy_s(info_tip->pszText, info_tip->cchTextMax, text.str().c_str());
    }
  }

  return 0;
}

int LogListView::GetImageIndexForSeverity(int severity) {
  // Levels above VERBOSE are VLOG verbosity levels -- see GetSeverityText --
  // and there is no upper bound on them, so they share the VERBOSE icon
  // rather than falling off the end of the table.
  if (severity > TRACE_LEVEL_VERBOSE)
    severity = TRACE_LEVEL_VERBOSE;

  if (severity >= 0 &&
      static_cast<size_t>(severity) < image_indexes_.size()) {
    return image_indexes_[severity];
  }

  return -1;
}

void LogListView::AppendRowText(int row, std::wstring* text) {
  DCHECK(text != NULL);

  NMLVDISPINFO info = {};

  info.hdr.hwndFrom = m_hWnd;
  info.item.iItem = row;
  info.item.mask = LVIF_TEXT;

  for (int col = COL_SEVERITY; col < COL_MAX; ++col) {
    info.item.iSubItem = col;

    OnGetDispInfo(&info.hdr);

    // Tab separate the columns.
    if (col != COL_SEVERITY)
      text->push_back(L'\t');
    // OnGetDispInfo points pszText at item_text_, one buffer it reuses for
    // every call, so the text has to be taken before the next column
    // overwrites it. Appending here does that.
    text->append(info.item.pszText);
  }

  // Clipboard has CRLF separated lines.
  text->append(L"\r\n");
}

void LogListView::CopyTextToClipboard(const std::wstring& text) {
  // Copy the string to a global pointer for the clipboard.
  CHeapPtr<wchar_t, CGlobalAllocator> data;

  if (!data.Allocate(text.length() + 1)) {
    LOG(ERROR) << "Unable to allocate clipboard data";
    return;
  }

  // Copy the string and the terminating zero.
  memcpy(data.m_pData, text.c_str(), (text.length() + 1) * sizeof(wchar_t));

  if (::OpenClipboard(m_hWnd)) {
    ::EmptyClipboard();

    if (::SetClipboardData(CF_UNICODETEXT, data.m_pData)) {
      // The clipboard has taken ownership now.
      data.Detach();
    } else {
      LOG(ERROR) << "Unable to set clipboard data, error  "
          << ::GetLastError();
    }

    ::CloseClipboard();
  } else  {
    LOG(ERROR) << "Unable to open clipboard, error " << ::GetLastError();
  }
}

void LogListView::OnCopyCommand(UINT code, int id, CWindow window) {
  std::wstring selection;

  int item = GetNextItem(kNoItem, LVNI_SELECTED);
  for (; item != kNoItem; item = GetNextItem(item, LVNI_SELECTED))
    AppendRowText(item, &selection);

  CopyTextToClipboard(selection);
}

void LogListView::OnCopyRow(UINT code, int id, CWindow window) {
  // Deliberately the row the context menu was opened over, not the selection
  // and not the focused row. Right-clicking a list view does not move either
  // one, so acting on them would copy whatever the last left-click had
  // touched -- which is not the row the user just pointed at.
  if (context_menu_row_ == kNoItem) {
    NOTREACHED() << "Copy Row with no row under the context menu";
    return;
  }

  std::wstring text;
  AppendRowText(context_menu_row_, &text);
  CopyTextToClipboard(text);
}

void LogListView::ExportLog() {
  int num_rows = (log_view_ != NULL) ? log_view_->GetNumRows() : 0;
  if (num_rows == 0) {
    ::MessageBox(m_hWnd,
                 L"There is nothing to export. Capture or import a log first.",
                 L"Export Log",
                 MB_OK | MB_ICONINFORMATION);
    return;
  }

  // GetSaveFileName, by way of WTL's wrapper, rather than ATL's
  // CShellFileSaveDialog -- the SDK declares that one only from NTDDI_VISTA
  // up, which is a floor Sawbuck does not compile at. The import command uses
  // the same family of dialog.
  CFileDialog dialog(FALSE,  // Save rather than Open.
                     L"txt",
                     L"sawbuck.txt",
                     OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
                     L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0",
                     m_hWnd);
  if (dialog.DoModal(m_hWnd) != IDOK)
    return;

  // Built in memory and written once. A log big enough for that to matter is
  // one that is already in memory in a larger form -- these rows exist as
  // structures the capture path kept, and their text is smaller than that --
  // so the export adds a fraction rather than a multiple.
  std::wstring text;

  // A heading row, so the columns can be told apart in a text editor. The
  // same titles the list view shows, in the same order.
  for (int col = COL_SEVERITY; col < COL_MAX; ++col) {
    if (col != COL_SEVERITY)
      text.push_back(L'\t');
    text.append(kColumns[col].title);
  }
  text.append(L"\r\n");

  // Every row of *this* view, which is the filtered one when a filter is set.
  // What gets exported is what is on screen, which is the same contract Copy
  // and Copy Row have.
  for (int row = 0; row < num_rows; ++row)
    AppendRowText(row, &text);

  // UTF-8 with a byte order mark. The mark is what makes Notepad read the
  // file as Unicode instead of as the system code page, and log messages
  // routinely carry text that does not survive that. AppendRowText has
  // already used CRLF, which is what a text editor on Windows wants.
  std::string utf8("\xEF\xBB\xBF");
  utf8.append(base::WideToUTF8(text));

  base::FilePath path(dialog.m_szFileName);
  int written = base::WriteFile(path, utf8.data(),
                                static_cast<int>(utf8.size()));
  if (written != static_cast<int>(utf8.size())) {
    std::wstring message =
        base::StringPrintf(L"Could not write \"%ls\".", path.value().c_str());
    ::MessageBox(m_hWnd, message.c_str(), L"Export Log", MB_OK | MB_ICONERROR);
  }
}

void LogListView::OnSelectAll(UINT code, int id, CWindow window) {
  // Select all items.
  SetItemState(kNoItem, LVIS_SELECTED, LVIS_SELECTED);
}

void LogListView::OnClearAll(UINT code, int id, CWindow window) {
  // Clear all items from the log view and then wait for change notifications.
  log_view_->ClearAll();
  // And clear the stack trace as well.
  if (stack_trace_view_)
    stack_trace_view_->SetStackTrace(0, base::Time::Now(), 0, NULL);
}

void LogListView::OnSetFocus(CWindow window) {
  UpdateCommandStatus(true);

  // Give the list view a chance at the message.
  SetMsgHandled(FALSE);
}

void LogListView::OnKillFocus(CWindow window) {
  UpdateCommandStatus(false);

  // Give the list view a chance at the message.
  SetMsgHandled(FALSE);
}

void LogListView::OnContextMenu(CWindow wnd, CPoint point) {
  // A right-click on a column heading lands here too, and used to raise the
  // row menu -- so the headings offered "Copy Row", "Set Base Time" and the
  // rest, every one of them greyed, because the hit test below finds no row
  // under a point that is inside the header.
  //
  // The header is a child control of the list view. WM_CONTEXTMENU names the
  // window that was clicked rather than the one handling it, and the header
  // does not handle the message, so DefWindowProc passes it to the parent with
  // wParam still pointing at the header. Comparing against GetHeader() is what
  // tells the two apart; nothing in the point or the hit test can, since a
  // click just below the header also finds no row.
  if (wnd.m_hWnd == GetHeader().m_hWnd) {
    OnHeaderContextMenu(point);
    return;
  }

  int row = -1;
  int col = -1;
  if (point.x == -1 && point.y == -1) {
    // On shift-F10, the point is (-1, -1)
    row = GetNextItem(-1, LVIS_FOCUSED);
    if (row != -1) {
      // Set the point to the middle of the label of the found item.
      CRect rc;
      GetItemRect(row, &rc, LVIR_LABEL);
      point.x = (rc.left + rc.right) / 2;
      point.y = (rc.bottom + rc.top) / 2;
    } else {
      // If no found item, backoff to the top-left of our client area.
      point.x = point.y = 0;
    }
    ClientToScreen(&point);
  } else {
    // Hit test to make sure that we clicked on an item.
    CPoint client_point(point);
    ScreenToClient(&client_point);
    LVHITTESTINFO hit_test = { client_point };
    row = SubItemHitTest(&hit_test);
    col = hit_test.iSubItem;
  }

  CMenu menu;
  if (!menu.CreatePopupMenu()) {
    LOG(ERROR) << "Unable to create popup menu";
    return;
  }

  // Remember which row the menu was opened over, so OnCopyRow can act on it.
  // The commands arrive later as WM_COMMAND and carry only an id, and a
  // right-click moves neither the selection nor the focus, so the row cannot
  // be recovered once we return from here.
  context_menu_row_ = row;

  menu.AppendMenu(row == kNoItem ? MF_GRAYED : MF_ENABLED,
                  ID_COPY_ROW,
                  L"&Copy Row");

  // Relying on the default arguments, rather than passing an explicit id and
  // NULL text. CMenuT::AppendMenu is overloaded on both the second and third
  // parameters, and a zero-valued integer literal is also a null pointer
  // constant, so spelling the id out as 0 makes the UINT_PTR and HMENU
  // overloads equally good matches and the call ambiguous. Only the
  // three-defaulted overload can be called with one argument.
  menu.AppendMenu(MF_SEPARATOR);

  menu.AppendMenu(row == -1 ? MF_GRAYED : MF_ENABLED,
                  ID_SET_TIME_ZERO,
                  L"&Set Base Time");

  menu.AppendMenu(formatter_.base_time().is_null() ? MF_GRAYED : MF_ENABLED,
                  ID_RESET_BASE_TIME,
                  L"&Reset Base Time");

  // TODO(siggi): Implement popup menu items to include/exclude
  //      the clicked column by its value.
#if 0
  if (row != -1 && col != -1) {
    std::wstring column = kColumns[col].title;
    StringToLowerASCII(column);
    std::wstring item_text = StringPrintf(L"&Include %ls \"%ls\"",
                                          column.c_str(),
                                          L"foo");
    menu.AppendMenu(MF_ENABLED,
                    ID_INCLUDE_COLUMN,
                    item_text.c_str());

    item_text = StringPrintf(L"&Exclude %ls \"%ls\"",
                             column.c_str(),
                             L"foo");
    menu.AppendMenu(MF_ENABLED,
                    ID_EXCLUDE_COLUMN,
                    item_text.c_str());
  }
#endif

  // Zero means TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON, which is what a
  // context menu wants: it is raised from OnContextMenu, i.e. on right-button
  // *up*, so the button is no longer down to select with.
  menu.TrackPopupMenu(0, point.x, point.y, wnd);
}

void LogListView::OnHeaderContextMenu(CPoint point) {
  CMenu menu;
  if (!menu.CreatePopupMenu()) {
    LOG(ERROR) << "Unable to create header popup menu";
    return;
  }

  // Shift-F10 reports (-1, -1), which as a screen position is the top-left
  // corner of the primary monitor. Put the menu on the header instead.
  if (point.x == -1 && point.y == -1) {
    CRect rect;
    GetHeader().GetWindowRect(&rect);
    point.SetPoint((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2);
  }

  // Mirrors the Edit menu's "Auto Size Columns", and shares its command id, so
  // OnAutoSizeColumns and the F12 accelerator already do the work. The text
  // and the accelerator are repeated from viewer.rc rather than read out of
  // the menu resource: a context menu built by hand has no access to the one
  // the frame loaded, and a GetSubMenu walk to find it would couple this to
  // the Edit menu's layout.
  //
  // Enabled on the same condition the Edit menu uses, because sizing a column
  // to its content needs content: LVSCW_AUTOSIZE measures the items, so with
  // no rows it would shrink every column to nothing.
  menu.AppendMenu(log_view_->GetNumRows() ? MF_ENABLED : MF_GRAYED,
                  ID_VIEW_AUTOSIZE_COLUMNS,
                  L"Auto &Size Columns\tF12");

  // m_hWnd, not the header: TrackPopupMenu sends WM_COMMAND to the window
  // named here, and the header has no handler for this id. The row menu above
  // can pass its own `wnd` because there that is this window already.
  menu.TrackPopupMenu(0, point.x, point.y, m_hWnd);
}

void LogListView::OnFind(UINT code, int id, CWindow window) {
  FindDialog find(find_params_);
  if (find.DoModal(m_hWnd) == IDOK) {
    find_params_ = find.find_params();
    Find(find_params_.direction_down_);
  }
}

void LogListView::OnFindNext(UINT code, int id, CWindow window) {
  if (!find_params_.expression_.empty())
    Find(find_params_.direction_down_);
}

void LogListView::OnFindPrevious(UINT code, int id, CWindow window) {
  if (!find_params_.expression_.empty())
    Find(!find_params_.direction_down_);
}

void LogListView::OnAutoSizeColumns(UINT code, int id, CWindow window) {
  int columns = GetHeader().GetItemCount();
  // Skip resizing the severity column.
  for (int i = 1; i < columns; ++i)
    SetColumnWidth(i, LVSCW_AUTOSIZE);
}

void LogListView::Find(bool down) {
  pcrecpp::RE_Options options = PCRE_UTF8;
  options.set_caseless(!find_params_.match_case_);
  pcrecpp::RE expression(find_params_.expression_, options);

  int start = GetNextItem(-1, LVIS_FOCUSED);
  int num_rows = log_view_->GetNumRows();
  int i = down ? start + 1 : start - 1;
  if (i < 0)
    i = 0;  // in case start == -1.

  for (; down ? i < num_rows : i >= 0; down ? ++i : --i) {
    std::string message(log_view_->GetMessage(i));
    if (expression.PartialMatch(message))
      break;
  }

  if (i >= 0 && i < num_rows) {
    // Clear the existing selection.
    if (start >= 0)
      SetItemState(start, 0, LVIS_SELECTED | LVIS_FOCUSED);

    // Select and focus the new item.
    SetItemState(i, LVIS_SELECTED | LVIS_FOCUSED,
                 LVIS_SELECTED | LVIS_FOCUSED);
    EnsureVisible(i, false);
  } else {
    MessageBox(L"The specified text was not found.");
  }
}

void LogListView::OnSetBaseTime(UINT code, int id, CWindow window) {
  // The row the context menu was opened over, which is the only way to reach
  // this command. It used to read GetNextItem(-1, LVIS_FOCUSED) instead, and
  // that is a different row: a right-click moves neither the selection nor
  // the focus, so invoking this from the context menu set the base time from
  // whichever row had last been left-clicked, not the one pointed at.
  //
  // It also used to reach the NOTREACHED below whenever nothing had ever been
  // focused, even though the menu item was enabled -- OnContextMenu decides
  // that from its hit test, which succeeds on a row the focus has never
  // visited. Both now consult the same row, so enabled and usable agree.
  //
  // Shift+F10 is unaffected: that path has no cursor to hit test, so
  // OnContextMenu resolves the row from LVIS_FOCUSED exactly as before and
  // stores that.
  if (context_menu_row_ == kNoItem) {
    NOTREACHED() << "Set Base Time with no row under the context menu";
    return;
  }

  // Get the corresponding time.
  formatter_.set_base_time(log_view_->GetTime(context_menu_row_));

  // Refresh the list.
  RedrawItems(0, GetItemCount());
}

void LogListView::OnResetBaseTime(UINT code, int id, CWindow window) {
  formatter_.set_base_time(base::Time());

  // Refresh the list.
  RedrawItems(0, GetItemCount());
}

void LogListView::LogViewNewItems() {
  DCHECK_EQ(ui_loop_, base::MessageLoop::current());

  if (IsWindow()) {
    // Check if last item was previously visible...
    BOOL is_last_item_visible = ListView_IsItemVisible(m_hWnd,
                                                       GetItemCount() - 1);
    int num_rows = log_view_->GetNumRows();
    SetItemCountEx(num_rows, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);

    // We want to show the latest items if the
    // previously latest one was visible.
    if (is_last_item_visible)
      EnsureVisible(num_rows - 1, TRUE /* PartialOK */);
  }
}

void LogListView::LogViewCleared() {
  DCHECK_EQ(ui_loop_, base::MessageLoop::current());
  DeleteAllItems();
}

void LogListView::UpdateCommandStatus(bool has_focus) {
  bool has_selection = GetSelectedCount() != 0;

  update_ui_->UIEnable(ID_EDIT_COPY, has_focus && has_selection);
  update_ui_->UIEnable(ID_EDIT_SELECT_ALL, has_focus);
  update_ui_->UIEnable(ID_EDIT_CLEAR_ALL, has_focus);
  update_ui_->UIEnable(ID_EDIT_FIND, has_focus);
  update_ui_->UIEnable(ID_EDIT_FIND_NEXT, has_focus &&
                       !find_params_.expression_.empty());
  update_ui_->UIEnable(ID_EDIT_FIND_PREVIOUS, has_focus &&
                       !find_params_.expression_.empty());
  update_ui_->UIEnable(ID_VIEW_AUTOSIZE_COLUMNS,
                       has_focus && log_view_->GetNumRows());
}
