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
            # MSBuild's own $(Platform), so the package architecture
            # follows the configuration being built: the x64 configuration
            # produces a 64-bit MSI around the x64 binary, and Win32 a 32-bit
            # one. sawbuck.wxs maps Win32/x64 onto WiX's spelling and derives
            # Win64, ProgramFiles64Folder and the component GUIDs from it.
            #
            # Expanded by MSBuild at build time rather than by GYP, for the
            # same reason copy_dlls needs a per-configuration post-build step:
            # one generated project holds all four configurations, so nothing
            # GYP substitutes here could differ between them.
            '-dMsiPlatform=$(Platform)',
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
          # Note that the finished MSI installs on Windows XP, so there is no
          # separate packaging for it and no .zip to produce. That is not an
          # accident of WiX 3 and should not be treated as one: sawbuck.wxs
          # declares InstallerVersion='200' -- Windows Installer 2.0, which XP
          # ships with -- and its launch condition names 5.01 on x86 and 5.02
          # on x64, matching what viewer.gyp links the subsystem version to.
          # Raising InstallerVersion for a newer WiX feature would drop XP
          # support silently, with the binary still running there perfectly
          # well and only the installer refusing.
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
            # Capitalised to match Sawbuck.exe, which viewer.gyp sets through
            # 'product_name'. Note that NTFS is case-insensitive: an out/
            # directory that already holds a lowercase sawbuck.msi from an
            # earlier build will be written through rather than renamed, so
            # delete it once if the casing looks unchanged.
            #
            # No architecture in the name, and not for want of trying: one
            # generated project holds all four configurations, so nothing GYP
            # substitutes here can differ between them -- the same constraint
            # that makes candle take -dMsiPlatform=$(Platform) above. MSBuild
            # has no macro spelling the architecture as x86 either ($(Platform)
            # is Win32), and the one GYP mechanism that would emit a custom
            # per-configuration property, 'msbuild_settings', makes GYP ignore
            # msvs_settings for every configuration inheriting it, which would
            # throw away common.gypi's whole compiler and linker setup.
            #
            # So build.bat renames the result to Sawbuck_x86.msi or
            # Sawbuck_x64.msi after each flavour. Driving msbuild directly
            # leaves the name as it is here. Moving it means this action runs
            # again on the next build, since its declared output is gone.
            '<(PRODUCT_DIR)/Sawbuck.msi',
          ],
          'action': [
            '<(light_exe)',
            '<(INTERMEDIATE_DIR)/sawbuck.wixobj',
            '-sice:ICE49',  # suppress the 'default value is not string' ICE
            # ICE61 is "this product should remove only older versions of
            # itself", which is precisely what sawbuck.wxs now asks for: its
            # Upgrade row includes the current version so that a rebuilt MSI
            # of the same version replaces the installed one instead of
            # installing beside it. Suppressed deliberately rather than worked
            # around, because the ICE is right about what the table says and
            # wrong about whether we want it -- Product Id='*' means every
            # build is a different ProductCode, so same-version replacement is
            # the only way a rebuild is recognised at all.
            '-sice:ICE61',
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
