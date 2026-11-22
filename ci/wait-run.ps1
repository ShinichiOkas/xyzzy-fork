# Waits for the CI run (.github/workflows/test.yml) of a pushed commit and
# prints the result: the conclusion, the failed tests and the end of the log.
#
# For work on a branch (a role's worktree-agent-* branch, feature/x64-only):
# commit, `git push -u origin HEAD`, then
#   powershell -NoProfile -ExecutionPolicy Bypass -File ci/wait-run.ps1
# Options:
#   -Sha <commit>   the commit to wait for (default: HEAD)
#   -Minutes <n>    stop waiting after n minutes (default 9, under the
#                   10-minute limit of an agent's shell command)
# Exit code: 0 passed; 1 failed or cancelled; 2 still queued or running
# (run the script again to keep waiting); 3 no run found for the commit.
# The logs of a finished run are saved in ci-out/run-<id>/ (ignored by git).
param([string]$Sha = '', [int]$Minutes = 9)
# Not 'Stop': gh writes progress to stderr, which must not end the script.
$ErrorActionPreference = 'Continue'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
if (-not $Sha) { $Sha = (git rev-parse HEAD).Trim() }
$deadline = (Get-Date).AddMinutes($Minutes)

# The run appears a few seconds after the push.
$id = ''
while ($true) {
  $id = "$(gh run list --workflow test.yml --commit $Sha --limit 1 --json databaseId --jq '.[0].databaseId')".Trim()
  if ($id) { break }
  if ((Get-Date) -gt $deadline) {
    Write-Host "No CI run for $Sha."
    Write-Host 'Check: pushed? to feature/x64-only, develop or worktree-agent-*? no [skip ci]? not only docs (*.md, docs/, bugs/, .pair-agent/)?'
    exit 3
  }
  Start-Sleep -Seconds 15
}

while ($true) {
  $r = gh run view $id --json status,conclusion,url,headBranch | ConvertFrom-Json
  if ($r -and $r.status -eq 'completed') { break }
  if ((Get-Date) -gt $deadline) {
    $queued = @(gh run list --workflow test.yml --status queued --json databaseId --jq '.[].databaseId' | Where-Object { $_ }).Count
    Write-Host "Run $id ($($r.headBranch)) is still $($r.status); $queued run(s) queued on the runner. Run this script again to keep waiting."
    Write-Host $r.url
    exit 2
  }
  Start-Sleep -Seconds 20
}

Write-Host "Run $id ($($r.headBranch)): $($r.conclusion)"
Write-Host $r.url
$dir = Join-Path $root "ci-out\run-$id"
if (-not (Test-Path (Join-Path $dir 'tests.log')) -and -not (Test-Path (Join-Path $dir 'install.log'))) {
  gh run download $id -n test-logs -D $dir | Out-Null
}
$tests = Join-Path $dir 'tests.log'
$inst = Join-Path $dir 'install.log'
if (Test-Path $tests) {
  Select-String -Path $tests -Pattern '\.\.\.Failed' | ForEach-Object { $_.Line }
  Get-Content $tests -Tail 3
  Write-Host "logs: $dir"
} elseif (Test-Path $inst) {
  Get-Content $inst -Tail 40
  Write-Host "logs: $dir"
} else {
  Write-Host "No logs were uploaded; see: gh run view $id --log-failed"
}
if ($r.conclusion -eq 'success') { exit 0 }
if ($r.conclusion -eq 'cancelled') { Write-Host 'Cancelled (a newer push to the same branch replaces the older run).' }
exit 1
