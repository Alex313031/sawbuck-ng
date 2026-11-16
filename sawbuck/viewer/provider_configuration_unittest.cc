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
// Provider settings unittests.
#include "sawbuck/viewer/provider_configuration.h"

#include <atlbase.h>
#include <string>
#include <vector>
#include "gtest/gtest.h"
#include "sawbuck/viewer/registry_test.h"
#include "sawbuck/viewer/const_config.h"

namespace {

// This test fixture redirects the HKLM and HKCU registry hives for
// the duration of the test to make it independent of the machine
// and user settings.
class ProviderConfigurationTest : public testing::RegistryTest {
 protected:
  // Creates the HKLM registrations that ProviderConfiguration::
  // ReadProviders() walks: one fully specified provider with named flags,
  // and one bare provider that exercises the fallback defaults.
  bool RegisterProviders();

  // Creates the HKCU per-provider settings that ReadSettings() layers over
  // those defaults.
  bool RegisterProviderSettings();
};

const wchar_t kChromeFrameGuidString[] =
    L"{0562BFC3-2550-45b4-BD8E-A310583D3A6F}";
const wchar_t kChromeGuidString[] =
    L"{7FE69228-633E-4f06-80C1-527FEA23E3A7}";

const GUID kChromeFrameGuid = {
    0x0562BFC3, 0x2550, 0x45b4,
    0xBD, 0x8E, 0xA3, 0x10, 0x58, 0x3D, 0x3A, 0x6F };

const GUID kChromeGuid = {
    0x7FE69228, 0x633E, 0x4f06,
    0x80, 0xC1, 0x52, 0x7F, 0xEA, 0x23, 0xE3, 0xA7 };

// Builds a registry path from a parent path and a subkey name.
std::wstring Join(const std::wstring& parent, const wchar_t* child) {
  return parent + L"\\" + child;
}

bool ProviderConfigurationTest::RegisterProviders() {
  const std::wstring chrome_frame_key =
      Join(config::kProviderNamesKey, kChromeFrameGuidString);
  const std::wstring flags_key =
      Join(chrome_frame_key, config::kProviderFlagsKey);

  // A provider's display name is the key's default value, and each named
  // flag is a subkey of Flags whose default value is the flag's mask. The
  // keys are created in enumeration order, so that the expectations below
  // hold whether or not the registry hands back subkeys sorted.
  return
      WriteString(HKEY_LOCAL_MACHINE, chrome_frame_key.c_str(), NULL,
                  L"Chrome Frame") &&
      WriteDword(HKEY_LOCAL_MACHINE, chrome_frame_key.c_str(),
                 config::kProviderDefaultFlagsValue, 1) &&
      WriteDword(HKEY_LOCAL_MACHINE, chrome_frame_key.c_str(),
                 config::kProviderDefaultLevelValue, 2) &&
      WriteDword(HKEY_LOCAL_MACHINE, Join(flags_key, L"Dummy").c_str(),
                 NULL, 2) &&
      WriteDword(HKEY_LOCAL_MACHINE,
                 Join(flags_key, L"StackDummyTrace").c_str(), NULL, 3) &&
      WriteDword(HKEY_LOCAL_MACHINE, Join(flags_key, L"StackTrace").c_str(),
                 NULL, 1) &&
      // Name only -- no default_level, no default_flags and no Flags key,
      // so ReadProviders() has to supply its own defaults for this one.
      WriteString(HKEY_LOCAL_MACHINE,
                  Join(config::kProviderNamesKey, kChromeGuidString).c_str(),
                  NULL, L"Chrome");
}

bool ProviderConfigurationTest::RegisterProviderSettings() {
  const std::wstring chrome_frame_key =
      Join(config::kProviderLevelsKey, kChromeFrameGuidString);
  const std::wstring chrome_key =
      Join(config::kProviderLevelsKey, kChromeGuidString);

  return
      WriteDword(HKEY_CURRENT_USER, chrome_frame_key.c_str(),
                 config::kProviderEnableFlagsValue, 0xcafebabe) &&
      WriteDword(HKEY_CURRENT_USER, chrome_frame_key.c_str(),
                 config::kProviderLevelValue, 3) &&
      WriteDword(HKEY_CURRENT_USER, chrome_key.c_str(),
                 config::kProviderEnableFlagsValue, 1) &&
      WriteDword(HKEY_CURRENT_USER, chrome_key.c_str(),
                 config::kProviderLevelValue, 2);
}

TEST_F(ProviderConfigurationTest, ReadProviders) {
  ASSERT_TRUE(RegisterProviders());

  ProviderConfiguration settings;
  ASSERT_TRUE(settings.ReadProviders());

  ASSERT_EQ(2, settings.settings().size());
  const ProviderConfiguration::Settings* set = &settings.settings()[0];
  EXPECT_TRUE(kChromeFrameGuid == set->provider_guid);
  EXPECT_STREQ(L"Chrome Frame", set->provider_name.c_str());
  EXPECT_EQ(2, set->log_level);
  EXPECT_EQ(1, set->enable_flags);

  ASSERT_EQ(3, set->flag_names.size());
  EXPECT_STREQ(L"Dummy", set->flag_names[0].first.c_str());
  EXPECT_EQ(2, set->flag_names[0].second);
  EXPECT_STREQ(L"StackDummyTrace", set->flag_names[1].first.c_str());
  EXPECT_EQ(3, set->flag_names[1].second);
  EXPECT_STREQ(L"StackTrace", set->flag_names[2].first.c_str());
  EXPECT_EQ(1, set->flag_names[2].second);

  set = &settings.settings()[1];
  EXPECT_TRUE(kChromeGuid == set->provider_guid);
  EXPECT_STREQ(L"Chrome", set->provider_name.c_str());
  EXPECT_EQ(4, set->log_level);
  EXPECT_EQ(0xFFFFFFFF, set->enable_flags);
  ASSERT_EQ(0, set->flag_names.size());
}

TEST_F(ProviderConfigurationTest, ReadSettings) {
  ASSERT_TRUE(RegisterProviders());
  ASSERT_TRUE(RegisterProviderSettings());

  ProviderConfiguration settings;
  ASSERT_TRUE(settings.ReadProviders());
  ASSERT_TRUE(settings.ReadSettings());

  ASSERT_EQ(2, settings.settings().size());
  const ProviderConfiguration::Settings* set = &settings.settings()[0];

  EXPECT_EQ(3, set->log_level);
  EXPECT_EQ(0xcafebabe, set->enable_flags);

  set = &settings.settings()[1];
  EXPECT_EQ(2, set->log_level);
  EXPECT_EQ(0x1, set->enable_flags);
}

// Round trips the default registrations: write them, read them back through
// the same code the viewer uses, and check they survived. This is what makes
// RegisterDefaultProviders worth having as a menu command -- the failure it
// guards against is not an error return but a wrong mask or level, which
// shows up only as a provider dialog checkbox that quietly does nothing.
TEST_F(ProviderConfigurationTest, RegisterDefaultProviders) {
  ProviderConfiguration settings;
  ASSERT_EQ(ERROR_SUCCESS, settings.RegisterDefaultProviders());
  ASSERT_TRUE(settings.ReadProviders());

  std::vector<ProviderConfiguration::Settings> expected;
  ProviderConfiguration::GetDefaultProviders(&expected);
  ASSERT_FALSE(expected.empty());
  ASSERT_EQ(expected.size(), settings.settings().size());

  // ReadProviders enumerates by key name, which is the GUID string, so it
  // comes back sorted rather than in table order. Match on the GUID.
  for (size_t i = 0; i < expected.size(); ++i) {
    const ProviderConfiguration::Settings* actual = NULL;
    for (size_t j = 0; j < settings.settings().size(); ++j) {
      if (settings.settings()[j].provider_guid == expected[i].provider_guid) {
        actual = &settings.settings()[j];
        break;
      }
    }
    ASSERT_TRUE(actual != NULL) << "Missing provider "
                                << expected[i].provider_name;

    EXPECT_STREQ(expected[i].provider_name.c_str(),
                 actual->provider_name.c_str());
    EXPECT_EQ(expected[i].log_level, actual->log_level);
    EXPECT_EQ(expected[i].enable_flags, actual->enable_flags);

    // Flags come back sorted by key name too, so compare as a set.
    ASSERT_EQ(expected[i].flag_names.size(), actual->flag_names.size())
        << "Wrong flag count for " << expected[i].provider_name;
    for (size_t k = 0; k < expected[i].flag_names.size(); ++k) {
      bool found = false;
      for (size_t m = 0; m < actual->flag_names.size(); ++m) {
        if (actual->flag_names[m].first == expected[i].flag_names[k].first) {
          EXPECT_EQ(expected[i].flag_names[k].second,
                    actual->flag_names[m].second)
              << "Wrong mask for " << expected[i].provider_name << " flag "
              << expected[i].flag_names[k].first;
          found = true;
          break;
        }
      }
      EXPECT_TRUE(found) << "Missing flag " << expected[i].flag_names[k].first
                         << " for " << expected[i].provider_name;
    }
  }
}

TEST_F(ProviderConfigurationTest, WriteSettings) {
  ASSERT_TRUE(RegisterProviders());

  ProviderConfiguration settings;
  ASSERT_TRUE(settings.ReadProviders());

  // Write the configuration from the default settings.
  ASSERT_TRUE(settings.WriteSettings());

  CRegKey key;
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                    config::kProviderLevelsKey,
                                    KEY_READ));

  CRegKey provider;
  ASSERT_EQ(ERROR_SUCCESS, provider.Open(
      key, L"{0562BFC3-2550-45b4-BD8E-A310583D3A6F}", KEY_READ));

  DWORD temp = 0;
  ASSERT_EQ(ERROR_SUCCESS, provider.QueryDWORDValue(L"log_level", temp));
  EXPECT_EQ(2, temp);

  ASSERT_EQ(ERROR_SUCCESS, provider.QueryDWORDValue(L"enable_flags", temp));
  EXPECT_EQ(1, temp);

  ASSERT_EQ(ERROR_SUCCESS, provider.Open(
      key, L"{7FE69228-633E-4f06-80C1-527FEA23E3A7}", KEY_READ));

  ASSERT_EQ(ERROR_SUCCESS, provider.QueryDWORDValue(L"log_level", temp));
  EXPECT_EQ(4, temp);

  ASSERT_EQ(ERROR_SUCCESS, provider.QueryDWORDValue(L"enable_flags", temp));
  EXPECT_EQ(0xFFFFFFFF, temp);
}

}  // namespace
