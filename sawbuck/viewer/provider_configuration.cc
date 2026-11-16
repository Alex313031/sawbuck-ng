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
// Provider settings class implementation.
#include "sawbuck/viewer/provider_configuration.h"

#include <atlbase.h>
#include <string>
#include "base/logging.h"
#include "base/win/registry.h"
#include "sawbuck/viewer/const_config.h"

ProviderConfiguration::ProviderConfiguration() {
}

void ProviderConfiguration::Copy(const ProviderConfiguration& other) {
  settings_ = other.settings_;
}

namespace {

// A named enable-flag bit, as it appears under a provider's Flags subkey:
// the key name is the label the provider dialog shows, and its default value
// is the mask.
struct FlagEntry {
  const wchar_t* name;
  DWORD mask;
};

// Shared by the providers that use Chromium's base logging, and the same two
// entries installer\chrome_log_provider.wxi writes.
const FlagEntry kChromeLogFlags[] = {
  { L"Stack Trace", 1 },
  { L"Text Only", 2 },
};

// Omaha's own flag set. LC_ALL is 4094 rather than 4095 because it covers the
// LC_* bits without Stack Trace.
const FlagEntry kOmahaFlags[] = {
  { L"Stack Trace", 1 },
  { L"LC_LOGGING", 2 },
  { L"LC_UTIL", 4 },
  { L"LC_SETUP", 8 },
  { L"LC_SHELL", 16 },
  { L"LC_CORE", 32 },
  { L"LC_JS", 64 },
  { L"LC_PLUGIN", 128 },
  { L"LC_SERVICE", 256 },
  { L"LC_OPT", 512 },
  { L"LC_NET", 1024 },
  { L"LC_REPORT", 2048 },
  { L"LC_ALL", 4094 },
};

const FlagEntry kStackTraceOnlyFlags[] = {
  { L"Stack Trace", 1 },
};

struct DefaultProvider {
  // The provider GUID, in the registry string form used as the key name.
  const wchar_t* guid;
  // Display name, stored as the key's default value.
  const wchar_t* name;
  DWORD default_level;
  DWORD default_flags;
  const FlagEntry* flags;
  size_t num_flags;
};

// Must stay in step with installer\sawbuck.wxs. The whole point of this table
// is that a zip distribution registers exactly what the MSI does, so a
// provider added in one place belongs in the other.
//
// Omaha is Google Update and IE Toolbar is Google Toolbar for Internet
// Explorer; both products are discontinued and both entries are inherited
// from upstream. They are kept only so the two lists match.
const DefaultProvider kDefaultProviders[] = {
  // Chrome, and every Chromium-based browser that builds chrome/ -- they all
  // share this GUID. See the note in sawbuck.wxs.
  { L"{7FE69228-633E-4f06-80C1-527FEA23E3A7}", L"Chromium", 4, 1,
    kChromeLogFlags, arraysize(kChromeLogFlags) },
  // Sawbuck's own logs default to ERROR only, so capturing does not drown the
  // view in Sawbuck's own output.
  { L"{C43B1318-C63D-465b-BCF4-7A89A369F8ED}", L"Sawbuck", 1, 1,
    kChromeLogFlags, arraysize(kChromeLogFlags) },
  { L"{93BCE0BF-3FAF-43b1-9E28-BEB6FAB5ECE7}", L"Chrome Setup", 4, 1,
    kChromeLogFlags, arraysize(kChromeLogFlags) },
  { L"{3DADA31D-19EF-4dc1-B345-037927193422}", L"Chrome Perf Traces", 0, 0,
    NULL, 0 },
  { L"{9B18BFF9-915E-4cc1-9C3E-F4AC112CB36C}", L"Omaha", 4, 4095,
    kOmahaFlags, arraysize(kOmahaFlags) },
  { L"{79FDF710-2D7F-488f-B00C-C6EFAE507169}", L"IE Toolbar", 4, 1,
    kStackTraceOnlyFlags, arraysize(kStackTraceOnlyFlags) },
};

}  // namespace

