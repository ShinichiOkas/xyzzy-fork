# Print preview of cp932.txt: print-dialog (F3 in show.l) -> Preview button (BM_CLICK, no real input) -> pick a zoom -> shoot.
# Run inside deskrun.
# env: R3_HOME (XYZZYHOME; default GV_WT / repository root), R3_TAG (file name prefix), R3_DATA (folder with cp932.txt; required),
#      PV_SCALE (index of the zoom combo item to select; compare two builds only at a fixed zoom, 200%: see README.md)
# Output: work\r3\<tag>-print-*.png, work\print-transcript.txt
. "$PSScriptRoot\lib.ps1"
Start-Transcript -Path "$global:work\print-transcript.txt" -Force | Out-Null
$home_dir = if ($env:R3_HOME) { $env:R3_HOME } else { $global:wt }; $tag = if ($env:R3_TAG) { $env:R3_TAG } else { 'run' }
$data = $env:R3_DATA
if (-not $data -or -not (Test-Path (Join-Path $data 'cp932.txt'))) { Write-Error "R3_DATA (folder with cp932.txt) is not set or not found"; exit 1 }
[void][IO.Directory]::CreateDirectory("$global:work\r3")
$cfg = "$global:work\cfg-r3-print-$tag"
Remove-Item $cfg -Recurse -Force -ErrorAction SilentlyContinue; [void][IO.Directory]::CreateDirectory($cfg); New-Item -ItemType File "$cfg\xyzzy.ini" | Out-Null
$env:XYZZYHOME = $home_dir; $env:XYZZYCONFIGPATH = $cfg; Remove-Item Env:XYZZYINIFILE -ErrorAction SilentlyContinue
$env:TFILE = (Join-Path $data 'cp932.txt'); $env:TENC = 'sjis'; $env:TMODE = ''
$p = Start-Process "$home_dir\xyzzy.exe" -ArgumentList '-q', '-l', "$global:gv\show.l" -PassThru
$top = $null
for ($i = 0; $i -lt 40 -and -not $top; $i++) { Start-Sleep -Milliseconds 500; $top = Get-Top $p }
Start-Sleep -Seconds 3
Key $top 0x72    # F3: (print-dialog)
Start-Sleep -Seconds 3
$wins = [G]::Tops($p.Id) | Where-Object { [G]::Cls($_) -like '#32770*' }
foreach ($w in $wins) { "dialog $w [$([G]::Txt($w))]" }
$dlg = $wins | Select-Object -First 1
if (-not $dlg) { "no print dialog"; Stop-Process -Id $p.Id -Force; exit }
Shot $dlg "r3\$tag-print-dialog.png"
# click the Preview button (IDC_PREVIEW = 2024) with WM_COMMAND / BM_CLICK (a message to our own window)
$btn = [G]::Kids($dlg) | Where-Object { [G]::GetDlgCtrlID($_) -eq 2024 } | Select-Object -First 1
"preview button=$btn [$([G]::Txt($btn))]"
[void][G]::PostMessage($btn, 0xF5, [IntPtr]0, [IntPtr]0)   # BM_CLICK
Start-Sleep -Seconds 4
$wins = [G]::Tops($p.Id)
foreach ($w in $wins) { "top $w class=[$([G]::Cls($w))] [$([G]::Txt($w))]" }
$pv = $null
foreach ($w in $wins) { foreach ($k in ,$w + [G]::Kids($w)) { if ([G]::Cls($k) -eq 'PreviewPage') { $pv = $k } } }
"preview page window = $pv"
if ($pv) { Shot $pv "r3\$tag-print-preview.png" }
$pdlg = [G]::Tops($p.Id) | Where-Object { [G]::Cls($_) -like '#32770*' -and $_ -ne $dlg } | Select-Object -First 1
if ($pdlg) {
  $cb = [G]::Kids($pdlg) | Where-Object { [G]::Cls($_) -eq 'ComboBox' } | Select-Object -First 1
  if ($cb) {
    $n = [int][G]::SendMessage($cb, 0x146, [IntPtr]0, [IntPtr]0)
    $items = @(); for ($i = 0; $i -lt $n; $i++) { $items += "$i=[$([RM]::ComboItem($cb, $i))]" }
    "scale combo items: $($items -join ' ')   current=$([G]::SendMessage($cb, 0x147, [IntPtr]0, [IntPtr]0))"
    if ($env:PV_SCALE) {
      $id = [G]::GetDlgCtrlID($cb)
      [void][G]::SendMessage($cb, 0x14E, [IntPtr][int]$env:PV_SCALE, [IntPtr]0)          # CB_SETCURSEL
      [void][G]::PostMessage($pdlg, 0x111, [IntPtr](($id) -bor (1 -shl 16)), $cb)          # WM_COMMAND CBN_SELCHANGE
      "selected scale index $($env:PV_SCALE)"
    }
  }
  Start-Sleep -Seconds 3
  for ($i = 1; $i -le 4; $i++) { [void](Shot $pdlg "r3\$tag-print-preview-dialog-$i.png"); Start-Sleep -Seconds 1 }
  Shot $pdlg "r3\$tag-print-preview-dialog.png"
  foreach ($i in 2, 3, 4) { "capture $($i - 1) vs ${i}: " + (& "$global:gv\cmp.exe" -f "$global:work\r3\$tag-print-preview-dialog-$($i - 1).png" "$global:work\r3\$tag-print-preview-dialog-$i.png") }
}
Stop-Process -Id $p.Id -Force
Stop-Transcript | Out-Null
