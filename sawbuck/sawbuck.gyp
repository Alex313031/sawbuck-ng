# Copyright 2011 Google Inc.
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
  },
  'targets': [
    {
      'target_name': 'build_all',
      'type': 'none',
      'dependencies': [
        'common/common.gyp:*',
        'installer/installer.gyp:*',
        'log_lib/log_lib.gyp:*',
        # 'py/py.gyp:*' is deliberately absent. It builds the Python ETW
        # bindings as a py2.6 egg, via the vendored setuptools 0.6c11 and the
        # bundled Python 2.6 that DEPS no longer fetches. The package itself is
        # still Python 2 as well, evntrace.py included. Porting it is real work
        # and nothing else here depends on it, so it is out of the build until
        # then. Note that dropping it from here means GYP no longer loads
        # py/py.gyp at all, so the target is not generated into the solution
        # either; run GYP against py/py.gyp directly if you need it.
        'sym_util/sym_util.gyp:*',
        'viewer/viewer.gyp:*',
      ],
    },
    {
      # Add new unittests to this target as inputs.
      'target_name': 'run_unittests',
      'type': 'none',
      'variables': {
        # The file that marks success of all unittests.
        'success_file': '<(PRODUCT_DIR)/unittest_success.txt',

        # Add all unit test targets here.
        'unittest_targets': [
          '<(DEPTH)/sawbuck/common/common.gyp:common_unittests',
          '<(DEPTH)/sawbuck/log_lib/log_lib.gyp:log_lib_unittests',
          '<(DEPTH)/sawbuck/sym_util/sym_util.gyp:sym_util_unittests',
          '<(DEPTH)/sawbuck/viewer/viewer.gyp:viewer_unittests',
        ],
      },
      'dependencies': [
        '<@(unittest_targets)',
      ],
      'actions': [
        {
          'action_name': 'run_unittests',
          'msvs_cygwin_shell': 0,
          'inputs': [
            'tools/run_unittests.py',
            'tools/verifier.py',
            '<(PRODUCT_DIR)/common_unittests.exe',
            '<(PRODUCT_DIR)/log_lib_unittests.exe',
            '<(PRODUCT_DIR)/sym_util_unittests.exe',
            '<(PRODUCT_DIR)/viewer_unittests.exe',
          ],
          'outputs': [
            # Created only if all unittests succeed
            '<(success_file)',
          ],
          'action': [
            # Was third_party/python_26/python, a bundled Python 2.6 that DEPS
            # no longer fetches and that has no googlesource mirror. The
            # scripts this runs are Python 3 now, so use whatever python is on
            # PATH -- which is what viewer.gyp's sawbuck_version does too.
            'python',
            'tools/run_unittests.py',
            '--exe-dir=<(PRODUCT_DIR)\\.',
            '--success-file=<(success_file)',
            # SymSrv.dll abandons a critical section on
            # unlock on 32 bit systems
            #
            # No inner quotes. run_unittests.py does a plain
            # exception.split(',') on this value, so it wants the bare
            # string, and GYP already wraps each argument in quotes of its
            # own when it writes the command. The quotes that used to be
            # here produced "--exception="dbghelp!SymCleanup,Locks,513"" in
            # the generated project, which survives only because MSVCRT's
            # argv parser happens to cancel the four quotes out -- and cmd
            # re-parses the line once per 'call', of which the MSBuild
            # generator emits two.
            '--exception=dbghelp!SymCleanup,Locks,513',
            '--exception=dbghelp!SymCleanup,Leak,2304',
            '<@(unittest_targets)',
          ],
        },
      ],
    }
  ]
}
