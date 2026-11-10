@echo off
setlocal
REM Work in the folder this script lives in, not the caller's current folder.
cd /d "%~dp0"
echo === xyzzy install ===

echo [1/3] Building xyzzy (Release, x64)...
call "%~dp0build.bat" Release Build minimal
if errorlevel 1 (
  echo   [ERROR] build failed. See the messages above.
  exit /b 1
)

echo [2/3] Compiling the Lisp sources...
call "%~dp0bytecompile.bat"
if errorlevel 1 (
  echo   [ERROR] byte compile failed. See the messages above.
  exit /b 1
)

echo [3/3] Checking the built executable...
if not exist xyzzy.exe (
  echo   [ERROR] xyzzy.exe was not built.
  exit /b 1
)
findstr /c:"PerMonitorV2" xyzzy.exe >nul
if errorlevel 1 (
  echo   [ERROR] the application manifest is missing from xyzzy.exe.
  exit /b 1
)
echo   OK: %CD%\xyzzy.exe
exit /b 0
