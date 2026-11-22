# gui-verify: GUI checks on a private desktop

Runs xyzzy on a separate desktop made with CreateDesktop, so its windows never appear on the user's screen and the
user's mouse cursor, keyboard and foreground window are never touched. Input goes only by PostMessage/SendMessage to
our own windows; capture is PrintWindow (PW_RENDERFULLCONTENT). The rule and the limits are in
`.pair-agent/skills/xyzzy-verify-gui-on-a-separate-desktop.md`.

Forbidden everywhere: SendInput, SetCursorPos, mouse_event, keybd_event, SetKeyboardState, BlockInput,
SetForegroundWindow, AllowSetForegroundWindow, SwitchToThisWindow, activating ShowWindow. Do not write to the clipboard.

## Files
- `deskrun.cs` -> `deskrun.exe <timeout-sec> <command line>`: creates desktop `xyzzy-verify-<pid>`, runs the command on it
  (STARTUPINFO.lpDesktop), waits, closes the desktop. While the child runs it polls (every 250 ms) and prints how many
  windows of the child process tree were seen on the Default desktop (must be 0) and whether the foreground window was
  ever owned by the child tree (must be False). The foreground before/after lines may differ through the user's own
  activity; only the "owned by the child tree" line matters. Exit code = the child's.
- `dropfiles.cs` -> `dropfiles.exe <hwnd> <x> <y> <file>...`: sends WM_DROPFILES to a window of OUR xyzzy with an HDROP that
  is valid in the target (memory GlobalAlloc'ed inside xyzzy through a CreateRemoteThread stub, then SendMessageTimeout;
  PostMessage of WM_DROPFILES is refused with ERROR_INVALID_HANDLE). x64 only.
- `build.ps1`: builds both exes with the .NET Framework csc (`/r:System.Management.dll` for deskrun). Exes and images are not committed.
- `lib.ps1`: dot-source it INSIDE the desktop. `Start-Xyzzy` (XYZZYHOME = `$env:GV_WT`, default this repository's root;
  own config in `work\cfg`; loads `gui.l`), `Get-Top`, `Key` (PostMessage WM_KEYDOWN/UP), `Shot` (PrintWindow ->
  png under `work\`; "distinct colours sampled: 1" means a black capture), `[SB]::Get` (status bar text by SB_GETTEXTW),
  `[G]::` window enumeration helpers.
- `gui.l`: Lisp helper loaded by `Start-Xyzzy`. Keys: F4 clear buffer, F5 log buffer char codes, F6 log get-disk-usage of
  `$env:G_DIR`, F7 open the common-settings dialog on the filer page, F8 open a dual modeless filer in `G_DIR`, F9 log
  buffer-list, F12 kill-xyzzy. Output goes to `work\log.txt` (cp932). Add keys here for new checks.
- `proof.ps1`: the proof script (start xyzzy, capture, F12). `r10.ps1`: example that posts WM_PRIVATE_WCHAR / WM_CHAR and reads
  the buffer back.
- `work/`: outputs (config, log, transcripts, captures); git-ignored.

## Run
    powershell -NoProfile -File tools\gui-verify\build.ps1
    $env:GV_WT = '<worktree whose xyzzy.exe to test>'      # optional; default is this repository's root
    tools\gui-verify\deskrun.exe 240 "C:\Program Files\PowerShell\7\pwsh.exe" -NoProfile -File tools\gui-verify\proof.ps1

Read the `deskrun.exe` lines at the end of the run: `windows of the child tree seen on Default desktop: 0` and
`foreground window ever owned by the child tree: False`. Then read `work\log.txt`, the transcript and the capture
(`distinct colours sampled` > 1).

A fresh build has no dump image (`xyzzy.wxp`); the first start writes it. Do that once through deskrun with a script
that starts xyzzy and kills it (for example proof.ps1) before a script that depends on it.

Which build? `xyzzy.exe` is built with XYZZY_CHAR_UNICODE 0 or 1. Check before labelling a result: 0 gives the old
internal code (hiragana A = 0x82A0), 1 gives the code point (0x3042). `r10.ps1` shows the codes with F5.

## Limits (record, do not work around)
- OLE DoDragDrop from a real mouse drag cannot be done; OLE-registered drop targets (the filer pane, the buffer bar) are
  not exercised. The main window is a WM_DROPFILES target and is.
- PrintWindow does not paint the tab row of an inactive property sheet (blank band at the top of the dialog).
- IME (IMM32) cannot be checked.
- The clipboard is shared within the window station (WinSta0): no clipboard checks.
