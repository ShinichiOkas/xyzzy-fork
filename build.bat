@echo off
setlocal
cd /d %~dp0

rem Locate MSBuild: vswhere (VS2017 or later) first, then PATH (Developer Prompt).
set MSBUILD=
set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
if exist "%VSWHERE%" (
  for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set MSBUILD=%%i
)
if not defined MSBUILD (
  for /f "delims=" %%i in ('where msbuild 2^>nul') do if not defined MSBUILD set MSBUILD=%%i
)
if not defined MSBUILD (
  echo MSBuild not found. Install Visual Studio 2022 Build Tools ^(C++ desktop workload^).
  exit /b 1
)

if "%1"=="" (set CONFIGURATION=Release) else (set CONFIGURATION=%1)
if "%2"=="" (set TARGET=Build) else (set TARGET=%2)
if "%3"=="" (set VERBOSITY=normal) else (set VERBOSITY=%3)

echo Build xyzzy...
"%MSBUILD%" xyzzy.sln /nologo /m /p:Configuration=%CONFIGURATION% /p:Platform=Win32 /target:%TARGET% /verbosity:%VERBOSITY% %4 %5 %6 %7 %8 %9 || exit /b 1
