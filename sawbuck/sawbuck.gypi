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

    # Warnings-as-errors, off while the VS2022 port is in progress.
    #
    # build/common.gypi turns it on for all first-party code at /W4, with a
    # suppression list curated in 2014. Every warning MSVC has added since
    # then therefore lands as a hard error -- that is the C2220 ("the following
    # warning is treated as an error") wall. Set to 'true' to put it back once
    # the remaining warnings are dealt with.
    'sawbuck_warn_as_error%': 'false',
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

        # Warnings MSVC introduced after VS2013, which common.gypi's 2014-era
        # suppression list cannot know about. Appended to that list.
        #
        # This is a curated set, not a measured one: the build log available so
        # far contained only the Error rows, so the warning codes behind the
        # C2220s are still unknown. Narrow this once they are.
        'msvs_disabled_warnings': [
          4091,  # 'typedef ' ignored when no variable is declared. Fires inside
                 # dbghelp.h, which sym_util and the viewer both depend on.
          4267,  # Conversion from size_t to a smaller type.
          4456,  # Declaration hides previous local declaration.
          4457,  # Declaration hides function parameter.
          4458,  # Declaration hides class member.
          4459,  # Declaration hides global declaration.
          4464,  # Relative include path contains '..' -- which Chromium and
                 # Sawbuck both use throughout.
          4577,  # 'noexcept' used with no exception handling mode specified;
                 # common.gypi compiles with ExceptionHandling 0.
          4838,  # Conversion requires a narrowing conversion.
          4996,  # Deprecated declarations, now including C++17 STL removals.
          5038,  # Data member will be initialized after another member.
        ],

        'msvs_settings': {
          'VCCLCompilerTool': {
            'WarnAsError': '<(sawbuck_warn_as_error)',
            # Pin the C++ standard. common.gypi predates /std: entirely, so
            # the version in force was whatever the compiler defaulted to --
            # C++14 for VS2022. Stating it keeps that from drifting with the
            # toolchain. Raising it is a separate job: this is 2014 code, and
            # C++17 removes things it uses.
            'AdditionalOptions': ['/std:c++14'],
          },
        },
      }],
    ],
  },
}
