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
          # These are spelled with their real on-disk case. sawbuck.wxs asks
          # for DbgHelp.dll and SymSrv.dll, which Windows resolves anyway, but
          # a build input that only matches case-insensitively is a trap on
          # any host that does not.
          'inputs': [
            '<(INTERMEDIATE_DIR)/sawbuck.wixobj',
            '<(PRODUCT_DIR)/Sawbuck.exe',
            # Embedded into the installer UI via the WixUILicenseRtf variable.
            'license.rtf',
            # The third-party payload, in sawbuck.wxs File element order.
            '<(DEPTH)/third_party/icu/source/data/in/icudtl.dat',
            '<(DEPTH)/third_party/debugging_tools/files/dbghelp.dll',
            '<(DEPTH)/third_party/debugging_tools/files/symsrv.dll',
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
