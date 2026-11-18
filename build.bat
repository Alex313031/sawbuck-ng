REM Copyright 2026 Alex313031

@echo off

REM Licensed under the Apache License, Version 2.0 (the "License");
REM you may not use this file except in compliance with the License.
REM You may obtain a copy of the License at
REM
REM     http://www.apache.org/licenses/LICENSE-2.0
REM
REM Unless required by applicable law or agreed to in writing, software
REM distributed under the License is distributed on an "AS IS" BASIS,
REM WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
REM See the License for the specific language governing permissions and
REM limitations under the License.
REM
REM Syncs dependencies and builds every flavour of Sawbuck in one go.
REM
REM   build.bat                  sync, generate, then build Release x86 + x64
REM   build.bat --debug          build the Debug flavours as well
REM   build.bat --no-sync        skip the gclient steps and just rebuild
REM   build.bat --no-winwrap     build without Windows XP support
REM   build.bat --jobs 32        cap MSBuild parallelism (default: all cores)
REM   build.bat --help           show usage
REM
REM Steps:
REM   0. Find Visual Studio with vswhere, so this works from a plain cmd
REM      prompt and not only a Developer Command Prompt, and hand its version
REM      to the generator through GYP_MSVS_VERSION.
REM   1. Copy this repo's sample 'gclient' to '.gclient' in the parent
REM      directory if there is not one there already, which is where
REM      gclient expects it: DEPS sets
REM      use_relative_paths, and the solution entry names this directory, so
REM      the file belongs one level above the repository root.
REM   2. gclient sync --force --reset --delete_unversioned_trees, whose hooks
REM      apply patches/ to the synced dependencies and then run
REM      tools/gyp_sawbuck.py to generate sawbuck/sawbuck.sln -- with
REM      GYP_DEFINES=sawbuck_use_winwrap=1 unless --no-winwrap was given.
REM   3. msbuild for Release|Win32 and Release|x64, plus Debug|Win32 and
REM      Debug|x64 if --debug was given. Release only by default because that
REM      is what gets shipped and what anyone reaching for this script wants;
REM      a Debug build doubles the time for output most runs never open.
REM      Each flavour's installer is then renamed to carry its architecture,
REM      so the four cannot collide once collected in one place.
REM
REM Three notes on that sync. --reset reverts the patches that patches/
REM applies to build/common.gypi, so they have to be re-applied afterwards --
REM which is what the hooks do: gclient runs them once the whole sync has
REM finished, so apply_patches.py always sees a freshly reset tree and lands
REM after the revert, not before it. This used to be --nohooks plus a
REM separate 'gclient runhooks' to force that ordering; the ordering was
REM already guaranteed, so the extra step was doing nothing. apply_patches.py
REM is a no-op when the patches are already there, so a re-run is harmless
REM either way.
REM
REM --force --reset is safe for your own work because the .gclient solution is
REM "managed": False -- gclient syncs and resets the dependency checkouts but
REM never touches the sawbuck-ng checkout itself.
REM
REM One cost of folding the two together: a hook failure now surfaces as a
REM failure of this step rather than a step of its own, so read the tail of
REM the gclient output to tell a sync problem from a patch or generation one.
REM
REM Building the solution also builds the run_unittests project, which runs
REM the unit tests as a build step, so a test failure fails the build. To
REM build without running them, add /t:build_all to the msbuild line below.

setlocal

set "REPO=%~dp0"
if "%REPO:~-1%"=="\" set "REPO=%REPO:~0,-1%"
for %%I in ("%REPO%\..") do set "PARENT=%%~fI"
set "SLN=%REPO%\sawbuck\sawbuck.sln"

set "DO_SYNC=1"
set "DO_WINWRAP=1"
set "DO_DEBUG=0"
set "JOBS="

