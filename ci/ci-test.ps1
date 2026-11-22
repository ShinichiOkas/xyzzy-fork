# CI entry point: install.bat -> start once to write the dump image -> run-tests.bat.
# Run from anywhere; works in the repository root. Exit code 0 = all tests passed.
# Logs go to ci-out/ (uploaded as an artifact by the workflow).
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
if (-not $i.WaitForExit(2700000)) { Stop-Tree $i.Id; Write-Host 'install.bat timed out (45 min)'; exit 1 }
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
$t = Start-Process -FilePath 'cmd.exe' -ArgumentList '/c', "`"`"$root\run-tests.bat`" > `"$log`" 2>&1`"" -PassThru -WindowStyle Hidden
if (-not $t.WaitForExit(3600000)) { Stop-Tree $t.Id; Write-Host 'run-tests.bat timed out (60 min)'; exit 1 }
$t.WaitForExit()
$r = $t.ExitCode
Select-String -Path $log -Pattern '\.\.\.Failed' | ForEach-Object { $_.Line }
Get-Content $log -Tail 3
exit $r
