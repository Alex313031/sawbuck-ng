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
// Log viewer window declaration.
#ifndef SAWBUCK_VIEWER_LOG_LIST_VIEW_H_
#define SAWBUCK_VIEWER_LOG_LIST_VIEW_H_

#include "sawbuck/viewer/framework.h"

#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "sawbuck/viewer/find_dialog.h"
#include "sawbuck/viewer/list_view_base.h"
#include "sawbuck/viewer/resource.h"

// Callback interface for ILogView.
class ILogViewEvents {
 public:
  // Called on the UI thread.
  virtual void LogViewNewItems() = 0;
  virtual void LogViewCleared() = 0;
};

// Provides a view on a log, the view may be filtered or sorted.
class ILogView {
 public:
  // Returns the number of rows in this view.
  virtual int GetNumRows() = 0;

  // Clear all the items in this view.
  virtual void ClearAll() = 0;

  virtual int GetSeverity(int row) = 0;
  virtual DWORD GetProcessId(int row) = 0;
  virtual DWORD GetThreadId(int row) = 0;
  virtual base::Time GetTime(int row) = 0;
  virtual std::string GetFileName(int row) = 0;
  virtual int GetLine(int row) = 0;
  virtual std::string GetMessage(int row) = 0;
  virtual void GetStackTrace(int row, std::vector<void*>* trace) = 0;

  // Register for change notifications. Notifications will be issued
  // on the thread where the registration was made.
  virtual void Register(ILogViewEvents* event_sink,
                        int* registration_cookie) = 0;
  virtual void Unregister(int registration_cookie) = 0;
};

class LogViewFormatter {
 public:
  enum Column {
    SEVERITY,
    PROCESS_ID,
    THREAD_ID,
    TIME,
    FILE,
    LINE,
    MESSAGE,

    // Must be last.
    NUM_COLUMNS
  };

  LogViewFormatter();

  bool FormatColumn(ILogView* log_view,
                    int row,
                    Column col,
                    std::string* str);

  base::Time base_time() const { return base_time_; }
  void set_base_time(base::Time base_time) { base_time_ = base_time; }

 private:
  // The time delta subtracted from the displayed time stamp in each row.
  base::Time base_time_;
};

// Forward decls.
class StackTraceListView;
class IProcessInfoService;
namespace WTL {
class CUpdateUIBase;
};

// Traits specialization for log list view.
typedef CWinTraits<WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
    LVS_REPORT | LVS_SHOWSELALWAYS | LVS_OWNERDATA, 0>
        LogListViewTraits;