REM Options may appear in any order. 'shift' takes effect from the next line
REM on, which is why each branch jumps back to re-read %1.
:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="--no-sync" set "DO_SYNC=0" & shift & goto :parse_args
if /i "%~1"=="--no-winwrap" set "DO_WINWRAP=0" & shift & goto :parse_args
if /i "%~1"=="--debug" set "DO_DEBUG=1" & shift & goto :parse_args
if /i "%~1"=="--jobs" set "JOBS=%~2" & shift & shift & goto :parse_args
if /i "%~1"=="--help" goto :usage
if /i "%~1"=="-h" goto :usage
if "%~1"=="/?" goto :usage
echo ERROR: unknown option: %~1
echo.
goto :usage
:args_done

if not defined JOBS goto :jobs_ok
echo %JOBS%| findstr /r /c:"^[1-9][0-9]*$" >nul
if errorlevel 1 (
  echo ERROR: --jobs takes a positive integer, got "%JOBS%".
  echo.
  goto :usage
)
:jobs_ok

REM Bare /m tells MSBuild to use every core. A number caps it, which is worth
REM doing on a machine with a very high core count: each node is a separate
REM MSBuild process, and past some point the disk rather than the CPU is the
REM limit. Note this is unrelated to the GYP parallelism we had to disable --
REM that was Python's multiprocessing hitting the 64-handle ceiling in
REM WaitForMultipleObjects, which MSBuild does not share.
set "MFLAG=/m"
if defined JOBS set "MFLAG=/m:%JOBS%"

REM Run from the repository root no matter where this was invoked from.
REM gclient locates .gclient by walking up from the current directory, so it
REM has to start inside the tree or it will not find the file we just wrote.
pushd "%REPO%" || exit /b 1

echo ==========================================================
echo  Sawbuck-ng build
echo    repo   %REPO%
echo    parent %PARENT%
echo ==========================================================

REM ------------------------------------------------------- 0. Visual Studio
REM Located through vswhere so this works from a plain cmd prompt and not
REM only from a Developer Command Prompt. Done before the sync because the
REM sync's hooks need it too: GYP cannot reliably detect VS2017 or newer on its
REM own -- its probe reads a registry key those versions no longer write --
REM so tools/gyp_sawbuck.py wants the version named explicitly. Setting it
REM here is contained by this script's setlocal and does not leak out.
REM
REM %ProgramFiles(x86)% has to be expanded outside a parenthesised block,
REM because the closing paren in "(x86)" would end the block early.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

