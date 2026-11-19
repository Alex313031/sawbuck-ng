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
    'etw_sources': [
      'etw/__init__.py',
      'etw/consumer.py',
      'etw/controller.py',
      'etw/evntcons.py',
      'etw/evntrace.py',
      'etw/guiddef.py',
      'etw/provider.py',
      'etw/util.py',
      'etw/descriptors/__init__.py',
      'etw/descriptors/binary_buffer.py',
      'etw/descriptors/event.py',
      'etw/descriptors/field.py',
      'etw/descriptors/fileio.py',
      'etw/descriptors/image.py',
      'etw/descriptors/pagefault.py',
      'etw/descriptors/pagefault_xp.py',
      'etw/descriptors/process.py',
      'etw/descriptors/registry.py',
      'etw/descriptors/thread.py',
    ],
  },
  'targets': [
    {
      'target_name': 'etw',
      'type': 'none',
      'sources': [
        '<@(etw_sources)',
      ],
      # This listed the sources and nothing more. It used to carry a
      # 'build_etw' action producing ETW-0.6.5.0-py2.6.egg, run as
      #
      #   third_party/setuptools/setup_env.bat &&
      #   third_party/python_26/python setup.py bdist_egg
      #     --dist-dir=<(PRODUCT_DIR) --bdist-dir=<(PRODUCT_DIR)/temp/etw/
      #
      # where setup_env.bat only put the vendored setuptools-0.6c11-py2.6.egg
      # on PYTHONPATH. Both of those are gone: DEPS stopped fetching
      # third_party/python_26 once GYP moved to Python 3, and
      # third_party/setuptools was deleted since it existed solely to serve
      # this one action.
      #
      # The egg could not have been built anyway -- the package is Python 2
      # (setup.py's own 'print sys.version' is a statement, and ez_setup.py
      # bootstraps setuptools 0.6c11 from a URL that has been dead for years),
      # so a working build needs the Python 3 port first. Whatever runs the
      # build then should be the interpreter on PATH with a modern setuptools,
      # and the artefact a wheel rather than an egg, which also means the
      # output filename stops being something GYP can predict.
      #
      # Until that port, the sources are listed so the files appear in the
      # generated project, and no build step claims to produce anything. See
      # the note beside py/py.gyp in sawbuck/sawbuck.gyp.
    },
  ],
}
