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

    # Link winwrap and target Windows XP.
    #
    # The declared default here is off, matching how upstream declares
    # use_winwrap in winwrap.gni -- but the generator turns it on, so every
    # build covers XP through 11 from a single binary unless asked not to.
    #
    # tools/gyp_sawbuck.py is what applies it, by appending
    # -D sawbuck_use_winwrap=1 unless told otherwise. Decline it with
    # gyp_sawbuck.py --no-winwrap, or build.bat --no-winwrap, which regenerates
    # with that flag once its sync has finished.
    #
    # It lives in the generator because that is the one place every route into
    # a generated solution passes through, and the previous arrangement proved
    # why that matters: build.bat exported
    # GYP_DEFINES=sawbuck_use_winwrap=1 and relied on GYP reading it inside a
    # gclient hook, five processes away. Generating by hand worked; build.bat
    # did not, with no diagnostic anywhere.
    #
    # The failure is invisible by construction. A solution generated without
    # winwrap builds, runs and passes its tests; the only symptom is
    # Sawbuck.exe importing FlsAlloc, IsThreadAFiber,
    # InitializeCriticalSectionEx and the SRWLOCK family straight from
    # kernel32, and then not loading on XP. Generation now says which way it
    # went, and check_imports.py answers it from a finished binary -- see
    # docs/BUILDING.md.
    #
    # The same switch sets the linker's subsystem version, because the two
    # have to agree in both directions. Without winwrap, a binary claiming
    # 5.01 still dies on XP at load time on the first import the OS does not
    # export, before any of its own code runs; with winwrap but without 5.01,
    # the loader rejects it for the subsystem version instead.
    #
    # What it costs, which is little but not nothing:
    #
    #  - A cached pointer load and a jump on every call winwrap covers.
    #    Nothing is replaced on a modern Windows: each shim asks the running
    #    OS for the real function first and only falls back where there is
    #    none, so Windows 7 and later run the system's own code throughout.
    #
    #  - On Vista specifically, the twelve SRWLOCK and CONDITION_VARIABLE
    #    functions become winwrap's. They resolve as a group or not at all --
    #    mixing them is memory corruption, since each implementation reads
    #    that one pointer-sized word differently -- and the group is only
    #    complete from Windows 7, because Vista has no TryAcquireSRWLock* in
    #    kernel32 and no RtlTryAcquireSRWLock* under it. winwrap's is a real
    #    reader/writer lock rather than a CRITICAL_SECTION wearing the name,
    #    but it is not the kernel's, so contended performance differs.
    #
    #  - The subsystem version on every build, and the winwrap members pulled
    #    in behind the /INCLUDE: anchor, which is a handful of KB.
    'sawbuck_use_winwrap%': 0,

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
        # Measured rather than guessed: this is what survived emptying both
        # suppression lists and building Debug x86 and x64 in full. Six
        # entries went away in that pass -- 4091, 4456, 4457 and 4459 because
        # common.gypi's global list (common.gypi:5691) already carries them
        # and nothing of ours fires them, and 4464 and 5038 because both are
        # off-by-default warnings that /W4 never emits at all, so no code
        # change could ever wake them.
        #
        # 4458, 4838 and 4996 are in common.gypi's list too. They are
        # repeated here on purpose, so the reason we need them lives in our
        # own file instead of depending on Chromium's list surviving a
        # build_revision bump.
        'msvs_disabled_warnings': [
          4201,  # Nameless struct/union -- a Microsoft extension rather than
                 # C++, but one the Windows SDK headers use throughout, and
                 # which KernelProcessEvents::ProcessInfo uses deliberately to
                 # keep a variable-length SID reachable under its own name.
          4267,  # Conversion from size_t to a smaller type. x64 only, where
                 # size_t is 64 bits: the list views narrow row and column
                 # counts into the int and ULONG that the common control's own
                 # API takes, and PCRE does the same in pcreposix.c.
          4458,  # Declaration hides class member. All of it is vendored WTL
                 # (atldlgs.h's task dialog setters) and ICU's
                 # messagepattern.h.
          4577,  # 'noexcept' used with no exception handling mode specified;
                 # common.gypi compiles with ExceptionHandling 0. Fires
                 # nowhere today, and kept anyway: that condition is still
                 # true, so writing a noexcept -- or raising /std: -- could
                 # bring it back.
          4838,  # Narrowing conversion required. All of it is vendored WTL
                 # (atlgdi.h, atlframe.h).
          4996,  # Deprecated declarations. The _CRT_SECURE_* family is
                 # already handled by macro -- common.gypi:2910 defines
                 # _CRT_SECURE_NO_DEPRECATE and _SCL_SECURE_NO_DEPRECATE for
                 # the whole tree -- so what is left here is genuinely
                 # deprecated Win32 APIs, plus the entire _CXX17_DEPRECATE_*
                 # family once _HAS_CXX17 becomes 1. Worth unsuppressing
                 # deliberately after the C++17 move rather than carrying it
                 # blind.
        ],

        'msvs_settings': {
          'VCCLCompilerTool': {
            'WarnAsError': '<(sawbuck_warn_as_error)',
            # Pin the C++ standard. common.gypi predates /std: entirely, so
            # the version in force was whatever the compiler defaulted to --
            # C++14 for VS2022. Stating it keeps that from drifting with the
            # toolchain.
            #
            # Raising it to C++17 looks cheap. Scanning the tree for what
            # C++17 removed or deprecated turns up nothing real in sawbuck/
            # at all, and in the dependencies every hit is either a comment
            # (auto_ptr, random_shuffle, mem_fun), an ICU tool we do not
            # build (tzcode's tz2icu.cpp uses ptr_fun/bind2nd), or gtest and
            # gmock's own test suites, which we also do not build (tr1,
            # unary_function). No dynamic exception specifications anywhere,
            # no std::iterator base classes, no stdext array iterators. So
            # none of _HAS_AUTO_PTR_ETC, _HAS_TR1_NAMESPACE or
            # _HAS_FUNCTION_ALLOCATOR_SUPPORT should be needed.
            #
            # What does need doing, in patches/ since both are synced deps:
            # two 'register' declarations, which C++17 removed and MSVC
            # reports as C5033 -- base/third_party/dmg_fp/g_fmt.cc:32-33
            # (built, see base.gypi:634) and
            # third_party/icu/source/common/ustr_wcs.cpp:343 (icuuc).
            #
            # The parts a scan cannot settle: [[nodiscard]] goes live across
            # much of the STL and reports as C4834, which neither suppression
            # list carries, and ExceptionHandling 0 (so _HAS_EXCEPTIONS=0)
            # combined with C++17 is a thinly travelled MSVC STL
            # configuration. Note that <hash_map> is unaffected --
            # _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS above is not gated on
            # the standard version, so it keeps working either way.
            #
            # /Zc:__cplusplus because without it MSVC reports __cplusplus as
            # 199711L no matter what /std: says, and _MSVC_LANG -- the probe
            # that is accurate on MSVC -- is used nowhere in this tree. Five
            # places version-gate on __cplusplus and were all taking their
            # pre-C++11 branch:
            #
            #   base/strings/safe_sprintf.cc:119 enables a static_assert that
            #   numeric_limits<ssize_t>::max() is constexpr. The comment
            #   there blames MSVS2013's library; VS2022's does mark it.
            #
            #   testing/gtest/include/gtest/internal/gtest-port.h:326 sets
            #   GTEST_LANG_CXX11 to 1. This is the largest single effect of
            #   the flag: it moves gtest and gmock onto std::tuple, rvalue
            #   references and '= delete'. Everything that sees it has to
            #   recompile together, so take this as a clean build rather than
            #   an incremental one.
            #
            #   icu unicode/umachine.h:135 stops defining U_OVERRIDE and
            #   U_FINAL to nothing and gives them override and final. An ICU
            #   class that mislabels either becomes a hard error rather than
            #   a warning -- upstream builds as C++11, so it should hold.
            #
            #   icu unicode/platform.h:667 takes U_HAVE_CHAR16_T from 0 to 1,
            #   which only reaches U_DECLARE_UTF16 and u"" literals. UChar is
            #   deliberately NOT gated on it: umachine.h:303-313 has that
            #   typedef commented out as "type-incompatible with pre-C++11
            #   callers", and Windows takes the U_SIZEOF_WCHAR_T==2 branch
            #   instead. So UChar stays wchar_t and no call site in base_i18n
            #   or here changes type.
            #
            #   icu putilimp.h:215 reaches the C++11 arm of
            #   U_HAVE_STD_ATOMICS, but that arm still wants U_HAVE_ATOMIC,
            #   which autoconf supplies and the MSVC build does not, so this
            #   one should stay at 0.
            #
            # Set while still on C++14 on purpose: it moves "which branch
            # compiles" by itself, which keeps the /std: bump a separate and
            # smaller change.
            'AdditionalOptions': ['/std:c++14', '/Zc:__cplusplus'],
          },
        },
      }],
    ],
  },
}
