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
// Wire-format constants for the ETW events Sawbuck decodes.
//
// These used to be taken from base/debug/trace_event_win.h, which Chromium
// removed after M37. Sawbuck owns them now, and that is the right place for
// them regardless of which Chromium base happens to be checked out: Sawbuck
// *decodes* events emitted by some other build of Chrome, possibly years
// older than this tree. The format it has to read is fixed by whatever wrote
// the log, not by the headers compiled against here. Tracking base would mean
// silently losing the ability to read older logs every time base moved on.
//
// Values are reproduced from base/debug/trace_event_win.{h,cc} as of Chromium
// r271365 (37.0.2002.0), the last revision that carried them.
#ifndef SAWBUCK_LOG_LIB_ETW_CONSTANTS_H_
#define SAWBUCK_LOG_LIB_ETW_CONSTANTS_H_

#include <guiddef.h>

#include "base/win/event_trace_provider.h"

// Event class GUID for Chrome's 32-bit trace events.
// {B967AE67-BB22-49d7-9406-55D91EE1D560}
constexpr GUID kTraceEventClass32 = {
    0xb967ae67, 0xbb22, 0x49d7,
    { 0x94, 0x06, 0x55, 0xd9, 0x1e, 0xe1, 0xd5, 0x60 } };

// Event class GUID for Chrome's 64-bit trace events. Not currently decoded;
// kept for completeness, since it is part of the same format.
// {97BE602D-2930-4ac3-8046-B6763B631DFE}
constexpr GUID kTraceEventClass64 = {
    0x97be602d, 0x2930, 0x4ac3,
    { 0x80, 0x46, 0xb6, 0x76, 0x3b, 0x63, 0x1d, 0xfe } };

// Event types, carried in EVENT_TRACE::Header.Class.Type.
constexpr base::win::EtwEventType kTraceEventTypeBegin = 0x10;
constexpr base::win::EtwEventType kTraceEventTypeEnd = 0x11;
constexpr base::win::EtwEventType kTraceEventTypeInstant = 0x12;

#endif  // SAWBUCK_LOG_LIB_ETW_CONSTANTS_H_
