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

vars = {
  # Chromium M37.0.2062.124
  "chrome_revision": "448c438598eefecd7809b9fa70fece1e89d79172",
  "gmock_revision": "6b1759c3816d574bddde3e1725c51a811c8870e7",
  "gtest_revision": "74de57c951aedebc5dfe26a27604353432392b98",
  "gyp_revision": "63a1f78eb0ec6c4ddcc23e920230bdb0b14f7855",

  # Paths to installed utilities used in hooks. These need to use
  # Windows style paths.
  "python_path": "src\\third_party\\python_26\\python.exe",
  "gyp_path": "src\\tools\\gyp\\gyp_main.py",

  "chrome_base": "https://chromium.googlesource.com/chromium/src.git",
  "base_base": "https://chromium.googlesource.com/chromium/src/base.git",
  "build_base": "https://chromium.googlesource.com/chromium/src/build.git",
  "code_coverage_base": "https://chromium.googlesource.com/chromium/src/tools/code_coverage.git",
  "gmock_base": "https://github.com/svn2github/chromium-gmock.git",
  "google_apis_base": "https://chromium.googlesource.com/chromium/src/google_apis.git",
  "gtest_base": "https://github.com/svn2github/chromium-gtest.git",
  "gyp_base": "https://chromium.googlesource.com/external/gyp.git",
  "icu_base": "https://chromium.googlesource.com/chromium/third_party/icu46.git",
  "modp_b64_base": "https://chromium.googlesource.com/chromium/src/third_party/modp_b64.git",
  "psyco_win32_base": "https://chromium.googlesource.com/chromium/deps/psyco_win32.git",
  "python_26_base": "https://github.com/svn2github/chromium-python26.git",
  "tcmalloc_base": "https://chromium.googlesource.com/chromium/src/third_party/tcmalloc/chromium.git",
  "testing_base": "https://github.com/svn2github/chromium-testing.git",
  "wintools_base": "https://chromium.googlesource.com/chromium/src/tools/win.git",
  "wtl_base": "https://github.com/Win32-WTL/WTL.git",
  "zlib_base": "https://chromium.googlesource.com/chromium/src/third_party/zlib.git",
}

deps = {
  #"src/base":
    #Var("chrome_base") + "/src/base@" + Var("chrome_revision"),
  #"src/build":
    #Var("chrome_base") + "/src/build@" + Var("chrome_revision"),
  # This brings in code coverage tools, like croc. This is required for our
  # coverage generation.
  #"src/tools/code_coverage":
    #Var("chrome_base") + "/src/tools/code_coverage@" + Var("chrome_revision"),

  "src/testing/gmock":
    Var("gmock_base") + "@" + Var("gmock_revision"),
  #"src/google_apis":
    #Var("google_apis_base") + "@" + 'NA',
  "src/testing/gtest":
    Var("gtest_base") + "@" + Var("gtest_revision"),

  # This brings in GYP.
  "src/tools/gyp":
    Var("gyp_base") + "@" + Var("gyp_revision"),

  "src/third_party/icu":
    Var("icu_base") + "@" + '78597121d71a5922f5726e715c6ad06c50ae6cdc',
  #"src/third_party/modp_b64":
    #Var("modp_b64_base") + "@" + 'NA',
  "src/third_party/psyco_win32":
    Var("psyco_win32_base") + "@" + 'f5af9f6910ee5a8075bbaeed0591469f1661d868',
  "src/third_party/python_26":
    Var("python_26_base") + "@" + '5bb4080c33f369a81017c2767142fb34981f2a54',
  #"src/third_party/tcmalloc":
    #Var("tcmalloc_base") + "@" + 'NA',
  "src/testing":
    Var("testing_base") + "@" + 'b238c6d7605796d252f18973c6f46f1a00e04e79',
  #"src/tools/win":
    #Var("wintools_base") + "@" + 'd6c1a0710805b5fb057095924ebff33185c4d62d',

  #"src/third_party/wtl":
    #Var("chrome_base") + "/src/third_party/wtl@" + Var("chrome_revision"),
  #"src/third_party/zlib":
    #Var("chrome_base") + "/src/third_party/zlib@" + Var("chrome_revision"),
}


include_rules = [
  # Everybody can use some things.
  "+base",
  "+build",
  "+googleurl",
]

hooks = [
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "pattern": ".",
    "action": [Var("python_path"),
               Var("gyp_path"),
               "--include=src/build/common.gypi",
               "--include=src/sawbuck/sawbuck.gypi",
               "--no-circular-check",
               "src/sawbuck/sawbuck.gyp"],
  },
]