// static
void ProviderConfiguration::GetDefaultProviders(
    std::vector<Settings>* providers) {
  DCHECK(providers != NULL);
  providers->clear();

  for (size_t i = 0; i < arraysize(kDefaultProviders); ++i) {
    const DefaultProvider& entry = kDefaultProviders[i];

    Settings settings = {};
    // The table stores GUIDs in the registry string form, because that form
    // is also the key name, so there is one spelling to keep right.
    HRESULT hr = ::CLSIDFromString(entry.guid, &settings.provider_guid);
    if (FAILED(hr)) {
      NOTREACHED() << "Malformed GUID in kDefaultProviders: " << entry.guid;
      continue;
    }

    settings.provider_name = entry.name;
    settings.log_level =
        static_cast<base::win::EtwEventLevel>(entry.default_level);
    settings.enable_flags =
        static_cast<base::win::EtwEventFlags>(entry.default_flags);
    for (size_t j = 0; j < entry.num_flags; ++j) {
      settings.flag_names.push_back(
          std::make_pair(std::wstring(entry.flags[j].name),
                         entry.flags[j].mask));
    }

    providers->push_back(settings);
  }
}

LONG ProviderConfiguration::RegisterDefaultProviders() {
  // The native view explicitly, which is the one ReadProviders prefers. The
  // installer is a 32-bit MSI so its writes land in WOW6432Node, and
  // ReadProviders reads both; writing a named view here keeps this command
  // from depending on whether Sawbuck itself is 32- or 64-bit, which would
  // otherwise put the keys somewhere different for each build. Ignored on
  // 32-bit Windows, where there is no redirection.
  REGSAM access = KEY_READ | KEY_WRITE | KEY_WOW64_64KEY;

  base::win::RegKey providers_key;
  LONG err = providers_key.Create(HKEY_LOCAL_MACHINE,
                                  config::kProviderNamesKey, access);
  if (err != ERROR_SUCCESS) {
    // Retry without naming a view, for the same reason ReadProviders makes an
    // unqualified attempt: the unit tests point HKLM at a subkey of HKCU with
    // RegOverridePredefKey, and the view flags are not obviously harmless
    // against an override handle. This cannot mask a real permission problem,
    // because that fails either way. Everything below uses whichever access
    // worked, so the whole set lands in one view.
    access = KEY_READ | KEY_WRITE;
    err = providers_key.Create(HKEY_LOCAL_MACHINE, config::kProviderNamesKey,
                               access);
  }
  if (err != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to create the provider names key, error " << err;
    return err;
  }

  std::vector<Settings> providers;
  GetDefaultProviders(&providers);

  for (size_t i = 0; i < providers.size(); ++i) {
    const Settings& provider = providers[i];

    wchar_t guid_string[40] = {};
    if (!::StringFromGUID2(provider.provider_guid, guid_string,
                           arraysize(guid_string))) {
      NOTREACHED() << "Unable to format a provider GUID";
      continue;
    }

    base::win::RegKey key;
    err = key.Create(providers_key.Handle(), guid_string, access);
    if (err == ERROR_SUCCESS) {
      // The display name is the key's default value, which is what
      // ReadProviders queries for it.
      err = key.WriteValue(NULL, provider.provider_name.c_str());
    }
    if (err == ERROR_SUCCESS)
      err = key.WriteValue(config::kProviderDefaultLevelValue,
                           static_cast<DWORD>(provider.log_level));
    if (err == ERROR_SUCCESS)
      err = key.WriteValue(config::kProviderDefaultFlagsValue,
                           static_cast<DWORD>(provider.enable_flags));
    if (err != ERROR_SUCCESS) {
      LOG(ERROR) << "Unable to register provider " << provider.provider_name
                 << ", error " << err;
      return err;
    }

    if (provider.flag_names.empty())
      continue;

    base::win::RegKey flags;
    err = flags.Create(key.Handle(), config::kProviderFlagsKey, access);
    for (size_t j = 0;
         err == ERROR_SUCCESS && j < provider.flag_names.size(); ++j) {
      base::win::RegKey flag;
      err = flag.Create(flags.Handle(), provider.flag_names[j].first.c_str(),
                        access);
      if (err == ERROR_SUCCESS) {
        // Each flag's mask is its key's default value, matching how
        // ReadProviders reads them back.
        err = flag.WriteValue(
            NULL, static_cast<DWORD>(provider.flag_names[j].second));
      }
    }
    if (err != ERROR_SUCCESS) {
      LOG(ERROR) << "Unable to write the flags for provider "
                 << provider.provider_name << ", error " << err;
      return err;
    }
  }

  return ERROR_SUCCESS;
}

