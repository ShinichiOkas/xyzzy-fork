# Inventory: archiver (archiver, external tools, etc binaries)

Base: feature/x64-only, HEAD 8dc9230. Read-only survey. Line numbers are from the working tree at that commit.

Facts measured (COFF Machine field in the PE header; e_lfanew at 0x3C):
- x86 (0x014C), tracked in Git: etc/extract.exe, etc/gendic.exe, etc/gendicnm.exe, etc/jdwrap.exe, etc/tail-f.exe, etc/den8util.dll, etc/dxflush.dxa (a DLL), etc/libxpi.lib (member xpi.obj is 0x014C).
- x64 (0x8664), tracked in Git: src/zlib/Debug/zlib.lib, src/zlib/Release/zlib.lib (members checked).
- No archiver DLL (UNLHA32 etc.) and no UNLHA32.LIB is in the repo. .gitignore:138 ignores src/etc/unlha/UNLHA32.LIB.
- Callers of the archiver backend (Lisp API stays the same): lisp/filer.l:144,899,962,972,1005; lisp/files.l:439-466 (find-file-in-archive); lisp/about.l:39-43; builtins at lisp/builtin.l:1245-1251.

Format: file:line | what it is | category | modern replacement | BUG ids

## Archiver backend (C++)
src/arc-if.cc:54 | LoadLibrary of one of 12 archiver DLLs by bare name; all are 32-bit only | REWRITE | Run an external 7-Zip CLI as a child process; no in-process DLL | BUG-0004
src/arc-if.cc:43 | GetProcAddress of "<Prefix>OpenArchive" etc. (Common Archiver API) | REWRITE | Goes away with the DLL layer; one archiver-program interface | BUG-0004
src/arc-if.cc:96-181 | zip_open/zip_next_file emulation: parses UNZIP32 text listing by fixed column offsets | REWRITE | Parse `7z l -slt` output (UTF-8 key=value); delete emulation | BUG-0004
src/arc-if.cc:190-236 | UnzipInterface::patch_module patches UNZIP32 import table (MessageBeep); IsBadReadPtr | REWRITE | Delete with the DLL layer (IsBadReadPtr is deprecated) | BUG-0004
src/arc-if.h:124-211 | Interface classes naming ISH32, TAR32, UNARJ32J, UNLHA32, UNZIP32, ZIP32J, CAB32, UNRAR32, BGA32, YZ1, UnGCA32, JACK32, 7-zip32 | DECIDE | One program path + format table once archiver policy is set | BUG-0004
src/comm-arc.h:14-19 | `typedef long time_t` guard and FNAME_MAX32 512; time_t would be 4 bytes if the guard failed | REWRITE | Delete with the Common Archiver API headers | -
src/comm-arc.h:33-81 | pack(1) INDIVIDUALINFO/EXTRACTINGINFO structs: DWORD sizes, 512-byte names, DOS date/time | REWRITE | Delete; use 64-bit sizes and FILETIME from CLI output | BUG-0004
src/archiver.cc:131-208 | Hidden "ArcNotify" window receives WM_ARCEXTRACT progress (DWORD sizes, A APIs) | REWRITE | Read child stdout/exit code; progress to status bar | BUG-0004
src/archiver.cc:211-258 | ArchiverP::doit: in-process DLL call, catch Win32Exception, EnableWindow, tar32 key-state hack (253) | REWRITE | CreateProcessW with pipes and timeout; delete key-state hack | BUG-0004
src/archiver.cc:41-98 | docopy/copy_path build char* command lines with _ismbblead (ACP/cp932) and ad hoc quoting | REWRITE | Build a UTF-16 command line; correct CreateProcessW quoting | BUG-0004
src/archiver.cc:505-554 | Lha options "e -a1m1nx1jf0", create "a -d1n", remove "d -p1n", SFX "s" (UNLHA32 syntax) | DECIDE | LZH read/create depends on archiver policy (7-Zip cannot create LZH) | BUG-0004
src/archiver.cc:410-492 | Tar::extract uses TarGetArchiveType, gz/Z/bz2/lzma/xz naming, options "xfo" | DECIDE | 7-Zip handles tar/gz/bz2/xz; drop TAR32 naming code | BUG-0004
src/archiver.cc:714-756 | SevenZip: "-hide" (invalid), bare dest dir, cp932 list file (no -scsWIN) | REWRITE | `-o{dir}`, UTF-8 list with -scsUTF-8, drop -hide | BUG-0004
src/archiver.cc:758-772,975-1000 | Archiver ctor lists 12 formats; get() dispatches on DLL keywords | DECIDE | Format table keyed by suffix once policy is set | BUG-0004
src/archiver.cc:853-910 | list-archive: DOS date bitfields (obsolete_date), szAttribute strings from INDIVIDUALINFO | REWRITE | Fill list from 7z -slt (FILETIME, attributes) | BUG-0004
src/archiver.cc:1148-1202 | create-archive: process-wide SetCurrentDirectory, fopen temp list, PATH_MAX buffers | REWRITE | Child-process working dir; wide temp file; long-path aware | BUG-0004
src/archiver.cc:1205-1237 | convert-to-SFX: SetCurrentDirectory, LHA "s" command | DECIDE | Drop SFX, or 7-Zip SFX module if policy keeps it | BUG-0004
src/archiver.cc:1250-1266 | archiver-dll-version / archiver-dll-config-dialog (DLL's own dialog) | REWRITE | Version from exe version resource or `7z i`; drop config dialog | BUG-0004
src/msgdef.h:426 | Message text names "UNZIP32.DLL" | REWRITE | Neutral wording or delete with the UNZIP path | BUG-0004

## Archiver backend (Lisp)
lisp/defs.l:147-166 | *archiver-dll-list* (12 keywords) and DLL file names; exported | DECIDE | A program-path setting (default: none, optional) | BUG-0004
lisp/about.l:39-43 | About dialog loads all 12 DLLs to show their versions (long-operation) | REWRITE | Show only the configured archiver version | BUG-0004
lisp/verup.l:12-21 | version-up-xyzzy runs etc\extract.exe on an .lzh; dist.bat now makes .zip (dist.bat:40) | REWRITE | Expand the .zip with tar.exe (Win11 bsdtar) or PowerShell; no helper exe | BUG-0005

## etc binaries and their builds
src/etc/extract.cc:3-35 | extract.exe: imports Unlha (UNLHA32.LIB), `#pragma data_seg(".text")`, cp932 MessageBox text | DECIDE | Delete; verup uses tar.exe on .zip. Depends on LZH policy | BUG-0005, BUG-0006
projects/extract.vcxproj:67,83 | Links UNLHA32.LIB (not in repo); fails LNK1181; ProjectGuid duplicated with xyzzyenv | DECIDE | Delete project with extract.exe | BUG-0006, BUG-0027
etc/extract.exe | x86 tracked binary, depends on UNLHA32.DLL | DECIDE | Untrack; remove with extract.cc | BUG-0005, BUG-0017
etc/gendic.exe, etc/gendicnm.exe | x86 tracked prebuilt dictionary tools; no source/project in repo (only etc/README.gendic) | DECIDE | Propose REMOVE (orphan binaries) or rebuild from source if found | BUG-0017
etc/dxflush.dxa, etc/libxpi.lib, etc/xpi.h, src/xpi.cc | x86 plugin DLL and import lib; no reference in lisp/src; src/xpi.cc is in no vcxproj | DECIDE | Propose REMOVE (orphan x86 plugin SDK) | BUG-0017
lisp/tail-f.l:30-35 | tail-f runs etc\tail-f.exe through make-process | REWRITE | Rebuild tail-f.exe x64 (or Lisp timer reader); untrack binary | BUG-0017
src/etc/tail-f.cc:57-110 | tail-f.exe: A APIs, argv parser, 1 s Sleep polling, no manifest | REWRITE | Wide main, Unicode path, long-path manifest | -
projects/tail-f.vcxproj:25,31 | CharacterSet MultiByte (also extract, jdwrap, den8util vcxproj); RuntimeLibrary MT already set | REWRITE | CharacterSet Unicode + manifest; keep /MT x64-only | -
projects/etc.targets:7 | Post-build Copy into etc\ only for Release|Win32; never runs for x64 | REWRITE | Stage binaries into dist from build output, never into tracked etc\ | BUG-0026, BUG-0017
projects/etc.sln:3-4,7-12 | Header "Express 2013", 4 projects (extract, jdwrap, tail-f, den8util); x64 configs only | REWRITE | Keep only tail-f (and extract if policy keeps it) | BUG-0008
projects/etc.sln, projects/jdwrap.vcxproj, src/etc/jdwrap.cc, etc/jdwrap.exe | jdwrap project, source and x86 binary (used only by den8view) | REMOVE | Delete | BUG-0017
projects/etc.sln, projects/den8util.vcxproj, src/etc/den8util.cc, etc/den8util.dll | den8util project, source and x86 DLL | REMOVE | Delete | BUG-0003, BUG-0017

## Tracked binaries and packaging
src/zlib/Debug/zlib.lib, src/zlib/Release/zlib.lib (+ .pdb) | x64 libs tracked; .gitignore:131-135 un-ignores them; rebuilds dirty the tree | REWRITE | Build zlib from source in a vcxproj (or vcpkg); untrack outputs | BUG-0016, BUG-0007
dist.bat:45 | `xcopy etc` ships whole etc\ including tracked x86 exes; dist.bat:33,52 needs 7za in PATH | REWRITE | Copy only x64 build outputs; zip with tar.exe/7-Zip path | BUG-0017, BUG-0004
.gitignore:138, src/etc/unlha/README.txt | UNLHA32.LIB ignore rule and build instructions for it | DECIDE | Delete with extract.exe | BUG-0006

## Hits outside the area (marked archiver)
src/ctl3d.cc:25 | LoadLibrary("ctl3d32.dll"), a Win9x-era 32-bit DLL (archiver) | REWRITE | Delete ctl3d; Win11 visual styles/common controls v6 via manifest | -
src/sock.cc:43 | LoadLibrary("WSOCK32.DLL") and wsock32.lib (xyzzy.vcxproj:82,121) (archiver) | REWRITE | Link ws2_32 directly; drop dynamic Winsock 1.1 loading | -
src/sysdep.cc:11 | GetVersionEx/Win9x platform switch (also src/privctrl/privctrl.cc:70) (archiver) | REWRITE | Win11 baseline; drop version branching; manifest supportedOS | BUG-0013

## Group summary
- Common Archiver API surface (arc-if.*, comm-arc.h, archiver.cc DLL paths) is one unit: replace as a whole with a child-process backend; sites above are listed individually only where behaviour differs.
- All x86 files under etc\ are tracked and none can load in an x64 process except as child processes (exe). Only tail-f is a clear keeper.
- Not seen: any manifest in etc projects; any _M_IX86 or __asm in my area.
