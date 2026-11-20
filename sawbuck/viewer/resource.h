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

#ifndef SAWBUCK_VIEWER_RESOURCE_H_
#define SAWBUCK_VIEWER_RESOURCE_H_

// {{NO_DEPENDENCIES}}  // NOLINT
// Microsoft Visual C++ generated include file.
// Used by viewer.rc

// Number of Chromium log levels
#define IDR_LOGLEVELS                   4

// Main application resource, used for menus and icon
#define IDR_MAIN_FRAME                  101

// MFC dialogs
#define IDD_ABOUT                       102
#define IDD_PROVIDERDIALOG              103
#define IDD_FILTERDIALOG                104
#define IDR_LIST_VIEW_CONTEXT_MENU      105
#define IDD_SYMBOLPATH                  106
#define IDD_FINDDIALOG                  107
#define IDD_FILTERDIALOG2               108

// Controls
#define IDC_PROVIDERS                   1002
#define IDC_EXCLUDE_RE                  1003
#define IDC_INCLUDE_RE                  1004
#define IDC_SYMBOLPATH                  1005
#define IDC_FIND_TEXT                   1006
#define IDC_MATCH_CASE                  1007
#define IDC_DIRECTION_DOWN              1008
#define IDC_DIRECTION_UP                1009
#define IDC_FILTER_COLUMN               1010
#define IDC_FILTER_RELATION             1011
#define IDC_FILTER_TEXT                 1012
#define IDC_FILTER_ACTION               1013
#define IDC_FILTER_LIST                 1014
#define IDC_FILTER_RESET                1015
#define IDC_FILTER_ADD                  1016
#define IDC_FILTER_REMOVE               1017
#define IDC_FILTER_STATIC               1018
#define IDC_FILTER_SAVE                 1019
#define IDC_BUTTON2                     1020
#define IDC_FILTER_LOAD                 1021
// Given a real ID rather than IDC_STATIC so the text can be replaced at
// runtime if the About box ever wants more than the compiled-in version.
#define IDC_VERSION                     1022

// Our custom IDs
#define ID_FILE_EXIT                    4001
#define ID_FILE_IMPORT                  4002
#define ID_LOG_CAPTURE                  4003
#define ID_LOG_CONFIGUREPROVIDERS       4004
#define ID_LOG_FILTER                   4005
#define ID_SET_TIME_ZERO                4007
#define ID_RESET_BASE_TIME              4008
#define ID_LOG_SYMBOLPATH               4009
// 4010 was ID_EDIT_FIND_NEXT, which now comes from WTL's atlres.h instead --
// it joins ID_EDIT_FIND, ID_EDIT_COPY, ID_APP_ABOUT and the five other
// standard command IDs this file already leaves to WTL. WTL 8.0 had no
// ID_EDIT_FIND_NEXT, which is why there was a local one; 9.1 defines it as an
// alias for ID_EDIT_REPEAT.
//
// Defining it here as well was not merely a redefinition warning. atlres.h
// does not guard the name, and the two definitions won in different places:
// viewer.rc includes resource.h at the top and atlres.h nine lines later, so
// the menu item and the F3 accelerator compiled to WTL's 0xE128, while every
// .cc saw atlres.h first and resource.h last and so used 4010. The command
// would have been posted with an ID no handler claimed.
#define ID_VIEW_AUTOSIZE_COLUMNS        4011
#define ID_INCLUDE_COLUMN               4012
#define ID_EXCLUDE_COLUMN               4013
// List view context menu: copy the single row the menu was opened over,
// as distinct from ID_EDIT_COPY which copies the whole selection.
#define ID_COPY_ROW                     4014
// Log menu: write the default provider registrations, for installs that
// did not come from the MSI.
#define ID_LOG_REGISTER_PROVIDERS       4015
#define ID_LOG_EXPORT                   4016
#define ID_RESET_SETTINGS               4017

// Next default values for new objects
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE        109
#define _APS_NEXT_COMMAND_VALUE         4018
#define _APS_NEXT_CONTROL_VALUE         1023
#define _APS_NEXT_SYMED_VALUE           101
#endif
#endif

#endif  // SAWBUCK_VIEWER_RESOURCE_H_
