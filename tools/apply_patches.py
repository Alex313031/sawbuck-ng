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

"""Applies the patches in patches/ to the DEPS-synced dependencies.

Some fixes have to be made inside dependencies that DEPS fetches -- build/ most
of all, whose common.gypi still wires in Windows SDK 8.0 paths. Editing those
trees directly does not survive: the next `gclient sync` restores them. So the
changes live here as patches and are reapplied after every sync.

DEPS runs this as a hook, before the one that runs GYP.

Patches are listed in patches/series, one filename per line, applied in order;
blank lines and '#' comments are ignored. Paths inside each patch are relative
to the repository root with the usual a/ and b/ prefixes, i.e. -p1.

Reapplying is safe. Each patch is classified before anything is written:

  * already applied  -> skipped (this is the common case, since the hook runs
                        on every sync and gclient only re-fetches what changed)
  * applies cleanly  -> applied
  * neither          -> reported as an error, and nothing is written

That last case usually means the dependency moved under the patch, and the
patch needs rebasing against the new revision.

Usage:
  ./tools/apply_patches.py            # apply anything outstanding
  ./tools/apply_patches.py --check    # report status, change nothing
  ./tools/apply_patches.py --revert   # un-apply, in reverse order
  ./tools/apply_patches.py --list     # print the series and exit
"""

import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PATCH_DIR = os.path.join(ROOT, 'patches')
SERIES = os.path.join(PATCH_DIR, 'series')

# Applied, not reversed -> "already applied". Both are tried with --check
# first, so a patch is only written once we know which case it is.
APPLIED, APPLIES, NEITHER = 'applied', 'applies', 'neither'


def read_series():
    """Returns the patch filenames listed in patches/series, in order."""
    if not os.path.isfile(SERIES):
        return []
    names = []
    with open(SERIES) as f:
        for line in f:
            line = line.split('#', 1)[0].strip()
            if line:
                names.append(line)
    return names


def git_apply(path, *flags):
    """Runs `git apply` from the repository root. True if it succeeded."""
    return subprocess.run(
        ['git', 'apply', '-p1'] + list(flags) + [path],
        cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    ).returncode == 0


def classify(path):
    # --reverse --check succeeding means the content is already in the state
    # the patch produces.
    if git_apply(path, '--reverse', '--check'):
        return APPLIED
    if git_apply(path, '--check'):
        return APPLIES
    return NEITHER


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--check', action='store_true',
                       help='report what would happen, change nothing')
    group.add_argument('--revert', action='store_true',
                       help='un-apply the patches, in reverse order')
    group.add_argument('--list', action='store_true',
                       help='print the series and exit')
    args = parser.parse_args()

    names = read_series()
    if args.list:
        for name in names:
            print(name)
        return 0
    if not names:
        print('No patches listed in %s.' % os.path.relpath(SERIES, ROOT))
        return 0

    if args.revert:
        names = list(reversed(names))

    applied = skipped = 0
    failed = []
    for name in names:
        path = os.path.join(PATCH_DIR, name)
        if not os.path.isfile(path):
            print('  [FAIL]  %-52s missing' % name, file=sys.stderr)
            failed.append(name)
            continue

        state = classify(path)

        if args.revert:
            if state == APPLIED:
                if args.check:
                    print('  [would revert] %s' % name)
                elif git_apply(path, '--reverse'):
                    print('  [revert] %s' % name)
                else:
                    print('  [FAIL]   %s' % name, file=sys.stderr)
                    failed.append(name)
                    continue
                applied += 1
            else:
                print('  [skip]   %-52s not applied' % name)
                skipped += 1
            continue

        if state == APPLIED:
            print('  [skip]   %-52s already applied' % name)
            skipped += 1
        elif state == APPLIES:
            if args.check:
                print('  [would apply] %s' % name)
            elif not git_apply(path):
                print('  [FAIL]   %-52s apply failed' % name, file=sys.stderr)
                failed.append(name)
                continue
            else:
                print('  [ok]     %s' % name)
            applied += 1
        else:
            print('  [FAIL]   %-52s does not apply, and is not already applied'
                  % name, file=sys.stderr)
            print('           The dependency it patches has probably changed; '
                  'rebase it.', file=sys.stderr)
            failed.append(name)

    verb = 'reverted' if args.revert else 'applied'
    if args.check:
        verb = 'outstanding'
    print('Patches: %d %s, %d skipped.' % (applied, verb, skipped))
    if failed:
        print('FAILED: %s' % ', '.join(failed), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