// List view control subclass that manages the log view.
class LogListView
    : public ListViewBase<LogListView, LogListViewTraits>,
      public ILogViewEvents {
 public:
  typedef ListViewBase<LogListView, LogListViewTraits> WindowBase;
  DECLARE_WND_SUPERCLASS(NULL, WindowBase::GetWndClassName())

  BEGIN_MSG_MAP_EX(LogList)
    MESSAGE_HANDLER(WM_CREATE, OnCreate)
    MSG_WM_CONTEXTMENU(OnContextMenu)
    MSG_WM_DESTROY(OnDestroy)
    MSG_WM_SETFOCUS(OnSetFocus)
    MSG_WM_KILLFOCUS(OnKillFocus)
    COMMAND_ID_HANDLER_EX(ID_VIEW_AUTOSIZE_COLUMNS, OnAutoSizeColumns)
    COMMAND_ID_HANDLER_EX(ID_EDIT_COPY, OnCopyCommand)
    COMMAND_ID_HANDLER_EX(ID_COPY_ROW, OnCopyRow)
    COMMAND_ID_HANDLER_EX(ID_EDIT_CLEAR_ALL, OnClearAll)
    COMMAND_ID_HANDLER_EX(ID_EDIT_SELECT_ALL, OnSelectAll)
    COMMAND_ID_HANDLER_EX(ID_EDIT_FIND, OnFind)
    COMMAND_ID_HANDLER_EX(ID_EDIT_FIND_NEXT, OnFindNext)
    COMMAND_ID_HANDLER_EX(ID_EDIT_FIND_PREVIOUS, OnFindPrevious)
    COMMAND_ID_HANDLER_EX(ID_SET_TIME_ZERO, OnSetBaseTime)
    COMMAND_ID_HANDLER_EX(ID_RESET_BASE_TIME, OnResetBaseTime)
    REFLECTED_NOTIFY_CODE_HANDLER_EX(LVN_GETDISPINFO, OnGetDispInfo)
    REFLECTED_NOTIFY_CODE_HANDLER_EX(LVN_ITEMCHANGED, OnItemChanged)
    REFLECTED_NOTIFY_CODE_HANDLER_EX(LVN_GETINFOTIP, OnGetInfoTip)
    DEFAULT_REFLECTION_HANDLER()
  END_MSG_MAP()

  explicit LogListView(CUpdateUIBase* update_ui);

  void set_stack_trace_view(StackTraceListView* stack_trace_view) {
    stack_trace_view_ = stack_trace_view;
  }
  void set_process_info_service(IProcessInfoService* process_info_service) {
    process_info_service_ = process_info_service;
  }

  void SetLogView(ILogView* log_view);

  // Asks for a file name and writes every row of this view to it as
  // tab-separated UTF-8 text. Public because the command arrives at the
  // splitter rather than here -- see LogViewer::OnExportLog.
  void ExportLog();

  virtual void LogViewNewItems();
  virtual void LogViewCleared();

  // Our column definitions and config data to satisfy our contract
  // to the ListViewImpl superclass.
  static const ColumnInfo kColumns[];
  static const wchar_t* kConfigKeyName;
  static const wchar_t* kColumnOrderValueName;
  static const wchar_t* kColumnWidthValueName;

 protected:
  // The columns our list view displays.
  // @note COL_MAX must be equal to arraysize(kColumns).
  enum Columns {
    COL_SEVERITY = LogViewFormatter::SEVERITY,
    COL_PROCESS = LogViewFormatter::PROCESS_ID,
    COL_THREAD = LogViewFormatter::THREAD_ID,
    COL_TIME = LogViewFormatter::TIME,
    COL_FILE = LogViewFormatter::FILE,
    COL_LINE = LogViewFormatter::LINE,
    COL_MESSAGE = LogViewFormatter::MESSAGE,

    // Must be last.
    COL_MAX,
  };

  LRESULT OnCreate(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);
  void OnDestroy();

  LRESULT OnGetDispInfo(LPNMHDR notification);
  LRESULT OnItemChanged(LPNMHDR notification);
  LRESULT OnGetInfoTip(LPNMHDR notification);

  void OnCopyCommand(UINT code, int id, CWindow window);
  void OnCopyRow(UINT code, int id, CWindow window);

  // Appends one row to |text| as tab separated columns followed by CRLF, in
  // the form the clipboard gets. Shared by the two copy commands.
  void AppendRowText(int row, std::wstring* text);
  // Puts |text| on the clipboard as CF_UNICODETEXT.
  void CopyTextToClipboard(const std::wstring& text);
  virtual void OnClearAll(UINT code, int id, CWindow window);
  void OnSelectAll(UINT code, int id, CWindow window);
  void OnSetFocus(CWindow window);
  void OnKillFocus(CWindow window);
  void OnContextMenu(CWindow wnd, CPoint point);
  // The column-heading menu, which is not the row menu. Called from
  // OnContextMenu when the click was on the header rather than on the list.
  void OnHeaderContextMenu(CPoint point);
  void OnFind(UINT code, int id, CWindow window);
  void OnFindNext(UINT code, int id, CWindow window);
  void OnFindPrevious(UINT code, int id, CWindow window);
  void OnAutoSizeColumns(UINT code, int id, CWindow window);

  // Context menu command handlers.
  void OnSetBaseTime(UINT code, int id, CWindow window);
  void OnResetBaseTime(UINT code, int id, CWindow window);

  // Updates the UI status for commands we support, disables
  // all our commands unless we have focus.
  // @param has_focus true iff this window has the focus.
  void UpdateCommandStatus(bool has_focus);

  // Finds the next item matching the current find parameters, searching
  // down the list if |down| and up it otherwise. See |find_params_|.
  //
  // The direction is a parameter rather than read from find_params_ here,
  // because Find Previous is "the other way from whatever the dialog is set
  // to" and must not change that setting -- F3 and Shift+F3 are relative to
  // the dialog, not absolute.
  void Find(bool down);

  // To help unittest mocking.
  virtual BOOL DeleteAllItems() {
    return WindowBase::DeleteAllItems();
  }

  // The stack trace view that displays our stack trace.
  StackTraceListView* stack_trace_view_;

  // Our process info service, if any.
  IProcessInfoService* process_info_service_;

  ILogView* log_view_;
  int event_cookie_;

  // Image indexes for severity, stored by severity value.
  std::vector<int> image_indexes_;
  int GetImageIndexForSeverity(int severity);

  // Used to update our command state.
  CUpdateUIBase* update_ui_;

  // Backing store for the strings returned from OnGetDispInfo. The list view
  // reads this after the handler returns, so it must outlive the call -- and
  // must not be reallocated by a later call, which would free a buffer the
  // control is still holding. See OnGetDispInfo.
  std::wstring item_text_;

  // The last piece of text we searched for.
  FindParameters find_params_;

  // Asserting on correct threading.
  base::MessageLoop* ui_loop_;

  // Our context menu.
  CMenu context_menu_bar_;
  CMenu context_menu_;

  // The row the context menu was last opened over, or kNoItem if it was
  // opened over empty space. OnCopyRow needs it because a WM_COMMAND
  // carries only an id, and a right-click moves neither the selection nor
  // the focus, so there is nothing to recover it from afterwards.
  int context_menu_row_;

  // Used to format the text we display.
  LogViewFormatter formatter_;
};

#endif  // SAWBUCK_VIEWER_LOG_LIST_VIEW_H_
