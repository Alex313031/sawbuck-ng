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

The Visual Studio version is detected, not hardcoded: --msvs-version wins, then
$GYP_MSVS_VERSION, then whatever is installed. So the DEPS hook works unattended
on any machine with a supported VS.

Detection runs with GYP's fallback disabled, and the result is pinned
explicitly. Left to itself, GYP quietly falls back to *Visual Studio 2005* when
it finds no installation, emitting .vcproj files in the 2005 format -- a
failure whose only symptom is the VS migrator later offering to upgrade a
17-year-old project. Here, failing to detect a VS is an error.

Build files are loaded serially. GYP's parallel loader sizes a multiprocessing
pool from the CPU count, and on Windows that breaks above ~62 cores, because
WaitForMultipleObjects accepts at most 64 handles. Serial loading costs ~50ms
here and gives better diagnostics. --parallel opts back in.

Usage:
  ./tools/gyp_sawbuck.py                    # generate (msvs on Windows, ninja elsewhere)
  ./tools/gyp_sawbuck.py --check            # validate the graph only; writes to a temp dir
  ./tools/gyp_sawbuck.py --format=ninja     # pick a generator explicitly
  ./tools/gyp_sawbuck.py --msvs-version=2019  # override VS detection
  ./tools/gyp_sawbuck.py -- <gyp args>      # pass anything else straight to GYP
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

