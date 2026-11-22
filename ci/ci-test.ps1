# CI entry point: install.bat -> start once to write the dump image -> run-tests.bat.
# Run from anywhere; works in the repository root. Exit code 0 = all tests passed.
# Logs go to ci-out/ (uploaded as an artifact by the workflow).
#
# -Tests <file>...  run only these test files (default: all). For a check on
#                   this PC before pushing, run the whole script on a separate
#                   desktop so no window reaches the user's screen:
#   & <deskrun.exe> 2400 'C:\Program Files\PowerShell\7\pwsh.exe' -NoProfile -ExecutionPolicy Bypass -File <tree>\ci\ci-test.ps1 -Tests unittest\a-tests.l,unittest\b-tests.l
#                   Start it from PowerShell, not Git Bash (which rewrites
#                   arguments such as /c), and use pwsh: Windows PowerShell 5.1
#                   fails to start on a separate desktop. Separate several files
#                   with commas (-File passes one string). deskrun.exe: build it
#                   with tools\gui-verify\build.ps1.
#                   The clipboard is still the user's there: never run tests
#                   that touch the clipboard this way, and never the whole suite.
param([string[]]$Tests = @())
$Tests = @($Tests | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$out = Join-Path $root 'ci-out'
New-Item -ItemType Directory -Force $out | Out-Null

function Stop-Tree([int]$id) {
  # Only the processes this script started (and their children).
  cmd /c "taskkill /T /F /PID $id" | Out-Null
}

Write-Host '[1/3] install.bat (build, byte compile, check)'
$i = Start-Process -FilePath 'cmd.exe' -ArgumentList '/c', "`"`"$root\install.bat`" > `"$out\install.log`" 2>&1`"" -PassThru -WindowStyle Hidden
if (-not $i.WaitForExit(1200000)) { Stop-Tree $i.Id; Write-Host 'install.bat timed out (20 min)'; exit 1 }
$i.WaitForExit()
if ($i.ExitCode -ne 0) {
  Get-Content "$out\install.log" -Tail 60
  Write-Host "install.bat failed ($($i.ExitCode))"
  exit 1
}

Write-Host '[2/3] start xyzzy once to write the dump image'
$cfg = Join-Path $env:TEMP ('xyzzy-ci-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $cfg | Out-Null
New-Item -ItemType File (Join-Path $cfg 'xyzzy.ini') | Out-Null
$env:XYZZYHOME = $root
$env:XYZZYCONFIGPATH = $cfg
Remove-Item Env:XYZZYINIFILE -ErrorAction SilentlyContinue
$p = Start-Process -FilePath (Join-Path $root 'xyzzy.exe') -ArgumentList '-q', '-l', (Join-Path $PSScriptRoot 'kill.l') -PassThru
if (-not $p.WaitForExit(300000)) { Stop-Tree $p.Id; Write-Host 'timed out writing the dump image'; exit 1 }
if (-not (Test-Path (Join-Path $root 'xyzzy.wxp'))) { Write-Host 'no dump image (xyzzy.wxp)'; exit 1 }
Remove-Item -Recurse -Force $cfg

Write-Host '[3/3] run-tests.bat'
$log = Join-Path $out 'tests.log'
$targs = ($Tests | ForEach-Object { "`"$_`"" }) -join ' '
$t = Start-Process -FilePath 'cmd.exe' -ArgumentList '/c', "`"`"$root\run-tests.bat`" $targs > `"$log`" 2>&1`"" -PassThru -WindowStyle Hidden
if (-not $t.WaitForExit(1200000)) { Stop-Tree $t.Id; Write-Host 'run-tests.bat timed out (20 min)'; exit 1 }
$t.WaitForExit()
$r = $t.ExitCode
Select-String -Path $log -Pattern '\.\.\.Failed' | ForEach-Object { $_.Line }
Get-Content $log -Tail 3

if ($Tests.Count -eq 0) {
  # The S4 value switch: the 1 side must build and link (it is not tested
  # yet). Rebuild, because the CL variable is not tracked by an incremental
  # build. This leaves a 1-side xyzzy.exe; the next job checks out clean.
  Write-Host '[4/4] build with XYZZY_CHAR_UNICODE=1 (build and link only)'
  $env:CL = '/DXYZZY_CHAR_UNICODE=1'
  $b = Start-Process -FilePath 'cmd.exe' -ArgumentList '/c', "`"`"$root\build.bat`" Release Rebuild minimal > `"$out\build-unicode1.log`" 2>&1`"" -PassThru -WindowStyle Hidden
  if (-not $b.WaitForExit(1200000)) { Stop-Tree $b.Id; Write-Host 'the 1-side build timed out (20 min)'; exit 1 }
  $b.WaitForExit()
  Remove-Item Env:CL
  if ($b.ExitCode -ne 0) {
    Select-String -Path "$out\build-unicode1.log" -Pattern ' error ' | Select-Object -First 20 | ForEach-Object { $_.Line }
    Write-Host "the 1-side build failed ($($b.ExitCode))"
    if ($r -eq 0) { $r = 1 }
  } else {
    Write-Host '  OK: the 1 side builds and links'
  }
}
exit $r
