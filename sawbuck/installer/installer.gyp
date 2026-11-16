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
  'variables': {
    'chromium_code': 1,
    'candle_exe': '<(DEPTH)\\third_party\\wix\\files\\candle.exe',
    'light_exe': '<(DEPTH)\\third_party\\wix\\files\\light.exe',
  },
  'targets': [
    {
      'target_name': 'sawbuck_msi',
      'project_name': 'Sawbuck.msi',
      'type': 'none',
      'sources': [
        'license.rtf',
        'sawbuck.wxs',
        'version.wxi.template',
      ],
      'dependencies': [
        '../viewer/viewer.gyp:sawbuck_exe',
      ],
      'msvs_cygwin_shell': 0,
      'actions': [
        {
          'action_name': 'make_version_wxi',
          'inputs': [
            '../tools/template_replace.py',
            '../VERSION',
            'version.wxi.template',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/version.wxi',
          ],
          'action': [
            'python',
            '../tools/template_replace.py',
            '--input', 'version.wxi.template',
            '--output', '<(INTERMEDIATE_DIR)/version.wxi',
            '../VERSION',
          ],
          'process_outputs_as_sources': 1,
        },
        {
          'action_name': 'candle',
          'inputs': [
            'sawbuck.wxs',
            '<(INTERMEDIATE_DIR)/version.wxi',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/sawbuck.wixobj',
          ],
          'action': [
            '<(candle_exe)',
            '-I<(INTERMEDIATE_DIR)\\.',
            'sawbuck.wxs',
            '-out',
            '<@(_outputs)',
            '-dSAWBUCK_EXE_PATH=<(PRODUCT_DIR)\\Sawbuck.exe',
            # Taken from the build output, not from third_party: these come in
            # per-architecture flavours and viewer.gyp's copy_dlls has already
            # placed the pair matching this configuration beside the
            # executable. Sourcing them from there is what makes the installer
            # follow the architecture being built.
            '-dDBGHELP_DLL_PATH=<(PRODUCT_DIR)\\dbghelp.dll',
            '-dSYMSRV_DLL_PATH=<(PRODUCT_DIR)\\symsrv.dll',
          ],
          'process_outputs_as_sources': 1,
        },
        {
          'action_name': 'light',
          'extension': 'wxs',
          # Everything light reads, not just the wixobj. candle only compiles
          # the authoring; light is what pulls file contents into the cab and
          # binds the UI, so a change to any of these has to rebuild the MSI.
          # Only Sawbuck.exe used to be listed, which meant editing the
          # licence text or rolling a dependency left a stale installer.
          #
          'inputs': [
            '<(INTERMEDIATE_DIR)/sawbuck.wixobj',
            '<(PRODUCT_DIR)/Sawbuck.exe',
            # Embedded into the installer UI via the WixUILicenseRtf variable.
            'license.rtf',
            # ICU's data comes straight out of third_party; it is
            # architecture-neutral, so there is one copy to point at.
            '<(DEPTH)/third_party/icu/source/data/in/icudtl.dat',
            # The debugging tools come from the build output instead, because
            # they are per-architecture and copy_dlls has already selected the
            # right pair. The ordering that guarantees they exist by now comes
            # from the dependency chain, not from this list: sawbuck_msi
            # depends on sawbuck_exe, which depends on copy_dlls.
            '<(PRODUCT_DIR)/dbghelp.dll',
            '<(PRODUCT_DIR)/symsrv.dll',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/sawbuck.msi',
          ],
          'action': [
            '<(light_exe)',
            '<(INTERMEDIATE_DIR)/sawbuck.wixobj',
            '-sice:ICE49',  # suppress the 'default value is not string' ICE
            '-ext', 'WixUIExtension',
            '-ext', 'WixUtilExtension',
            '-out', '<@(_outputs)',
          ],
          'process_outputs_as_sources': 1,
        },
      ],
    },
  ],
}
