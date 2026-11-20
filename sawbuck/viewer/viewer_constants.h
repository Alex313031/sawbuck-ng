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

#ifndef SAWBUCK_VIEWER_VIEWER_CONSTANTS_H_
#define SAWBUCK_VIEWER_VIEWER_CONSTANTS_H_

// Constants used by the sawbuck viewer app.
//
// Not wrapped in an unnamed namespace, although a constants header is the
// place one looks natural. In a header it is the wrong tool: every translation
// unit that includes the file gets its own distinct entity, which is not what
// "one place for the constants" is meant to mean, and it stops being harmless
// the moment two files include it and pass one of these across the boundary.
//
// It is also unnecessary here. Each definition below is either an array of
// const wchar_t or a constexpr int, and both of those have internal linkage at
// namespace scope by themselves -- so each file still gets its own copy, with
// no risk of a duplicate-symbol error, and without the unnamed namespace.
//
// That is also why the symbol server URLs are arrays rather than
// `const wchar_t*` pointers, which is what they were while they lived in
// viewer_window.cc. A pointer-to-const is not itself const, so it would have
// external linkage, and the second file to include this would be a link error
// rather than a warning.

// Main window title
const wchar_t kWindowTitle[] = L"Sawbuck Log Viewer";

// Session name title
const wchar_t kSessionName[] = L"Sawbuck Log Session";

// What Help opens. The document in the repository rather than a copy shipped
// beside the executable: it is then current for whoever reads it, and the
// portable build stays four files.
const wchar_t kHelpUrl[] =
    L"https://github.com/Alex313031/sawbuck-ng/blob/main/docs/USAGE.md";

// Microsoft's public symbol server. See InitSymbolPath for why there are two
// of each: http below Windows 8, where TLS 1.2 is not on by default, and
// https from 8 up.
const wchar_t kMicrosoftSymSrvHttp[] =
    L"http://msdl.microsoft.com/download/symbols";
const wchar_t kMicrosoftSymSrvHttps[] =
    L"https://msdl.microsoft.com/download/symbols";

// Google's Chromium symbol server
const wchar_t kChromeSymSrvHttp[] =
    L"http://chromium-browser-symsrv.commondatastorage.googleapis.com";
const wchar_t kChromeSymSrvHttps[] =
    L"https://chromium-browser-symsrv.commondatastorage.googleapis.com";

// Where Windows records whether TLS 1.2 may be used, read by
// IsTls12EnabledForWinHttp below Windows 8 to decide whether an https symbol
// server is worth offering. Read-only, and both are readable by any user.
//
// The schannel key is under SYSTEM, which WOW64 does not redirect, so there is
// one copy of it whatever the architecture of the process asking. The WinHttp
// key is under SOFTWARE, which is redirected -- and that is wanted here, since
// a 32-bit Sawbuck should read the same Wow6432Node copy its own WinHTTP will.
const wchar_t kTls12ClientKey[] =
    L"SYSTEM\\CurrentControlSet\\Control\\SecurityProviders\\SCHANNEL\\"
    L"Protocols\\TLS 1.2\\Client";
const wchar_t kWinHttpKey[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings\\WinHttp";

const wchar_t kEnabledValue[] = L"Enabled";
const wchar_t kDisabledByDefaultValue[] = L"DisabledByDefault";
const wchar_t kDefaultSecureProtocolsValue[] = L"DefaultSecureProtocols";

// The TLS 1.2 bit in DefaultSecureProtocols. The others, for reference, are
// 0x08 SSL 2.0, 0x20 SSL 3.0, 0x80 TLS 1.0 and 0x200 TLS 1.1; only 1.2 is
// asked about here, since it is the only one these symbol servers accept.
//
// unsigned long rather than DWORD, which is the same type on Windows, so that
// this file needs no #include of its own. A constants header that compiles
// standing alone cannot be broken by the order someone includes it in.
constexpr unsigned long kTls12Protocol = 0x00000800;

// The size of the window on a first run, when there is no saved placement to
// restore: 60% of the work area, but never smaller than this and never larger
// than the work area itself. See SetDefaultPlacement.
//
// 60% leaves the window obviously smaller than the screen -- so it reads as a
// window rather than as something that failed to maximize -- while being large
// enough on any ordinary display to show the whole log view. The floor is what
// makes it usable on a small one: 1180 is a little over the sum of the log
// view's default column widths (log_list_view.cc's kColumns comes to about
// 1140 once the heading floors are applied) plus the frame and the vertical
// scrollbar, so every column is visible without scrolling sideways. 600 has
// nothing to derive it from; it is simply enough rows to be worth reading,
// split between the list and the stack trace pane.
constexpr int kDefaultWorkAreaFraction = 60;  // percent
constexpr int kMinDefaultWidth = 1180;
constexpr int kMinDefaultHeight = 600;

// The smallest the user can drag the window, which is a different and much
// lower bound: small enough to tuck away in a corner while something else has
// the screen, large enough that the menu bar does not wrap and the status bar
// still has somewhere to put text.
//
// Windows enforces a minimum of its own -- SM_CXMINTRACK by SM_CYMINTRACK,
// around 136 by 39 -- which is far below this, so ours is what applies.
constexpr int kMinTrackWidth = 400;
constexpr int kMinTrackHeight = 300;

#endif  // SAWBUCK_VIEWER_VIEWER_CONSTANTS_H_
