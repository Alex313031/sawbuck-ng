#!/usr/bin/env python3

# Copyright 2026 Alex313031

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Runs GYP over the Sawbuck tree.

DEPS runs this as its gyp hook, so `gclient sync` generates the build. It
replaces the hook in the original DEPS.sawbuck, which invoked the bundled
Python 2.6 against tools/gyp (GYP r1919):

  src\\third_party\\python_26\\python.exe src\\tools\\gyp\\gyp_main.py
      --include=src/build/common.gypi --include=src/sawbuck/sawbuck.gypi
      --no-circular-check src/sawbuck/sawbuck.gyp

Neither of those exists any more: Python 2 is gone and GYP r1919 is Python 2
only. This drives gyp-next (fetched to tools/gyp-next by `gclient sync`, or by
tools/sync_deps.py) under Python 3 instead, passing the same includes and the
same build file, so the generated build is the one DEPS.sawbuck described.

Two details the old hook got for free and this has to arrange:

  * build/ must be on sys.path. Chromium's own gyp_chromium.py does the same;
    common.gypi calls into it via GYP's <!(pymod_do_main ...), e.g.
    dir_exists.py.
  * The tree is its own source root. DEPS.sawbuck checked everything out under
    'src/', so GYP saw depth 'src'. Here base/, build/ and sawbuck/ sit at the
    repository root, so depth is '.' and sawbuck.gypi's '<(src)' resolves
    to it.

Paths are always resolved against the repository root, not the working
directory, so this can be run from anywhere.

Usage:
  ./tools/gyp_sawbuck.py                 # generate (msvs on Windows, ninja elsewhere)
  ./tools/gyp_sawbuck.py --check         # validate the graph only; writes to a temp dir
  ./tools/gyp_sawbuck.py --format=ninja  # pick a generator explicitly
  ./tools/gyp_sawbuck.py -- <gyp args>   # pass anything else straight to GYP
"""

import argparse
import os
import sys
import tempfile

# This script lives in tools/; everything it refers to is relative to the
# repository root above it.
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GYP_NEXT = os.path.join(ROOT, 'tools', 'gyp-next', 'pylib')
BUILD_DIR = os.path.join(ROOT, 'build')

# The build file and includes come verbatim from the DEPS.sawbuck hook.
BUILD_FILE = 'sawbuck/sawbuck.gyp'
INCLUDES = ['build/common.gypi', 'sawbuck/sawbuck.gypi']


def parse_args():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--format',
                        help='GYP generator (default: msvs on Windows, '
                             'ninja elsewhere)')
    parser.add_argument('--check', action='store_true',
                        help='only check that the graph loads and evaluates; '
                             'generate into a temporary directory and discard '
                             'it. Works off-Windows, where msvs cannot run.')
    parser.add_argument('--msvs-version', default='2022',
                        help='Visual Studio version (default: 2022)')
    parser.add_argument('gyp_args', nargs='*', metavar='-- GYP_ARG',
                        help='additional arguments passed through to GYP')
    return parser.parse_args()


def main():
    args = parse_args()
    on_windows = sys.platform == 'win32'

    if not os.path.isdir(GYP_NEXT):
        print('error: %s is missing. Run `gclient sync` (or '
              './tools/sync_deps.py) first.' % GYP_NEXT, file=sys.stderr)
        return 2

    fmt = args.format or ('msvs' if on_windows and not args.check else 'ninja')

    # GYP resolves every path relative to the cwd, and depth '.' below assumes
    # that cwd is the source root.
    os.chdir(ROOT)
    sys.path.insert(0, GYP_NEXT)
    sys.path.insert(1, BUILD_DIR)
    import gyp

    argv = [
        '--format=%s' % fmt,
        '--depth=.',
        # On a non-Windows host GYP would otherwise infer OS=linux, and
        # common.gypi would configure a build Sawbuck has no sources for.
        '-D', 'OS=win',
        # Carried over from the DEPS.sawbuck hook.
        '--no-circular-check',
    ]
    argv += ['--include=%s' % i for i in INCLUDES]

    if fmt == 'msvs':
        argv += ['-G', 'msvs_version=%s' % args.msvs_version]
    else:
        # The msvs generator computes these itself, but every other generator
        # leaves them undefined and common.gypi branches on both.
        argv += ['-D', 'MSVS_VERSION=%s' % args.msvs_version,
                 '-D', 'MSVS_OS_BITS=64']

    tmpdir = None
    if args.check:
        tmpdir = tempfile.mkdtemp(prefix='sawbuck-gyp-check-')
        argv += ['-G', 'output_dir=%s' % tmpdir]

    argv += args.gyp_args
    argv.append(BUILD_FILE)

    try:
        result = gyp.main(argv)
    finally:
        if tmpdir:
            import shutil
            shutil.rmtree(tmpdir, ignore_errors=True)

    if result == 0:
        if args.check:
            print('OK: the Sawbuck GYP graph loads and evaluates (%s).' % fmt)
        else:
            print('Generated %s build files for %s.' % (fmt, BUILD_FILE))
    return result


if __name__ == '__main__':
    sys.exit(main())
