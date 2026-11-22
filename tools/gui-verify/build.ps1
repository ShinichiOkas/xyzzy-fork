# Builds deskrun.exe and dropfiles.exe next to the sources with the .NET Framework C# compiler (no SDK needed).
# The .exe files are not committed (see .gitignore). Usage:  powershell -NoProfile -File build.ps1
$csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path $csc)) { Write-Error "csc.exe not found: $csc"; exit 1 }
$here = $PSScriptRoot
& $csc /nologo /target:exe /platform:x64 /r:System.Management.dll /out:"$here\deskrun.exe" "$here\deskrun.cs"
if ($LASTEXITCODE -ne 0) { exit 1 }
& $csc /nologo /target:exe /platform:x64 /out:"$here\dropfiles.exe" "$here\dropfiles.cs"
if ($LASTEXITCODE -ne 0) { exit 1 }
"built: $here\deskrun.exe, $here\dropfiles.exe"
