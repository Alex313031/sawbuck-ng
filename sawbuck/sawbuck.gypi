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

    # Same, for third-party code -- which needs its own switch. common.gypi
    # applies this one from a 'target_conditions' block, and those are
    # evaluated per target *after* target_defaults is merged in, so they
    # overwrite anything set there. ICU is the case in point: it kept /WX and
    # died on a warning in uresbund.c while first-party code built fine.
    'win_third_party_warn_as_error%': 'false',

    # NOTE: clang/host_clang are NOT set here. They have to be command-line
    # defines -- see tools/gyp_sawbuck.py -- because a '%' default declared
    # here loses to common.gypi's own nested declaration in any .gyp that
    # brings its own 'variables' block, third_party/pcre/pcre.gyp among them.
  },

  # NOTE: GYP applies this file to *every* .gyp in the tree (it is passed with
  # --include), so anything here also hits Chromium's base, testing/ and
  # third_party/. Settings meant only for Sawbuck's own code belong in
  # sawbuck_targets.gypi instead, which the Sawbuck .gyp files include
  # explicitly.
  'target_defaults': {
    # Build output goes to out/<Config>_<arch>, not build/<Config>.
    #
    # common.gypi sets OutputDirectory once, on the Common_Base configuration
    # that all four concrete configurations inherit:
    #     <(DEPTH)\build\<(build_dir_prefix)$(ConfigurationName)
    # and build_dir_prefix is declared '' and never set to anything. That has
    # two problems.
    #
    #   * build/ is a directory gclient syncs. Compiled output inside a
    #     dependency is output that re-syncing the dependency destroys --
    #     tools/sync_deps.py does an rmtree, so the M49 build_revision bump
    #     would have taken the whole build tree with it. It is also invisible
    #     to git status, because .gitignore ignores /build/ wholesale.
    #   * GYP names the 64-bit configurations Debug_x64 and Release_x64, but
    #     the msvs generator strips that suffix when emitting MSVS
    #     configuration names -- _ConfigBaseName() in generator/msvs.py drops
    #     a trailing '_' + platform -- so both Win32 and x64 see
    #     $(ConfigurationName) == 'Debug'. With build_dir_prefix empty, the
    #     32- and 64-bit builds therefore share one OutDir, and one IntDir
    #     with it, since that is $(OutDir)\obj\$(ProjectName).
    #
    # Setting the directory on the per-platform abstract configurations fixes
    # both at once. Every concrete configuration inherits
    # ['Common_Base', '<arch>_Base', '<Debug|Release>_Base'], and GYP merges
    # msvs_configuration_attributes along that chain in order, so these win
    # over Common_Base. IntermediateDirectory is left alone and follows.
    #
    # This is in sawbuck.gypi rather than patches/ on purpose: it needs no
    # rebasing when build_revision moves, which four patches against
    # common.gypi already do.
    'configurations': {
      'x86_Base': {
        'abstract': 1,
        'msvs_configuration_attributes': {
          'OutputDirectory': '<(DEPTH)\\out\\$(ConfigurationName)_x86',
        },
      },
      'x64_Base': {
        'abstract': 1,
        'msvs_configuration_attributes': {
          'OutputDirectory': '<(DEPTH)\\out\\$(ConfigurationName)_x64',
        },
      },
    },

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
          4201,  # Nameless struct/union -- a Microsoft extension rather than
                 # C++, but one the Windows SDK headers use throughout, and
                 # which KernelProcessEvents::ProcessInfo uses deliberately to
                 # keep a variable-length SID reachable under its own name.
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
