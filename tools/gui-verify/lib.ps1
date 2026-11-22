# Helpers for screen checks that run INSIDE a private desktop (started by deskrun.exe).
# No real input (SendInput/SetCursorPos/mouse_event/keybd_event), no foreground APIs.
# Dot-source this file:  . "$PSScriptRoot\lib.ps1"
$global:wt = if ($env:GV_WT) { $env:GV_WT } else { (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path }   # default: this repository's root
$global:gv = $PSScriptRoot
$global:work = if ($env:GV_WORK) { $env:GV_WORK } else { Join-Path $PSScriptRoot 'work' }   # cfg, logs, test data
[void][IO.Directory]::CreateDirectory($global:work)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Text; using System.Collections.Generic; using System.Runtime.InteropServices; 
public class G {
 public delegate bool EP(IntPtr h, IntPtr l);
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
 [DllImport("user32.dll")] public static extern bool EnumWindows(EP p, IntPtr l);
 [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EP p, IntPtr l);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
 [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
 [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int l, t, r, b; }
 public static List<IntPtr> Tops(uint pid){ var r=new List<IntPtr>(); EnumWindows((h,l)=>{uint p; GetWindowThreadProcessId(h,out p); if(p==pid && IsWindowVisible(h)) r.Add(h); return true;}, IntPtr.Zero); return r;}
 public static string Cls(IntPtr h){ var cn=new StringBuilder(256); GetClassName(h,cn,256); return cn.ToString(); }
 public static string Txt(IntPtr h){ var cn=new StringBuilder(1024); GetWindowText(h,cn,1024); return cn.ToString(); }
 public static List<IntPtr> Kids(IntPtr top){ var r=new List<IntPtr>(); EnumChildWindows(top,(c,l2)=>{r.Add(c); return true;}, IntPtr.Zero); return r;}
 public static RECT WR(IntPtr h){ RECT r; GetWindowRect(h,out r); return r; }
}
public class RM {
 [DllImport("kernel32.dll")] static extern IntPtr OpenProcess(uint a, bool i, uint pid);
 [DllImport("kernel32.dll")] static extern IntPtr VirtualAllocEx(IntPtr p, IntPtr a, UIntPtr s, uint t, uint pr);
 [DllImport("kernel32.dll")] static extern bool VirtualFreeEx(IntPtr p, IntPtr a, UIntPtr s, uint t);
 [DllImport("kernel32.dll")] static extern bool ReadProcessMemory(IntPtr p, IntPtr a, byte[] b, int n, out IntPtr rd);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
 [DllImport("user32.dll")] static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 // text of a list/combo item (CB_GETLBTEXTW = 0x0148 is not marshalled across processes, so read it via remote memory)
 public static string ComboItem(IntPtr cb, int idx) {
  uint pid; GetWindowThreadProcessId(cb, out pid);
  IntPtr hp = OpenProcess(0x1F0FFF, false, pid);
  IntPtr mem = VirtualAllocEx(hp, IntPtr.Zero, (UIntPtr)4096, 0x3000, 4);
  try { SendMessage(cb, 0x0148, (IntPtr)idx, mem); byte[] b = new byte[4096]; IntPtr rd; ReadProcessMemory(hp, mem, b, 4096, out rd);
    string s = Encoding.Unicode.GetString(b); int z = s.IndexOf((char)0); return z >= 0 ? s.Substring(0, z) : s; }
  finally { VirtualFreeEx(hp, mem, UIntPtr.Zero, 0x8000); CloseHandle(hp); }
 }
}
public class SB {
 [DllImport("kernel32.dll")] static extern IntPtr OpenProcess(uint a, bool i, uint pid);
 [DllImport("kernel32.dll")] static extern IntPtr VirtualAllocEx(IntPtr p, IntPtr a, UIntPtr s, uint t, uint pr);
 [DllImport("kernel32.dll")] static extern bool VirtualFreeEx(IntPtr p, IntPtr a, UIntPtr s, uint t);
 [DllImport("kernel32.dll")] static extern bool ReadProcessMemory(IntPtr p, IntPtr a, byte[] b, int n, out IntPtr rd);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
 [DllImport("user32.dll")] static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 // status bar text (SB_GETTEXTW) read through remote memory
 public static string Get(IntPtr sb, int part) {
  uint pid; GetWindowThreadProcessId(sb, out pid);
  IntPtr hp = OpenProcess(0x1F0FFF, false, pid);
  IntPtr mem = VirtualAllocEx(hp, IntPtr.Zero, (UIntPtr)4096, 0x3000, 4);
  try {
   SendMessage(sb, 0x040D, (IntPtr)part, mem);
   byte[] b = new byte[4096]; IntPtr rd;
   ReadProcessMemory(hp, mem, b, 4096, out rd);
   string s = Encoding.Unicode.GetString(b); int z = s.IndexOf('\0'); if (z >= 0) s = s.Substring(0, z);
   return s;
  } finally { VirtualFreeEx(hp, mem, UIntPtr.Zero, 0x8000); CloseHandle(hp); }
 }
 public static int Parts(IntPtr sb) { return (int)SendMessage(sb, 0x0406, IntPtr.Zero, IntPtr.Zero); }
}
"@
[void][G]::SetProcessDPIAware()

# Capture one window with PrintWindow(PW_RENDERFULLCONTENT). Works for windows on this private desktop,
# covered or not; never touches the foreground. Reports the number of distinct colours sampled
# (a black/blank capture has 1).
function Shot([IntPtr]$h, [string]$name) {
  $path = Join-Path $global:work $name
  $r = [G]::WR($h); $w = $r.r - $r.l; $hh = $r.b - $r.t
  if ($w -le 0 -or $hh -le 0) { return "shot FAILED: empty window rect" }
  $bmp = New-Object System.Drawing.Bitmap $w, $hh
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $dc = $g.GetHdc()
  [void][G]::PrintWindow($h, $dc, 2)
  $g.ReleaseHdc($dc); $g.Dispose()
  $seen = New-Object 'System.Collections.Generic.HashSet[int]'
  for ($y = 0; $y -lt $hh -and $seen.Count -lt 50; $y += 7) { for ($x = 0; $x -lt $w -and $seen.Count -lt 50; $x += 7) { [void]$seen.Add($bmp.GetPixel($x, $y).ToArgb()) } }
  $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
  "shot $path ${w}x${hh} (distinct colours sampled: $($seen.Count); 1 = blank/black)"
}

# Post a key (virtual key) to a window of OUR xyzzy. WM_KEYDOWN/WM_KEYUP via PostMessage; no real input.
function Key($h, $vk) {
  [void][G]::PostMessage($h, 0x100, [IntPtr]$vk, [IntPtr]1)
  [void][G]::PostMessage($h, 0x101, [IntPtr]$vk, [IntPtr]0xC0000001)
  Start-Sleep -Milliseconds 500
}

# Start xyzzy from this worktree with the helper script gui.l. Env: XYZZYHOME=worktree, own config dir.
function Start-Xyzzy([hashtable]$extraEnv) {
  $cfg = Join-Path $global:work 'cfg'
  [void][IO.Directory]::CreateDirectory($cfg)
  if (-not (Test-Path "$cfg\xyzzy.ini")) { New-Item -ItemType File "$cfg\xyzzy.ini" | Out-Null }
  $env:XYZZYHOME = $global:wt
  $env:XYZZYCONFIGPATH = $cfg
  Remove-Item Env:XYZZYINIFILE -ErrorAction SilentlyContinue
  $env:G_SP = $global:work.Replace('\', '/')
  foreach ($k in 'G_OLD', 'G_DIR') { Remove-Item "Env:$k" -ErrorAction SilentlyContinue }
  if ($extraEnv) { foreach ($k in $extraEnv.Keys) { Set-Item "Env:$k" $extraEnv[$k] } }
  Remove-Item "$global:work\log.txt" -ErrorAction SilentlyContinue
  $p = Start-Process -FilePath "$global:wt\xyzzy.exe" -ArgumentList '-q', '-l', "$global:gv\gui.l" -PassThru
  Start-Sleep -Seconds 5
  $p
}

function Get-Top($p) {
  [G]::Tops($p.Id) | Where-Object { [G]::Txt($_) -like 'xyzzy*' } | Select-Object -First 1
}
