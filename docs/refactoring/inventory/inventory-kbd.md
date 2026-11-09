# Inventory: kbd (keyboard, IME, DDE, WinHelp, OLE) - HEAD 8dc9230
Line format: file:line | what it is | category | modern replacement | BUG ids
Note: src\gen\* (symtable.cc, fns-decl.h, vars-decl.h, msgcode.h, msgdef.cc) is untracked generated output with many dde/winhelp hits; it regenerates from gen-syms.cc and msgdef.h. Nothing to edit by hand.

## DDE removal (exact list)
src\dde.cc:1-429 | DDE client builtins and server callbacks; eval_callback runs any Lisp form sent by a DDE client | REMOVE | delete file | -
src\xdde.cc:1-277, src\xdde.h:1-278 | Dde class: DdeInitialize(APPCMD_FILTERINITS), registers server "Xyzzy" | REMOVE | delete both files | -
src\init.cc:7,488,921 | #include xdde.h; *dde-timeout* init (30000); Dde::initialize() at startup | REMOVE | delete the three lines | -
src\ed.h:203-229,265-271; src\data.cc:378,590,2255-2270; src\dataP.h:40; src\lisp.h:130; src\lprint.cc:244,1553-1556,1787-1789; src\pred.cc:124-125,414-417 | Twin32_dde_handle object type (HCONV) and dde-handle-p | REMOVE | delete cases and class; enum shift changes image layout | -
src\signal.cc:147-154,645-690,823-830; src\signal.h:38-45; src\msgdef.h:387-402,557-558 | dde-error family (incl. dde-server-died), FEdde_* raisers, 9 Edde_* messages | REMOVE | delete definitions; msgcode.h regenerates | -
src\gen-syms.cc:747-754,1634-1641,1893,1903,2523-2529 | DDE condition symbols, dde-handle(-p), 5 DEFUNs, *dde-timeout* | REMOVE | delete lines | -
src\process.cc:1362-1369 | ShellExecute SE_ERR_DDE* mapped to FEdde_busy/FEdde_error | REWRITE | map to FEsimple_win32_error or generic shell-execute error | -
lisp\builtin.l:789,1195-1199; lisp\condition.l:118-125; lisp\typespec.l:122 | builtins dde-*, 8 condition accessors, ed:dde-handle typespec | REMOVE | delete lines | -
src\stdafx.h:14; src\xyzzycli.rc:2; projects\xyzzy.vcxproj:159,260,409; projects\xyzzy.vcxproj.filters:93,366,508 | ddeml.h includes; dde.cc, xdde.cc, xdde.h project entries | REMOVE | delete includes and 6 entries | -
etc\Lisp:285-289,2123-2131; reference\reference.xml:3211-3218,8707-8830,38659 | keyword file and manual entries for dde-* and conditions | REMOVE | delete lines and doc sections | -
lisp\den8view.l:356-382,759-775,816,888; lisp\winhelp.l:50-71 | only Lisp callers of dde-*: den8view and Info Viewer (MSIN) | REMOVE | go with den8view and winhelp.l | -

## WinHelp removal (exact list)
src\winhelp.cc:6-26,28-423 | WinHelp() calls (winhlp32 absent on Win11); WinHelp .idx parser and find-winhelp-path | REMOVE | delete both blocks | -
src\winhelp.cc:425-462 | Fhtml_help: HtmlHelpA via LoadLibrary("hhctrl.ocx"), A strings, W2A error text | REWRITE | owner to keep or drop; if kept HtmlHelpW, W strings | BUG-0018
lisp\winhelp.l:1-113 | run/kill/show/find-winhelp, Info Viewer over DDE, show-html-help; binds F1, C-F1, F3 | REMOVE | delete file; move get-winhelp-topic if needed | -
lisp\loadup.l:67; lisp\optprop.l:681-746; lisp\paths.l:10-13 | loads winhelp; option page MSVC2.x/Info viewer/HTML help; "d:/msvc20/help" default | REMOVE | delete load line, page fields, defaults (keep dictionary field) | -
src\gen-syms.cc:2513-2517; lisp\builtin.l:1189-1192; src\msgdef.h:667-670 | symbols, builtins, messages for winhelp and html-help | REMOVE | delete (html-help lines only if html-help dropped) | -
lisp\dexplorer.l:1-33; lisp\defs.l:314-315; etc\Lisp:1258,1260,1801-1802,2387,2481,2741-2742,2795,2834,3108-3109,3147,3149 | VS .NET DExplore driver uses get-winhelp-topic; keyword entries | REMOVE | obsolete product; delete with winhelp.l | -

