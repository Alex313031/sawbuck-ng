# Copyright 2012 Google Inc. All Rights Reserved.
#
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
#
# Sawbuck builds on Chrome base, uses GYP, GTest, all of which requires
# this build configuration.
#
# -----------------------------------------------------------------------------
# Rewritten 2026-11-14 for modern, git-based gclient; was DEPS.sawbuck.
#
# The original file could not be synced at all -- every URL in it was dead.
# Chromium left SVN in August 2014 (src.chromium.org/svn is gone) and Google
# Code shut down in 2016.
#
# Three things about the port are worth knowing.
#
# 1. There is no single 'chrome_revision' any more. The old file checked out ten
#    *subdirectories* of the Chromium SVN tree at one revision. Git cannot check
#    out a subdirectory of a repository, so each of those now points at
#    Chromium's own per-directory mirror -- chromium/src/base,
#    chromium/src/build, and so on -- and each mirror carries its own SHA.
#
#    Those SHAs are not approximations. Each is the commit in that mirror whose
#    git *tree hash* is byte-identical to chromium/src@80d0cd87:<path>, where
#    80d0cd877a17c0678363b392b5d92403c15f1848 is SVN r271365 -- the revision
#    pinned by Sawbuck's last dependency roll, 8b329322, "Rolls Sawbuck deps to
#    Chromium r271365 and deals with fallout." (2014-07-31). r271365 is
#    37.0.2002.0: M37 *trunk*, 2014-05-19, not the 2062 release branch. So this
#    file describes exactly the content the old one did, addressed differently.
#
#    Where a mirror's date below predates 2014-05-19, that is simply the newest
#    commit that touched the directory at or before r271365 -- which is what an
#    SVN checkout at that revision produced.
#
# 2. third_party/wtl is committed in-tree and so is absent below. It has no
#    standalone mirror anywhere on googlesource, and Chromium's copy is patched
#    (min/max renamed to __min/__max), so upstream WTL is not a substitute.
#
# 3. third_party/libxml is new, and was missing from the old file. base.gyp's
#    test_support_base target depends on libxml.gyp, and GYP resolves
#    dependencies at *file* granularity: loading base.gyp at all forces every
#    target in it to resolve, so generation fails without this. The roll commit
#    above added '/third_party/libxml/' to .gitignore and no matching DEPS
#    entry, which is presumably why nobody noticed.
#
# use_relative_paths makes every path below relative to *this file*, so the
# repository root is the source root and GYP's depth is '.'. The old file
# prefixed every path with 'src/' because gclient checked Sawbuck out into a
# directory of that name.
# -----------------------------------------------------------------------------

use_relative_paths = True

vars = {
  'chromium_git': 'https://chromium.googlesource.com',
  'github_git': 'https://github.com',

  # base and build are pinned to Chromium 49, the rest to SVN r271365 (M37).
  #
  # M49 is the last Chromium that supported Windows XP, and it is still
  # entirely GYP -- there is no build/BUILD.gn at that revision -- so it clears
  # both constraints at once. It is also newer about the toolchain: M49 targets
  # the Windows 10 SDK and the Win10 API level, where M37 targeted SDK 8.0.
  #
  # These two are the Chromium 49 *branch point*,
  # chromium/src 92d77538a86529ca35f9220bd3cd512cbea1f086 = master@{#369907},
  # not the 49.0.2623.112 release. The difference is deliberate and forced:
  # the per-directory mirrors carry refs/branch-heads only up to 2372, so
  # branch 2623 is not in them, and git cannot check out a subdirectory of
  # chromium/src. base and build both did pick up changes on the M49
  # stabilization branch (their release trees differ from the branch point's),
  # so if those ever matter, tools/sync_deps.py can fetch the exact release
  # trees through Gitiles' archive endpoint.
  #
  # Both were resolved two ways that agree: the mirror commits' own
  # Cr-Original-Commit-Position footers (#369906, the newest at or below the
  # branch point), and a tree-hash comparison against
  # chromium/src@92d77538:{base,build}.
  'base_revision': '7b907946fe89fbdb5ceab832a6c960b11aeb2437',            # 2016-01-15, M49
  'build_revision': '2e175f8fb88446c0bd4999bb5ab24677ddce63b6',           # 2016-01-15, M49

  # Still M37 (SVN r271365). The dates are the pinned commit's own date; see
  # note 1 above.
  'testing_revision': 'cbc76319f284b6ab55fd1f36a954afea7f8aac03',         # 2014-05-16
  # Not fetched; see the commented-out google_apis entry below. The resolved
  # pin is kept here so restoring it needs no re-resolving.
  # 'google_apis_revision': '6c69993c84c1eed8d4194ad704a19b02be67cb58',   # 2014-05-15
  'tools_win_revision': 'ea95f6435a0becd1fabd26d202a12690c762ead7',       # 2014-04-16
  'code_coverage_revision': '23081ea84f5af2b0445e4475581ce5e3f8485164',   # 2013-11-14
  'zlib_revision': '7e84479ea07c6fd65d77412bcbf735751f42e230',            # 2014-05-07
  'modp_b64_revision': 'a73c4b6e07087ea5259e61618010a465a00e5253',        # 2014-04-07
  'libxml_revision': '5a6abdfef348a88a5df848e731244e0c4844bfde',          # 2014-04-30
  'tcmalloc_revision': '694f2a1366c4431e0c076474328ce6125aa897e0',        # 2014-05-18

  # Standalone repositories. These were already whole-repo checkouts in the old
  # file, so they port over directly; only the URLs changed.
  #
  # icu46 was pinned at chrome_revision (r271365), and the newest icu46 commit
  # at or before that is r262949 -- the very commit Chromium itself pinned at
  # r271365, so the two agree exactly.
  'icu_revision': 'c25f7de14670091b1b7dfeb7af4d04050cbbb1d8',    # svn r262949
  # gtest_revision was 664; r664 did not touch trunk, so r663 is the checkout.
  'gtest_revision': 'be1b3b66822cc3929f3da700973cef88bf45849a',  # svn r663
  # gmock_revision was 453; r453 did not touch trunk, so r451 is the checkout.
  'gmock_revision': 'd138e25941faa1a80f11a8e4cf2c7636402cc720',  # svn r451
  'gyp_revision': '1a622e23d0c344c55ccac285cab60c2ab991ac9a',    # svn r1919
  'gyp_next_revision': 'b5b1525b5b834559d9b84d4eb48117adc3dedb23',  # v0.22.4
}

