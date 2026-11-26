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
# Four things about the port are worth knowing.
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
# 4. third_party/winwrap is our own repository rather than a third-party pin,
#    and it tracked 'main' while the XP work was still landing. It is pinned
#    now, so this file is once again fully pinned and a sync is reproducible.
#
#    Pinned deliberately, and not only for reproducibility: winwrap's public
#    surface can move. 0.3.8 reorganized it -- winwrap.h stopped being on a
#    consumer's include path, in favour of src/public/winwrap_public.h -- so a
#    floating 'main' is a build that can break without a commit here. Roll it
#    the way every other entry below is rolled: by hand, to a release tag's
#    commit, after checking the build.
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

  # Everything is pinned to Chromium 49 now except third_party/icu, which is
  # still the icu46 repository at SVN r271365 (M37). That one is deliberately
  # last: it is a repository swap as well as a version bump, ICU 4.6 to 54.1,
  # and it is the only dependency here that is optional -- it works as it is.
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

  # Also at the Chromium 49 branch point. These are not in Chromium 49's own
  # DEPS because they were in-tree by then, so "what M49 used" is simply the
  # content at 92d77538. Each was resolved the same two ways as base and build
  # -- Cr-Original-Commit-Position footer, then tree-hash comparison against
  # chromium/src@92d77538:<path> -- and both agree.
  'zlib_revision': '8ce285440282702dfe4935baa2aa2f6819bed292',            # 2016-01-14, M49
  'modp_b64_revision': '781604dfe4c368363cec8c46d07dd9925e0c8f6b',        # 2015-10-08, M49
  'libxml_revision': 'fc3da0e5d6c0914e019979489c50cd588d1fb23a',          # 2016-01-15, M49
  # testing/ supplies gtest.gyp and gmock.gyp. Bumping it moves the source
  # list out to a new gtest.gypi and adds GTEST_LANG_CXX11=0 to the gtest
  # target, which is a no-op here: gtest derives that macro from
  # '__GXX_EXPERIMENTAL_CXX0X__ || __cplusplus >= 201103L', and MSVC reports
  # __cplusplus as 199711L unless /Zc:__cplusplus is passed, which nothing
  # here passes. So it was already 0; M49 merely states it.
  'testing_revision': '776a9b3263577d56931c32f5b8f5d3667be1d8ee',         # 2016-01-16, M49

  # Resolved pins for entries that are no longer fetched, kept so restoring any
  # of them needs no re-resolving. See the commented-out deps below.
  # 'google_apis_revision': '6c69993c84c1eed8d4194ad704a19b02be67cb58',   # 2014-05-15
  # 'tools_win_revision': 'ea95f6435a0becd1fabd26d202a12690c762ead7',     # 2014-04-16
  # 'code_coverage_revision': '23081ea84f5af2b0445e4475581ce5e3f8485164', # 2013-11-14
  # 'tcmalloc_revision': '694f2a1366c4431e0c076474328ce6125aa897e0',      # 2014-05-18

  # Standalone repositories. These were already whole-repo checkouts in the old
  # file rather than subdirectories of chromium/src, so they port over
  # directly.
  #
  # ICU is Chromium 49's own pin, read out of chromium/src@92d77538:DEPS. This
  # is a repository change as well as a version bump: chromium/deps/icu46 held
  # ICU 4.6 and was retired, chromium/deps/icu holds the line Chromium moved
  # to, and the pin below is ICU 54.1 (2015-12-14).
  #
  # Nothing moves on disk. The new repository keeps windows/icudt.dll at the
  # same path, so sawbuck.wxs needs no change, and icu.gyp still exports the
  # same three targets Sawbuck and base depend on -- icudata, icui18n, icuuc.
  # Both versions also declare 'icu_use_data_file_flag%': 0 in their own
  # variables block, so the Windows data mechanism is unchanged: icudata stays
  # a copy of windows/icudt.dll rather than becoming a copy of icudtl.dat.
  #
  # The M37 pin this replaces: chromium/deps/icu46 @
  # c25f7de14670091b1b7dfeb7af4d04050cbbb1d8 (svn r262949, which was the newest
  # icu46 commit at or before r271365 and the very commit Chromium pinned
  # there, so the two agreed exactly).
  'icu_revision': '8d342a405be5ae8aacb1e16f0bc31c3a4fbf26a2',    # ICU 54.1, M49
  # gtest and gmock are the pins from Chromium 49's own DEPS, read out of
  # chromium/src@92d77538:DEPS. Both are 2015-07-28, fourteen minutes apart.
  #
  # Note gtest's *repository* changes with this: Chromium had already moved
  # to the GitHub mirror by M49, which is a different SHA space from the old
  # googlecode one, so the pins are not comparable. Nothing moves around on
  # disk though -- 6f8a6643 still predates googletest's mid-2015 merge with
  # gmock, so the top level is still include/, src/, test/, msvc/ rather than
  # the later googletest/ + googlemock/ split.
  #
  # The M37 pins these replace, which were resolved from SVN footers and are
  # not recoverable from the new repository:
  #   external/googletest @ be1b3b66822cc3929f3da700973cef88bf45849a  # r663
  #   external/googlemock @ d138e25941faa1a80f11a8e4cf2c7636402cc720  # r451
  # (DEPS.sawbuck asked for gtest r664 and gmock r453; neither touched trunk,
  # so r663 and r451 were what an SVN checkout at those pins produced.)
  'gtest_revision': '6f8a66431cb592dad629028a50b3dd418a408c87',   # 2015-07-28, M49
  'gmock_revision': '0421b6f358139f02e102c9c332ce19a33faf75be',   # 2015-07-28, M49
  'gyp_revision': '1a622e23d0c344c55ccac285cab60c2ab991ac9a',    # svn r1919
  'gyp_next_revision': 'b5b1525b5b834559d9b84d4eb48117adc3dedb23',  # v0.22.4

  # See note 4 above. Roll this by hand, to the commit a winwrap release tag
  # points at, after checking Sawbuck still builds and runs against it.
  'winwrap_revision': '0caa20cfe1d37caf0eb27a131d09ea2377b8ae38',  # 0.5.1
}

