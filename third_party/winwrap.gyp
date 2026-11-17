# Copyright 2026 Alex313031
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
# GYP build for winwrap, which DEPS syncs into third_party/winwrap.
#
# ---------------------------------------------------------------------------
# Why this file is beside that directory rather than inside it
#
# third_party/winwrap is a gclient-managed checkout. build.bat syncs with
# --reset --delete_unversioned_trees, which deletes anything in there that
# upstream does not carry, and .gitignore ignores the directory, so a file put
# inside could neither be committed nor survive a sync. Upstream is GN-only --
# BUILD.gn plus winwrap.gni, no .gyp -- so until it ships one of its own, the
# build file has to live out here where git and gclient both leave it alone.
#
# Referenced as <(DEPTH)/third_party/winwrap.gyp:winwrap.
#
# ---------------------------------------------------------------------------
# READ THIS BEFORE DEPENDING ON IT: link order is load-bearing
#
# winwrap works by defining the __imp_<name> slots that dllimport call sites
# load through, so the linker binds those calls to winwrap instead of to
# kernel32. That only happens if winwrap is searched BEFORE the import
# libraries it is standing in front of -- upstream's README says it "must be
# linked ahead of any other libraries or default libs".
#
# Explicit libraries are searched before default ones, so winwrap.lib being on
# the link line at all gets most of the way there. What has not been verified
# on Windows yet is its position relative to the import libraries that
# common.gypi puts in AdditionalDependencies -- kernel32.lib and friends at
# Common_Base, and another ten at common.gypi:5788. GYP appends, so the
# ordering needs checking rather than assuming.
#
# If a consumer still binds to the system DLL, the fix is /WHOLEARCHIVE:
# winwrap.lib on the consumer's VCLinkerTool AdditionalOptions, which pulls
# every winwrap object in up front and so defines the slots before kernel32 is
# ever consulted.
#
# The other half of XP support is not here: it is MinimumRequiredVersion
# '5.01' on the consumer's VCLinkerTool, which is what produces
# /SUBSYSTEM:WINDOWS,5.01.
#
# ---------------------------------------------------------------------------
# Only the static form is built
#
# Upstream defaults to static (winwrap.gni's is_component_winwrap = false) and
# that is what a single self-contained Sawbuck.exe wants. The shared form is
# deliberately left out for now: it needs src/winwrap.def through
# ModuleDefinitionFile, src/winwrap.rc compiled in, src/winwrap_dll.cc added,
# and the MSVC static CRT, none of which is useful until something actually
# wants one winwrap.dll shared between binaries.
{
  'variables': {
    # Held to the same /W4 as Sawbuck's own code, matching pcre.gyp next door.
    # Set this to 0 for WarningLevel 3 if winwrap's warnings turn out to be
    # noise that belongs in the winwrap repository rather than in this build.
    'chromium_code': 1,
  },
  # No 'includes' here on purpose. tools/gyp_sawbuck.py passes
  # build/common.gypi and sawbuck/sawbuck.gypi to every .gyp with --include,
  # so both are already in force. pcre.gyp's explicit include of common.gypi
  # is a leftover from when GYP was driven differently.
  'targets': [
    {
      'target_name': 'winwrap',
      'type': 'static_library',
      # Mirrors _winwrap_sources in upstream's BUILD.gn, which notes that
      # static_redirects.cc is the entire mechanism in this mode: it is what
      # defines the __imp_<name> slots the consumer's call sites load through.
      'sources': [
        'winwrap/src/app_user_model_id.cc',
        'winwrap/src/cancel_io.cc',
        'winwrap/src/condition_variable.cc',
        'winwrap/src/critical_section.cc',
        'winwrap/src/directories.cc',
        'winwrap/src/dispatch.cc',
        'winwrap/src/dispatch.h',
        'winwrap/src/fibers.cc',
        'winwrap/src/files.cc',
        'winwrap/src/fls.cc',
        'winwrap/src/framework.h',
        'winwrap/src/init_once.cc',
        'winwrap/src/io_completion.cc',
        'winwrap/src/locale.cc',
        'winwrap/src/lock_family.cc',
        'winwrap/src/lock_family.h',
        'winwrap/src/misc.cc',
        'winwrap/src/parking_lot.cc',
        'winwrap/src/parking_lot.h',
        'winwrap/src/pipes.cc',
        'winwrap/src/process.cc',
        'winwrap/src/registry.cc',
        'winwrap/src/srwlock.cc',
        'winwrap/src/static_redirects.cc',
        'winwrap/src/sys.cc',
        'winwrap/src/sys.h',
        'winwrap/src/threads.cc',
        'winwrap/src/tick_count.cc',
        'winwrap/src/winwrap.h',
        'winwrap/src/winwrap_version.h',
      ],
      'include_dirs': [
        'winwrap/src',
      ],
      'defines': [
        # WINWRAP_STATIC picks the static form.
        'WINWRAP_STATIC',
        # USING_WINWRAP comes from winwrap_public_config, and GN applies a
        # public_config to the target itself as well as to its dependents --
        # hence here and in direct_dependent_settings below.
        'USING_WINWRAP',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'winwrap/src',
        ],
        'defines': [
          'USING_WINWRAP',
        ],
      },
      'msvs_settings': {
        'VCCLCompilerTool': {
          'AdditionalOptions': [
            # These are in AdditionalOptions rather than 'defines', and in
            # this order, for a reason: MSBuild emits AdditionalOptions last
            # on the command line, and cl honours /D and /U in command-line
            # order, so these are what finally takes effect. common.gypi
            # contributes _WIN32_WINNT=0x0A00 and WINVER=0x0A00
            # (common.gypi:5573) and NTDDI_VERSION=0x0A000000
            # (common.gypi:2914) through PreprocessorDefinitions, and all
            # three have to come down for winwrap's own sources.
            #
            # This is a guardrail, not a target setting. winwrap has to run on
            # the oldest OS it supports, so it must not see a declaration the
            # SDK would only offer on something newer -- that is exactly the
            # mistake it exists to repair. Note this applies to winwrap's
            # sources ONLY; Sawbuck keeps building against the current SDK.
            '/U_WIN32_WINNT',
            '/D_WIN32_WINNT=0x0501',  # XP
            # NTDDI_VERSION too, or the guardrail only half works: plenty of
            # SDK headers gate on it rather than on _WIN32_WINNT. Upstream's
            # winwrap.gni records OVERLAPPED_ENTRY as the case that caught
            # this -- visible at _WIN32_WINNT=0x0501 until NTDDI came down.
            '/UNTDDI_VERSION',
            '/DNTDDI_VERSION=0x05010300',  # NTDDI_WINXPSP3
            # WINVER as well. winwrap.gni has no need to undefine this one
            # because Chromium's GN build does not set it; common.gypi does.
            '/UWINVER',
            '/DWINVER=0x0501',
            # winwrap is a C++17 project. Appended rather than substituted:
            # the last /std: on the command line is the one that counts, and
            # sawbuck.gypi pins the rest of the tree to /std:c++14. Keeping
            # winwrap on its own standard is deliberate upstream -- it is
            # meant to be consumable elsewhere, so it should not come to
            # depend on whatever standard this tree happens to use.
            '/std:c++17',
          ],
        },
      },
    },
  ],
}
