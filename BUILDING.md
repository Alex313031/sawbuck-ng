## Building Sawbuck

Sawbuck is built on Windows with Visual Studio and [GYP](https://github.com/nodejs/gyp-next).
There is no cross-compile: it is an ATL/WTL application that links against the
Windows SDK and ETW.

### Requirements

 - **Windows 10 or 11.** Building on the OS you intend to run on is not
   required; Sawbuck itself runs on Vista and newer.
 - **Visual Studio 2022** (Community is fine), with two pieces installed:
   - the **Desktop development with C++** workload, and
   - the **C++ ATL for latest v143 build tools** individual component.

   ATL is not part of the workload by default and has to be ticked
   separately. Twenty-three files in `sawbuck/` include an `atl*.h`, so the
   build fails early without it.
 - **[depot_tools](https://chromium.googlesource.com/chromium/tools/depot_tools.git)**
   on `PATH`, for `gclient`. It also supplies the `python3` that the DEPS hooks
   invoke.
 - **Python 3** on `PATH` as `python`. The GYP actions call the interpreter by
   that name, so `python --version` has to report 3.x.

The Windows SDK comes with the C++ workload and does not need to be chosen
explicitly — GYP emits `WindowsTargetPlatformVersion` and MSBuild resolves the
SDK and ATL paths from it. WiX is vendored in `third_party/wix`, so nothing
needs installing to build the installer.

Everything else — Chromium's `base`, `build` and `testing`, GoogleTest,
GoogleMock, ICU, zlib, libxml, modp_b64, gyp-next and winwrap — is fetched by
`gclient`. See [DEPS](./DEPS).

### Build it

Clone, then run the build script from the repository root:

```bat
git clone https://github.com/Alex313031/sawbuck-ng.git
cd sawbuck-ng
build.bat
```

That is the whole process. `build.bat` finds Visual Studio with `vswhere` (so it
works from a plain `cmd` prompt, not only a Developer Command Prompt), installs
the sample `gclient` solution file one directory above the repository root,
syncs the dependencies, applies `patches/` to them, generates
`sawbuck\sawbuck.sln`, and then builds all four flavours.

Useful options:

| Option | Effect |
|---|---|
| `build.bat --no-sync` | Skip the `gclient` steps and just rebuild. Fails if the solution has not been generated yet. |
| `build.bat --jobs N` | Pass `/m:N` to MSBuild instead of bare `/m`, which uses every core. |
| `build.bat --help` | Show usage. |

Output lands in:

```
out\Debug_x86\     out\Release_x86\
out\Debug_x64\     out\Release_x64\
```

with `Sawbuck.exe` and `Sawbuck.msi` in each.

#### Doing it by hand

If you would rather drive the steps yourself:

```bat
:: 1. Place the gclient solution file one level above the repository root.
::    DEPS sets use_relative_paths and the solution entry names this
::    directory, so it belongs in the parent, not here.
copy gclient ..\.gclient

:: 2. Sync. The hooks that run afterwards apply patches/ to the synced
::    dependencies and then generate the solution.
gclient sync --force --reset --delete_unversioned_trees

:: 3. Build one flavour.
msbuild sawbuck\sawbuck.sln /m /p:Configuration=Debug /p:Platform=Win32
```

`--reset` reverts the patches that `patches/` applies to `build/common.gypi`;
the hooks re-apply them, and `gclient` runs hooks only once the whole sync has
finished, so the ordering works out. Re-running them is harmless.

To regenerate the solution without syncing:

```bat
python3 tools\gyp_sawbuck.py
```

GYP cannot reliably detect Visual Studio 2017 or newer on its own — its probe
reads a registry key those versions no longer write — so set
`GYP_MSVS_VERSION=2022` if generation complains. `build.bat` does this for you
from `vswhere`.

#### Tests

Building the solution also builds the `run_unittests` project, which runs the
unit tests as a build step, so a failing test fails the build. To build without
running them:

```bat
msbuild sawbuck\sawbuck.sln /t:build_all /p:Configuration=Debug /p:Platform=Win32
```

The tests can also be run directly — each is a plain GoogleTest executable in
the output directory:

```bat
out\Debug_x86\common_unittests.exe
out\Debug_x86\log_lib_unittests.exe
out\Debug_x86\sym_util_unittests.exe
out\Debug_x86\viewer_unittests.exe
```

Installing [Application Verifier](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/application-verifier)
is optional. `run_unittests.py` uses it when present and prints a notice when
not.
