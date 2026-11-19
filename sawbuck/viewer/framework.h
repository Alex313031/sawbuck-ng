// Copyright 2026 Alex313031
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
// The platform preamble shared by the viewer's GUI translation units: the
// character set, the Windows version constants, and the Windows, ATL, WTL and
// standard library headers that nearly all of them need.
//
// This is the same idea as winwrap's src/framework.h, and the same caveat
// applies: it is *not* a precompiled header. Nothing in the build references
// it as one, no /Yc or /Yu goes near it, and adding that is a separate
// decision. It exists so a file can say what it is -- a Win32 GUI
// translation unit -- in one line instead of nine, and so the few things here
// that have a required order have it in exactly one place.
//
// What belongs here: a header that most of the viewer's GUI files need, or
// one whose position relative to another header matters. What does not: a
// header two or three files need, anything from base/, and anything whose
// cost is paid by files that will never use it. Those stay where they are
// used, which is also what keeps the rest of the tree honest about its
// includes. The deliberate omissions are listed at the bottom.
#ifndef SAWBUCK_VIEWER_FRAMEWORK_H_
#define SAWBUCK_VIEWER_FRAMEWORK_H_

// Every define below is already on the compiler command line, so none of these
// #ifndefs fire in this build. They are here because a header that states the
// platform it assumes can be read on its own and cannot be quietly
// contradicted: if some future target forgets one, the value it gets is this
// one rather than the SDK's default.
//
// Which file each comes from, since it is not one file:
//
//   build/common.gypi          WIN32_LEAN_AND_MEAN (5581), NOMINMAX (5577),
//                              _USING_V110_SDK71_ (2916), and the version
//                              constants (5573, 2914) -- but see below, those
//                              are overridden for Sawbuck's own targets
//   sawbuck/sawbuck_targets.gypi  the version constants that actually apply
//                              here, and _ATL_XP_TARGETING
//   build/common.gypi:3261     UNICODE and _UNICODE, by way of
//                              'CharacterSet': '1', which is how MSBuild
//                              spells them rather than as /D flags
//
// Values in this file are kept equal to what those two set. If you change one
// there, change it here, or the header starts lying about the platform -- which
// is the one thing it exists to describe.

#ifndef UNICODE
 #define UNICODE
#endif
#ifndef _UNICODE
 #define _UNICODE
#endif

// winsdkver.h first, then the version macros, then sdkddkver.h. That is the
// order Microsoft documents: winsdkver.h publishes the SDK's own maximum,
// sdkddkver.h derives everything else -- _WIN32_IE, the NTDDI_* comparisons --
// from whatever is set when it is reached.
#include <winsdkver.h>

// 0x0600 is Windows Vista. Sawbuck runs on Windows XP, and these are not in
// conflict: the macro decides which declarations the SDK and WTL headers make
// visible to the compiler, and nothing more. It does not decide which Windows
// the binary runs on.
//
// Two files set them, and the second wins for Sawbuck's own code:
// build/common.gypi puts 0x0A00 on every target in the tree, and
// sawbuck/sawbuck_targets.gypi excludes that and substitutes 0x0600 for the
// four Sawbuck .gyp files that compile C++. So 0x0600 is what the viewer
// actually builds against, and it is the value repeated here.
//
// Running on XP comes from /SUBSYSTEM:WINDOWS,5.01 -- the subsystem version
// in the PE header, which is what XP's loader checks -- and from
// third_party/winwrap, which defines the __imp_ slots for the post-XP imports
// the v143 CRT and the SDK headers reference, asks the running OS for the real
// function first, and falls back only where there is none. See
// third_party/winwrap/docs/USAGE.md. Where Sawbuck's own code wants an API
// newer than its oldest target, the answer is GetProcAddress at the call site
// with a working path for when it is absent, or winwrap. Both need the
// declaration visible, which is exactly what keeping this floor up provides.
//
// So the Vista floor is a settled decision rather than a way station, and
// lowering it to 0x0501 would cost visible declarations while buying nothing.
// sawbuck_targets.gypi carries the full reasoning and names the two specific
// things -- one in WTL, one in ATL's shell dialogs -- that an XP floor breaks.
//
// Keep all three in step. sdkddkver.h derives and cross-checks them, so a
// mismatched pair is not a coherent configuration.
#ifndef _WIN32_WINNT
 #define _WIN32_WINNT 0x0600
