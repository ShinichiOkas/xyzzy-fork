# Inventory: system (process, files, environment, configuration location, init)

Repo S:\work\develop\xyzzy-fork, HEAD 8dc9230. Read-only survey. src\*.cc are cp932 (read via iconv; line numbers unchanged).
Format: file:line | what it is | category | modern replacement | BUG ids

## Config location (~/.xyzzy) and BUG-0029
src\init.cc:171-206 | init_user_config_path: -config/XYZZYCONFIGPATH, else <exe>\usr\<user>\wxp created at run time | CONFIG-MOVE | %USERPROFILE%\.xyzzy via SHGetKnownFolderPath; one CreateDirectory; one-time copy from old usr\ | BUG-0029
src\init.cc:173-175,211-213,230-236 | getenv value used even when ""; empty ini name makes the folder itself the ini path | REWRITE | one helper: "" = unset for XYZZYCONFIGPATH and XYZZYINIFILE; add empty-env test | BUG-0029
src\init.cc:177-188,190-205 | missing override dir silently ignored; default path falls back to the exe folder when creation fails | CONFIG-MOVE | create/warn on override; fall back to %LOCALAPPDATA%\xyzzy or temp, never the exe folder | -
src\init.cc:101-147,262-264 | init_home_dir (ini homeDir, XYZZYHOME, HOME, HOMEDRIVE+HOMEPATH, else exe folder) runs AFTER config/ini path, yet reads [init] homeDir from that ini | CONFIG-MOVE | resolve profile dir first; keep ~/.xyzzy (profile) separate from Lisp home-dir (Qhome_dir); no exe-folder fallback | BUG-0029
src\conf.cc:10-85,480-690;init.cc:612 | all settings via Get/WritePrivateProfileStringA (ANSI, per-key write); one-time registry to ini migration, reg_delete_tree | CONFIG-MOVE | keep xyzzy.ini under ~/.xyzzy; W APIs or own UTF-8 reader; drop registry migration or fold into one-time copy | -
lisp\history.l:61-66,80,155 | history and resume files live in (user-config-path); 8.3 name fallback _xyzzy.his | CONFIG-MOVE | follows user-config-path automatically; drop the long-file-name check | -
src\xyzzycli.cc:497-517 | xyzzycli.ini read next to the exe (path, multipleInstances, options) | CONFIG-MOVE | read ~/.xyzzy\xyzzycli.ini first, exe-folder file as fallback | -
unittest\editor-tests.l:1028-1096;lisp-tests.l:20-110 | xyzzy-ini-path test reads the real user-config-path; lisp-tests assert w2k/wxp names | REWRITE | every child gets -config <tmp>; delete OS-named tests | BUG-0029
run-tests.bat:18-20;bytecompile.bat:11-13 | set XYZZYINIFILE= / XYZZYCONFIGPATH= (cmd clears them), XYZZYHOME=repo | CONFIG-MOVE | set XYZZYCONFIGPATH to a temp dir so tests never touch the user's ~/.xyzzy | BUG-0029

## Runtime writes into the install folder (must not happen for Program Files)
src\data.cc:2712-2725,2836;lisp\startup.l:13-17;init.cc:240-252 | no image found: startup calls dump-xyzzy, writing xyzzy.<wxp> (exe name + OS short name) next to the exe | REWRITE | build-time image only; fixed name not tied to OS; run-time fallback to per-user cache | -
src\fileio.cc:613-627 | unnamed-buffer auto-save file #unnamed.xxx created next to xyzzy.exe | REWRITE | write under user config/temp dir | -
src\except.cc:399-423 | crash log xyzzy.BUG written to the exe folder first, temp only as fallback | CONFIG-MOVE | write to ~/.xyzzy (or temp) first | BUG-0021
lisp\verup.l:14-21 | self-update extracts over the install folder with 32-bit etc\extract.exe (LZH) | DECIDE | depends on archiver and code-signing policy; impossible under Program Files | BUG-0005

