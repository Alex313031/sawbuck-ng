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

"""Fetches Sawbuck's dependencies at the exact revisions it was last built against.

This is the depot_tools-free alternative to `gclient sync`. DEPS describes the
same dependencies at the same revisions and is the primary path; this script
exists because it needs nothing but Python 3, and because it can fetch a
*subdirectory* of a repository, which git cannot. Use one or the other, not
both: gclient wants a git checkout at each path, and this script writes plain
directories.

Both replace the original DEPS.sawbuck, every URL of which was dead: Chromium
left SVN in August 2014 (src.chromium.org/svn is gone) and Google Code shut
down in 2016.

Sawbuck's last dependency roll was commit 8b329322, "Rolls Sawbuck deps to
Chromium r271365 and deals with fallout." (2014-07-31). Rather than approximate
that with a nearby milestone, each SVN revision below was mapped to its exact
git commit, so this reproduces the tree Sawbuck was actually maintained against:

  * Chromium preserves the SVN revision in every commit footer, e.g.
      git-svn-id: svn://svn.chromium.org/chrome/trunk/src@271365 0039d316-...
    so r271365 was resolved via Google's crrev service:
      https://cr-rev.appspot.com/_ah/api/crrev/v1/redirect/271365
    It is 37.0.2002.0 -- M37 *trunk*, 2014-05-19. (Note this is the trunk line,
    not the 2062 release branch.)

  * The googlecode mirrors (gyp, gtest, gmock) and chromium/deps/icu46 keep the
    same footers, so their pins were resolved by grepping the mirrors' logs.
    Where a pinned revision did not touch trunk, the nearest preceding trunk
    commit is used -- which is exactly what an SVN checkout at that pin yielded.

Content is fetched through Gitiles' +archive endpoint, so no repository clone is
needed; each directory is downloaded at its exact revision and nothing more.

Every path written here is covered by .gitignore, which lists precisely the
directories gclient used to supply, so these stay untracked and the repository
stays small. The one exception is third_party/wtl, which is committed in-tree
and therefore absent from the manifest below.

Paths are always resolved against the repository root, not the working
directory, so this can be run from anywhere.

Usage:
  ./tools/sync_deps.py                 # fetch anything missing
  ./tools/sync_deps.py --force         # re-fetch everything
  ./tools/sync_deps.py --only base     # fetch a single entry
  ./tools/sync_deps.py --list          # print the manifest and exit
"""

import argparse
import io
import os
import shutil
import sys
import tarfile
import urllib.error
import urllib.request

GITILES = 'https://chromium.googlesource.com'

# Chromium src @ SVN r271365 == git 80d0cd87 == 37.0.2002.0 (M37 trunk, 2014-05-19).
CHROMIUM_SRC = '80d0cd877a17c0678363b392b5d92403c15f1848'

# base/ and build/ are pinned newer: the Chromium 49 branch point,
# master@{#369907}. M49 is the last Chromium that supported Windows XP and is
# still entirely GYP. Fetching these by subpath from chromium/src gives the
# same trees DEPS gets from the per-directory mirrors -- verified by tree hash
# -- and unlike git, Gitiles' archive endpoint can also reach the
# 49.0.2623.112 release commit (4ec79b7f) if the stabilization-branch changes
# to base/ and build/ ever turn out to matter.
CHROMIUM_SRC_M49 = '92d77538a86529ca35f9220bd3cd512cbea1f086'

# GYP itself. The pinned tools/gyp below is r1919, which is Python 2 only, and
# no Python 2 is available any more. gyp-next is the maintained successor and
# the only actively developed GYP line; it generates Sawbuck's whole graph
# unmodified. See tools/gyp_sawbuck.py, which drives it.
GYP_NEXT_VERSION = 'v0.22.4'