set "MSBUILD="
set "VS_YEAR="
if exist "%VSWHERE%" (
  REM -find can match both Bin\MSBuild.exe and Bin\amd64\MSBuild.exe; keep
  REM the first, which is the 32-bit one.
  for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
    if not defined MSBUILD set "MSBUILD=%%I"
  )
  for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -prerelease -products * -property catalog_productLineVersion`) do (
    if not defined VS_YEAR set "VS_YEAR=%%I"
  )
)
if not defined MSBUILD (
  where msbuild >nul 2>&1
  if not errorlevel 1 set "MSBUILD=msbuild"
)
if not defined MSBUILD (
  echo ERROR: MSBuild not found. Install the "Desktop development with C++"
  echo        workload for Visual Studio, or run this from a Developer
  echo        Command Prompt so msbuild is on PATH.
  goto :fail
)
if defined GYP_MSVS_VERSION (
  echo [0/3] Using GYP_MSVS_VERSION=%GYP_MSVS_VERSION% from the environment.
) else if defined VS_YEAR (
  set "GYP_MSVS_VERSION=%VS_YEAR%"
  echo [0/3] Visual Studio %VS_YEAR%, GYP_MSVS_VERSION set from vswhere.
) else (
  REM Nothing in the environment and nothing from vswhere, so fall back rather
  REM than leave it unset. GYP cannot detect VS2017 or newer on its own -- its
  REM probe reads a registry key those versions no longer write -- so an unset
  REM GYP_MSVS_VERSION means generation fails outright. 2022 is the only
  REM version this tree is known to build with, so guessing it is strictly
  REM better than guessing nothing.
  REM
  REM Loudly, though: MSBuild was found, so something is installed, and
  REM vswhere not naming it means the guess may be wrong. If it is, that shows
  REM up as a generation failure mentioning the toolset, and the fix is to set
  REM GYP_MSVS_VERSION in the environment.
  set "GYP_MSVS_VERSION=2022"
  echo [0/3] WARNING: could not determine the Visual Studio version from
  echo       vswhere, so GYP_MSVS_VERSION has been set to 2022. If that is
  echo       wrong, set GYP_MSVS_VERSION yourself before running this.
)

REM Before the winwrap block below, and not after it: with --no-sync nothing
REM is regenerated, so winwrap is whichever way the existing solution was
REM generated and this script has no say in it. Announcing a default it is not
REM applying would be worse than saying nothing.
if "%DO_SYNC%"=="0" (
  echo [1-2/3] Skipping sync and generation ^(--no-sync^).
  echo         Windows XP support is whatever the solution was generated with;
  echo         changing it means regenerating. See BUILDING.md.
  goto :build
)

REM Windows XP support, which is this script's default rather than the GYP
REM file's: sawbuck.gypi declares sawbuck_use_winwrap as 0, matching how
REM upstream winwrap declares use_winwrap, and this turns it on so that the
REM scripted build -- the one releases come from -- covers XP through 11 from
REM one binary. See sawbuck/sawbuck.gypi for what that costs.
REM
REM Through GYP_DEFINES rather than a -D argument, because this script does not
REM run the generator: tools/gyp_sawbuck.py is a gclient hook, invoked by the
REM sync below, so there is no command line here to add to. GYP reads
REM GYP_DEFINES itself and merges it ahead of any -D, so a -D on a hand
REM generation still overrides this.
REM
REM Three-way precedence, strongest first: --no-winwrap, then the variable if
REM GYP_DEFINES already names it, then this script's default of on. Appending
REM is what makes the first of those work -- GYP splits GYP_DEFINES into a
REM list and the last setting of a name is the one it keeps -- so
REM --no-winwrap genuinely overrides an ambient sawbuck_use_winwrap=1 rather
REM than being quietly ignored by it.
REM
REM Written with labels rather than an if/else chain because every branch here
REM reads or writes GYP_DEFINES, and a parenthesised block expands %VAR% when
REM the block is parsed rather than when it runs.
if "%DO_WINWRAP%"=="0" (
  set "WINWRAP_DEFINE=sawbuck_use_winwrap=0"
  goto :winwrap_apply
)
if not defined GYP_DEFINES goto :winwrap_on
echo "%GYP_DEFINES%"| findstr /i /c:"sawbuck_use_winwrap" >nul
if errorlevel 1 goto :winwrap_on
echo [0/3] Windows XP support left to sawbuck_use_winwrap in your GYP_DEFINES.
goto :winwrap_done

:winwrap_on
set "WINWRAP_DEFINE=sawbuck_use_winwrap=1"

:winwrap_apply
if defined GYP_DEFINES (
  set "GYP_DEFINES=%GYP_DEFINES% %WINWRAP_DEFINE%"
) else (
  set "GYP_DEFINES=%WINWRAP_DEFINE%"
)
if "%DO_WINWRAP%"=="0" (
  echo [0/3] Windows XP support off ^(--no-winwrap^).
) else (
  echo [0/3] Windows XP support on ^(sawbuck_use_winwrap=1^).
)

:winwrap_done

REM ---------------------------------------------------------------- 1. gclient
where gclient >nul 2>&1
if errorlevel 1 (
  echo ERROR: gclient is not on PATH. Install depot_tools and add it to PATH:
  echo        https://chromium.googlesource.com/chromium/tools/depot_tools.git
  goto :fail
)

echo.
echo [1/3] Installing .gclient into %PARENT%
if not exist "%REPO%\gclient" (
  echo ERROR: %REPO%\gclient is missing. It is the sample solution file this
  echo        script installs; restore it from the repository.
  goto :fail
)
REM An existing .gclient is left alone. The sample has empty
REM custom_deps/custom_vars and points at the canonical URL; anyone who
REM has filled those in, aimed it at their own fork, or set a cache_dir
REM would not want it replaced on every build. The errorlevel check is
REM inside the branch on purpose: out here it would be inspecting
REM whatever the preceding command happened to leave behind, not the copy.
if exist "%PARENT%\.gclient" (
  echo       Using the existing %PARENT%\.gclient
) else (
  copy /Y "%REPO%\gclient" "%PARENT%\.gclient" >nul
  if errorlevel 1 (
    echo ERROR: could not write %PARENT%\.gclient
    goto :fail
  )
  echo       Installed %PARENT%\.gclient from the sample gclient file
)

REM ------------------------------------------------------------------ 2. sync
REM 'call' matters: gclient is a batch file, and without it this script would
REM exit as soon as gclient returned.
echo.
echo [2/3] gclient sync ^(then its hooks: apply patches, generate the solution^)
call gclient sync --force --reset --delete_unversioned_trees
if errorlevel 1 (
  echo ERROR: gclient sync failed. If the sync itself succeeded, the failure
  echo        is in a hook -- either tools/apply_patches.py or
  echo        tools/gyp_sawbuck.py. Check the end of the output above.
  goto :fail
)

:build
REM ----------------------------------------------------------------- 3. build
if not exist "%SLN%" (
  echo ERROR: %SLN% does not exist.
  echo        Run without --no-sync, or generate it with:
  echo            python3 tools\gyp_sawbuck.py
  goto :fail
)

echo.
echo [3/3] Building with %MSBUILD%
REM Each flavour is a 'call' to the :build_one routine below rather than a
REM for loop, so that one failure does not stop the rest: a flavour that fails
REM records itself in FAILED and the next one still runs. The routine also
REM sidesteps the usual batch trap -- appending to a variable from inside a
REM parenthesised for block does not work, because the whole block is parsed
REM once and %FAILED% would expand to its value from before the loop started.
REM A called routine is re-parsed per invocation, so plain expansion is right
REM and delayed expansion is not needed.
REM
REM Release only unless --debug. The Debug calls are guarded one at a time
REM rather than wrapped in a block so that each stays a plain 'call', which is
REM what lets :build_one append to FAILED by ordinary expansion.
set "FAILED="
if "%DO_DEBUG%"=="1" call :build_one Debug   Win32
call :build_one Release Win32
if "%DO_DEBUG%"=="1" call :build_one Debug   x64
call :build_one Release x64

echo.
echo ==========================================================
if defined FAILED (
  echo  FAILED:%FAILED%
  echo.
  echo  Any flavour not listed above did build; its output is under
  echo  %REPO%\out. Re-run just the build with:  build.bat --no-sync
  echo ==========================================================
  popd
  endlocal
  exit /b 1
)
if "%DO_DEBUG%"=="1" (
  echo  All four flavours built. Output is under:
) else (
  echo  Both Release flavours built. Output is under:
)
if "%DO_DEBUG%"=="1" echo    %REPO%\out\Debug_x86
echo    %REPO%\out\Release_x86
if "%DO_DEBUG%"=="1" echo    %REPO%\out\Debug_x64
echo    %REPO%\out\Release_x64
if not "%DO_DEBUG%"=="1" echo  Add --debug for the Debug flavours.
echo ==========================================================
popd
endlocal
exit /b 0

REM Builds one flavour. %1 is the configuration, %2 the platform. Always
REM returns 0 so the caller keeps going; the outcome is reported through
REM FAILED, which is shared because 'call :label' does not start a new scope.
:build_one
set "CFG=%~1"
set "PLAT=%~2"
REM MSBuild's platform is Win32 or x64; the output directory and the installer
REM name both use x86 or x64. sawbuck/sawbuck.gypi sets OutputDirectory to
REM out\$(ConfigurationName)_x86 and _x64 on the per-architecture abstract
REM configurations, so this has to agree with it.
if /i "%PLAT%"=="Win32" (set "ARCH=x86") else (set "ARCH=%PLAT%")
echo.
echo ---- %CFG% %PLAT% ------------------------------------------------
"%MSBUILD%" "%SLN%" /nologo %MFLAG% /v:minimal /clp:Summary /p:Configuration=%CFG% /p:Platform=%PLAT%
if errorlevel 1 (
  echo ---- %CFG% %PLAT%: FAILED, continuing with the next flavour
  set "FAILED=%FAILED% %CFG%/%PLAT%"
  exit /b 0
)
call :name_msi "%REPO%\out\%CFG%_%ARCH%" "%ARCH%"
echo ---- %CFG% %PLAT%: ok
exit /b 0

REM Renames the flavour's installer to carry its architecture, so all four can
REM sit in one directory without colliding: Sawbuck_x86.msi, Sawbuck_x64.msi.
REM %1 is the output directory, %2 the architecture.
REM
REM Here rather than in sawbuck/installer/installer.gyp, which is where it
REM belongs and cannot go. One generated project holds all four
REM configurations, so nothing GYP substitutes into the light command line can
REM differ between them -- the same constraint that makes the installer pass
REM -dMsiPlatform=$(Platform) and makes copy_dlls a post-build step. MSBuild
REM has no macro spelling the architecture as x86 either: $(Platform) says
REM Win32. The remaining GYP-side option was a custom per-configuration
REM property through 'msbuild_settings', which is emitted verbatim -- but
REM setting that on a configuration makes GYP ignore msvs_settings for every
REM configuration inheriting it, which would discard common.gypi's entire
REM compiler and linker setup.
REM
REM One consequence, stated rather than hidden: the light action's declared
REM output is Sawbuck.msi, so moving it means MSBuild rebuilds the installer
REM on the next build even when nothing changed. That costs a few seconds at
REM the end of a flavour -- the cab holds the executable and the debugging
REM DLLs -- and it is why this moves rather than copies: two installers of the
REM same build, one of them anonymous, is worse than rebuilding one.
:name_msi
if exist "%~1\Sawbuck.msi" goto :name_msi_move

REM Nothing to rename, which has two meanings and they are not equally fine.
REM An already-renamed installer means MSBuild considered the light action up
REM to date despite its declared output being absent -- so that file is from an
REM earlier build. Nothing at all means the installer did not build, which the
REM msbuild output above will have said more about.
if exist "%~1\Sawbuck_%~2.msi" (
  echo        installer: Sawbuck_%~2.msi ^(not rebuilt this run^)
) else (
  echo        WARNING: no installer in %~1
)
exit /b 0

:name_msi_move
move /Y "%~1\Sawbuck.msi" "%~1\Sawbuck_%~2.msi" >nul
if errorlevel 1 (
  echo        WARNING: could not rename Sawbuck.msi to Sawbuck_%~2.msi in %~1
) else (
  echo        installer: Sawbuck_%~2.msi
)
exit /b 0

:usage
echo Usage: build.bat [--debug] [--no-sync] [--no-winwrap] [--jobs N]
echo.
echo Builds Release for x86 and x64. Each flavour's installer is named after
echo its architecture: Sawbuck_x86.msi, Sawbuck_x64.msi.
echo.
echo   --debug        Build the Debug flavours as well, for all four in total.
echo   --no-sync      Skip gclient sync and runhooks; build the solution as it
echo                  stands. Fails if it has not been generated yet.
echo   --no-winwrap   Generate without winwrap, giving up Windows XP support.
echo                  The default links it, so one binary runs on XP through
echo                  11. Needs generation, so it does nothing with --no-sync.
echo   --jobs N       Pass /m:N to MSBuild instead of bare /m. Default is /m,
echo                  which uses every core.
endlocal
exit /b 1

:fail
echo.
echo BUILD FAILED.
popd
endlocal
exit /b 1
