# Inventory: lisp (Lisp library, wip modules, samples, misc)

Format: file:line | what it is | category | modern replacement | BUG ids
Paths relative to S:\work\develop\xyzzy-fork. Lisp sources are cp932 (line numbers from raw files).
"propose delete" = not in the decided list; proposed under the "remove unneeded features" rule (shown as REWRITE).

## A. Decided removals: sites beyond the files the owner named
lisp/loadup.l:67 | dump-time load of "winhelp" (si:*load-library) | REMOVE | delete line; regenerate xyzzy.wxp dump image | -
lisp/winhelp.l:1-114 | WinHelp, MS InfoViewer over DDE, HtmlHelp (show-html-help 107); binds F1, C-F1, F3; kill hook; get-winhelp-topic 73 | REMOVE | delete file; F1/C-F1/F3 unbound (pick new F1 binding if wanted); html-help builtin goes unless CHM kept | BUG-0018
lisp/dexplorer.l:21 + defs.l:314-315 | show-dexplorer calls get-winhelp-topic; OLE "DExplore.AppObj" (VS.NET 2002 help); autoload in defs.l | REMOVE | delete file and autoload (dead; breaks when winhelp.l goes) | -
lisp/optprop.l:682-746 | "directory" option page: MSVC2.x WinHelp dir, idx, InfoViewer, HTML help, edict dir; reads winhelp vars | REWRITE | keep only edict field; drop rows, handlers, register-history-variable | -
lisp/paths.l:10-13 | sets *winhelp-index-files*, *winhelp-directory* (d:/msvc20), *info-viewer-title* | REMOVE | delete lines (vars vanish with winhelp.l) | -
lisp/condition.l:118-125 | eight dde-* condition accessors incl. dde-server-died | REMOVE | delete the eight lines | -
lisp/typespec.l:122,131; help.l:259-260 | ed:dde-handle and si:c-callable type predicates; describe-function c-callable clause | REMOVE | delete (unittest/typespec-tests.l:120-138 test them) | BUG-0001
lisp/builtin.l:507,586,789,1189-1192,1195-1199 | declarations: c-callable, dde-*, *run-winhelp, *kill-winhelp, find-winhelp-path, html-help | REMOVE | delete with the C++ builtins (html-help only if CHM dropped) | BUG-0001,BUG-0018
lisp/wip/winapi.l:16-253 + fractal.l:30, hellowin.l:37, turtle.l:66 | Win32 binding table (A-suffixed APIs, 32-bit WPARAM/LPARAM/LRESULT/MSG) and 3 demos using defun-c-callable | REMOVE | delete all four (cannot run without c-callable; not worth samples\) | BUG-0030,BUG-0001
lisp/wip/oletest.l:1-29 | OLE demos: Excel chart; InternetExplorer.Application (no IE on Win11); dead URL | REWRITE | move to samples\; drop ie-demo | -
lisp/wip/ftp.l:1-320 | plain FTP client on sockets; no provide; referenced nowhere | REWRITE | move to samples\ or delete (cleartext) | -
stale *.lc on disk (gitignored): lisp/wip/*.lc, lisp/winhelp.lc, den8view.lc, dexplorer.lc | compiled leftovers | REMOVE | delete with the sources | -

## B. Undecided items
lisp/builtin.l:1245-1251; condition.l:94 | extract/create/list/delete-in-archive, archiver-dll-version/config-dialog, convert-to-SFX; archiver-error | DECIDE | follows 7-Zip / libarchive / LZH policy | BUG-0004
lisp/about.l:36-43 | About dialog lists archiver DLL versions (*archiver-dll-list*, defs.l:147-154) | DECIDE | drop list or show new library version | BUG-0004
lisp/filer.l:55-57,890-1012,1569-1585 | filer archive keys E/O/V; default ".lzh" (948); SFX via convert-to-SFX (959-968) | DECIDE | default .zip/.7z by policy; drop SFX | BUG-0004,BUG-0005
lisp/filer.l:1411-1413 | SendTo drop handler via registry DropHandler + ole-drop-files | REWRITE | IDropTarget via shell item API | -

## C. OS version checks and Win9x/NT4 leftovers (BUG-0013 covers C++ only)
lisp/app-menu.l:385-395 | "NT/DOS prompt" label by (featurep :windows-nt); admin console by :windows-vista | REWRITE | one label; drop both featurep | BUG-0013
lisp/process.l:18-20,52-63 | shell = cmd.exe unless not :windows-nt then command.com; 8.3 short-path workaround (dead on NT) | REWRITE | cmd.exe / %COMSPEC% only; delete branch | BUG-0013
lisp/filer.l:790-791,62-66,1426 | NT check for "compressed"; default dirs c:/msdev, c:/winnt40; SendTo passes 8.3 short paths | REWRITE | drop check/defaults; quote long paths (Win11 has 8.3 off on non-system volumes) | BUG-0013
lisp/dialogs.l:447 | start-menu shortcut hidden when (featurep :windows-7) (always true) | REWRITE | always hide; no featurep | -
src/environ.cc:688-740 (lisp-visible) | features/os-platform from GetVersionEx: Win11 reports 6.2 (:windows-8) without manifest; sysdep.h:127 Win7p() false for 10.0 | REWRITE | RtlGetVersion/manifest; add :windows-10/:windows-11 | -
unittest/lisp-tests.l:20-75 (cross-area) | os-features, dump-image-path tests per OS 2000..7 | REWRITE | keep only the Win10/11 case | -
lisp/keyboard.l:90-100 | *keyboard-layout-list* keyed by HKL language ids | KEEP | verify HKL widths on x64 | BUG-0023

## D. Config location (target ~/.xyzzy/; -config and XYZZYCONFIGPATH stay, empty = unset)
lisp/estartup.l:166-167 | loads user init file "~/.xyzzy" (or "~/_xyzzy"); NAME COLLIDES with new ~/.xyzzy/ directory | CONFIG-MOVE | load ~/.xyzzy/init.l; legacy ~/.xyzzy file only as fallback if not a directory | -
lisp/history.l:61-66,80,154,196-197 | history ".xyzzy.history"/"_xyzzy.his", resume files in (user-config-path); 8.3 fallbacks | CONFIG-MOVE | ~/.xyzzy/history, resume.N; delete 8.3 names | BUG-0029
lisp/abbrev.l:28; paths.l:15 | "~/.abbrev_defs"; edict dir "~/xyzzy/edict" | CONFIG-MOVE | ~/.xyzzy/abbrev_defs, ~/.xyzzy/edict | -
lisp/history.l:168-204; estartup.l:161-162 | registry-to-file migration at startup (read-registry, yes-or-no-p, delete-registry-tree) | REWRITE | propose delete (old migration) | -
misc/makelc.l:125 + bytecompile.bat:8-12 | build deletes si:dump-image-path (xyzzy.wxp in install root); sets empty XYZZYINIFILE/XYZZYCONFIGPATH | CONFIG-MOVE | follow new dump location; empty env = unset | BUG-0029
lisp/misc.l:364 | *etc-path* = <install>\etc | KEEP | stays beside exe | BUG-0017

## E. Misc tools
misc/update_zlib.bat:1-35 | zlib 1.2.8, http://zlib.net, 7za, VS2013 vsvars32 (x86), nmake | REWRITE | zlib 1.3.x over https, VS2022 x64 /MT; settle tracked zlib.lib | BUG-0007,BUG-0016
misc/mkwords.rake:5 | IPADIC from dead mecab.googlecode.com URL (feeds detect-char-encoding-test.l) | REWRITE | new https source or drop both | -
misc/update_version_describe.bat, makelc.l, mode-settings.l, guess.scm, detect-char-encoding-test.l | git-describe header (projects/xyzzy.vcxproj:430), byte-compile driver, dev indent hooks, Gauche guess generator, test | KEEP | no change | -

## F. Group summaries
Dialogs: ~45 dialog templates in 16 files (filer.l 32, dialogs.l 14, optprop.l 10, gmark 5, kbdmacro 4, ...) with fixed dialog units and (:font 9 "MS UI Gothic") | REWRITE | system dialog font; verify PerMonitorV2 scaling | -
IME: toggle-ime in kanji.l:125,160, minibuf.l:262, LaTeX.l:56, filer.l:1532 | KEEP (IMM32 layer); misc.l init-misc-options enable-global-ime/*use-global-ime* (ActiveIMM, IE4 era) | REWRITE | drop global IME option | -
Dead modules (propose delete): niftylog.l (defs.l:220, app-menu.l:452), hanoi/life/gomoku/ccurve/tarai/Gates/pi (defs.l:173-217, app-menu.l:458-460), ispell.l (needs ispell.exe), keyboard.l:511-520 NEC PC-9800/Fujitsu OASYS VK names | REWRITE | delete or samples\ | -
Generated/doc files naming removed symbols (other areas): etc/Lisp, reference/reference.xml | REWRITE | regenerate after removals | -

No 32-bit literals found in lisp/*.l outside foreign.l/files.l/wip/winapi.l (grep: #xFFFFFFFF, ash 32, :int32, 2^31, 2038); only wip/winapi.l:16,207,243 and gentempl.l:65 (signed WS_POPUP style literal, harmless).
Startup chain that must survive removals: startup.l -> loadup.l (line 67 winhelp) -> optprop.l (winhelp vars, 682-746) -> paths.l (winhelp vars) -> estartup.l (~/.xyzzy). Nothing else in lisp/ calls DDE, den8 or c-callable beyond the files above.
