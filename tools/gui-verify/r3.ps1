# Display scenes of one xyzzy build: 17 scenes, PrintWindow of the top level window + the caret rectangle. Run inside deskrun.
# env: R3_HOME (XYZZYHOME of the build to shoot; default GV_WT / repository root), R3_TAG (file name prefix, e.g. before / after),
#      R3_DATA (folder with the scene data cp932.txt iso1.txt ... utf8.txt; required), R3_ONLY (comma list of scene names, optional)
# Output: work\r3\<tag>-<scene>.png, .mask (caret rectangle), <tag>-log.txt. Compare two tags with cmp.exe (see README.md).
. "$PSScriptRoot\lib.ps1"
$out = "$global:work\r3"; [void][IO.Directory]::CreateDirectory($out)
$home_dir = if ($env:R3_HOME) { $env:R3_HOME } else { $global:wt }; $tag = if ($env:R3_TAG) { $env:R3_TAG } else { 'run' }
$data = $env:R3_DATA
if (-not $data -or -not (Test-Path $data)) { Write-Error "R3_DATA (folder with cp932.txt, iso1.txt, ...) is not set or not found"; exit 1 }
Add-Type @"
using System; using System.Runtime.InteropServices;
public class CI2 {
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int l, t, r, b; }
 [StructLayout(LayoutKind.Sequential)] public struct GTI { public int cb, flags; public IntPtr active, focus, capture, menu, move, caret; public RECT rc; }
 [DllImport("user32.dll")] public static extern bool GetGUIThreadInfo(uint tid, ref GTI g);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [StructLayout(LayoutKind.Sequential)] public struct PT { public int x, y; }
 [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref PT p);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
 // caret rectangle in the image coordinates of the PrintWindow capture of the top level window: "x0 y0 x1 y1"
 public static string CaretMask(IntPtr top) {
   uint pid; uint tid = GetWindowThreadProcessId(top, out pid);
   GTI g = new GTI(); g.cb = Marshal.SizeOf(g);
   if (!GetGUIThreadInfo(tid, ref g) || g.caret == IntPtr.Zero) return "";
   RECT tr; GetWindowRect(top, out tr);
   PT p0 = new PT(); p0.x = g.rc.l; p0.y = g.rc.t; ClientToScreen(g.caret, ref p0);
   PT p1 = new PT(); p1.x = g.rc.r; p1.y = g.rc.b; ClientToScreen(g.caret, ref p1);
   return (p0.x - tr.l) + " " + (p0.y - tr.t) + " " + (p1.x - tr.l) + " " + (p1.y - tr.t);
 }
 public static string Caret(IntPtr top) {
   uint pid; uint tid = GetWindowThreadProcessId(top, out pid);
   GTI g = new GTI(); g.cb = Marshal.SizeOf(g);
   if (!GetGUIThreadInfo(tid, ref g)) return "GetGUIThreadInfo failed";
   return "flags=" + g.flags + " caretwnd=" + g.caret.ToInt64() + " rc=(" + g.rc.l + "," + g.rc.t + ")-(" + g.rc.r + "," + g.rc.b + ") size=" + (g.rc.r - g.rc.l) + "x" + (g.rc.b - g.rc.t);
 }
}
"@
$sc = @(
 @('cp932','cp932.txt','sjis',''), @('cp932-fold','cp932.txt','sjis','fold'), @('cp932-attr','cp932.txt','sjis','attr'),
 @('cp932-hs','cp932.txt','sjis','hscroll'), @('cp932-hs2','cp932.txt','sjis','hscroll2'), @('cp932-fw','cp932.txt','sjis','fw'),
 @('iso1','iso1.txt','iso1',''), @('iso2','iso2.txt','iso2',''), @('iso5','iso5.txt','iso5',''), @('iso7','iso7.txt','iso7',''),
 @('iso9','iso9.txt','iso9',''), @('iso13','iso13.txt','iso13',''), @('gb','gb.txt','gb',''), @('big5','big5.txt','big5',''),
 @('kr','ksc.txt','kr',''), @('utf8','utf8.txt','utf8',''), @('utf8-fw','utf8.txt','utf8','fw')
)
$only = if ($env:R3_ONLY) { $env:R3_ONLY -split ',' } else { $null }
$log = @()
foreach ($x in $sc) {
  if ($only -and ($only -notcontains $x[0])) { continue }
  $cfg = "$global:work\cfg-r3-$tag"
  Remove-Item $cfg -Recurse -Force -ErrorAction SilentlyContinue; [void][IO.Directory]::CreateDirectory($cfg); New-Item -ItemType File "$cfg\xyzzy.ini" | Out-Null
  $env:XYZZYHOME = $home_dir; $env:XYZZYCONFIGPATH = $cfg; Remove-Item Env:XYZZYINIFILE -ErrorAction SilentlyContinue
  $env:TFILE = (Join-Path $data $x[1]); $env:TENC = $x[2]; $env:TMODE = $x[3]
  $p = Start-Process "$home_dir\xyzzy.exe" -ArgumentList '-q', '-l', "$global:gv\show.l" -PassThru
  $top = $null
  for ($i = 0; $i -lt 40 -and -not $top; $i++) { Start-Sleep -Milliseconds 500; $top = Get-Top $p }
  Start-Sleep -Seconds 4
  if (-not $top) { $log += "$($x[0]): no window"; if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }; continue }
  # capture until the text area is painted (non-blank) and two consecutive captures are identical
  $name = "r3\$tag-$($x[0]).png"; $r = ''
  for ($try = 0; $try -lt 8; $try++) {
    $mask = [CI2]::CaretMask($top); if (-not $mask) { $mask = "0 0 0 0" }
    $r = Shot $top $name
    Copy-Item "$global:work\$name" "$global:work\r3\prev-$tag.png" -Force
    Start-Sleep -Milliseconds 1200
    $r = Shot $top $name
    $nz = [int](& "$global:gv\cmp.exe" -n "$global:work\$name")
    $mk = $mask -split ' '; $same = (& "$global:gv\cmp.exe" -fm $mk[0] $mk[1] $mk[2] $mk[3] "$global:work\$name" "$global:work\r3\prev-$tag.png") -match 'total=0 '
    if ($nz -gt 0 -and $same) { break }
  }
  [IO.File]::WriteAllText("$global:work\$($name.Replace('.png',''))" + ".mask", $mask)
  $r = "$r tries=$($try + 1) nonwhite=$nz stable=$same"
  $cr = [CI2]::Caret($top)
  $kids = @(); foreach ($k in [G]::Kids($top)) { $kr = [G]::WR($k); if ([G]::IsWindowVisible($k)) { $kids += "$([G]::Cls($k).Trim())@($($kr.l - ([G]::WR($top)).l),$($kr.t - ([G]::WR($top)).t),$($kr.r - ([G]::WR($top)).l),$($kr.b - ([G]::WR($top)).t))" } }
  $log += "$($x[0]): $r | caret: $cr | kids: $($kids -join ' ')"
  Stop-Process -Id $p.Id -Force
  Start-Sleep -Milliseconds 300
}
$log | Set-Content -Encoding UTF8 "$out\$tag-log.txt"
$log
