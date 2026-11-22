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
#
# GitHub allows 5000 API calls an hour for the account, shared by every agent
# that waits. So this script asks once a minute, with one call that lists the
# recent runs, and when the limit is reached it waits for the reset (asking
# for the reset time does not count). Do not use `gh run watch` (it asks every
# few seconds) or loops of gh calls.
param([string]$Sha = '', [int]$Minutes = 9)
# Not 'Stop': gh writes to stderr, which must not end the script.
$ErrorActionPreference = 'Continue'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
if (-not $Sha) { $Sha = 'HEAD' }
$Sha = (git rev-parse $Sha).Trim()
$deadline = (Get-Date).AddMinutes($Minutes)

# One call: the latest runs of the workflow. Returns the run of $Sha, $null if
# it is not listed, or 'RATE' when the API limit is reached.
function Get-Run {
  $out = gh run list --workflow test.yml --limit 40 --json databaseId,headSha,status,conclusion,headBranch,url 2>&1
  if ($LASTEXITCODE -ne 0) {
    if ("$out" -match 'rate limit') { return 'RATE' }
    Write-Host "gh run list failed: $out"
    return $null
  }
  $runs = (($out | ForEach-Object { "$_" }) -join "`n") | ConvertFrom-Json
  return ($runs | Where-Object { $_.headSha -eq $Sha } | Select-Object -First 1)
}

# Waits for the API limit to reset when that happens before the deadline.
# Returns $false when the deadline comes first.
function Wait-Rate {
  $reset = "$(gh api rate_limit --jq .resources.core.reset 2>$null)".Trim()
  $at = if ($reset) { [DateTimeOffset]::FromUnixTimeSeconds([long]$reset).LocalDateTime } else { (Get-Date).AddMinutes(5) }
  if ($at.AddSeconds(5) -gt $deadline) {
    Write-Host ("GitHub API limit reached; it resets at {0:HH:mm:ss}. Run this script again after that." -f $at)
    return $false
  }
  Write-Host ("GitHub API limit reached; waiting until {0:HH:mm:ss}." -f $at)
  Start-Sleep -Seconds ([Math]::Max(5, [int](($at - (Get-Date)).TotalSeconds) + 5))
  return $true
}

# The run is listed (queued) a few seconds after the push, even when the runner
# is busy, so stop looking for it after 2 minutes.
$findDeadline = (Get-Date).AddMinutes([Math]::Min(2, $Minutes))
$r = $null
$found = $false
while ($true) {
  $r = Get-Run
  if ($r -eq 'RATE') { if (-not (Wait-Rate)) { exit 2 }; continue }
  if ($r) { $found = $true }
  if ($r -and $r.status -eq 'completed') { break }
  $now = Get-Date
  if (-not $found -and $now -gt $findDeadline) {
    Write-Host "No CI run for $Sha."
    Write-Host 'Check: pushed? to feature/x64-only, develop or worktree-agent-*? no [skip ci]? not only docs (*.md, docs/, bugs/, .pair-agent/)?'
    exit 3
  }
  if ($now -gt $deadline) {
    Write-Host "Run $($r.databaseId) ($($r.headBranch)) is still $($r.status). Run this script again to keep waiting."
    Write-Host $r.url
    exit 2
  }
  Start-Sleep -Seconds $(if ($found) { 60 } else { 15 })
}

$id = $r.databaseId
Write-Host "Run $id ($($r.headBranch)): $($r.conclusion)"
Write-Host $r.url
$dir = Join-Path $root "ci-out\run-$id"
if (-not (Test-Path (Join-Path $dir 'tests.log')) -and -not (Test-Path (Join-Path $dir 'install.log'))) {
  gh run download $id -n test-logs -D $dir 2>&1 | Out-Null
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
  Write-Host "No logs were downloaded (API limit?); later: gh run download $id -n test-logs -D $dir"
}
if ($r.conclusion -eq 'success') { exit 0 }
if ($r.conclusion -eq 'cancelled') { Write-Host 'Cancelled (a newer push to the same branch replaces the older run).' }
exit 1
