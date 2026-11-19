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
# Settings that apply to Sawbuck's *own* targets only.
#
# Unlike sawbuck.gypi -- which GYP passes to every .gyp in the tree via
# --include, and so also reaches Chromium's base, testing/ and third_party/ --
# this file is included explicitly, by each Sawbuck .gyp that compiles C++:
#
#   sawbuck/common/common.gyp
#   sawbuck/log_lib/log_lib.gyp
#   sawbuck/sym_util/sym_util.gyp
#   sawbuck/viewer/viewer.gyp
#
# Add an 'includes' entry here if a new Sawbuck .gyp starts compiling code.

{
  'target_defaults': {
    'conditions': [
      ['OS=="win"', {
        # Compile against the Windows Vista API surface, and stay there.
        #
        # This is settled rather than provisional. It used to say the floor was
        # headed down to XP (0x0501) once XP support landed; XP support landed
        # in 0.7.5 and the floor turned out to be the wrong lever entirely, so
        # 0x0600 is the deliberate answer and not a way station.
        #
        # The macro decides one thing only: which declarations the SDK and WTL
        # headers make visible at compile time. It has no bearing on which
        # Windows the resulting binary runs on. That is settled by two other
        # mechanisms, neither of which cares what this says:
        #
        #   * /SUBSYSTEM:WINDOWS,5.01, which is the subsystem version in the
        #     PE header and the thing XP's loader actually checks.
        #   * third_party/winwrap, linked ahead of kernel32.lib, which defines
        #     the __imp_ slots for the post-XP imports the v143 CRT and the
        #     SDK headers reference, asks the running OS for the real function
        #     first, and falls back only where there is none.
        #
        # Where Sawbuck's own code needs an API newer than its oldest target,
        # the answer is to resolve it dynamically at the call site -- GetProcAddress
        # against the module that owns it, with a working path for when it is
        # absent -- or to let winwrap handle it. Both of those keep the
        # declaration visible, which is what makes them possible.
        #
        # So lowering this to 0x0501 would buy nothing for XP while hiding
        # from the compiler the APIs that the two mechanisms above exist to
        # make usable. It would also cost two things that currently work for
        # free:
        #
        #   * WTL's atltheme.h includes the long-removed <tmschema.h> below
        #     NTDDI_LONGHORN. WTL has an escape hatch -- define
        #     _WTL_USE_VSSYM32 to get <vssym32.h> instead -- so this one is
        #     survivable, but it is a workaround to carry for no gain.
        #   * viewer/filter_dialog.cc uses ATL's CShellFileOpenDialog and
        #     CShellFileSaveDialog with the FOS_* flags, which the SDK
        #     declares only from NTDDI_VISTA up. There is no escape hatch for
        #     that one short of writing the IFileDialog calls by hand.
        #
        # Raising it instead, back to common.gypi's 0x0A00, is the direction
        # with something to be said for it: it would remove this override
        # altogether. It is not done because nothing needs it, and because the
        # two items above are evidence that the headers between Vista and 10
        # hold more of these than anyone has looked for.
        #
        # build/common.gypi sets its own Windows version, which has to be
        # removed before ours is added: two /D flags for the same macro with
        # different values is a redefinition. 'defines!' is GYP's exclusion
        # list -- every entry is removed from the target's merged 'defines'.
        #
        # The values below must match what common.gypi actually sets, or the
        # exclusion silently does nothing and both end up on the command line.
        # Chromium 49 targets Windows 10 (0x0A00); the previous M37 pin
        # targeted Windows 8 (0x0602). Re-check this when moving pins.
        'defines!': [
          '_WIN32_WINNT=0x0A00',
          'WINVER=0x0A00',
          'NTDDI_VERSION=0x0A000000',
        ],
        'defines': [
          '_WIN32_WINNT=0x0600',
          'WINVER=0x0600',
          # All three move together: the SDK's sdkddkver.h derives and
          # cross-checks them, so a mismatched pair is not a coherent
          # configuration.
          'NTDDI_VERSION=0x06000000',

          # Ask ATL for the XP-safe spellings of its own functions. Without
          # this, ATL's internals reach for InitOnceExecuteOnce, GetThreadId
          # and SetFileInformationByHandle among others, none of which XP
          # exports -- and they are ATL's calls, inside ATL's headers, so
          # there is nothing of ours to change at the call site.
          #
          # build/common.gypi already defines _USING_V110_SDK71_ (line 2916),
          # which is the older spelling of the same request. This is the
          # current one, and it is here rather than in a patch against
          # common.gypi for two reasons.
          #
          # It needs no rebasing when build_revision moves, which four
          # patches against common.gypi already do. And the scope is exactly
          # right: only Sawbuck's own code includes ATL headers at all --
          # nothing under base/, testing/ or third_party/ does -- so this
          # reaches every translation unit that instantiates an ATL template
          # and none that do not. The consistency is the part that matters,
          # not the breadth: ATL is header-only, so a macro set in some
          # translation units and not others means two different bodies for
          # one mangled name, and the linker silently keeping whichever it
          # saw first.
          #
          # sawbuck/viewer/framework.h also defines it, under #ifndef, so
          # that a file including that header can see what it assumes. This
          # is the definition that actually applies.
          '_ATL_XP_TARGETING',
        ],
      }],
    ],
  },
}
