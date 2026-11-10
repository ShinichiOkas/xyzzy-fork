@echo off
setlocal
cd /d %~dp0

echo Byte compile...
rem startup.lc だけ「存在するファイルを作成することは出来ません」と言われて
rem たまにエラーになるので削除
del /S /Q lisp\startup.lc >nul 2>&1

rem Stale compiled files (lisp\*.lc) must go first: a .lc of a removed definition breaks the load.
del /S /Q lisp\*.lc samples\*.lc >nul 2>&1
del /S /Q xyzzy.wxp >nul 2>&1
set XYZZYHOME=%~dp0
set XYZZYINIFILE=
rem The build uses a fresh temporary config folder: the user's settings are not touched.
set BC_CONFIG=%TEMP%\xyzzy-bc-%RANDOM%%RANDOM%
mkdir "%BC_CONFIG%"
type nul > "%BC_CONFIG%\xyzzy.ini"
set XYZZYCONFIGPATH=%BC_CONFIG%
"%~dp0xyzzy.exe" -q -trace -load misc/makelc.l -e "(makelc:makelc-and-exit t)"
set BC_RESULT=%ERRORLEVEL%
rd /s /q "%BC_CONFIG%"
if not %BC_RESULT%==0 goto error
del /S /Q xyzzy.wxp 2> nul
goto :eof

:error
echo Byte compile error!
exit /b 1
