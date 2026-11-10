@echo off
setlocal
cd /d %~dp0

rem Usage: run-tests.bat [TESTS...]
rem   run-tests.bat                          all tests
rem   run-tests.bat unittest/lisp-tests.l    the given test files only
rem
rem The tests never touch the user's settings. Every run uses a fresh temporary
rem config folder through XYZZYCONFIGPATH; the child xyzzy processes that the
rem tests start inherit it. XYZZYINIFILE is removed here, not set to an empty
rem string, because xyzzy takes an empty string as a setting.
rem The report is printed to standard output.
rem Exit code: 0 = all tests passed, 1 = some failed.

set TESTDIR=%~dp0unittest
set XYZZYHOME=%TESTDIR%\..
set TESTROOT=%TEMP%\xyzzy-test-%RANDOM%%RANDOM%
set XYZZYCONFIGPATH=%TESTROOT%\config
set XYZZYINIFILE=
set XYZZY_TEST_LOG=%TESTROOT%\results.txt

mkdir "%XYZZYCONFIGPATH%"
if errorlevel 1 (
  echo run-tests.bat: cannot create "%XYZZYCONFIGPATH%"
  exit /b 1
)
type nul > "%XYZZYCONFIGPATH%\xyzzy.ini"

"%XYZZYHOME%\xyzzy.exe" -q -trace -l "%TESTDIR%\run-tests-helper.l" %*
set RESULT=%ERRORLEVEL%

if exist "%XYZZY_TEST_LOG%" type "%XYZZY_TEST_LOG%"
rd /s /q "%TESTROOT%"
exit /b %RESULT%