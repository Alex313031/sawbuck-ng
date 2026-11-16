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
REM   build.bat                  sync, generate, then build all four flavours
REM   build.bat --no-sync        skip the gclient steps and just rebuild
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
REM      tools/gyp_sawbuck.py to generate sawbuck/sawbuck.sln.
REM   3. msbuild for Debug|Win32, Release|Win32, Debug|x64, Release|x64.
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
set "JOBS="

REM Options may appear in any order. 'shift' takes effect from the next line
REM on, which is why each branch jumps back to re-read %1.
:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="--no-sync" set "DO_SYNC=0" & shift & goto :parse_args
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
  echo [0/3] WARNING: could not determine the Visual Studio version. If
  echo       generation fails, set GYP_MSVS_VERSION yourself, e.g. 2022.
)

if "%DO_SYNC%"=="0" (
  echo [1-2/3] Skipping sync and generation ^(--no-sync^).
  goto :build
)

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
set "FAILED="
call :build_one Debug   Win32
call :build_one Release Win32
call :build_one Debug   x64
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
echo  All four flavours built. Output is under:
echo    %REPO%\out\Debug_x86
echo    %REPO%\out\Release_x86
echo    %REPO%\out\Debug_x64
echo    %REPO%\out\Release_x64
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
echo.
echo ---- %CFG% %PLAT% ------------------------------------------------
"%MSBUILD%" "%SLN%" /nologo %MFLAG% /v:minimal /clp:Summary /p:Configuration=%CFG% /p:Platform=%PLAT%
if errorlevel 1 (
  echo ---- %CFG% %PLAT%: FAILED, continuing with the next flavour
  set "FAILED=%FAILED% %CFG%/%PLAT%"
) else (
  echo ---- %CFG% %PLAT%: ok
)
exit /b 0

:usage
echo Usage: build.bat [--no-sync] [--jobs N]
echo.
echo   --no-sync   Skip gclient sync and runhooks; build the solution as it
echo               stands. Fails if it has not been generated yet.
echo   --jobs N    Pass /m:N to MSBuild instead of bare /m. Default is /m,
echo               which uses every core.
endlocal
exit /b 1

:fail
echo.
echo BUILD FAILED.
popd
endlocal
exit /b 1
