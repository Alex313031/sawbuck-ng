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
// Test fixture for registry tests.
#include "sawbuck/viewer/registry_test.h"

#include <shlwapi.h>
#include "gtest/gtest.h"

namespace testing {

const wchar_t* kHKCUReplacement =
    L"Software\\Alex313031\\RegistryTest\\HKCU";
const wchar_t* kHKLMReplacement =
    L"Software\\Alex313031\\RegistryTest\\HKLM";

namespace {

// Creates |root|\|key_path| and leaves it open for reading and writing in
// |key|. RegKey::Create creates missing intermediate keys for us.
LONG CreateForWrite(HKEY root, const wchar_t* key_path,
                    base::win::RegKey* key) {
  return key->Create(root, key_path, KEY_READ | KEY_WRITE);
}

}  // namespace

RegistryTest::~RegistryTest() {
  EXPECT_TRUE(hkcu_.Handle() == NULL);
  EXPECT_TRUE(hklm_.Handle() == NULL);
}

void RegistryTest::SetUp() {
  // Wipe the keys we redirect to.
  // This gives us a stable run, even in the presence of previous
  // crashes or failures.
  LSTATUS err = SHDeleteKey(HKEY_CURRENT_USER, kHKCUReplacement);
  EXPECT_TRUE(err == ERROR_SUCCESS || err == ERROR_FILE_NOT_FOUND);
  err = SHDeleteKey(HKEY_CURRENT_USER, kHKLMReplacement);
  EXPECT_TRUE(err == ERROR_SUCCESS || err == ERROR_FILE_NOT_FOUND);

  // Create the keys we're redirecting HKCU and HKLM to.
  //
  // KEY_ALL_ACCESS, not KEY_READ: these handles are what RegOverridePredefKey
  // redirects the predefined keys to, and the tests write through them --
  // the helpers below create the provider keys, and ProviderConfiguration
  // writes settings back. Read-only left that dependent on whether a
  // given Windows build enforces the override handle's access rights on
  // subsequent opens, which is not something to rely on.
  ASSERT_EQ(ERROR_SUCCESS,
            hkcu_.Create(HKEY_CURRENT_USER, kHKCUReplacement,
                         KEY_ALL_ACCESS));
  ASSERT_EQ(ERROR_SUCCESS,
            hklm_.Create(HKEY_CURRENT_USER, kHKLMReplacement,
                         KEY_ALL_ACCESS));

  // And do the switcharoo.
  ASSERT_EQ(ERROR_SUCCESS,
            ::RegOverridePredefKey(HKEY_CURRENT_USER, hkcu_.Handle()));
  ASSERT_EQ(ERROR_SUCCESS,
            ::RegOverridePredefKey(HKEY_LOCAL_MACHINE, hklm_.Handle()));
}

void RegistryTest::TearDown() {
  // Undo the redirection.
  EXPECT_EQ(ERROR_SUCCESS, ::RegOverridePredefKey(HKEY_CURRENT_USER, NULL));
  EXPECT_EQ(ERROR_SUCCESS, ::RegOverridePredefKey(HKEY_LOCAL_MACHINE, NULL));

  // Close our handles and delete the temp keys we redirected to.
  hkcu_.Close();
  hklm_.Close();
  EXPECT_EQ(ERROR_SUCCESS, SHDeleteKey(HKEY_CURRENT_USER, kHKCUReplacement));
  EXPECT_EQ(ERROR_SUCCESS, SHDeleteKey(HKEY_CURRENT_USER, kHKLMReplacement));
}

// These three used to be a single Register() that handed an ATL registry
// script to CRegObject::StringRegister. That dragged the whole statreg.h
// script engine into the tests to create a handful of keys and DWORDs, and it
// did not behave the same everywhere: on one machine StringRegister reported
// S_OK while leaving nothing behind in the redirected hive, so the following
// ReadProviders() failed to open a key the script was supposed to have
// created. The script engine resolves "HKLM" itself and has its own ideas
// about registry redirection and per-user registration, none of which the
// tests need. Writing the keys directly is both shorter and deterministic.
bool RegistryTest::CreateKey(HKEY root, const wchar_t* key_path) {
  base::win::RegKey key;
  LONG err = CreateForWrite(root, key_path, &key);
  EXPECT_EQ(ERROR_SUCCESS, err) << "Creating key " << key_path;
  return err == ERROR_SUCCESS;
}

bool RegistryTest::WriteString(HKEY root, const wchar_t* key_path,
                               const wchar_t* value_name,
                               const wchar_t* value) {
  base::win::RegKey key;
  LONG err = CreateForWrite(root, key_path, &key);
  if (err == ERROR_SUCCESS)
    err = key.WriteValue(value_name, value);
  EXPECT_EQ(ERROR_SUCCESS, err) << "Writing string to " << key_path;
  return err == ERROR_SUCCESS;
}

bool RegistryTest::WriteDword(HKEY root, const wchar_t* key_path,
                              const wchar_t* value_name, DWORD value) {
  base::win::RegKey key;
  LONG err = CreateForWrite(root, key_path, &key);
  if (err == ERROR_SUCCESS)
    err = key.WriteValue(value_name, value);
  EXPECT_EQ(ERROR_SUCCESS, err) << "Writing DWORD to " << key_path;
  return err == ERROR_SUCCESS;
}

}  // namespace testing