# Only these hosts are allowed for dependencies in this DEPS file.
allowed_hosts = [
  'chromium.googlesource.com',
  'github.com',
]

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
  # ICU 54.1. Required by base_i18n and the icudata target; the viewer calls
  # base::i18n::InitializeICU() and formats timestamps through it. Not
  # deps/icu46: that repository held ICU 4.6 and Chromium had left it by M49.
  # Changing a dep's URL is something gclient does not always resolve on its
  # own -- if a sync complains about this path, delete third_party/icu and
  # sync again.
  'third_party/icu':
    '{chromium_git}/chromium/deps/icu.git@{icu_revision}',
  'third_party/modp_b64':
    '{chromium_git}/chromium/src/third_party/modp_b64.git@{modp_b64_revision}',
  # Not fetched. The only thing that refers to it is
  # base/allocator/allocator.gyp's 'tcmalloc_dir', and the allocator target is
  # not in anything Sawbuck builds -- removing it loses no targets at all.
  # Commented rather than dropped so the pin survives; it was the 'chromium'
  # subdirectory only, which is the one path allocator.gyp uses (the sibling
  # 'vendor' copy of gperftools is unused and has no faithful mirror).
  # 'third_party/tcmalloc/chromium':
  #   '{chromium_git}/chromium/src/third_party/tcmalloc/chromium.git@{tcmalloc_revision}',
  # Not in the original DEPS; required to generate at all. See note 3 above.
  'third_party/libxml':
    '{chromium_git}/chromium/src/third_party/libxml.git@{libxml_revision}',

  # Windows XP support, and ours rather than third-party. winwrap supplies
  # Win32 functions that pre-Win7 Windows does not export, so a binary built
  # against a current SDK still loads there -- a statically imported symbol
  # the target OS does not export is fatal at load, before any process code
  # runs, so it cannot be fixed at the call site. It works by defining the
  # __imp_<name> slots that dllimport call sites load through, which means no
  # call site here has to change. At run time each function asks the running
  # Windows for the real one first and only falls back to its own, so one
  # binary covers XP and Win7+ both.
  #
  # This is why _WIN32_WINNT does not move for the XP work: winwrap lowers it
  # to 0x0501 for its own sources only, and Sawbuck keeps building against the
  # current SDK and links /SUBSYSTEM:5.01.
  #
  # Needed by an ordinary build rather than an optional extra: build.bat
  # exports GYP_DEFINES=sawbuck_use_winwrap=1, so the scripted build links it
  # even though sawbuck/sawbuck.gypi declares the variable as 0. Hence not
  # commented out here the way the genuinely unused entries are -- a sync
  # without this produces a tree that cannot generate the default build. See
  # docs/BUILDING.md for what it costs and how to build without it.
  #
  # winwrap ships its own winwrap.gyp alongside BUILD.gn, so this needs no
  # build file written here -- deliberately, since gclient sync
  # --delete_unversioned_trees would remove one placed in the checkout.
  # sawbuck/viewer/viewer.gyp names the target and carries the two settings the
  # consumer owns: winwrap has to come ahead of the kernel32.lib that
  # common.gypi names explicitly, or call sites bind straight to the system
  # DLL, and /SUBSYSTEM has to come down. Its own sources want /std:c++17,
  # which is appended rather than replacing our /std:c++14 since the last
  # /std: on the command line wins.
  #
  # winwrap.gyp needs patches/06-gyp-next-masm-settings.patch, which the hook
  # applies to tools/gyp-next below; without it the MASM define that selects
  # the architecture is dropped at generation time.
  #
  # The path matters. BUILD.gn imports //third_party/winwrap/winwrap.gni by
  # absolute label, and README.chromium says to read the revision from the
  # DEPS entry, so upstream already expects to live exactly here.
  'third_party/winwrap':
    '{github_git}/Alex313031/winwrap.git@{winwrap_revision}',

  'build':
    '{chromium_git}/chromium/src/build.git@{build_revision}',
  # Not fetched. Windows dev utilities (ChromeDebug, DebugVisualizers,
  # link_limiter, sizeviewer, split_link, static_initializers); nothing in the
  # tree references any of them, and it was the largest item here by line count.
  # 'tools/win':
  #   '{chromium_git}/chromium/src/tools/win.git@{tools_win_revision}',

  # Supplies gtest.gyp, gtest.gypi and gmock.gyp -- the build files, not the
  # sources, which come from the two standalone repositories below.
  'testing':
    '{chromium_git}/chromium/src/testing.git@{testing_revision}',
  'testing/gmock':
    '{chromium_git}/external/googlemock.git@{gmock_revision}',
  # Not external/googletest: that is the old googlecode mirror, which Chromium
  # had already left by M49. Changing a dep's URL is something gclient does not
  # always resolve on its own -- if a sync complains about this path, delete
  # testing/gtest and sync again.
  'testing/gtest':
    '{chromium_git}/external/github.com/google/googletest.git@{gtest_revision}',

  # Not fetched. This was GYP at its original r1919 pin, kept for a while so
  # the known-good baseline stayed reproducible. That rationale is gone: the
  # baseline is Chromium 49 now, and this GYP is Python 2 only so it cannot run
  # regardless. gyp-next below is what generates the build.
  # 'tools/gyp':
  #   '{chromium_git}/external/gyp.git@{gyp_revision}',
  # The maintained GYP line, and the only one that runs under Python 3. It
  # generates Sawbuck's graph unmodified. Driven by tools/gyp_sawbuck.py.
  'tools/gyp-next':
    '{github_git}/nodejs/gyp-next.git@{gyp_next_revision}',

  # Not fetched. This brought in code coverage tools, like croc, for coverage
  # generation -- never for building. Nothing references it.
  # 'tools/code_coverage':
  #   '{chromium_git}/chromium/src/tools/code_coverage.git@{code_coverage_revision}',
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
