# Proof of the launch tool: runs inside the private desktop (see README.md).
. "$PSScriptRoot\lib.ps1"
Start-Transcript -Path "$global:work\proof-transcript.txt" -Force | Out-Null
$p = Start-Xyzzy @{}
$top = Get-Top $p
"xyzzy pid=$($p.Id) top=$top class=[$([G]::Cls($top))] title=[$([G]::Txt($top))]"
$r = [G]::WR($top); "top rect $($r.l),$($r.t)-$($r.r),$($r.b)"
Shot $top 'proof-xyzzy.png'
Key $top 0x7B    # F12: g-done -> kill-xyzzy (posted message, no real input)
Start-Sleep -Seconds 2
if (-not $p.HasExited) { "still running; stopping our own pid"; Stop-Process -Id $p.Id -Force }
Get-Content "$global:work\log.txt" -Encoding UTF8
Stop-Transcript | Out-Null
