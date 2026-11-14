# Copyright 2012 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# This include file will be in force for all gyp files processed in the
# Sawbuck tree.

{
  'variables': {
    # This allows us to decouple the repository root from '<(DEPTH)', as
    # the relative depth of a pure git repository and an SVN repository
    # is different.
    'src': '<(DEPTH)',

    # Disable the compatibility manifest that common.gypi brings in by
    # default for all executables.
    'win_exe_compatibility_manifest': '',

    # Disable test isolation.
    'test_isolation_mode': 'noop',
  },

  # NOTE: GYP applies this file to *every* .gyp in the tree (it is passed with
  # --include), so anything here also hits Chromium's base, testing/ and
  # third_party/. Settings meant only for Sawbuck's own code belong in
  # sawbuck_targets.gypi instead, which the Sawbuck .gyp files include
  # explicitly.
  'target_defaults': {
    'conditions': [
      ['OS=="win"', {
        'defines': [
          # base/containers/hash_tables.h includes <hash_map>, which MSVC has
          # turned into a hard #error since VS2015 ("<hash_map> is deprecated
          # and will be REMOVED"). This is the suppression the error message
          # itself names.
          #
          # It has to be global: the offending include is in base, which DEPS
          # fetches, so it cannot be patched durably -- the next gclient sync
          # would revert it. Dropping it is a real porting job, since
          # stdext::hash_map and std::unordered_map differ in more than name.
          '_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS',
        ],
      }],
    ],
  },
}
