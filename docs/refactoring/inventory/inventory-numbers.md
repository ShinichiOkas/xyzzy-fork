# inventory-numbers (area: numbers, strings, encodings, memory, generated symbols)

Base: HEAD 8dc9230, feature/x64-only. Read-only survey. Format: file:line | what | category | replacement | BUG ids.
"[outside]" = hit outside my starting list, found by repo-wide grep.

## Strings / encodings (what a UTF-16 core replaces)
src\charset.h:5-45 | Char is a private 16-bit multi-charset code (CP932 as SJIS values, BIG5, KSC, GB), not UTF-16 | REWRITE | Char = UTF-16 code unit; cp932/UTF-8 conversion only at file I/O | -
src\charset.h:383-396 | i2w/w2i 64K tables convert internal code to UCS-2 at every draw and API call | REWRITE | Drop once Char is UTF-16; keep tables only in file encoders | -
src\chtype.h:25-100 | CC_META_BIT 0x8000 and CCF_* 0xff00.. key codes live in the Char space | REWRITE | Separate key-code type; avoid clash with U+FF00 fullwidth forms | -
src\chtype.h:177-183 | SJISP/DBCP: char* strings assumed Shift_JIS bytes, Char>=256 means double byte | REWRITE | wchar_t strings; delete lead-byte logic | -
src\string.cc:46-210,1097-1120 | s2w/w2s turn internal code into CP932 bytes for every A API (71 pathname2cstr sites) | REWRITE | Pass UTF-16 to W APIs; keep s2w/w2s only in encoders | -
projects\xyzzy.vcxproj:24,31 | CharacterSet NotSet: no UNICODE, every unsuffixed Win32 call binds to the A variant | REWRITE | Define UNICODE/_UNICODE; W calls, wchar_t paths | -
src\msgdef.h:1-end [+69 src files] | Sources and message literals are CP932 bytes; no /utf-8; build needs ACP 932 | REWRITE | Convert to UTF-8 with /utf-8; wide message table | -
src\cdecl.h:34-35 | PATH_MAX 1024 bytes; char[PATH_MAX] buffers at about 120 sites | REWRITE | Dynamic wide path buffers (32767 for long-path manifest) | -
src\gen-ucs2tab.cc:440,529,581,831-887 | Build-time tables come from OS NLS (WideCharToMultiByte CP 932/949/950/936) | REWRITE | Generate from checked-in mapping data; host-independent | -
src\com.cc:34,94,124,197,267; DnD.cc:227-255,571,648,689; filer.cc:481 [outside] | MultiByteToWideChar(CP_ACP)/WideCharToMultiByte(CP_OEMCP) on SJIS bytes | REWRITE | Native wchar paths, no ACP/OEM step | -
src\kanji.cc:10-17,668-669,779-780,862,974-975,1033 | CP932 lead-byte scan, Greek/Cyrillic ranges, wc2cp932 for detect/convert | KEEP | File encodings stay cp932; table-driven, no OS code page call | -
src\gime.cc:140,152; src\guid.cc:4-11 | Active IMM (CLSID_CActiveIMM) with SetCompositionFontA/GetCompositionStringA | REWRITE | Plain IMM32 W calls inside the single IME layer | -
src\targetver.h:11 [outside] | _WIN32_WINNT 0x0502 (XP) | REWRITE | 0x0A00 (Win10/11 SDK baseline) | -
src\sysdep.cc:11,75-130; src\init.cc:240-251 [outside] | GetVersionEx tiers; windows_short_name "wxp" names the image xyzzy.wxp beside the exe | CONFIG-MOVE | Drop OS tiers; image/state under ~/.xyzzy/ | BUG-0013
src\utils.cc:261-264 | check_integer_format trims with b[-1]/b--: reads before the buffer, trailing trim never works | REWRITE | Trim with be[-1]/be--; port to wide string | -

