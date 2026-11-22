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
- `cmp.cs` -> `cmp.exe`: pixel comparison of two PNG captures (see "Before/after pixel comparison" below).
- `build.ps1`: builds the three exes with the .NET Framework csc (`/r:System.Management.dll` for deskrun, `System.Drawing.dll` for cmp). Exes and images are not committed.
- `lib.ps1`: dot-source it INSIDE the desktop. `Start-Xyzzy` (XYZZYHOME = `$env:GV_WT`, default this repository's root;
  own config in `work\cfg`; loads `gui.l`), `Get-Top`, `Key` (PostMessage WM_KEYDOWN/UP), `Shot` (PrintWindow ->
  png under `work\`; "distinct colours sampled: 1" means a black capture), `[SB]::Get` (status bar text by SB_GETTEXTW),
  `[G]::` window enumeration helpers, `[RM]::ComboItem(hwnd, index)` (text of a combo/list item, CB_GETLBTEXTW read through
  remote memory because the message is not marshalled across processes).
- `gui.l`: Lisp helper loaded by `Start-Xyzzy`. Keys: F4 clear buffer, F5 log buffer char codes, F6 log get-disk-usage of
  `$env:G_DIR`, F7 open the common-settings dialog on the filer page, F8 open a dual modeless filer in `G_DIR`, F9 log
  buffer-list, F12 kill-xyzzy. Output goes to `work\log.txt` (cp932). Add keys here for new checks.
- `proof.ps1`: the proof script (start xyzzy, capture, F12). `r10.ps1`: example that posts WM_PRIVATE_WCHAR / WM_CHAR and reads
  the buffer back.
- `r3.ps1`: shoots 17 display scenes of one build (PrintWindow of the top-level window) and records the caret rectangle
  (`work\r3\<tag>-<scene>.png`, `.mask`, `<tag>-log.txt`). `print.ps1`: opens the print preview (print-dialog -> Preview button),
  picks a zoom, shoots it. `show.l`: the scene script both load (env `TFILE`, `TENC`, `TMODE`; F3 = print-dialog, F12 = kill).
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

Which build? `xyzzy.exe` is built with XYZZY_CHAR_UNICODE 0 or 1. Check before labelling a result: run `r10.ps1` (it
posts WM_CHAR 0x82 then 0xA0 and reads the buffer back with F5). A 0 build gives 33440 (0x82A0, the old internal code), a 1 build
gives 12354 (U+3042, the code point). `(char "<hiragana A>" 0)` alone does NOT tell them apart: both builds read the literal as
#x82A0 there. Another sign: a 1 build reading still-internal buffer codes as Unicode garbles the kanji.

## Before/after pixel comparison (two builds, same window)
Used to show that a change to the drawing code leaves the screen identical (R3: glyph_t 64bit, ExtTextOutW, print width table).

1. Build the tools once: `powershell -NoProfile -File tools\gui-verify\build.ps1` (gives `cmp.exe`).
2. Put the scene data in a folder `<data>`: `cp932.txt iso1.txt iso2.txt iso5.txt iso7.txt iso9.txt iso13.txt gb.txt big5.txt
   ksc.txt utf8.txt` (the data is not in the repository; `*.txt` is git-ignored). Each build needs its `xyzzy.wxp` written once first.
3. Shoot each build with its own tag (R3_HOME = the folder with that `xyzzy.exe`, its `lisp\` and `xyzzy.wxp`):

       $env:R3_DATA = '<data>'
       $env:R3_HOME = '<before build folder>'; $env:R3_TAG = 'before'
       tools\gui-verify\deskrun.exe 900 "C:\Program Files\PowerShell\7\pwsh.exe" -NoProfile -File tools\gui-verify\r3.ps1
       $env:R3_HOME = '<after build folder>'; $env:R3_TAG = 'after'
       (same deskrun line)

   Shoot the "before" build twice (tags `before` and `before2`) and require before == before2 first: that is the noise floor.
   `R3_ONLY=cp932,utf8` limits the scenes. Each capture is repeated until the text area is non-blank and two consecutive
   captures agree.
4. Compare: `tools\gui-verify\cmp.exe tools\gui-verify\work\r3 before after` prints, for each scene, the number of differing
   pixels per band and the bounding box of the differences: `[0-89: n] [89-1433: n bbox=...] ...`. The bands are title+menu+ruler,
   text area, mode line, minibuffer, status bar, fixed for a 2880x1537 window (edit `bands` in `cmp.cs` for another size; a size
   mismatch prints `SIZE DIFFERS`). Single files: `cmp.exe -f a.png b.png`; `cmp.exe -fm x0 y0 x1 y1 a.png b.png` with an explicit
   mask; `cmp.exe -n a.png` counts the non-white pixels of the text area (0 = not painted yet).
5. A verdict is "all scenes, all bands: total=0" for before vs after, with before vs before2 also 0.

**The caret is excluded.** PrintWindow catches the blinking system caret, so two shots of the same screen differ by the caret
rectangle. `r3.ps1` writes the caret rectangle (GetGUIThreadInfo, in capture coordinates) to `<name>.mask` beside each png, and
`cmp.exe` skips that rectangle (+-3 px) in both images. The caret SHAPE is not pixel-compared: compare the size line of the log
(`caret: ... size=WxH`) between the two logs instead.

**Print preview: compare at a fixed zoom (200%).** The default "fit the window" zoom is not reproducible even for one binary against
itself (the page repaints and toggles between two slightly different bitmaps). `print.ps1` lists the zoom combo items in the transcript
(`work\print-transcript.txt`, line `scale combo items: ...`); set `PV_SCALE` to the index of the 200% item (CB_SETCURSEL + WM_COMMAND)
and run it for both builds with `R3_DATA`, `R3_HOME`, `R3_TAG` as above. The dialog is shot four times; the transcript prints
`capture n vs n+1` differences (must be 0 for the capture to be settled). Compare `<tag>-print-preview-dialog.png` of the two tags
with `cmp.exe -f`. It checks the page bitmap the print engine produced, not real printer output.

## Limits (record, do not work around)
- OLE DoDragDrop from a real mouse drag cannot be done; OLE-registered drop targets (the filer pane, the buffer bar) are
  not exercised. The main window is a WM_DROPFILES target and is.
- PrintWindow does not paint the tab row of an inactive property sheet (blank band at the top of the dialog).
- IME (IMM32) cannot be checked.
- The clipboard is shared within the window station (WinSta0): no clipboard checks.
