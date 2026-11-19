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
// List view utility base class.
#ifndef SAWBUCK_VIEWER_LIST_VIEW_BASE_H_
#define SAWBUCK_VIEWER_LIST_VIEW_BASE_H_

#include "sawbuck/viewer/framework.h"

#include "base/logging.h"

// The list view base class knows how to save and restore column order
// and widths.
// @param ImplClass a derivative of this class.
// @note ImplClass must declare the following:
//    # kColumns, an array of ColumnInfo
//    # kConfigKeyName a string that's the configuration key for this window,
//        e.g. L"Software\\MyProg".
//    # kColumnOrderValueName the name of the registry value that persists
//        the column order.
//    # kColumnWidthValueName the name of the registry value that persists
//        the column widths.
template <class ImplClass, class WinTraits>
class ListViewBase
    : public CWindowImpl<ImplClass, CListViewCtrl, WinTraits> {
 public:
  // Describes a column in the list view.
  struct ColumnInfo {
    int width;  // Default width for this column.
    const wchar_t* title;  // Title for this column.
  };

  // Create the columns and restore the order and widths from registry.
  void AddColumns() {
    AddColumns(ImplClass::kColumns,
               ImplClass::kConfigKeyName,
               ImplClass::kColumnOrderValueName,
               ImplClass::kColumnWidthValueName);
  }

  // Save the column order and colum widths for this list view to registry.
  void SaveColumns() {
    SaveColumns(ImplClass::kColumns,
                ImplClass::kConfigKeyName,
                ImplClass::kColumnOrderValueName,
                ImplClass::kColumnWidthValueName);
  }

 private:
  // Added to a heading's own text width to get the narrowest the column can
  // be without the heading looking cramped: comctl32 insets a header label
  // from both edges, and a column exactly as wide as its text leaves the
  // label touching the divider. 15 because that is what
  // CListViewCtrl::AddColumn uses for the same purpose, so the floor below
  // comes to the same width AddColumn itself just chose.
  static const int kHeadingPadding = 15;

  template <int N> void AddColumns(const ColumnInfo (&cols)[N],
                                   const wchar_t* key_name,
                                   const wchar_t* order_value_name,
                                   const wchar_t* width_value_name);
  template <int N> void SaveColumns(const ColumnInfo (&cols)[N],
                                    const wchar_t* key_name,
                                    const wchar_t* order_value_name,
                                    const wchar_t* width_value_name);
};

template <class ImplClass, class WinTraits>
template <int N>
void ListViewBase<ImplClass, WinTraits>::AddColumns(
    const ColumnInfo (&cols)[N], const wchar_t* key_name,
    const wchar_t* order_value_name, const wchar_t* width_value_name) {
  for (int col = 0; col < N; ++col) {
    AddColumn(cols[col].title, col);

    // Never narrower than the heading itself. The widths in kColumns are raw
    // pixel counts picked against the data a column holds rather than the
    // word at the top of it -- "Process ID" in 42 pixels and "Severity" in 24
    // are both clipped -- and with nothing stored in the registry yet, on a
    // first run or after the preferences key has been deleted, those are
    // exactly what the user is shown. Being raw pixels they do not follow the
    // font or the DPI in use either, while LVM_GETSTRINGWIDTH measures in the
    // control's own font and so does.
    //
    // AddColumn above has in fact already sized the column this way, to
    // GetStringWidth plus kHeadingPadding; the SetColumnWidth call that used
    // to be here simply discarded it.
    //
    // The floor applies only to the defaults. A width the user dragged and
    // SaveColumns stored is restored below just as it was, even if it clips:
    // that was the user's own doing, and overriding it would make a narrowed
    // column spring back open on every restart.
    int heading = GetStringWidth(cols[col].title) + kHeadingPadding;
    SetColumnWidth(col, cols[col].width > heading ? cols[col].width : heading);
  }

  // Restore column order and column widths.
  CRegKey settings;
  if (ERROR_SUCCESS == settings.Open(HKEY_CURRENT_USER,
                                     key_name,
                                     KEY_READ)) {
    // Named 'values' rather than 'cols': it shadowed the parameter of that
    // name, which is a different type holding different data -- ColumnInfo
    // titles and default widths, against the order indices and then the saved
    // widths read back here (C4457).
    std::vector<int> values;
    values.resize(N);

    ULONG len = values.size() * sizeof(values[0]);
    if (ERROR_SUCCESS == settings.QueryBinaryValue(order_value_name,
                                                   &values[0],
                                                   &len) &&
        len == values.size() * sizeof(values[0])) {
      SetColumnOrderArray(values.size(), &values[0]);
    }

    len = values.size() * sizeof(values[0]);
    if (ERROR_SUCCESS == settings.QueryBinaryValue(width_value_name,
                                                   &values[0],
                                                   &len) &&
        len == values.size() * sizeof(values[0])) {
      for (int col = 0; col < N; ++col) {
        SetColumnWidth(col, values[col]);
      }
    }
  }
}

template <class ImplClass, class WinTraits>
template <int N>
void ListViewBase<ImplClass, WinTraits>::SaveColumns(
    // Unnamed deliberately: saving reads the live column order and widths out
    // of the control, so only the array's extent N is wanted here. The
    // parameter exists to deduce it, and naming it only produced a C4100.
    const ColumnInfo (&)[N], const wchar_t* key_name,
    const wchar_t* order_value_name, const wchar_t* width_value_name) {
  // Save column order and widths.
  CRegKey settings;
  if (ERROR_SUCCESS == settings.Create(HKEY_CURRENT_USER,
                                       key_name)) {
    std::vector<int> values;
    values.resize(N);

    if (GetColumnOrderArray(values.size(), &values[0])) {
      settings.SetBinaryValue(order_value_name,
                              &values[0],
                              values.size() * sizeof(values[0]));
    }

    for (int col = 0; col < N; ++col)
      values[col] = GetColumnWidth(col);

    settings.SetBinaryValue(width_value_name,
                            &values[0],
                            values.size() * sizeof(values[0]));
  } else {
    LOG(ERROR) << "Unable to open key " << key_name;
  }
}

#endif  // SAWBUCK_VIEWER_LIST_VIEW_BASE_H_
