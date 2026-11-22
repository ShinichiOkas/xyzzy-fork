# Registers this PC as a GitHub Actions self-hosted runner for this repository and
# installs it as a Windows service (so CI never shows windows on the desktop or
# takes the mouse: a service runs on its own, invisible desktop).
#
# Run once from an ELEVATED PowerShell (a service needs administrator rights):
#   powershell -ExecutionPolicy Bypass -File ci\install-runner.ps1
# Options:
#   -InstallDir <dir>  where the runner goes (default: $env:AGENT_RUNNER_DIR,
#                      else %USERPROFILE%\actions-runner\<repository name>)
#   -Token <token>     registration token (default: asked from GitHub with `gh`)
param(
  [string]$InstallDir = '',
  [string]$Token = ''
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $admin) { throw 'Run this from an elevated PowerShell (the runner is installed as a Windows service).' }

# owner/name from the origin remote
$url = (git -C $root remote get-url origin).Trim()
if ($url -notmatch 'github\.com[:/](?<repo>[^/]+/[^/]+?)(\.git)?$') { throw "origin is not a GitHub repository: $url" }
$repo = $Matches.repo
$name = ($repo -split '/')[1]

if (-not $InstallDir) {
  $InstallDir = if ($env:AGENT_RUNNER_DIR) { $env:AGENT_RUNNER_DIR } else { Join-Path $env:USERPROFILE "actions-runner\$name" }
}
Write-Host "[1/4] runner for $repo in $InstallDir"
New-Item -ItemType Directory -Force $InstallDir | Out-Null
Set-Location $InstallDir

if (-not (Test-Path (Join-Path $InstallDir 'config.cmd'))) {
  Write-Host '[2/4] download the latest runner (checked against the release checksum)'
  [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
  $rel = Invoke-RestMethod 'https://api.github.com/repos/actions/runner/releases/latest'
  $asset = $rel.assets | Where-Object { $_.name -like 'actions-runner-win-x64-*.zip' } | Select-Object -First 1
  if (-not $asset) { throw 'no win-x64 runner in the latest release' }
  $zip = Join-Path $InstallDir $asset.name
  Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $zip -UseBasicParsing
  if ($rel.body -match '<!-- BEGIN SHA win-x64 -->(?<sha>[0-9a-f]{64})<!-- END SHA win-x64 -->') {
    $sha = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
    if ($sha -ne $Matches.sha) { throw "checksum mismatch: $sha" }
  } else { throw 'no checksum in the release notes' }
  Expand-Archive -Path $zip -DestinationPath $InstallDir -Force
  Remove-Item $zip
} else {
  Write-Host '[2/4] runner already downloaded'
}

if (-not $Token) {
  Write-Host '[3/4] registration token from GitHub (gh)'
  $Token = (gh api -X POST "repos/$repo/actions/runners/registration-token" --jq .token).Trim()
  if (-not $Token) { throw 'could not get a registration token (gh auth?)' }
} else {
  Write-Host '[3/4] registration token given'
}

Write-Host '[4/4] configure and install the service'
& .\config.cmd --unattended --url "https://github.com/$repo" --token $Token `
  --name "$env:COMPUTERNAME-$name" --labels xyzzy --runasservice --replace
if ($LASTEXITCODE -ne 0) { throw "config.cmd failed ($LASTEXITCODE)" }
Get-Service 'actions.runner.*' | Format-Table Name, Status, StartType -AutoSize
