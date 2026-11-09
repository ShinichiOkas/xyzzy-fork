# Inventory: ui (windows, dialogs, messages, drawing)

Repo S:\work\develop\xyzzy-fork, branch feature/x64-only, HEAD 8dc9230. Read-only audit. Paths are relative to the repo root.
Columns: file:line | what it is | category | modern replacement | BUG ids

src\targetver.h:11 | _WIN32_WINNT pinned at 0x0502 (XP); forces hand-copied #defines and old struct sizes | REWRITE | Set 0x0A00; delete fallback #defines (MIIM_*, SPI_*FOREGROUNDLOCKTIMEOUT) | -
projects\xyzzy.vcxproj:24,427 | CharacterSet NotSet = every Win32 call is the A variant (cp932); EmbedManifest false, no manifest | REWRITE | UNICODE build; manifest: PerMonitorV2, longPathAware, supportedOS, comctl32 v6 | -
src\init.cc:660-720,927 | WinMain(LPSTR) entry; five RegisterClass(A) calls for toplevel/frame/client/modeline/fnkey | REWRITE | wWinMain, RegisterClassExW, W windows (WM_CHAR becomes UTF-16) | -
src\sysdep.cc:11; src\sysdep.h:120-133; src\com.cc:26 | GetVersionEx; Win6_1p() is false for Windows 10.0 (needs minor>=1), set_appid silently stops once a manifest reports 10.0 | REWRITE | Delete all version gates (Win11 only); remove Win4p/Win5p/Win95p/Win98p/WinNTp | BUG-0013
(group) Window.cc:434; filer.cc:44,1675,3027; preview.cc:725-729; popup.cc:16-17; toplev.cc:718,1067-1075; mouse.cc:154,192; dialogs.cc:875,902,938,1231; init.cc:802,913; privctrl\privctrl.cc:66-71; xyzzycli.cc:18-26; ctl3d.cc/h | about 20 dead Win4p/Win5p/Win98p branches left after BUG-0013, plus the whole CTL3D32 loader (toplev.cc:673,687) | REWRITE | Delete branches and ctl3d.cc/h; keep the NT path; drop GetVersionEx | BUG-0013
src\menu.cc:26-77,80-104,213,323,788-823 | MENUITEMINFO5 shim with Win98/Win5 branches; HMENU passed through UINT id | REWRITE | Plain MENUITEMINFO; UINT_PTR ids; delete old-OS else branches | -
src\gime.cc:19-190; src\gime.h | IME wrapper over Active IMM COM (CLSID_CActiveIMM, HAVE_DIMM_H) plus imm32 GetProcAddress | REWRITE | Keep as the single IMM32 layer; drop ActiveIMM COM, call Imm* W directly | -
src\dialogs.cc:457,1464; src\filer.cc:390,1605-1609 | Direct ImmAssociateContext/ImmSetOpenStatus calls that bypass gime | REWRITE | Route through the IME layer (one disable-IME helper) | -
src\toplev.cc:366-470,814-868; kbd.cc:505-526 | IME via ImmGetCompositionString A/W, WM_IME_CHAR is a cp932 word, ToUnicode workaround for A-window WM_CHAR | REWRITE | W window: UTF-16 WM_CHAR/IME incl. surrogates; delete workaround; behind IME layer | -
src\toplev.cc:659-669; src\clipboard.cc:5-60 | SetClipboardViewer chain fallback and GetProcAddress of AddClipboardFormatListener | REWRITE | Call AddClipboardFormatListener directly; delete chain code | -
src\toplev.cc:266-322; DnD.cc:1010-1030 | WM_DROPFILES via DragQueryFile(A) into char[PATH_MAX] (non-cp932 names lost); text drop accepts CF_TEXT only | REWRITE | DragQueryFileW / wide CF_HDROP; CF_UNICODETEXT | -
src\Window.cc:1947-1952 | get-window-handle returns HWND through long(INT_PTR(...)) (32 bit) | REWRITE | Return handle at pointer width; see lisp\wip\winapi.l typing | BUG-0030
src\Window.cc:3230-3250; src\xyzzycli.cc:14-26 | Foreground-lock hack: Win98 gate, SPI_SETFOREGROUNDLOCKTIMEOUT, AttachThreadInput | REWRITE | AllowSetForegroundWindow handoff from xyzzycli; no SPI change | -
src\ldialog.cc:444-449,623-628 | Owner-draw list/combo item data holds Lisp pointers, compared as LONG_PTR | KEEP | Correct on x64 (LPARAM-wide); keep | BUG-0019
src\ldialog.cc:572,585,720 | LB/CB_GETITEMDATA (LRESULT) passed to make_fixnum(long); 720 is the owner-draw :index case | REWRITE | Checked int cast for :index; owner-draw returns lisp() as at 728 | BUG-0019
src\ldialog.cc:75-95,385,1099-1133,1970; dialogs.cc (9 w2s sites) | Lisp text converted by w2s to ANSI cp932 buffers for controls; type-ahead matches cp932 bytes (1099) | REWRITE | Convert to UTF-16, use W controls/messages, compare UTF-16 | -
src\ldialog.cc:1303-1343,1887-1990 | PropSheetFont copies font from comctl32 IDD_PROPSHEET (GetModuleHandle COMCTL32.DLL); PropertySheet(A), header not zeroed | REWRITE | Fixed DS_SHELLFONT face; zero-init; PropertySheetW; v6 comctl32 | -
src\xyzzy.rc:41-560 | 17 DIALOG templates, FONT 9 "MS UI Gothic" (IDD_COLOR, IDD_FONT use cp932 face); rc is cp932 | REWRITE | DIALOGEX + DS_SHELLFONT "Segoe UI"/"Yu Gothic UI"; UTF-8 rc | -
src\xyzzy.rc:66-175 | MULTIFILEOPENORD, FILEOPENORD, IDD_DIRECTORY old-style Open templates with ES_OEMCONVERT (owner-draw combos) | REWRITE | IFileOpenDialog + IFileDialogCustomize (encoding, EOL) | BUG-0019
src\dialogs.cc:858-1055 | Ffile_name_dialog: GetOpen/SaveFileName(A) hook, char[32K] buffer, OPENFILENAME size gates | REWRITE | IFileDialog (W), long paths, no hook template | -
src\dialogs.cc:1204-1250 | Directory dialog is GetOpenFileName with old IDD_DIRECTORY template (lst2/cmb2) | REWRITE | IFileOpenDialog with FOS_PICKFOLDERS | -
src\msgbox.cc:32,133; src\msgbox.h:23 | Hand-built XMessageBox: XOFF=14,YOFF=12, 32 px icon, SM_CXSCREEN width, MapDialogRect | REWRITE | TaskDialogIndirect, or DPI-scale the offsets and icon | -
src\dockbar.cc:503-531 | tab_bar::modify_spin reads comctl32 private window-extra memory (buf[6], buf[9] as HWND) with IsBadWritePtr | REWRITE | High risk on x64 (layout differs, writes foreign memory): own up-down control | -
src\dockbar.cc:289-292,413-414 | Toolbar fixed 16x15 bitmap and 23x22 button; TBADDBITMAP nID = UINT(HBITMAP) | REWRITE | DPI-scaled ImageList; UINT_PTR nID | -
src\wheel.cc:6-37,71-72 | Win95 IntelliMouse path (FindWindow MouseZ, MSH_* messages, registry), scroll lines cached once | REWRITE | SPI_GETWHEELSCROLLLINES per event; WM_MOUSEHWHEEL; drop MSH_* | BUG-0022
(group) wheel.cc:98-103; popup.cc:57,217-228; preview.cc:37-38,51; ChooseFont.cc:385; filer.cc:392,417-436; dialogs.cc:114,1478; ldialog.cc:431 | Fixed pixel sizes: 28 px wheel bitmap, 16 px margins and image lists, DPI read once from GetDC(0) | REWRITE | GetDpiForWindow, WM_DPICHANGED, scaled bitmaps/ImageLists | -
src\mouse.cc:411-446 | WH_MOUSE hook compares positions packed as 16-bit MAKELONG(x,y) | REWRITE | Compare POINT values | -
src\disp.cc:34-50; src\ColorDialog.cc:88,267-276 | 256-colour palette logic (GetSystemPaletteEntries, RC_PALETTE, PALETTEINDEX) and bitmap caret | REWRITE | Drop palette path; own-drawn caret on 32-bit displays | -
src\print.cc:208-333,1761-1830; src\printdlg.cc:655-681 | GDI printing: PrintDlg/DEVMODE/CreateDC (A), SetAbortProc, static user_abort | REWRITE | PrintDlgEx/W structures; keep GDI path | -
src\privctrl\privctrl.cc:34-41; printdlg.cc:28; dockbar.cc:39,1126-1141; filer.cc:3027 | Raw WNDPROC subclassing (GWLP_WNDPROC/SetProp); ViewerWindow::init returns HWND narrowed to int | REWRITE | SetWindowSubclass; return bool | BUG-0020
src\conf.cc:13-85,42-50,159-175,240-300,315-325; dialogs.cc:112-123,448; preview.cc:762 | Dialog geometry/columns and binary LOGFONTA blobs in ini via GetPrivateProfileString(A); key from SM_CXSCREEN x SM_CYSCREEN | CONFIG-MOVE | ~/.xyzzy/ UTF-8 file; text font record; key independent of DPI | BUG-0029
(group) src\cdecl.h:34 PATH_MAX 1024; dialogs.cc 6, init.cc 7, DnD.cc 2, print.cc 2, toplev.cc 1, filer.cc 2 | fixed char[PATH_MAX] buffers for paths | REWRITE | Long-path aware W buffers (manifest alone does not widen them) | -
src\archiver.cc:131-196 | ArcNotify hidden window receives WM_ARCEXTRACT (EXTRACTINGINFO pointer) from archiver DLLs | DECIDE | Depends on archiver policy; may vanish with 7-Zip CLI/libarchive | BUG-0004
src\pane.cc:508-540; src\xpi.h | XPI plugin API (xpiCreatePane etc.) handed to plugin DLLs as FARPROC table | REWRITE | x64 plugins only (no 32-bit DLLs) or drop; fix table address cast | BUG-0015
src\xyzzy.rc:52-64; resource.h:101,300,302; winhelp.cc:298-355; init.cc:7,488,921 | IDD_HELPDLG (WinHelp) and Dde::initialize at startup, xdde.h, Vdde_timeout | REMOVE | Remove with winhelp and DDE server | BUG-0018
src\Window.cc:317-323; 84 Get/SetWindowLongPtr sites; INT_PTR dialog procs | Editor text via ExtTextOutW; all window-long calls are Ptr; dialog procs return INT_PTR; zero non-Ptr calls | KEEP | Already fits x64; no WPARAM/LPARAM/HWND held in int/long found | -