# (dest, repo, commit, subpath_in_repo, provenance)
#
# subpath None means "the whole repository", which is how the gclient-managed
# standalone checkouts (gtest, gmock, icu, gyp) were laid out.
#
# A repo of the form 'github:owner/name' is fetched from GitHub's release
# tarballs instead of Gitiles, with 'commit' naming a tag.
DEPS = [
    # --- From chromium/src at the Chromium 49 branch point. ---
    ('base', 'chromium/src', CHROMIUM_SRC_M49, 'base',
     'M49 branch point, master@{#369907}'),
    ('build', 'chromium/src', CHROMIUM_SRC_M49, 'build',
     'M49 branch point, master@{#369907}'),

    # --- From chromium/src at the pinned chrome_revision (SVN r271365). ---
    # Vestigial, so not fetched -- nothing in the tree references google_apis
    # and generation succeeds without it. Left here, commented, so the pin
    # survives if it turns out to be needed. Also commented out in DEPS.
    # ('google_apis', 'chromium/src', CHROMIUM_SRC, 'google_apis',
    #  'src/google_apis@271365'),
    # Supplies gtest.gyp and gmock.gyp, which sawbuck's .gyp files depend on.
    ('testing', 'chromium/src', CHROMIUM_SRC, 'testing',
     'src/testing@271365'),
    # Not fetched -- nothing in the tree references either, and removing them
    # loses no generated targets. Left commented so the pins survive.
    # ('tools/win', 'chromium/src', CHROMIUM_SRC, 'tools/win',
    #  'src/tools/win@271365'),
    # ('tools/code_coverage', 'chromium/src', CHROMIUM_SRC, 'tools/code_coverage',
    #  'src/tools/code_coverage@271365'),
    # third_party/wtl is committed in-tree, not fetched: it has no standalone
    # mirror anywhere and Chromium's copy is patched, so there is nothing to
    # sync it from. See DEPS.
    ('third_party/zlib', 'chromium/src', CHROMIUM_SRC, 'third_party/zlib',
     'src/third_party/zlib@271365'),
    ('third_party/modp_b64', 'chromium/src', CHROMIUM_SRC, 'third_party/modp_b64',
     'src/third_party/modp_b64@271365'),
    # Not fetched. Only base/allocator/allocator.gyp's 'tcmalloc_dir' refers to
    # it, and the allocator target is not in anything Sawbuck builds.
    # ('third_party/tcmalloc', 'chromium/src', CHROMIUM_SRC, 'third_party/tcmalloc',
    #  'src/third_party/tcmalloc@271365'),
    # Not in DEPS.sawbuck -- see NOT_IN_DEPS below. base.gyp's
    # test_support_base target depends on libxml.gyp, and GYP resolves
    # dependencies at *file* granularity, so loading base.gyp at all requires
    # this file to exist. Only the .gyp file matters: libxml's actual sources
    # were a separate DEPS entry in Chromium and nothing Sawbuck builds links
    # them, so they are not fetched.
    ('third_party/libxml', 'chromium/src', CHROMIUM_SRC, 'third_party/libxml',
     'src/third_party/libxml@271365 (added; not in DEPS.sawbuck)'),

    # --- Standalone checkouts, each pinned separately by DEPS.sawbuck. ---
    # DEPS.sawbuck pinned icu46 at chrome_revision (r271365); the newest icu46
    # commit at or before that is r262949 -- the very commit Chromium itself
    # pinned at r271365, so the two agree exactly.
    ('third_party/icu', 'chromium/deps/icu46',
     'c25f7de14670091b1b7dfeb7af4d04050cbbb1d8', None,
     'deps/third_party/icu46@271365 -> r262949'),
    # gtest_revision 664; r664 did not touch trunk, so r663 is the checkout.
    ('testing/gtest', 'external/googletest',
     'be1b3b66822cc3929f3da700973cef88bf45849a', None,
     'googletest/trunk@664 -> r663'),
    # gmock_revision 453; r453 did not touch trunk, so r451 is the checkout.
    ('testing/gmock', 'external/googlemock',
     'd138e25941faa1a80f11a8e4cf2c7636402cc720', None,
     'googlemock/trunk@453 -> r451'),
    # Not fetched. GYP at its original r1919 pin, kept for a while so the
    # known-good baseline stayed reproducible. That rationale is gone -- the
    # baseline is Chromium 49 now -- and it is Python 2 only, so it cannot run
    # regardless. gyp-next below is what generates the build.
    # ('tools/gyp', 'external/gyp',
    #  '1a622e23d0c344c55ccac285cab60c2ab991ac9a', None,
    #  'gyp/trunk@1919'),
    # The GYP actually used to generate, since tools/gyp cannot run here.
    ('tools/gyp-next', 'github:nodejs/gyp-next', GYP_NEXT_VERSION, None,
     'gyp-next %s (added; replaces tools/gyp under Python 3)' % GYP_NEXT_VERSION),
]

# This script lives in tools/; dependencies are fetched into the repository
# root above it.
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


GITHUB_PREFIX = 'github:'


def archive_url(repo, commit, subpath):
    """Builds the archive URL for a directory at a commit.

    GitHub release tarballs wrap everything in a '<name>-<version>/' directory;
    Gitiles' +archive output is already rooted at the requested path. Returns
    the number of leading path components to strip alongside the URL.
    """
    if repo.startswith(GITHUB_PREFIX):
        return ('https://github.com/%s/archive/refs/tags/%s.tar.gz'
                % (repo[len(GITHUB_PREFIX):], commit), 1)
    if subpath:
        return ('%s/%s/+archive/%s/%s.tar.gz'
                % (GITILES, repo, commit, subpath), 0)
    return ('%s/%s/+archive/%s.tar.gz' % (GITILES, repo, commit), 0)


