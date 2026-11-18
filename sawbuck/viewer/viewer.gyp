# Copyright 2009 Google Inc.
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

{
  # Settings for Sawbuck's own targets, e.g. the Windows version we
  # target. Deliberately not in sawbuck.gypi, which GYP applies to the
  # whole tree including Chromium's base.
  'includes': ['../sawbuck_targets.gypi'],
  'variables': {
    'chromium_code': 1,

    # Which tier of Debugging Tools copy_dlls puts beside the binaries. The
    # tiers differ by the oldest Windows they run on, which is the only axis
    # that matters here:
    #
    #   xp    6.12.2.633 from WDK 7.1. Links the OS-provided msvcrt.dll, so it
    #         needs no redistributable and runs from XP up.
    #   win7  10.0.26100 from the Windows SDK. Links the Universal CRT.
    #         Resolves essentially all of tip-of-tree Chromium's symbols; only
    #         a handful of functions need the newer 10.0.28000 build, which is
    #         not vendored because it does not load on Windows 7.
    #
    # Override with -D debugging_tools_tier=win7 to try the other one without
    # editing this file. Declared at file scope rather than on copy_dlls
    # because GYP cannot expand a '%' default from the same variables block
    # that declares it.
    'debugging_tools_tier%': 'xp',
  },
  'target_defaults': {
    'include_dirs': [
      '<(DEPTH)',
      '<(DEPTH)/third_party/wtl/include',
    ],
    'defines': [
      '_WTL_NO_CSTRING',
    ],
  },
  'targets': [
    {
      'target_name': 'sawbuck_version',
      'type': 'none',
      'msvs_cygwin_shell': 0,
      'sources': [
        'version.gen.template',
      ],
      'actions': [
        {
          'action_name': 'make_version_gen',
          'inputs': [
            '../tools/template_replace.py',
            '../VERSION',
            'version.gen.template',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/version.gen',
          ],
          'action': [
            'python',
            '../tools/template_replace.py',
            '--input', 'version.gen.template',
            '--output', '<(SHARED_INTERMEDIATE_DIR)/version.gen',
            '../VERSION',
          ],
          'process_outputs_as_sources': 1,
        },
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
    },
    {
      'target_name': 'viewer_lib',
      'type': 'static_library',
      'sources': [
        'const_config.h',
        'filter.cc',
        'filter.h',
        'filter_dialog.cc',
        'filter_dialog.h',
        'filtered_log_view.cc',
        'filtered_log_view.h',
        'find_dialog.cc',
        'find_dialog.h',
        'log_viewer.h',
        'log_viewer.cc',
        'log_list_view.h',
        'log_list_view.cc',
        'preferences.cc',
        'preferences.h',
        'provider_configuration.cc',
        'provider_configuration.h',
        'provider_dialog.cc',
        'provider_dialog.h',
        'sawbuck_guids.h',
        'stack_trace_list_view.h',
        'stack_trace_list_view.cc',
        'viewer_window.cc',
        'viewer_window.h',
      ],
      'dependencies': [
        '../log_lib/log_lib.gyp:log_lib',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/pcre/pcre.gyp:pcre_lib',
      ],
    },
    {
      # Our tests and sawbuck.exe need the dbghelp and symsrv DLLs beside
      # them, and those have to match the architecture being built. The names
      # are fixed, so an x64 binary cannot be handed the x86 dbghelp.dll.
      #
      # 'copies' cannot express that. It is a target-level key, and the msvs
      # generator emits all four configurations into a single project, so
      # there is nothing for a condition to branch on. 'msvs_postbuild' *is*
      # per configuration -- generator/msvs.py reads it out of each
      # configuration dict -- so the copy lives there instead. Each
      # xp/<arch> directory holds exactly dbghelp.dll and symsrv.dll, so the
      # wildcard copies precisely those two and stays correct if the set
      # changes.
      #
      # Ninja ignores msvs_postbuild, so a non-msvs generator does not copy
      # these at all. That only matters off Windows, where Sawbuck does not
      # build anyway.
      'target_name': 'copy_dlls',
      'type': 'none',
      'variables': {
        # debugging_tools_tier is declared at file scope above; see the
        # comment there for what the tiers mean.
        'debugging_tools_dir':
            '<(DEPTH)\\third_party\\debugging_tools\\files\\'
            '<(debugging_tools_tier)',
      },
      'configurations': {
        'Debug': {
          'msvs_postbuild':
              'copy /Y "<(debugging_tools_dir)\\x86\\*.dll" "$(OutDir)"',
        },
        'Release': {
          'msvs_postbuild':
              'copy /Y "<(debugging_tools_dir)\\x86\\*.dll" "$(OutDir)"',
        },
        'Debug_x64': {
          'msvs_postbuild':
              'copy /Y "<(debugging_tools_dir)\\x64\\*.dll" "$(OutDir)"',
        },
        'Release_x64': {
          'msvs_postbuild':
              'copy /Y "<(debugging_tools_dir)\\x64\\*.dll" "$(OutDir)"',
        },
      },
    },
    {
      'target_name': 'sawbuck_exe',
      'product_name': 'Sawbuck',
      'type': 'executable',
      'sources': [
        'resource.h',
        'viewer_module.cc',
        'viewer_module.h',
        'viewer.rc',
        'version.rc',
      ],
      'dependencies': [
        'copy_dlls',
        'sawbuck_version',
        'viewer_lib',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/third_party/icu/icu.gyp:icudata',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': 2,
          # 2 is requireAdministrator UAC level. sawbuck.exe.manifest, merged
          # in by VCManifestTool below, declares the same level, and the two
          # snippets have to keep agreeing: mt.exe fails the build with
          # "manifest authoring error c1010001" if they ever differ. Removing
          # this line is not the way to drop the duplication -- EnableUAC
          # defaults to true, so the level would fall back to asInvoker and
          # disagree. 'EnableUAC': 'false' is what suppresses it.
          'UACExecutionLevel': 2,
          # NOTE: 'Profile': 'true' (/PROFILE, for the PDB's FIXUP stream)
          # used to be set here and is deliberately gone. common.gypi sets it
          # on Release_Base already, next to 'LinkIncremental': '1', so the
          # release PDB still carries FIXUPs. Setting it at target level
          # applied it to Debug as well, where it is useless and not free:
          # /PROFILE implies /INCREMENTAL:NO, so every debug link was a full
          # one, and the linker said so with LNK4075.
        },
        'VCManifestTool': {
          'AdditionalManifestFiles': ['$(ProjectDir)\\sawbuck.exe.manifest'],
        },
      },
      'conditions': [
        # Windows XP support, off unless asked for -- see sawbuck_use_winwrap
        # in sawbuck/sawbuck.gypi for what it costs to leave on.
        ['sawbuck_use_winwrap!=0', {
          'dependencies': [
            '<(DEPTH)/third_party/winwrap/winwrap.gyp:winwrap',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              # The '+' is the whole point: it is GYP's prepend policy for a
              # list being merged into an existing one, so winwrap.lib lands
              # at the FRONT of AdditionalDependencies, ahead of the
              # kernel32.lib that common.gypi puts there. Plain append --
              # which is what a key without the '+' does -- is what made the
              # first two attempts fail.
              #
              # winwrap captures a call by defining the __imp_<name> slot the
              # call site loads through, and libraries are searched in
              # command-line order. winwrap's own notes say an explicitly
              # linked winwrap wins without reordering, because kernel32.lib
              # arrives only through the /DEFAULTLIB directives the SDK embeds
              # and those are processed after command-line libraries. That
              # holds for Chromium's GN build and not for this one:
              # common.gypi names kernel32.lib and a dozen others explicitly,
              # so they are command-line libraries too and position decides.
              #
              # Two attempts failed before this, both diagnosed from the
              # import table rather than guessed at:
              #
              #   /WHOLEARCHIVE:winwrap.lib changed nothing whatsoever. The
              #   option was on the link line, winwrap.lib was passed, the
              #   archive held all 24 members including static_redirects.obj,
              #   and dumpbin confirmed the symbols were present and correctly
              #   decorated. It force-includes members at the point the
              #   library appears in input order, which is after kernel32.lib
              #   had already resolved the slots.
              #
              #   /INCLUDE: of a slot then made GetTickCount64 appear as a new
              #   KERNEL32 import where there had been none. That is the
              #   finding that settled it: the reference was created and
              #   kernel32.lib, being earlier, is what satisfied it.
              #
              # Naming the library here means it appears twice on the command
              # line, since MSBuild adds it again for the project reference.
              # Harmless -- the second occurrence has nothing left to offer --
              # and it is the first that decides.
              'AdditionalDependencies+': [
                '$(OutDir)lib\\winwrap.lib',
              ],
            },
          },
          # /INCLUDE: is kept as well, per architecture below, because the
          # ordering fix alone relies on something already referencing each
          # slot. It guarantees static_redirects.obj is pulled even for a slot
          # nothing happens to call, and one is enough: every
          # WINWRAP_REDIRECT lives in that single object, whose initialisers
          # then drag the ww_* implementations in behind them.
          # The subsystem version, which is the other half of the recipe:
          # winwrap supplies the imports XP does not export, and this is what
          # lets the loader accept the binary in the first place.
          #
          # Split per architecture because the floor is not the same. 5.01 is
          # Windows XP on x86, but 64-bit Windows begins at 5.02 and the
          # linker will not accept anything lower for that machine.
          #
          # It has to go through these abstract configurations rather than a
          # GYP condition on target_arch, for exactly the reason sawbuck.gypi
          # gives for setting OutputDirectory the same way: one generated
          # project holds all four configurations, so nothing GYP substitutes
          # at generation time can differ between them. Every concrete
          # configuration inherits ['Common_Base', '<arch>_Base',
          # '<Debug|Release>_Base'], so this reaches both of that arch.
          #
          # 'abstract' is restated only to be explicit. These are already
          # declared abstract in sawbuck.gypi's target_defaults, and that
          # survives the merge into this target.
          'configurations': {
            'x86_Base': {
              'abstract': 1,
              'msvs_settings': {
                'VCLinkerTool': {
                  # /SUBSYSTEM:WINDOWS,5.01 -- SubSystem 2 is set above.
                  'MinimumRequiredVersion': '5.01',
                  'AdditionalOptions': [
                    # Pulls static_redirects.obj in. winwrap_static_redirects
                    # is the anchor winwrap provides for exactly this, so the
                    # dependency is on winwrap rather than on whichever shim
                    # we happened to name -- it used to be
                    # __imp__GetTickCount64@0, which worked but would have
                    # broken with LNK2001 if that shim were ever dropped.
                    # Ordinary decoration, so x86 takes a leading underscore.
                    '/INCLUDE:_winwrap_static_redirects',
                  ],
                },
              },
            },
            'x64_Base': {
              'abstract': 1,
              'msvs_settings': {
                'VCLinkerTool': {
                  # /SUBSYSTEM:WINDOWS,5.02. XP x64 and Server 2003, which is
                  # as low as the linker goes for this machine.
                  'MinimumRequiredVersion': '5.02',
                  'AdditionalOptions': [
                    # Same anchor, undecorated: x64 has no leading underscore.
                    '/INCLUDE:winwrap_static_redirects',
                  ],
                },
              },
            },
          },
        }],
      ],
    },
    {
      'target_name': 'viewer_unittests',
      'type': 'executable',
      'sources': [
        'filter_unittest.cc',
        'filtered_log_view_unittest.cc',
        'preferences_unittest.cc',
        'provider_configuration_unittest.cc',
        'registry_test.h',
        'registry_test.cc',
        'sawbuck_guids.h',
        'viewer_unittest_main.cc',
        'viewer_window_unittest.cc',
        'viewer.rc',
      ],
      'dependencies': [
        'copy_dlls',
        # viewer.rc is in this target's sources too, and it includes
        # sawbuck_version.h -> version.gen, which this target generates into
        # SHARED_INTERMEDIATE_DIR and puts on the include path of anything
        # depending on it. Without this the resource compile fails here with
        # "Cannot open include file: 'version.gen'", even though sawbuck_exe
        # builds fine.
        'sawbuck_version',
        'viewer_lib',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
    },
  ]
}