## HKL and keyboard layout
src\kbd.cc:1442,1506-1507 | HKL to fixnum via int(LONG_PTR(hkl)) in list/current-kbd-layout | REWRITE | return unsigned 32-bit (HKL is 32-bit significant); add round-trip test | BUG-0023
src\kbd.cc:1492 | fixnum back to HKL via HKL(LONG_PTR(fixnum)) in select-kbd-layout | REWRITE | sign-extend from int32 explicitly; test with E0xx IME HKL | BUG-0023
src\kbd.cc:1412,1441,1474,1505; src\kbd.h:132 | HIWORD/LOWORD(HKL), %08x KLID registry lookup | KEEP | fits; macros are safe on x64 | BUG-0023
src\kbd.cc:14-16,970-980,1418-1423; src\kbd.h:26-27 | GetKeyboardLayout/List via GetProcAddress, GetKeyboardLayoutName fallback (Win95 era) | REWRITE | call USER32 directly; drop fallback | BUG-0013

## IMM32 and IME
src\gime.h:1-45; src\gime.cc:1-250 | GlobalIME wrapper over IMM32; the one layer to keep | KEEP | stays the only IMM32 entry point | -
src\gime.cc:11-60,71-85; src\guid.cc:4-11; projects\xyzzy.vcxproj:60,97; lisp\builtin.l:1295; lisp\misc.l:378-380; lisp\optprop.l:431,471-474 | Active IMM (dimm.h, HAVE_DIMM_H), enable-global-ime, *use-global-ime* | REMOVE | owner to confirm; plain IMM32 in the layer | -
src\dialogs.cc:457,1464; src\filer.cc:390,1605-1609 | direct ImmAssociateContext/GetContext/SetOpenStatus bypass the layer | REWRITE | route through GlobalIME layer | -
src\gime.h:24-40 | IMM32 used: GetContext, ReleaseContext, Get/SetOpenStatus, Get/SetCompositionString(A,W), SetCompositionWindow, SetCompositionFont, ConfigureIME, GetVirtualKey, GetDescription, GetProperty, AssociateContext | KEEP | keep behind one layer; A forms move to W | -
src\kbd.cc:350-374,383-389,548,986,1368-1373,1394,1415; src\toplev.cc:329-349,356-362,373-475 | call sites of the IMM32 layer | KEEP | unchanged except A to W | -
src\toplev.cc:380-408; src\kbd.cc:1368-1394 | ANSI composition path, ANSI ImmSetCompositionString, ANSI REGISTERWORD | REWRITE | W only; delete non-Unicode IME branch (*unicode-ime*) | -
src\kbd.cc:788-950; src\toplev.cc:1050-1087 | reconvert/documentfeed use CP_ACP; MSIME/ATOK reconvert block guarded by !Win98p && !Win5p (dead on Win11) | REWRITE | W path only; delete registered-message block | BUG-0013
src\kbd.cc:84-108,954-960,983-1032; src\toplev.cc:814-818; lisp\keyboard.l:90-130 | ANSI WM_CHAR bytes via *keyboard-layout-list*, kbd-encoding, lead-byte queue | REWRITE | W window class, WM_CHAR is UTF-16; drop per-layout code pages | -
src\kbd.cc:493,745 | ToAscii (ANSI) for ctrl chars and Meta-Ctrl | REWRITE | ToUnicodeEx with the active HKL | -

## OLE
src\oledata.cc:17-18; src\oleconv.h:14-19,33-35 | BSTR to Lisp string via W2A (CP_ACP); drops characters outside the ANSI code page | REWRITE | make_string from wchar_t directly | BUG-0002
src\oledata.cc:911-945 | CreateObject/GetObject: CoCreateInstance INPROC|LOCAL; 32-bit-only servers unreachable from x64 | KEEP | fits; document x64 limit; tests avoid ScriptControl | BUG-0002
src\oledata.cc:962-970 | typelib path via A2W (CP_ACP) | REWRITE | I2W from Lisp string | -
src\oledata.cc:1-1109 (rest) | IDispatch Invoke, SafeArray, event sink, enumerator; ole-* functions (builtin.l:1264-1269,1317-1326) | KEEP | fits modern rules; W-only COM | -
src\com.cc:34,94,124,197,231,267 | shortcut and drop code convert paths with CP_ACP | REWRITE | W paths from Lisp string | -
src\com.cc:224-300; lisp\filer.l:1395-1425 | ole-drop-files: in-proc Shellex DropHandler for send-to archivers | DECIDE | archiver policy; 32-bit shell extensions will not load | BUG-0004
src\init.cc:846,1001 | OleInitialize/OleUninitialize (STA, needed for drag and drop) | KEEP | fits | -

## Outside the list
src\toplev.cc:718; src\init.cc:913; src\sysdep.h:122-132 | sysdep.Win4p/Win5p/Win98p version checks | REWRITE | delete; Win11 baseline | BUG-0013
lisp\wip\oletest.l | OLE sample under wip | REMOVE | move wip to samples or delete | -
