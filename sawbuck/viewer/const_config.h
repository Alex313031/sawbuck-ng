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
// Configuration-related constants.
#ifndef SAWBUCK_VIEWER_CONST_CONFIG_H_
#define SAWBUCK_VIEWER_CONST_CONFIG_H_

// The vendor\product prefix every Sawbuck key hangs off, under both HKLM and
// HKCU. Was Software\Google\SawBuck upstream; renamed with the fork, and the
// product spelling normalised to Sawbuck (the old constants mixed "SawBuck"
// here with "Sawbuck" in the installer, which only worked because the
// registry is case-insensitive).
//
// Defined once and concatenated below rather than written out three times:
// installer\sawbuck.wxs has to spell the same paths by hand, and an installer
// writing one path while the viewer reads another is a silent failure -- the
// provider list simply comes up empty, with no error.
//
// Renaming orphans whatever an older build stored under the Google key:
// window placement, column order and widths, saved filters and the symbol
// path. Nothing migrates it. For the symbol path that is arguably a feature,
// since InitSymbolPath only ever fabricates a default when no value exists.
#define SAWBUCK_REGISTRY_ROOT L"Software\\Alex313031\\Sawbuck"

namespace config {

// Name of key under HKLM that stores provider configuration.
const wchar_t kProviderNamesKey[] = SAWBUCK_REGISTRY_ROOT L"\\Providers";

// Per-provider DWORD value for default enable flags value.
const wchar_t kProviderDefaultFlagsValue[] = L"default_flags";
// Per-provider DWORD value for default log level value.
const wchar_t kProviderDefaultLevelValue[] = L"default_level";
// Per-provider key name for storing name/mask enable flag data.
const wchar_t kProviderFlagsKey[] = L"Flags";

// Top-level settings key under HKCU.
const wchar_t kSettingsKey[] = SAWBUCK_REGISTRY_ROOT;

// Key that stores provider levels under HKCU.
const wchar_t kProviderLevelsKey[] = SAWBUCK_REGISTRY_ROOT L"\\Levels";
// Per-provider DWORD value for current log level.
const wchar_t kProviderLevelValue[] = L"log_level";
// Per-provider DWORD value for current enable flags.
const wchar_t kProviderEnableFlagsValue[] = L"enable_flags";

// Symbol path value.
const wchar_t kSymPathValue[] = L"symbol_path";

// Include and exclude regular expression value names.
const wchar_t kIncludeReValue[] = L"include_re";
const wchar_t kExcludeReValue[] = L"exclude_re";

const wchar_t kWindowPosValue[] = L"window_pos";
const wchar_t kLogViewColumnOrder[] = L"log_view_column_order";
const wchar_t kLogViewColumnWidths[] = L"log_view_column_widths";

const wchar_t kStackTraceColumnOrder[] = L"stack_trace_column_order";
const wchar_t kStackTraceColumnWidths[] = L"stack_trace_column_widths";

const wchar_t kFilterViewColumnOrder[] = L"filter_view_column_order";
const wchar_t kFilterViewColumnWidths[] = L"filter_view_column_widths";

const wchar_t kFilterValues[] = L"filter_values";

}  // namespace config

#endif  // SAWBUCK_VIEWER_CONST_CONFIG_H_
