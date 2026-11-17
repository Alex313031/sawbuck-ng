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
REM Builds and runs every Sawbuck unit test.
REM
REM   run_all_tests.bat                     all four flavours
REM   run_all_tests.bat --config Debug      only the Debug ones
REM   run_all_tests.bat --platform x64      only the 64-bit ones
REM   run_all_tests.bat --help              show usage
REM
REM "Builds and runs" is not loose wording. The run_unittests project runs the
REM tests as a build step, so building it is what runs them, and a failing test
REM fails the build.
REM
REM Three things about how it does that.
REM
REM It builds sawbuck\run_unittests.vcxproj directly rather than passing
REM /t:run_unittests to sawbuck.sln. A solution-level /t: wants the solution's
REM own mangled target name and answers MSB4057, "the target does not exist in
REM the project", for the plain one. Building the project is also enough on its
REM own: MSBuild builds its project references first, so the four test
REM executables get built on the way in.
REM
REM The stamp file has to be deleted first. run_unittests declares
REM out\<flavour>\unittest_success.txt as its output, so when that file already
REM exists MSBuild skips the whole custom build step as up to date -- and then
REM reports success without having run a single test. Deleting it is what makes
REM "run the tests again" mean anything.
REM
REM Each flavour is a 'call' to :run_one rather than a for loop, so one failure
REM does not stop the rest. See the longer note in build.bat for why appending
REM to FAILED works from a called routine but would not from inside a
REM parenthesised for block.
REM
REM This replaces a chain that had stopped working in four separate ways. The
REM old batch file ran tools/run_all_tests.py through
REM third_party/python_26/python.exe, which DEPS no longer fetches; that script
REM imports tools/build_project.py, which is a SyntaxError under Python 3
REM (Python 2's 'raise Error, value, traceback'), also needs pywin32, and drove
REM the Visual Studio IDE over COM automation to do the build. It also used
REM %~p0, which is the script's path without its drive letter, so it broke if
REM invoked from a different drive. None of that is worth porting when MSBuild
REM does the job directly.

setlocal

for %%I in ("%~dp0..") do set "REPO=%%~fI"
set "PROJ=%~dp0run_unittests.vcxproj"

set "ONLY_CFG="
set "ONLY_PLAT="

REM Options may appear in any order. 'shift' takes effect from the next line
REM on, which is why each branch jumps back to re-read %1.
:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="--config" set "ONLY_CFG=%~2" & shift & shift & goto :parse_args
if /i "%~1"=="--platform" set "ONLY_PLAT=%~2" & shift & shift & goto :parse_args
if /i "%~1"=="--help" goto :usage
if /i "%~1"=="-h" goto :usage
if "%~1"=="/?" goto :usage
echo ERROR: unknown option: %~1
echo.
goto :usage
:args_done

if not defined ONLY_CFG goto :cfg_ok
if /i "%ONLY_CFG%"=="Debug" goto :cfg_ok
if /i "%ONLY_CFG%"=="Release" goto :cfg_ok
echo ERROR: --config takes Debug or Release, got "%ONLY_CFG%".
echo.
goto :usage
:cfg_ok

if not defined ONLY_PLAT goto :plat_ok
if /i "%ONLY_PLAT%"=="Win32" goto :plat_ok
if /i "%ONLY_PLAT%"=="x64" goto :plat_ok
echo ERROR: --platform takes Win32 or x64, got "%ONLY_PLAT%".
echo.
goto :usage
:plat_ok

if not exist "%PROJ%" (
  echo ERROR: %PROJ% does not exist.
  echo        Generate the build first:
  echo            python3 tools\gyp_sawbuck.py
  goto :fail
)

REM Located through vswhere so this works from a plain cmd prompt and not only
REM from a Developer Command Prompt, the same way build.bat does it.
REM
REM %ProgramFiles(x86)% has to be expanded outside a parenthesised block,
REM because the closing paren in "(x86)" would end the block early.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

set "MSBUILD="
if exist "%VSWHERE%" (
  REM -find can match both Bin\MSBuild.exe and Bin\amd64\MSBuild.exe; keep
  REM the first, which is the 32-bit one.
  for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
    if not defined MSBUILD set "MSBUILD=%%I"
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

echo ==========================================================
echo  Sawbuck-ng unit tests
echo    repo %REPO%
echo ==========================================================

set "FAILED="
set "RAN="
call :run_one Debug   Win32
call :run_one Release Win32
call :run_one Debug   x64
call :run_one Release x64

echo.
echo ==========================================================
if not defined RAN (
  echo  Nothing matched --config / --platform.
  echo ==========================================================
  endlocal
  exit /b 1
)
if defined FAILED (
  echo  FAILED:%FAILED%
  echo.
  echo  A flavour fails here if any test in it failed, or if it could not be
  echo  built. Scroll up for the gtest output, which names the test.
  echo ==========================================================
  endlocal
  exit /b 1
)
echo  All tests passed.
echo ==========================================================
endlocal
exit /b 0

REM Runs one flavour. %1 is the configuration, %2 the platform. Always returns
REM 0 so the caller keeps going; the outcome is reported through FAILED, which
REM is shared because 'call :label' does not start a new scope.
:run_one
set "CFG=%~1"
set "PLAT=%~2"
if defined ONLY_CFG if /i not "%ONLY_CFG%"=="%CFG%" exit /b 0
if defined ONLY_PLAT if /i not "%ONLY_PLAT%"=="%PLAT%" exit /b 0
set "RAN=1"

REM sawbuck.gypi names the output directories out\<Config>_<arch>, where arch
REM is x86 or x64 -- not the MSBuild platform name, so Win32 has to be mapped.
set "ARCH=x86"
if /i "%PLAT%"=="x64" set "ARCH=x64"
set "STAMP=%REPO%\out\%CFG%_%ARCH%\unittest_success.txt"

echo.
echo ---- %CFG% %PLAT% ------------------------------------------------
if exist "%STAMP%" (
  del /q "%STAMP%"
  echo      cleared the previous success stamp, so the tests actually re-run
)

"%MSBUILD%" "%PROJ%" /nologo /m /v:minimal /clp:Summary /p:Configuration=%CFG% /p:Platform=%PLAT%
if errorlevel 1 (
  echo ---- %CFG% %PLAT%: FAILED, continuing with the next flavour
  set "FAILED=%FAILED% %CFG%/%PLAT%"
) else (
  echo ---- %CFG% %PLAT%: passed
)
exit /b 0

:usage
echo Usage: run_all_tests.bat [--config Debug^|Release] [--platform Win32^|x64]
echo.
echo   --config C     Only this configuration. Default is both.
echo   --platform P   Only this platform. Default is both.
echo.
echo Builds sawbuck\run_unittests.vcxproj, which runs the tests as a build
echo step. Requires the build to have been generated already.
endlocal
exit /b 1

:fail
echo.
echo TESTS DID NOT RUN.
endlocal
exit /b 1