## OS version checks, Win9x/NT4 remnants
src\sysdep.cc:73-139;sysdep.h:108-133 | wintype and windows_short_name for 32s/95/98/Me/NT/2000/XP/Vista; Win8 is named "7" (>= WIN7 tested first) | REWRITE | delete; no OS-derived names in paths | -
src\sysdep.cc:11;privctrl\privctrl.cc:70;xyzzycli.cc:18 | GetVersionEx (returns 6.2 without a manifest; deprecated) | REWRITE | RtlGetVersion plus manifest supportedOS | -
Win4p/Win5p/Win98p/WinNTp callers: menu.cc:62,788;mouse.cc:154,192;filer.cc:44,1675,3027;popup.cc:16;preview.cc:725;Window.cc:434,3233;dialogs.cc:875,902,938,1231;sysdep.cc:112;init.cc:913;vfs.cc:242 | branches always true or dead on Win11 (20+ sites) | REWRITE | keep the live arm, delete the other, delete the helpers | -
src\toplev.cc:718,1067-1076;ctl3d.cc:21 | !Win4p and !Win98p&&!Win5p branches never run; ctl3d32.dll loader never runs (init.cc:802) | REWRITE | delete branches and the ctl3d module | -
src\sysdep.cc:12-47;environ.cc:748-770 | machine_type x86/IA64, IsWow64Process via GetProcAddress, :wow64 feature; IA64 case lacks break and falls into UNKNOWN | REWRITE | x64 (arm64) only; remove process_type and :x86 :ia64 :wow64 | -
src\environ.cc:682-745;except.cc:428-433 | os-major/minor/build/csd, os-platform, :windows-95..8 features, crash report text | REWRITE | add :windows-10/11 via RtlGetVersion; fix Lisp users first: process.l:18,52 (command.com), filer.l:790, app-menu.l:385-390, dialogs.l:447 | -
src\targetver.h:11;menu.cc:26;projects\xyzzy.vcxproj:24,31,427 | _WIN32_WINNT 0x0502, WINVER 0x0500 shim; CharacterSet NotSet (ANSI APIs); no manifest, EmbedManifest false | REWRITE | 0x0A00; add manifest (PerMonitorV2, longPathAware, supportedOS); build UNICODE with W APIs | -

## Files, paths, limits
src\vfs.cc:98-461,223-225,306-323;vfs.h:11-44 | WINFS shim: every file API is the A form plus network-password retry; GetDiskFreeSpaceExA via GetProcAddress; MAX_PATH*2 share cache | REWRITE | W-API shim with \\?\ long paths; direct GetDiskFreeSpaceExW; keep credential retry | -
src\cdecl.h:34;pathname.h:4-6 | PATH_MAX 1024 bytes, WPATH_MAX 512 chars; ~130 fixed stack buffers (pathname.cc 42, fileio.cc 13, archiver.cc 9, dialogs.cc 6, glob.cc 5, filer/process/stream/com...), MAX_PATH in except/xyzzycli/doc/com | REWRITE | dynamic wide strings and \\?\ prefix, drop fixed limits (group) | -
src\mman.cc:33;mman.h:8 | mapf size is DWORD from GetFileSize (high part ignored): files over 4 GiB mis-map | REWRITE | GetFileSizeEx, 64-bit size | -
src\fileio.cc:473-509,523-527,654-659,728,833;pathname.cc:1217-1292 | FAT 8.3 backup/auto-save names (fatfs_basename), long-name probe, 8.3 short-name rename repair | REWRITE | delete 8.3 branches, always long names | -
src\environ.cc:942-971 | si:getenv/putenv through narrow CRT getenv/_putenv | REWRITE | _wgetenv/SetEnvironmentVariableW; keep empty-value semantics explicit | BUG-0029

## Process, IPC, sockets
src\process.cc:258,269,874,885 | call-process/make-process change the process-wide CWD around CreateProcess | REWRITE | rely on lpCurrentDirectory only (already passed) | -
src\process.cc:261,877,1442-1445,844 | CreateProcessA, ANSI env block, ShellExecuteExA via GetProcAddress; event HANDLE sent as u_int to xyzzyenv (xyzzyenv.cc:214) | REWRITE | CreateProcessW + CREATE_UNICODE_ENVIRONMENT; direct ShellExecuteExW; 64-bit handle text | -
src\listen.cc:157-181;listen.h:9-18;ipc.cc:14-17 | xyzzycli IPC: private window message + DuplicateHandle of file mapping; HWND table in a .shared section (global across sessions) | REWRITE | per-session named pipe/mapping when IPC is redesigned | -
src\sock.cc:43,139;stdafx.h:50;resolver.cc:188-205;sockinet.cc:77-82 | Winsock 1.1 via LoadLibrary WSOCK32.DLL, WSAAsyncGetHostBy*, inet_addr/gethostbyname, IPv4 only | REWRITE | Winsock 2 (ws2_32), getaddrinfo, IPv6 | -
except.cc:273,315,355;listen.cc:123,173;data.cc:793;dockbar.cc:512;arc-if.cc:203,208 | IsBad*Ptr probes (deprecated, unreliable) | REWRITE | SEH __try or explicit size checks | -

## Dead x86 / DDE / old ABI
src\except.cc:255-310,441-445 | _M_IX86 register and stack dump; x64 branch prints only RIP/RSP/RBP | REWRITE | delete x86 code; write a minidump or full x64 context | BUG-0021
src\bignum.cc:698-758;number.cc:206-233;dll.cc:452,611 | _M_IX86 inline asm (generic C branch exists), x86 FFI call path, c-callable stub (dll.cc:611) | REMOVE | delete x86 asm and c-callable per decision | BUG-0001
src\comm-arc.h:14 | typedef long time_t (4 bytes) vs 64-bit CRT time_t | REWRITE | use CRT time_t | -
src\init.cc:7,921;stdafx.h:14;xdde.cc:134-147 | Dde::initialize at startup, ddeml.h, xdde's own GetVersionEx | REMOVE | delete with src\dde.cc, xdde.cc/h | -
src\init.cc:793 | SetDllDirectory("") | KEEP | add SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32) | -
