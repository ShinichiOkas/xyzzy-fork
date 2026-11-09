# Inventory: build (build system, projects, scripts, zlib, manifest)

Facts checked: no Win32 configuration remains in any .vcxproj/.sln (BUG-0008 holds). No manifest file and no RT_MANIFEST exist in the repo. The built xyzzy.exe/xyzzycli.exe/xyzzyenv.exe are x64, DllCharacteristics 0x8160 (ASLR, high-entropy VA, NX, TS-aware; no GUARD_CF), Characteristics 0x22 (LAA). 51 files under src\ are cp932, 0 are UTF-8; the host is ja-JP ACP 932. No install.bat, no CI file. zlib is 1.2.8.

file:line | what it is | category | modern replacement | BUG ids
projects\*.vcxproj:2 ; xyzzy.sln:3-5 ; projects\etc.sln:3-5 | ToolsVersion 12.0 and VS Express 2013 headers (toolset already v143) | REWRITE | ToolsVersion Current; regenerate .sln for VS2022 | -
projects\*.vcxproj (9 files) | each project repeats all settings; no shared props file | REWRITE | one Directory.Build.props: v143, x64, /MT, Unicode, flags | -
projects\xyzzy.vcxproj:24,31 (+xyzzycli:23,30 xyzzyenv:24,31 gen-src1:25,32 gen-src2:25,32) | CharacterSet NotSet: ANSI build, no UNICODE/_UNICODE | REWRITE | CharacterSet Unicode; W APIs everywhere | -
projects\{extract,tail-f,jdwrap,den8util}.vcxproj:25,31 | CharacterSet MultiByte (_MBCS) for the etc tools | REWRITE | Unicode; wmain/wWinMain (extract, tail-f). jdwrap, den8util: REMOVE | -
projects\xyzzy.vcxproj:75,113 (+xyzzycli, xyzzyenv, gen-src*) | TreatWChar_tAsBuiltInType false: wchar_t is a typedef of unsigned short | REWRITE | /Zc:wchar_t on; fix overloads before moving to W APIs | -
projects\xyzzy.vcxproj (AdditionalOptions, absent) | no /source-charset or /utf-8; cp932 sources compile correctly only on an ACP 932 host | REWRITE | /source-charset:.932 now, or convert to UTF-8 and use /utf-8 | -
src\targetver.h:10-11 ; src\menu.cc:26 | _WIN32_WINNT 0x0502 (XP SP2) hides every Win10/11 API (DPI, long path) | REWRITE | _WIN32_WINNT 0x0A00 / NTDDI_WIN10_NI | -
projects\xyzzy.vcxproj:427 (EmbedManifest false, Release only) | no manifest file anywhere (xyzzycli, xyzzyenv too: default asInvoker); Release writes external Release\xyzzy.exe.manifest (asInvoker only); no comctl v6 | REWRITE | src\xyzzy.manifest via ApplicationManifest, embedded: PMv2 DPI, longPathAware, Win10/11 supportedOS, comctl6 | -
projects\xyzzy.vcxproj:68-69,106-109 | BufferSecurityCheck false, /Gs, OmitFramePointers; built exe has no GUARD_CF | REWRITE | /GS, /guard:cf, /sdl; keep /EHa (:71,104); check except.cc stack-overflow code | -
projects\xyzzy.vcxproj:84-89,123-130 | no explicit /DYNAMICBASE /HIGHENTROPYVA /LARGEADDRESSAWARE /CETCOMPAT /guard:cf; relies on v143 defaults | REWRITE | set explicitly: RandomizedBaseAddress, GuardCF, CETCompat; check dump-image is ASLR-safe | -
projects\xyzzy.vcxproj:61,67,74,110,112 | CallingConvention StdCall (no effect on x64); MinimalRebuild (Debug) and BrowseInformation (.bsc) | REWRITE | remove all three (MinimalRebuild conflicts with /MP) | -
projects\xyzzy.vcxproj:60,97 | defines DEBUG_GC also in Release; WIN32, _MT, HAVE_DIMM_H (Active IMM, src\gime.cc:9-26) | REWRITE | DEBUG_GC Debug only; drop _MT/HAVE_DIMM_H with the IME layer | -
projects\xyzzy.vcxproj:82,121 | links wsock32.lib (Winsock 1 import) while sock.cc uses Winsock2; mpr, imm32 | REWRITE | ws2_32.lib; imm32 stays behind the IME layer | -
projects\xyzzy.vcxproj:83,122 | zlib.lib taken from src\zlib\<Configuration>, a tracked prebuilt library | REWRITE | build zlib from source as a /MT x64 project (or vcpkg static) | BUG-0016
src\zlib\zlib.h:40 ; src\zlib\zconf.h | zlib 1.2.8 (2013); used only by src\gzip.cc; later releases fix CVE-2018-25032, CVE-2022-37434 | REWRITE | zlib 1.3.1 | BUG-0016
src\zlib\Debug\zlib.lib,.pdb ; src\zlib\Release\zlib.lib,.pdb | tracked x64 binaries (0x8664) of 1.2.8, rebuilt 2026-11-09; .gitignore has *.pdb but they were force-added | REWRITE | untrack; produce in the build tree | BUG-0016
misc\update_zlib.bat:7-27 | needs VS120COMNTOOLS and vsvars32 (x86 tools), curl over http, zlib128.zip, nmake, 7za | REWRITE | x64 script, https plus hash check; or project/vcpkg; or drop | BUG-0007
build.bat:6-23 | x64 build of xyzzy.sln only; etc.sln is never built; no -requires VC.Tools.x86.x64; errors only say "Build Tools" | REWRITE | add etc tools (extract, tail-f) to xyzzy.sln, or drop them; require the x64 VC component | BUG-0026
projects\etc.targets:7 | AfterBuild copy runs only for Release|Win32, so it never runs now | REWRITE | Release|x64, or remove with the tracked etc binaries | BUG-0026, BUG-0017
etc\extract.exe, tail-f.exe, gendic.exe, gendicnm.exe | tracked 32-bit PE (i386) tools; gendic* have no source in the tree | REWRITE (extract, tail-f); DECIDE (gendic*) | rebuild x64 in the build; gendic*: delete or restore source | BUG-0017, BUG-0005
etc\den8util.dll ; etc\jdwrap.exe | tracked 32-bit PE; den8view parts | REMOVE | delete with den8view | BUG-0003
projects\den8util.vcxproj,.filters ; projects\jdwrap.vcxproj,.filters ; etc.sln:7-10,17-20,25-32 | project files and solution entries of removed tools | REMOVE | delete the 4 files and the sln entries | BUG-0003
projects\xyzzy.vcxproj:159,258,260,409 ; xyzzy.vcxproj.filters:93,306,366,508 | dde.cc, winhelp.cc, xdde.cc, xdde.h items | REMOVE | delete the items | -
projects\xyzzy.vcxproj:163 ; xyzzy.vcxproj.filters:102 | dll.cc item (stays; c-callable part goes) | REWRITE | keep file, strip c-callable | BUG-0001
src\xyzzycli.rc:2 | includes ddeml.h; xyzzycli.cc has no DDE call | REMOVE | delete the include | -
projects\extract.vcxproj:67-68,83-84 ; src\etc\unlha\README.txt:1-4 | links UNLHA32.LIB (32-bit import lib, fetched by hand); extract.exe serves verup | DECIDE | depends on archiver policy (7-Zip / libarchive / LZH) | BUG-0006, BUG-0004, BUG-0005
etc\libxpi.lib ; etc\xpi.h | tracked i386 import library for XPI plug-ins (src\xpi.cc); .gitignore lists it but it is tracked | REWRITE | x64 lib or drop the plug-in ABI; x86 plug-in DLLs cannot load | -
src\etc\extract.cc:3 ; src\etc\tail-f.cc:4 | #pragma data_seg(".text") puts writable data in the code section | REWRITE | remove | -
src\etc\extract.cc, tail-f.cc ; src\xyzzycli.cc:109,415,500 ; src\xyzzyenv.cc | A APIs, IsDBCSLeadByte, GetCommandLine, MAX_PATH buffers | REWRITE | wWinMain/wmain, W APIs, long-path aware | -
src\xyzzycli.cc:16-18 | GetVersionEx check for Win98 (>= 4.10) before the foreground-lock call | REWRITE | drop the check; use AllowSetForegroundWindow | BUG-0013
src\xyzzycli.cc:500-513 | xyzzycli.ini is read from beside the exe (GetPrivateProfile, A) | CONFIG-MOVE | read from ~/.xyzzy\ (installed copy may be read-only) | -
src\sysdep.cc:73-135 ; src\init.cc:199,250 | Win32s/Win9x/Me/XP branches remain; windows_short_name "wxp" forms the user-config path | CONFIG-MOVE | drop OS names from the path; use ~/.xyzzy | BUG-0013
bytecompile.bat:9-11 ; run-tests.bat:17-18 | clear XYZZYINIFILE/XYZZYCONFIGPATH to isolate the run | CONFIG-MOVE | with ~/.xyzzy default, set XYZZYCONFIGPATH to a non-empty temp dir, else runs write the real home config | BUG-0029, BUG-0010
dist.bat:19-60 ; archive.bat:19-50 | zip with external 7za.exe; copies root *.exe, etc\ (x86 tools) and lisp\ (includes wip); no signing step; dist.bat tags git | DECIDE | Compress-Archive/tar or 7-Zip; signtool step if signing; exclude lisp\wip | BUG-0017
README.md:88,115-131 ; docs\README.md:7 | claims Windows XP SP3+, VS Express 2013, msysGit, 7za, and a run-tests-all.bat that does not exist | REWRITE | Windows 11 x64, VS2022, current scripts | -
(repo root, absent) | no install.bat (owner rule: environment from install.bat) | REWRITE | add install.bat: check VS2022 x64 tools, fetch/build zlib, then build | -
.gitignore:36-46,95-110 | no .vs/ or UpgradeLog.htm (both show as untracked) | REWRITE | add .vs/, UpgradeLog*.htm | -
src\xyzzy.rc:5-12 | includes afxres.h (MFC) and #pragma code_page(932) | REWRITE | winres.h; code_page 65001 after UTF-8 conversion | -
projects\extract.vcxproj:17 ; projects\xyzzyenv.vcxproj:14 | same ProjectGuid {2C956011-...} in two projects (extract is in etc.sln, xyzzyenv in xyzzy.sln) | REWRITE | new GUID for one | BUG-0027
src\bignum.cc:698-758 ; number.cc:206-229 ; dll.cc:346,452,611 ; except.cc:255-310,441 (build) | _M_IX86 branches: x86 __asm, x86 stack call, c-callable thunk (dll.cc:611), x86 register dump | REWRITE / REMOVE (c-callable) | keep the x64/C branch only; delete c-callable | BUG-0001, BUG-0021