## Numbers
src\number.h:37-38,47-56 | Fixnum is 30-bit (BITS_PER_LONG=32) inside a 64-bit word; most-positive-fixnum = 2^31-1 | REWRITE | intptr_t-based fixnum (62-bit); owner to keep or change the visible limit | -
src\number.h:7-12; number.cc:100-136 | llong_int.value is 4-byte long plus pad_; every int64 beyond 32 bits becomes a bignum | REWRITE | int64_t value, drop pad_; size/pointer values stay fixnum | BUG-0028
src\number.cc:206-231 | _M_IX86 __asm idiv in truncate(); x64 uses the C branch | REWRITE | Delete the asm branch | -
src\bignum.cc:698-758 | _M_IX86 __asm divide(); x64 uses the C fallback | REWRITE | Delete the asm branch | -
src\bignum.cc:4-7,89-260 + bignum.h:4-7 | 16-bit digits with 32-bit u_long accumulators (assumes sizeof long = 4) | REWRITE | 32-bit digits with uint64_t accumulators (speed, optional) | -
src\environ.cc:575 | get-internal-real-time = GetTickCount() & LONG_MAX, wraps at 49.7 days | REWRITE | GetTickCount64 or QueryPerformanceCounter | -
src\random.cc:118-128 | Seed held in long from time(0); legacy CryptAcquireContext(PROV_RSA_FULL) | REWRITE | BCryptGenRandom into uint32_t | -
src\cdecl.h:44,50,56 | u_int32_t = unsigned long, lChar = u_long, point_t = long (LLP64 only) | REWRITE | uint32_t/int32_t; decide point_t width (buffer limit INT_MAX, insdel.cc:475) | -
src\hash.cc:22,191; regex.cc:1698 | eq-hash and failure-cache hash cut the address to u_int | REWRITE | Mix all 64 bits of uintptr_t (low severity) | -
src\mman.h:8 + mman.cc:33 | mapf size is DWORD from GetFileSize low part: files of 4GB+ are silently truncated | REWRITE | GetFileSizeEx, size_t; clear error above INT_MAX chars | -
src\stream.cc:448,543,554; data.cc:2773,2801; archiver.cc:286 | fseek/ftell/_filelength use 32-bit long offsets | REWRITE | _fseeki64/_ftelli64, int64_t offsets | -
src\pane.cc:538 [outside] | make_fixnum(long(uintptr_t(procs))) cuts a 64-bit address | REWRITE | make_integer(uintptr_t) | BUG-0015
src\kbd.cc:1442,1506-1507 [outside] | HKL goes through int(LONG_PTR) into a fixnum | REWRITE | Keep sign-safe round trip or unsigned_pointer_value | BUG-0023
lisp\foreign.l:129-131 [outside] | size_t, time_t, ptrdiff_t mapped to 4 bytes (MSVC x64: 8) | REWRITE | 8-byte mapping on x64; fix foreign-tests expectation | BUG-0028

## Memory / image / generated symbols
src\gen-syms.cc:3054 + data.cc:2709,2762,2800 | dump_version is int from time(0): wraps in 2038 | REWRITE | int64_t or build-id hash; compare full value | BUG-0025
src\data.cc:1231-1240,2760-2801 | dump_header uses long fields, no sizeof(void*)/format tag; image max 2GB | REWRITE | Fixed-width header with pointer-width tag; 64-bit size | BUG-0025
src\data.cc:770-799 | Conservative stack scan: setjmp buffer may sit outside [&tem, initial_stack); IsBadWritePtr | REWRITE | RtlCaptureContext into a scanned local; drop IsBadWritePtr | -
src\init.cc:63 + ed.h:432 | initial_stack is the address of a local in the Application constructor, not the stack base | REWRITE | NtCurrentTeb()->NtTib.StackBase / GetCurrentThreadStackLimits | -
src\data.h:10-14,66-102 | GC bitmaps are 32-bit u_long, LDATA_MIN_SIZE 4 though x64 objects are 16B or more | REWRITE | uint64_t words and larger granule (memory/speed, optional) | -
src\data.cc:2587-2620,693-694; dataP.h:44; lisp.h:122; gen-syms.cc:891,983,989 | lc_callable object: image dump/restore, init_c_callable on load, GC mark, DEFUNs | REMOVE | Delete type and entries | BUG-0001
src\data.cc:378,590,2255-2265; dataP.h:40; lisp.h:130 | win32_dde_handle object type: cleanup, dump, restore | REMOVE | Delete type | -
src\gen-syms.cc:747-754,1634-1641,1893,1903,2513-2529; msgdef.h:387-399; dde.cc:349-368 | DDE conditions, DEFUNs, *dde-timeout*, *run-winhelp*, Edde_* messages, CP_WINANSI | REMOVE | Delete with DDE and WinHelp | -
src\comm-arc.h:14,274 + arc-if.h:127-199 | typedef long time_t; pack(1) 32-bit archiver DLL structs; DLL names | DECIDE | Depends on archiver policy (7-Zip/libarchive/LZH) | BUG-0004
src\dsfmt\dSFMT.cc:46,69 | HAVE_SSE2 branch not enabled in projects; generic path used | KEEP | Optional: define HAVE_SSE2 (x64 baseline has SSE2) | -

## Group summaries
Files checked without findings: sha2.cc, sha1.cc, md5c.cc (fixed-width uint32/u_int32_t, no asm), alloc.cc (VirtualAlloc, pointer_t masks, ok on x64), regex.cc (offsets in 16-bit Char slots are bounded, audit 135-140), guid.cc OLE GUIDs, number arithmetic in num-arith.d (int64_t based, ok).
Not enumerated (other areas): about 85 Get/SetWindowLong-class and SendMessage-to-int sites; about 71 pathname2cstr and 120 PATH_MAX sites; _M_IX86 blocks in dll.cc and except.cc.
Source for counts: grep over src (excluding zlib, unicode). No explicit *A API calls exist except IME/archiver names; the A binding comes from CharacterSet NotSet.
