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
// Provider settings class declaration.
#ifndef SAWBUCK_VIEWER_PROVIDER_CONFIGURATION_H_
#define SAWBUCK_VIEWER_PROVIDER_CONFIGURATION_H_

#include <string>
#include <vector>
#include "base/win/event_trace_provider.h"

class ProviderConfiguration {
 public:
  ProviderConfiguration();

  // Copies this from other.
  void Copy(const ProviderConfiguration& other);

  // Reads the provider information from root_key.
  bool ReadProviders();

  // Writes the default provider registrations under HKLM, the same set
  // installer\sawbuck.wxs writes. This exists so a zip distribution is
  // equivalent to running the MSI: the WiX installer cannot be used
  // everywhere Sawbuck runs, and without these registrations the provider
  // list is empty and nothing can be captured.
  //
  // Does not touch the caller's settings; follow a successful call with
  // ReadProviders and ReadSettings to pick the new registrations up.
  //
  // Returns ERROR_SUCCESS, or the Win32 error from the first key that could
  // not be written -- ERROR_ACCESS_DENIED if the process is not elevated.
  // Keys written before a failure are left in place.
  LONG RegisterDefaultProviders();

  // Read and write provider settings from/to root_key.
  bool ReadSettings();
  bool WriteSettings();

  typedef std::vector<std::pair<std::wstring, base::win::EtwEventFlags>>
      FlagNameList;

  // Log level settings for a provider.
  struct Settings {
    // The provider's GUID.
    GUID provider_guid;
    // The provider's name.
    std::wstring provider_name;
    // The current log level.
    base::win::EtwEventLevel log_level;
    // The current enable flags.
    base::win::EtwEventFlags enable_flags;
    // A list of (name, mask) pairs, where mask may have
    // one or more bit set, and the associated name.
    FlagNameList flag_names;
  };

  // Accessor for the current settings.
  const std::vector<Settings>& settings() const { return settings_; }

  // The registrations RegisterDefaultProviders writes, as Settings. Declared
  // after Settings because it needs the type.
  //
  // Exposed rather than kept private so the round trip is testable: register,
  // ReadProviders, and compare against this. Without it a test can only
  // assert that registering returned success, which would not catch a wrong
  // flag mask or default level -- and those fail silently, as a checkbox in
  // the provider dialog that does nothing.
  //
  // This is also the single description of the set. RegisterDefaultProviders
  // writes whatever this returns, so there is one list rather than one to
  // write and one to verify.
  static void GetDefaultProviders(std::vector<Settings>* providers);

 private:
  std::vector<Settings> settings_;

  DISALLOW_COPY_AND_ASSIGN(ProviderConfiguration);
};

#endif  // SAWBUCK_VIEWER_PROVIDER_CONFIGURATION_H_