#endif
#ifndef WINVER
 #define WINVER 0x0600
#endif
#ifndef NTDDI_VERSION
 #define NTDDI_VERSION 0x06000000
#endif

#include <sdkddkver.h>

// Both of these are read by windows.h below, so they have to be set before it
// and after nothing in particular. NOMINMAX matters to WTL especially, which
// wants std::min and std::max rather than the windows.h macros.
#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
 #define NOMINMAX
#endif

// atlbase.h includes windows.h itself, but naming it is worth a line: it makes
// the dependency visible, and it is the point at which the two macros above
// take effect.
#include <windows.h>

// Both of these ask ATL for the XP-safe spellings of its own functions --
// avoiding InitOnceExecuteOnce, GetThreadId, SetFileInformationByHandle and
// the like in ATL's internals. They are read by atlbase.h below, so they
// belong here rather than further up, and they are the one concession made in
// the headers rather than at link time.
//
// _USING_V110_SDK71_ is the older spelling, from build/common.gypi:2916;
// _ATL_XP_TARGETING is the current one, from sawbuck/sawbuck_targets.gypi.
// Both are build defines, so these #ifndefs are documentation like the rest.
//
// The .gypi is the one that matters: ATL is header-only, so this has to be
// consistent across every translation unit that instantiates an ATL template,
// and a header can only speak for the files that include it. Several Sawbuck
// files use ATL without going through this one -- preferences.h and
// provider_configuration.cc take <atlbase.h> directly, and so does everything
// under sawbuck/common.
#ifndef _USING_V110_SDK71_
 #define _USING_V110_SDK71_
#endif
#ifndef _ATL_XP_TARGETING
 #define _ATL_XP_TARGETING
#endif

// ATL and WTL, in the only order their own prerequisite checks allow.
// atlapp.h errors out without atlbase.h; atlctrls.h and atlframe.h error out
// without both atlapp.h and atlwin.h. WTL 9.1 added those checks, so an
// alphabetical sort here would not compile. atlapp.h also brings atluser.h and
// atlgdi.h with it, which is where CMenu and the DC classes come from.
#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include <atlframe.h>
#include <atlmisc.h>

// The two standard library headers essentially every viewer file wants.
#include <string>
#include <vector>

// Left out on purpose, so that nobody has to wonder:
//
//   <atlsplit.h>   log_viewer.h alone
//   <atldlgs.h>    viewer_window.h and filter_dialog.cc
//   <atltheme.h>   provider_dialog.cc alone
//   <atlalloc.h>   log_list_view.cc alone
//   <atlres.h>     viewer_window.h, and it defines resource ids rather than
//                  code, so it wants to stay next to resource.h
//   <wmistr.h>     must immediately precede <evntrace.h>, and only the two
//   <evntrace.h>   files that consume ETW events need either
//   <map>          three files
//   <utility>      two files
//   <algorithm>    one file
//   <string.h>     three files, and it is a C header rather than part of the
//                  platform preamble
//
//   base/...       none of it. base/logging.h is in twelve viewer files and
//                  would look like a candidate, but base/ is a dependency
//                  Sawbuck pins and rolls, not part of the platform, and
//                  pulling any of it in here would make every viewer file
//                  depend on it whether or not it logs.
//
// Also not here: preferences.h, filter.h, provider_configuration.h,
// filtered_log_view.h and mock_log_view_interfaces.h do not include this
// file. They need <atlbase.h> at most -- usually just <string> and <vector> --
// and making them pull in all of WTL would cost every file that includes them.

#endif  // SAWBUCK_VIEWER_FRAMEWORK_H_
