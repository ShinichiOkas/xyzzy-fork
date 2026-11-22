# Example: post WM_PRIVATE_WCHAR (0x416, UTF-16 units; value follows WM_PRIVATE_FIRST in src/wm.h, re-check it) and
# WM_CHAR (ANSI bytes) to xyzzy and read the buffer back with F5. Run with GV_WT = the worktree under test.
. "$PSScriptRoot\lib.ps1"
Start-Transcript -Path "$global:work\r10-transcript.txt" -Force | Out-Null
"worktree = $global:wt"
$p = Start-Xyzzy @{}
$top = Get-Top $p
"top=$top [$([G]::Txt($top))]"
Add-Type @"
using System; using System.Runtime.InteropServices;
public class U { [DllImport("user32.dll")] public static extern bool IsWindowUnicode(IntPtr h); }
"@
"IsWindowUnicode(toplevel) = $([U]::IsWindowUnicode($top))"
function Wchar([int]$u) { [void][G]::PostMessage($script:top, 0x416, [IntPtr]$u, [IntPtr]1); Start-Sleep -Milliseconds 300 }
function Chr([int]$c)   { [void][G]::PostMessage($script:top, 0x102, [IntPtr]$c, [IntPtr]1); Start-Sleep -Milliseconds 300 }
Key $top 0x74    # F5 (initial buffer text, expect empty)
"--- (a) WM_PRIVATE_WCHAR 0x41, 0x3042, 0x6f22"
Wchar 0x41; Wchar 0x3042; Wchar 0x6f22
Key $top 0x74
Key $top 0x73    # F4: clear
"--- (b) WM_PRIVATE_WCHAR 0xd83d 0xde00 (surrogate pair, one unit per message)"
Wchar 0xd83d; Wchar 0xde00
Key $top 0x74
Key $top 0x73
"--- (c) WM_CHAR ANSI bytes 0x82 0xa0 (separate messages)"
Chr 0x82; Chr 0xa0
Key $top 0x74
Shot $top 'r10-after.png'
Key $top 0x7B
Start-Sleep -Seconds 2
if (-not $p.HasExited) { "still running; stopping our own pid"; Stop-Process -Id $p.Id -Force }
Get-Content "$global:work\log.txt" -Encoding UTF8
Stop-Transcript | Out-Null