# Used only by non-msvs generators, which have no toolchain to probe. Any value
# common.gypi recognises will do; it only feeds conditions like
# MSVS_VERSION=="2013e".
FALLBACK_MSVS_VERSION = '2022'


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
    parser.add_argument('--msvs-version', default=None,
                        help='Visual Studio version, e.g. 2019. Defaults to '
                             '$GYP_MSVS_VERSION, or to whatever GYP '
                             'autodetects on the machine.')
    parser.add_argument('--parallel', action='store_true',
                        help='load build files in parallel. Off by default; '
                             'see the note on Windows and core counts below.')
    parser.add_argument('--no-winwrap', action='store_true',
                        help='generate without winwrap, giving up Windows XP '
                             'support. The default links it, so one binary '
                             'runs on XP through 11.')
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
        # We build with MSVC only. common.gypi sets host_clang to 1
        # unconditionally ("Always do this by default"), and then shells out to
        # tools/clang/scripts/update.py for a CR_CLANG_REVISION define -- a
        # directory DEPS does not fetch. Stating that we do not use clang is
        # better than adding the dependency or patching the call out.
        #
        # These are command-line defines rather than '%' defaults in
        # sawbuck.gypi deliberately: command-line values take precedence
        # everywhere, whereas a '%' default loses to common.gypi's own nested
        # declaration in any .gyp that brings its own 'variables' block.
        '-D', 'clang=0',
        '-D', 'host_clang=0',
        # Carried over from the DEPS.sawbuck hook.
        '--no-circular-check',
    ]

    # Windows XP support, decided here because this is the one place every
    # route into a generated solution passes through.
    #
    # It used to be build.bat exporting GYP_DEFINES=sawbuck_use_winwrap=1, and
    # that did not work. Generation happens inside a gclient hook, so the
    # setting had to survive build.bat -> gclient.bat -> vpython3 -> gclient.py
    # -> the hook subprocess; gclient copies os.environ for hooks and every
    # layer uses setlocal rather than scrubbing, so it should have arrived, and
    # empirically it did not. Running the generator by hand worked and
    # build.bat did not, which is as far as that was worth chasing: an
    # environment variable crossing five processes is a bad channel for a
    # setting whose failure mode is silent.
    #
    # Silent is the operative word. A solution generated without it builds,
    # runs and passes its tests, and the only symptom is Sawbuck.exe importing
    # FlsAlloc, IsThreadAFiber, InitializeCriticalSectionEx and the SRWLOCK
    # family straight from kernel32 -- so it fails at load on XP, and nowhere
    # anyone was looking.
    #
    # GYP_DEFINES is still honoured for anyone who sets it deliberately, and
    # an explicit -D still wins, but neither is how the default arrives.
    gyp_defines = os.environ.get('GYP_DEFINES', '')
    asked_for_winwrap = ('sawbuck_use_winwrap' in gyp_defines or
                         any('sawbuck_use_winwrap' in a for a in args.gyp_args))

    if args.no_winwrap:
        argv += ['-D', 'sawbuck_use_winwrap=0']
        winwrap_note = 'off (--no-winwrap)'
    elif asked_for_winwrap:
        winwrap_note = 'as requested on %s' % (
            'GYP_DEFINES' if 'sawbuck_use_winwrap' in gyp_defines
            else 'the command line')
    else:
        argv += ['-D', 'sawbuck_use_winwrap=1']
        winwrap_note = "on (this generator's default)"

    # Said out loud, because the alternative is reading it out of a finished
    # binary's import table. Whether XP support was in a build has been
    # ambiguous twice now, and one line in the generation log settles it --
    # naming where the answer came from, so a surprising one can be traced.
    print('Windows XP support: %s' % winwrap_note)

    if not args.parallel:
        # GYP's parallel loader does multiprocessing.Pool(cpu_count()), and on
        # Windows the pool's manager thread waits on one sentinel per worker.
        # WaitForMultipleObjects accepts at most 64 handles, so on a machine
        # with more than ~62 cores that raises
        #   ValueError: need at most 63 handles, got a sequence of length N
        # and generation dies. Serial loading costs ~50ms on this graph and
        # reports which target pulled in a missing file, which the parallel
        # loader does not.
        argv.append('--no-parallel')
    argv += ['--include=%s' % i for i in INCLUDES]

    msvs_version = args.msvs_version or os.environ.get('GYP_MSVS_VERSION')

    if fmt == 'msvs':
        # Resolve the Visual Studio version up front, with GYP's fallback
        # DISABLED, then pin the result.
        #
        # This matters a lot. GYP's default 'auto' handling falls back to
        # *Visual Studio 2005* when it cannot find an installation -- silently.
        # That emits .vcproj files in the 2005 format, and the only sign of
        # trouble is the VS migrator later offering to convert a 17-year-old
        # project. Failing loudly here is far better than generating a build
        # for the wrong toolchain.
        #
        # Passing the resolved name explicitly also stops GYP re-running 'auto'
        # and reaching that fallback on its own.
        if msvs_version:
            # Asked for explicitly, so take it at face value and skip GYP's
            # installation check. That check is unreliable past VS2015: its
            # primary probe reads HKLM\...\VisualStudio\<ver>\InstallDir, a key
            # VS2017 and newer no longer create, and its secondary probe only
            # special-cases 15.0 -- 16.0 and 17.0 fall through to a branch that
            # labels them Express editions. A VS2022 install is routinely not
            # found. GYP copes: given an explicit version it emits projects for
            # it and lets MSBuild locate the toolchain, which is what we want.
            resolved = msvs_version
            source = 'requested'
        else:
            # Nothing specified, so detect -- with GYP's fallback DISABLED.
            # Left to itself, GYP answers 'auto' with *Visual Studio 2005* when
            # it finds nothing, silently emitting .vcproj files in the 2005
            # format. The only symptom is the VS migrator later offering to
            # upgrade a 17-year-old project, so this must not be allowed to
            # pass quietly.
            from gyp.MSVSVersion import SelectVisualStudioVersion
            try:
                resolved = SelectVisualStudioVersion(
                    'auto', allow_fallback=False).ShortName()
            except Exception as e:
                print('error: %s' % e, file=sys.stderr)
                print('GYP cannot reliably detect VS2017 or newer. Name it '
                      'explicitly:\n'
                      '    set GYP_MSVS_VERSION=2022\n'
                      'or pass --msvs-version=2022. To check the build graph '
                      'without a toolchain, use --check.', file=sys.stderr)
                return 2
            source = 'detected'

        print('Using Visual Studio %s (%s).' % (resolved, source))
        argv += ['-G', 'msvs_version=%s' % resolved]
    else:
        # The msvs generator computes these itself, but every other generator
        # leaves them undefined and common.gypi branches on both. Off-Windows
        # there is nothing to autodetect, so fall back to a plausible value.
        argv += ['-D', 'MSVS_VERSION=%s' % (msvs_version or FALLBACK_MSVS_VERSION),
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
