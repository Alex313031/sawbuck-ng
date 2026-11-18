## Building Sawbuck

Sawbuck is built on Windows with Visual Studio and [GYP](https://github.com/nodejs/gyp-next).
There is no cross-compile: it is an ATL/WTL application that links against the
Windows SDK and ETW.

### Requirements

 - **Windows 10 or 11.** Building on the OS you intend to run on is not
   required, and there is nothing to choose: the default build runs on XP
   through 11 from one binary. See [Windows XP](#windows-xp) for how, and for
   how to build without that.
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
`sawbuck\sawbuck.sln`, and then builds **Release for x86 and x64**.

Release only, because that is what gets shipped and what anyone reaching for
the script wants; a Debug build doubles the time for output most runs never
open. Add `--debug` for all four.

The same script is what [CI](./.github/workflows/build_gyp.yml) runs, rather
than a reimplementation of it in YAML, so a green run there and a failure here
means the difference is the machine. It is manual-only — this syncs Chromium's
`base`, ICU and the rest before building anything — and it uploads the two
installers.

Useful options:

| Option | Effect |
|---|---|
| `build.bat --debug` | Build the Debug flavours as well, for all four in total. |
| `build.bat --no-sync` | Skip the `gclient` steps and just rebuild. Fails if the solution has not been generated yet. |
| `build.bat --no-winwrap` | Generate without winwrap, giving up [Windows XP](#windows-xp) support. Needs generation, so it does nothing with `--no-sync`. |
| `build.bat --jobs N` | Pass `/m:N` to MSBuild instead of bare `/m`, which uses every core. |
| `build.bat --help` | Show usage. |

Output lands in:

```
out\Release_x86\     Sawbuck.exe   Sawbuck_x86.msi
out\Release_x64\     Sawbuck.exe   Sawbuck_x64.msi
out\Debug_x86\       --debug only
out\Debug_x64\       --debug only
```

The installers carry their architecture so the two cannot collide once
collected in one place. That rename is `build.bat`'s doing, not the build's:
one generated project holds all four configurations, so nothing GYP
substitutes into the WiX command line can differ between them, and MSBuild has
no macro that spells the architecture as `x86` — `$(Platform)` says `Win32`.
Driving `msbuild` yourself therefore produces a plain `Sawbuck.msi`.

#### Doing it by hand

If you would rather drive the steps yourself:

```bat
:: 1. Place the gclient solution file one level above the repository root.
::    DEPS sets use_relative_paths and the solution entry names this
::    directory, so it belongs in the parent, not here.
copy gclient ..\.gclient

:: 2. Sync. The hooks that run afterwards apply patches/ to the synced
::    dependencies and then generate the solution, with Windows XP support,
::    which the generator defaults to.
gclient sync --force --reset --delete_unversioned_trees

:: 3. Only for a Vista-and-newer build: regenerate without winwrap. Setting
::    GYP_DEFINES here would not work -- it does not reach the hook that did
::    the generating, which is the whole reason the default moved into the
::    generator.
python3 tools\gyp_sawbuck.py --no-winwrap

:: 4. Build one flavour.
msbuild sawbuck\sawbuck.sln /m /p:Configuration=Release /p:Platform=Win32
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
msbuild sawbuck\sawbuck.sln /t:build_all /p:Configuration=Release /p:Platform=Win32
```

The tests can also be run directly — each is a plain GoogleTest executable in
the output directory. `Release_x86` below because that is what a default
`build.bat` produces; substitute `Debug_x86` if you built with `--debug`:

```bat
out\Release_x86\common_unittests.exe
out\Release_x86\log_lib_unittests.exe
out\Release_x86\sym_util_unittests.exe
out\Release_x86\viewer_unittests.exe
```

`sawbuck\run_all_tests.bat` runs them for every flavour, and builds each one
first. Worth knowing after a default `build.bat`: the Debug configurations will
not exist, so two of its four flavours build the tree from scratch before they
can run. `--config Release` keeps it to what you already built.

Installing [Application Verifier](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/application-verifier)
is optional. `run_unittests.py` uses it when present and prints a notice when
not.

### Windows XP

**Nothing to do: `build.bat` turns XP support on.** It produces a binary that
runs on XP through 11, and there is no separate XP build to make.

The switch is `sawbuck_use_winwrap`. `sawbuck/sawbuck.gypi` declares it `0`,
matching how upstream winwrap declares `use_winwrap`, and
`tools/gyp_sawbuck.py` turns it on. The default lives in the generator because
that is the one place every route into a generated solution passes through —
`build.bat`, a bare `python3 tools/gyp_sawbuck.py`, a plain `gclient runhooks`
— and generation now prints which way it went:

```
Windows XP support: on (this generator's default)
```

It was previously `build.bat` exporting `GYP_DEFINES=sawbuck_use_winwrap=1`,
which did not survive into the `gclient` hook that runs the generator, so
`build.bat` silently produced builds without XP support while generating by
hand worked. Worth knowing because of how that presents: the binary builds,
runs and passes its tests, and only fails at load on XP. See
[Checking the result](#checking-the-result).

It does two things that only work together:

 - Links [winwrap](https://github.com/Alex313031/winwrap), which supplies the
   Win32 functions XP does not export. A statically imported symbol the OS
   lacks is fatal at load, before any program code runs, so it cannot be
   handled at the call site. winwrap captures the calls by defining the
   `__imp_<name>` slots they load through, then asks the running Windows for
   the real function first and only falls back to its own where there is
   none — so the same binary uses the system's code on Windows 7 and the
   emulation on XP.
 - Sets the linker's minimum subsystem version: 5.01 on Win32 and 5.02 on x64,
   which is as low as the linker goes for that machine. Without this the loader
   rejects the binary outright, whatever its imports.

Note that `_WIN32_WINNT` does **not** move. Sawbuck keeps building against the
current SDK; winwrap lowers it to `0x0501` for its own sources only, as a
guardrail so it cannot accidentally use something newer than the OS it has to
run on.

The installer needs nothing either, so `Sawbuck_x86.msi` installs on XP as it
does anywhere else and there is no `.zip` to produce. Two things in
`sawbuck.wxs` make that so, and both are worth knowing before changing either:

 - `InstallerVersion='200'` — Windows Installer 2.0, which is the version XP
   ships with. Raising it for some newer WiX feature would drop XP support in
   the least obvious way, with the binary still running there perfectly well
   and only the installer refusing.
 - A launch condition of `VersionNT >= 501` on the 32-bit package and
   `VersionNT64 >= 502` on the 64-bit one, matching the subsystem versions
   above and for the same reasons. These are a floor rather than a gate: they
   turn Windows 2000 and older into a clear message instead of a failure
   further in, and `VersionNT64` is unset on 32-bit Windows, so the x64
   package declines it by the same route.

#### Building without it

```bat
build.bat --no-winwrap
```

The result needs Vista or newer. Worth doing to isolate a fault — if something
misbehaves and winwrap is a suspect, this is the comparison — and it is also
the configuration upstream Sawbuck shipped.

`build.bat --no-winwrap` works by regenerating once its sync has finished, with
`gyp_sawbuck.py --no-winwrap` — the hook takes no arguments, so declining the
default means saying so on a command line the script controls. Generation is
seconds; the sync before it is what takes the time. It only reaches the build
through generation, so it does nothing alongside `--no-sync`, and the script
says so rather than ignoring it.

#### Generating by hand

```bat
python3 tools\gyp_sawbuck.py
```

Nothing extra to pass: the generator applies the winwrap default itself, so
this is equivalent to what `build.bat` generates, and it says so as it runs.
Decline it with `--no-winwrap`.

This used to be the *only* way to get XP support, because the default lived in
`build.bat` as an exported `GYP_DEFINES` that never reached the hook doing the
generating. The resulting binary built, ran and passed its tests on the machine
that built it, and failed at load on XP — the one place nobody was looking.
Which is the argument for [Checking the result](#checking-the-result) rather
than for trusting any of this.

#### Additional requirements

None, as of winwrap 0.3.4. Nothing extra has to be installed: winwrap builds
with plain MSVC, from the same C++ workload as the rest of the tree. Earlier
revisions needed the **C++ Clang Compiler for Windows** component, because
winwrap's redirect table names symbols with Clang asm labels and MSVC has no
equivalent; it now assembles that table with MASM instead, which ships with
Visual Studio.

What it does need is `patches/06-gyp-next-masm-settings.patch`, and the
`gclient sync` hook applies that for you. It is noted here only because the
failure is misleading if the patch is ever missed: gyp-next's MASM settings
table does not list `PreprocessorDefinitions`, and `ConvertToMSBuildSettings`
drops any setting its table does not know after printing a warning. The symptom
is four `unrecognized setting MASM/PreprocessorDefinitions` warnings at
generation time, then `LNK2001` on `_winwrap_static_redirects` at link, because
the define selects which of winwrap's two `.asm` files has a body.

#### What it costs

Little, but not nothing, and worth knowing before assuming the default suits
every purpose:

 - **A cached pointer load and a jump** on every call winwrap covers. Nothing
   is replaced on a modern Windows: each shim asks the running OS for the real
   function first and falls back only where there is none, so Windows 7 and
   later run the system's own code throughout.
 - **On Vista, the twelve `SRWLOCK` and `CONDITION_VARIABLE` functions become
   winwrap's.** They resolve as a group or not at all — mixing them is memory
   corruption rather than a slower lock, since each implementation reads that
   one pointer-sized word differently — and the group is only complete from
   Windows 7, because Vista exports no `TryAcquireSRWLock*` and has no
   `RtlTryAcquireSRWLock*` under it either. winwrap's is a real reader/writer
   lock with concurrent readers and writer preference, not a
   `CRITICAL_SECTION` wearing the name, but it is not the kernel's, so
   contended performance differs.
 - **A few KB**, from the redirect table and the shims the `/INCLUDE:` anchor
   pulls in behind it.

Upstream's `winwrap.gni` declares `use_winwrap = false`, which is the opposite
default and not a disagreement: that one governs Chromium and Electron, where
the question is whether to apply the redirects across several hundred binaries.
This governs one small log viewer whose whole purpose is reading logs off
whatever machine produced them.

#### Checking the result

The quickest verification does not need an XP machine, and is worth doing every
time: linking winwrap and *using* it are different things, and when the
difference goes wrong nothing says so — the binary links, it runs here, and it
fails at load on XP. winwrap ships the check:

```bat
python3 third_party\winwrap\tools\check_imports.py --min-os 5.01 ^
    out\Release_x86\Sawbuck.exe
```

It reads the import table and the subsystem version out of the PE and reports on
both halves of the recipe. Use `--min-os 5.02` for x64. The covered-function
list comes from winwrap's own sources, so it cannot fall behind the library the
way a hand-written `findstr` pattern does:

```bat
dumpbin /imports out\Release_x86\Sawbuck.exe > imports.txt
findstr /i "Fls InitializeCriticalSectionEx SRWLock ConditionVariable GetTickCount64 IsThreadAFiber LCMapStringEx GetLocaleInfoEx CompareStringEx EventWriteTransfer" imports.txt
```

A function named by either is one that still needs a winwrap shim, or a sign
that winwrap is not winning the link.

**What a clean result does not mean.** `check_imports.py` reads the list of
covered functions out of winwrap's sources and checks only those, so it cannot
see an import of some post-XP function winwrap has no shim for — there is
nothing to compare it against. "All clear" says winwrap won the link and the
subsystem floor is right, which is necessary to run on XP and not sufficient.
If a binary passes this and still will not start there, `--show-all` lists
every import and marks the covered ones; the uncovered post-XP name will be in
that list.

The symbol and debugging DLLs follow
automatically: `third_party/debugging_tools` defaults to its `xp` tier
(dbghelp 6.12.2.633), and `copy_dlls` places the pair matching the
configuration beside the executable.