def download(url):
    try:
        with urllib.request.urlopen(url, timeout=300) as response:
            return response.read()
    except urllib.error.HTTPError as e:
        raise RuntimeError('HTTP %s fetching %s' % (e.code, url))
    except urllib.error.URLError as e:
        raise RuntimeError('cannot reach %s (%s)' % (url, e.reason))


def _is_within(base, target):
    """True if target resolves to base or something underneath it."""
    base = os.path.realpath(base)
    target = os.path.realpath(target)
    return target == base or target.startswith(base + os.sep)


def extract(blob, dest, strip=0):
    """Extracts a .tar.gz into dest and returns the links it skipped.

    'strip' drops that many leading path components from every member, the way
    tar's --strip-components does, for archives that wrap their contents in a
    top-level directory.

    A member whose extraction path would escape dest is a hard error. Symlinks
    and hardlinks *pointing* outside dest are skipped instead, because
    build/android ships NDK symlinks into third_party/android_tools -- a tree
    this script never fetches and which no Windows build needs. Letting those
    abort the sync would make build/ unfetchable.
    """
    os.makedirs(dest, exist_ok=True)
    skipped = []

    with tarfile.open(fileobj=io.BytesIO(blob), mode='r:gz') as tar:
        members = []
        for member in tar.getmembers():
            if strip:
                parts = member.name.split('/')[strip:]
                if not parts:
                    # The wrapper directory itself; dest already stands in
                    # for it.
                    continue
                member.name = '/'.join(parts)
            if not _is_within(dest, os.path.join(dest, member.name)):
                raise RuntimeError('refusing unsafe tar path: %s' % member.name)
            if member.issym() or member.islnk():
                # A symlink's target is relative to its own directory; a
                # hardlink's is relative to the archive root.
                anchor = (os.path.dirname(os.path.join(dest, member.name))
                          if member.issym() else dest)
                if not _is_within(dest, os.path.join(anchor, member.linkname)):
                    skipped.append(member.name)
                    continue
            members.append(member)

        # Paths are already validated above, so the permission-normalising
        # 'tar' filter is enough. It is absent before Python 3.11.4.
        try:
            tar.extractall(dest, members=members, filter='tar')
        except TypeError:
            tar.extractall(dest, members=members)

    return skipped


def sync(entry, force):
    dest, repo, commit, subpath, provenance = entry
    path = os.path.join(ROOT, dest)

    if os.path.isdir(path) and not force:
        print('  [skip]  %-24s already present' % dest)
        return 0

    if os.path.isdir(path):
        shutil.rmtree(path)

    url, strip = archive_url(repo, commit, subpath)
    blob = download(url)
    skipped = extract(blob, path, strip)

    count = sum(len(files) for _, _, files in os.walk(path))
    note = provenance
    if skipped:
        note += ', %d out-of-tree link%s skipped' % (
            len(skipped), '' if len(skipped) == 1 else 's')
    print('  [ok]    %-24s %6d files  (%s)' % (dest, count, note))
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--force', action='store_true',
                        help='re-fetch dependencies that are already present')
    parser.add_argument('--only', metavar='DEST', action='append',
                        help='fetch only this entry (repeatable)')
    parser.add_argument('--list', action='store_true',
                        help='print the manifest and exit')
    args = parser.parse_args()

    if args.list:
        print('%-24s %-24s %-12s %s' % ('DEST', 'REPO', 'COMMIT', 'PINNED AS'))
        for dest, repo, commit, _, provenance in DEPS:
            print('%-24s %-24s %-12s %s' % (dest, repo, commit[:10], provenance))
        return 0

    entries = DEPS
    if args.only:
        wanted = set(args.only)
        entries = [e for e in DEPS if e[0] in wanted]
        missing = wanted - {e[0] for e in DEPS}
        if missing:
            print('error: unknown entries: %s' % ', '.join(sorted(missing)),
                  file=sys.stderr)
            return 2

    print('Syncing %d dependencies into %s' % (len(entries), ROOT))
    total = 0
    failed = []
    for entry in entries:
        try:
            total += sync(entry, args.force)
        except RuntimeError as e:
            print('  [FAIL]  %-24s %s' % (entry[0], e), file=sys.stderr)
            failed.append(entry[0])

    print('Done: %d files fetched.' % total)
    if failed:
        print('FAILED: %s' % ', '.join(failed), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
