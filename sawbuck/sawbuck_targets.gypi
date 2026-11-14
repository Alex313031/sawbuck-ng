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
        # Target Windows XP.
        #
        # build/common.gypi targets Windows 8 (_WIN32_WINNT=0x0602,
        # WINVER=0x0602, NTDDI_VERSION=0x06020000). Those have to be removed
        # before ours are added: two /D flags for the same macro with different
        # values is a redefinition, and this build compiles with
        # warnings-as-errors. 'defines!' is GYP's exclusion list -- every entry
        # is removed from the target's merged 'defines'.
        'defines!': [
          '_WIN32_WINNT=0x0602',
          'WINVER=0x0602',
          'NTDDI_VERSION=0x06020000',
        ],
        'defines': [
          '_WIN32_WINNT=0x0501',
          'WINVER=0x0501',
          # Kept consistent with _WIN32_WINNT on purpose: the SDK's
          # sdkddkver.h derives and cross-checks these, so a Win8
          # NTDDI_VERSION against an XP _WIN32_WINNT is not a coherent
          # configuration. 0x05010300 is XP SP3, the last XP service pack;
          # drop to 0x05010000 for XP RTM.
          'NTDDI_VERSION=0x05010300',
        ],
      }],
    ],
  },
}