deps = {
  'base':
    '{chromium_git}/chromium/src/base.git@{base_revision}',

  # Vestigial. Google API keys / client config headers, inherited from the era
  # when this SVN repo was shared with Syzygy. Nothing references it: no .gyp,
  # .gypi or source file in the tree mentions it (the only hits are comments in
  # build/common.gypi and build/all.gyp, which is never loaded), and generation
  # succeeds with the directory deleted. Commented out rather than dropped so
  # the pin survives if it turns out to be needed.
  # 'google_apis':
  #   '{chromium_git}/chromium/src/google_apis.git@{google_apis_revision}',

  'third_party/zlib':
    '{chromium_git}/chromium/src/third_party/zlib.git@{zlib_revision}',
  # ICU 4.6. Required by base_i18n and the icudata target; the viewer calls
  # base::i18n::InitializeICU() and formats timestamps through it.
  'third_party/icu':
    '{chromium_git}/chromium/deps/icu46.git@{icu_revision}',
  'third_party/modp_b64':
    '{chromium_git}/chromium/src/third_party/modp_b64.git@{modp_b64_revision}',
  # Only the 'chromium' subdirectory, which is the one path
  # base/allocator/allocator.gyp refers to. The sibling 'vendor' copy of
  # gperftools is not used by the build and has no faithful mirror.
  'third_party/tcmalloc/chromium':
    '{chromium_git}/chromium/src/third_party/tcmalloc/chromium.git@{tcmalloc_revision}',
  # Not in the original DEPS; required to generate at all. See note 3 above.
  'third_party/libxml':
    '{chromium_git}/chromium/src/third_party/libxml.git@{libxml_revision}',

  'build':
    '{chromium_git}/chromium/src/build.git@{build_revision}',
  'tools/win':
    '{chromium_git}/chromium/src/tools/win.git@{tools_win_revision}',

  # Supplies gtest.gyp and gmock.gyp -- the build files, not the sources, which
  # come from the two standalone repositories below.
  'testing':
    '{chromium_git}/chromium/src/testing.git@{testing_revision}',
  'testing/gmock':
    '{chromium_git}/external/googlemock.git@{gmock_revision}',
  'testing/gtest':
    '{chromium_git}/external/googletest.git@{gtest_revision}',

  # This brings in GYP. Kept at its original pin so the known-good baseline
  # stays reproducible -- but note it is Python 2 only and therefore cannot
  # run any more. gyp-next below is what actually generates the build.
  'tools/gyp':
    '{chromium_git}/external/gyp.git@{gyp_revision}',
  # The maintained GYP line, and the only one that runs under Python 3. It
  # generates Sawbuck's graph unmodified. Driven by tools/gyp_sawbuck.py.
  'tools/gyp-next':
    '{github_git}/nodejs/gyp-next.git@{gyp_next_revision}',

  # This brings in code coverage tools, like croc. This is required for our
  # coverage generation.
  'tools/code_coverage':
    '{chromium_git}/chromium/src/tools/code_coverage.git@{code_coverage_revision}',
}

# Deliberately dropped from the old file:
#
#   third_party/python_26    A bundled Python 2.6 for Windows. No mirror exists
#                            under googlesource, and nothing needs it now that
#                            GYP runs under Python 3.
#   third_party/psyco_win32  Psyco, a dead JIT for Python 2, used only to speed
#                            up GYP runs under that interpreter.

include_rules = [
  # Everybody can use some things.
  "+base",
  "+build",
  # Vestigial: googleurl was dropped from DEPS by the r271365 roll and removed
  # from Chromium around M27. Only sawdust ever referenced it.
  "+googleurl",
]

hooks = [
  {
    # Must run before the generator: it patches build/common.gypi, which GYP
    # then reads. Some fixes have to be made inside the dependencies fetched
    # above, and a direct edit there would be undone by the next sync -- so
    # they live in patches/ and are reapplied here. Reapplying is a no-op when
    # a patch is already in place, which is the usual case.
    'name': 'apply_patches',
    'pattern': '.',
    'action': ['python3', 'tools/apply_patches.py'],
  },
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    'name': 'gyp_sawbuck',
    'pattern': '.',
    # Replaces the old hook, which invoked the bundled Python 2.6 against
    # tools/gyp directly. tools/gyp_sawbuck.py adds build/ to sys.path for
    # common.gypi's pymod_do_main calls, sets depth and OS, and picks the msvs
    # generator on Windows. With use_relative_paths, hooks run from the
    # repository root, so this path is relative to it.
    'action': ['python3', 'tools/gyp_sawbuck.py'],
  },
]