bool ProviderConfiguration::ReadProviders() {
  settings_.clear();

  CRegKey providers;
  // Every Open on this read path passes KEY_READ explicitly. ATL's
  // CRegKey::Open defaults samDesired to KEY_READ | KEY_WRITE, and asking for
  // write access under HKEY_LOCAL_MACHINE fails with ERROR_ACCESS_DENIED
  // unless the process is elevated. Sawbuck.exe always is -- its manifest sets
  // requireAdministrator -- so this only ever surfaced in the unit tests,
  // which are not elevated, as "Error opening provider key <guid>, 5".
  //
  // Both registry views are tried, because WOW64 redirects HKLM\Software and
  // the registrations can land in either one. The installer is a 32-bit MSI,
  // so on 64-bit Windows its writes are redirected into
  // HKLM\Software\WOW6432Node; a 32-bit Sawbuck reading this path is
  // redirected the same way and finds them, while a 64-bit Sawbuck is not and
  // sees nothing -- which presented as an empty Configure Providers dialog and
  // no capture at all in x64 builds. Someone registering a provider by hand
  // with a 64-bit tool, or a future 64-bit installer, puts them in the native
  // view instead, so neither view can be assumed.
  //
  // The view flags are stated rather than left implicit so the lookup does not
  // depend on this process's own bitness. Both are valid from either, and are
  // ignored on 32-bit Windows, where there is no redirection to begin with.
  // Note this affects HKLM only: plain HKCU\Software is not redirected, so the
  // per-provider levels ReadSettings and WriteSettings deal with are shared
  // between the two builds already.
  //
  // The third, unqualified attempt is for the unit tests. They point HKLM at a
  // subkey of HKCU with RegOverridePredefKey, and it is not obvious that the
  // view flags stay harmless against an override handle. Falling back to a
  // plain open costs nothing -- in a real process it resolves to the same view
  // as one of the two above -- and keeps the fixture working whatever the
  // answer turns out to be.
  const REGSAM kViews[] = {
    KEY_READ | KEY_WOW64_64KEY,
    KEY_READ | KEY_WOW64_32KEY,
    KEY_READ,
  };
  LONG err = ERROR_FILE_NOT_FOUND;
  for (size_t i = 0; i < arraysize(kViews) && err != ERROR_SUCCESS; ++i) {
    err = providers.Open(HKEY_LOCAL_MACHINE, config::kProviderNamesKey,
                         kViews[i]);
  }
  if (err == ERROR_FILE_NOT_FOUND) {
    // Not an error: the key is simply absent, which is the normal state
    // before the installer has run, and in any test that constructs a
    // ViewerWindow without populating the registry first. There is nothing
    // to capture until providers are registered, and the Register Providers
    // menu item exists for exactly that.
    //
    // WARNING rather than ERROR specifically because this is reached during
    // the unit tests. MSBuild matches Chromium's ":ERROR:file.cc(line)]" log
    // prefix against its own canonical "file(line): error CODE:" format,
    // logs it as a build error, and then fails the custom build step that
    // produced it -- reporting MSB8066 "exited with code -1" even though
    // run_unittests.py returned 0 and every test passed. A WARNING is
    // matched too, but a logged warning does not fail the build.
    LOG(WARNING) << "No ETW providers are registered, so nothing can be "
                 << "captured yet.";
    return false;
  }
  if (err != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to open provider names key in any registry view, "
               << "last error " << err;
    return false;
  }

  for (DWORD index = 0; true; ++index) {
    wchar_t tmp_string[256];
    DWORD tmp_len = arraysize(tmp_string);
    err = providers.EnumKey(index, tmp_string, &tmp_len);
    if (err == ERROR_NO_MORE_ITEMS) {
      break;
    } else if (err != ERROR_SUCCESS) {
      LOG(ERROR) << "Error enumerating provider names" << err;
      continue;
    }

    GUID provider_guid = {};
    if (FAILED(::CLSIDFromString(tmp_string, &provider_guid))) {
      LOG(ERROR) << "Non-GUID provider \"" << tmp_string << "\"";
      continue;
    }

    // Open the provider key, read its name etc.
    CRegKey provider;
    err = provider.Open(providers, tmp_string, KEY_READ);
    if (err != ERROR_SUCCESS) {
      LOG(ERROR) << "Error opening provider key " << tmp_string << ", " << err;
      continue;
    }

    tmp_len = arraysize(tmp_string);
    err = provider.QueryStringValue(NULL, tmp_string, &tmp_len);
    if (err != ERROR_SUCCESS) {
      LOG(ERROR) << "Error reading provider name " << err;
      continue;
    }

    // Read the default trace level, defaulting to INFO on error.
    DWORD default_level = TRACE_LEVEL_INFORMATION;
    provider.QueryDWORDValue(config::kProviderDefaultLevelValue, default_level);

    // Read the default trace flags, defaulting to all-on.
    DWORD default_flags = 0xFFFFFFFF;
    provider.QueryDWORDValue(config::kProviderDefaultFlagsValue, default_flags);

    settings_.push_back(Settings());
    Settings& settings = settings_.back();
    settings.provider_guid = provider_guid;
    settings.provider_name = tmp_string;
    settings.log_level = static_cast<base::win::EtwEventLevel>(default_level);
    settings.enable_flags = default_flags;

    // Read the flags names and value.
    CRegKey flags;
    err = flags.Open(provider, config::kProviderFlagsKey, KEY_READ);
    if (err != ERROR_SUCCESS)
      continue;

    for (int flags_index = 0; true; ++flags_index) {
      tmp_len = arraysize(tmp_string);
      err = flags.EnumKey(flags_index, tmp_string, &tmp_len);
      if (err == ERROR_NO_MORE_ITEMS) {
        break;
      } else if (err != ERROR_SUCCESS) {
        LOG(ERROR) << "Error enumerating provider names" << err;
        continue;
      }

      CRegKey flag;
      err = flag.Open(flags, tmp_string, KEY_READ);
      if (err != ERROR_SUCCESS) {
        LOG(ERROR) << "Error reading opening flag "
            << tmp_string << ", " << err;
        continue;
      }

      DWORD mask = 0;
      err = flag.QueryDWORDValue(NULL, mask);
      settings.flag_names.push_back(std::make_pair(tmp_string, mask));
    }
  }

  return true;
}

bool ProviderConfiguration::ReadSettings() {
  CRegKey levels_key;
  LONG err = levels_key.Create(HKEY_CURRENT_USER,
                               config::kProviderLevelsKey,
                               0,
                               0,
                               KEY_WRITE);
  if (err == ERROR_FILE_NOT_FOUND) {
    // No settings initialized at all, the caller is pre-set to defaults.
    return true;
  } else if (err != ERROR_SUCCESS) {
    LOG(ERROR) << "Error reading provider log levels: " << err;

    return false;
  }

  for (size_t i = 0; i < settings_.size(); ++i) {
    wchar_t provider_name[40] = {};
    CHECK(::StringFromGUID2(settings_[i].provider_guid,
                            provider_name,
                            arraysize(provider_name)));

    CRegKey settings_key;
    err = settings_key.Open(levels_key, provider_name, KEY_READ);
    if (err == ERROR_FILE_NOT_FOUND) {
      // No settings key for this provider, keep moving.
      continue;
    } else if (err != ERROR_SUCCESS) {
      LOG(ERROR) << "Error reading log level for provider " <<
          settings_[i].provider_name << ", error: " << err;
      continue;
    }

    DWORD log_level = 0;
    err = settings_key.QueryDWORDValue(config::kProviderLevelValue, log_level);
    if (err == ERROR_SUCCESS)
      settings_[i].log_level =
          static_cast<base::win::EtwEventLevel>(log_level);

    DWORD enable_flags = 0;
    err = settings_key.QueryDWORDValue(config::kProviderEnableFlagsValue,
                                       enable_flags);
    if (err == ERROR_SUCCESS)
      settings_[i].enable_flags = enable_flags;
  }

  return true;
}

bool ProviderConfiguration::WriteSettings() {
  CRegKey levels_key;
  LONG err = levels_key.Create(HKEY_CURRENT_USER,
                               config::kProviderLevelsKey,
                               0,
                               0,
                               KEY_WRITE);
  if (err != ERROR_SUCCESS) {
    LOG(ERROR) << "Error saving provider log levels: " << err;

    return false;
  }

  for (size_t i = 0; i < settings_.size(); ++i) {
    wchar_t provider_name[40] = {};
    CHECK(::StringFromGUID2(settings_[i].provider_guid,
                            provider_name,
                            arraysize(provider_name)));

    CRegKey settings_key;
    err = settings_key.Create(levels_key, provider_name);
    if (err == ERROR_SUCCESS) {
      err = settings_key.SetDWORDValue(config::kProviderLevelValue,
                                       settings_[i].log_level);
    }
    if (err == ERROR_SUCCESS) {
      err = settings_key.SetDWORDValue(config::kProviderEnableFlagsValue,
                                       settings_[i].enable_flags);
    }

    if (err != ERROR_SUCCESS) {
      LOG(ERROR) << "Error writing log level for provider " <<
          settings_[i].provider_name << ", error: " << err;
      return false;
    }
  }

  return true;
}
