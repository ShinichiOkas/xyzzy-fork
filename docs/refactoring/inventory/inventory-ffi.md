# Inventory: ffi (FFI and ABI: C types, calls, structs)

Repo S:\work\develop\xyzzy-fork, branch feature/x64-only, HEAD 8dc9230. Read-only survey.
Format: file:line | what it is | category | modern replacement | BUG ids

## c-callable (REMOVE, decided)
src/dll.cc:29-39,604-609 | make_c_callable allocator and funcall_c_callable trampoline | REMOVE | delete together with lc_callable | BUG-0001
src/dll.cc:611-805 | x86-only c-callable: opcode-patched push/call/ret stubs, __stdcall thunks, VirtualProtect RWX | REMOVE | none; no callbacks remain | BUG-0001
src/dll.cc:806-813,815-863 | x64 init_c_callable error stub, check_fn, si:make-c-callable | REMOVE | delete; keep check_calling_convention only for make-c-function | BUG-0001
src/dll.h:138-216 | lc_callable class, INSN_SIZE, accessors, make/init/funcall declarations | REMOVE | delete | BUG-0001
src/eval.cc:110-111,682-690,799-803 and src/chunk.cc:412-413 | c-callable cases in function designator, eval, funcall, cast_to_int64 | REMOVE | delete the cases | BUG-0001
src/pred.cc:139-140,323,332,353-357 | c-callable in type-of, functionp, compiled-function-p, *c-callable-p | REMOVE | delete the cases | -
src/lprint.cc:250,1592-1601,1803-1805 | #<c-callable ...> printer | REMOVE | delete | -
src/data.cc:693-694,2587-2621 and src/dataP.h:44 and src/lisp.h:122 | GC mark, dump/undump (rdump calls init_c_callable), Tc_callable tag | REMOVE | delete; the image version changes (rebuild xyzzy.wxp) | BUG-0001
src/gen-syms.cc:891,983,989 and src/msgdef.h:545-546 | *c-callable-p, make-c-callable, si:c-callable symbol, Ex64_ffi_unsupported message | REMOVE | delete (keep Ex64_ffi_too_many_args at msgdef.h:547) | BUG-0001
lisp/builtin.l:507,586 and lisp/typespec.l:131 and lisp/help.l:259-260 and etc/Lisp:3155,3291 | builtin declarations, si:c-callable typespec, help text, symbol index (condition.l has no entry) | REMOVE | delete the lines | -
lisp/foreign.l:17,206-224 | defun-c-callable, *defun-c-callable, exports, indent hooks | REMOVE | delete | BUG-0001
unittest/typespec-tests.l:120-139 | c-callable typep and check-type tests (3 tests; 2 fail by design on x64) | REMOVE | delete | BUG-0001
unittest/foreign-tests.l:1-60 | EnumWindows and qsort callback tests plus their two define-dll-entry forms | REMOVE | delete; the other 2 expected failures disappear too | BUG-0001,BUG-0011
lisp/den8view.l:456-466,587 | stdcall-decorated names (_x@8) and a defun-c-callable callback | REMOVE | goes with den8view removal | BUG-0001,BUG-0003
lisp/wip/hellowin.l:37 and lisp/wip/fractal.l:30 and lisp/wip/turtle.l:66 | c-callable WNDPROC samples (fail at load on x64) | REMOVE | delete, or move to samples/ without callbacks | BUG-0030
lisp/wip/winapi.l:51-53,122-128,156-181 | WPARAM/LPARAM/LRESULT 4 bytes, MSG 32 not 48 bytes, LONGLONG as :double, A-suffixed APIs | REMOVE | goes with wip; if kept in samples use 64-bit types and W APIs | BUG-0030

## x86 code, C type table, call path (dll.cc, dll.h, foreign.l)
src/dll.cc:200-219,294-344,452-523 | x86 stack-push path: calc_vaarg_size, call_proc, push_arg, push_vaarg, _M_IX86 branch | REWRITE | delete x86 branch, keep x64 path | BUG-0001
src/dll.cc:346-442,524-601 | x64 call: every arg as double via variadic prototype, 16-arg cap, float via low bits | KEEP | works and tested; an asm thunk would lift the cap | BUG-0001
src/dll.cc:83-151,221-232 and src/dll.h:44-72,131-136 | scalar-only C type ids, stdcall/cdecl flag, u_short arg_size, u_char nargs; no pointer/bool/struct by value | REWRITE | one type table with pointer/intptr; drop convention and arg_size | BUG-0001
src/data.cc:2504-2585 | DLL module/function dump; undump reloads by ANSI name; arg_size/nargs layout in image | REWRITE | dump name+signature only; LoadLibraryExW at undump | -
src/dll.cc:64-72,240-242 | LoadLibrary/GetModuleHandle via ANSI w2s, bare-name search path | REWRITE | LoadLibraryExW with LOAD_LIBRARY_SEARCH_DEFAULT_DIRS; GetProcAddress stays ANSI | -
src/chunk.cc:86-98,313-395,522-549 | string chunks convert only to/from ANSI (cp932); no UTF-16 chunk strings | REWRITE | add wide-string make/pack/unpack for W APIs | -
src/chunk.h:9 and src/chunk.cc:28-37,147,209,317,527 and src/number.cc:67-82 | chunk size is int; offsets read as 32-bit u_long | REWRITE | size_t sizes; offsets via pointer-width reader | -
src/chunk.cc:110,137 and src/number.cc:15-24,123-141 | addresses reach Lisp as uint64; fixnum is 32-bit long so most x64 addresses become bignums | KEEP | correct; widen fixnum only if the object model changes | -
lisp/foreign.l:74-75 and src/sysdep.cc:149-167 and src/environ.cc:748-767 | c-pointer-type tests :x64, which comes from GetNativeSystemInfo (host CPU, no ARM64 case); :wow64 left over | REWRITE | #ifdef _M_X64 sets :x64; pointer type fixed 64-bit; drop wow64 | BUG-0001
lisp/foreign.l:129-131 and unittest/foreign-tests.l:639-686 | size_t u_int, time_t long, ptrdiff_t int are 4 bytes; tests pin the wrong 4 | REWRITE | 64-bit size_t/ptrdiff_t/time_t; fix expectations | BUG-0028
lisp/foreign.l:70,281-282,299-307 and unittest/foreign-tests.l:690-706 | define-c-struct alignment from first member or size, not max member alignment | REWRITE | per-type alignment in c-type-definition | BUG-0031
lisp/foreign.l:139-199 | define-dll-entry default :stdcall, vaarg rejected for stdcall, struct by value rejected | REWRITE | accept and ignore :convention; decide struct by value | BUG-0001
unittest/foreign-tests.l:379-383 (and the 12 "msvcrt" entries) | sprintf varargs test needs msvcrt.dll; ucrtbase exports no sprintf | KEEP | keep msvcrt.dll (system DLL on Win11) | -
src/dll.cc:273-292 | last Win32 error saved after each call | KEEP | none | -

## Outside the list (area: ffi)
src/eval.cc:464-508 and src/lisp.h:144-148 and src/gen-syms.cc:2808 | every builtin is called through a 16-arg __stdcall prototype (UB, relies on caller cleanup) | REWRITE | generated arity-typed thunks; drop __stdcall | -
src/data.cc:776-800 and src/init.cc:63 | conservative stack scan: setjmp spill placement is compiler-dependent, IsBadWritePtr, end=&local | REWRITE | RtlCaptureContext into scanned buffer; TEB stack base; VirtualQuery | -
src/number.cc:206-232 and src/bignum.cc:698-758 | x86 inline asm (idiv, 16-bit div) with portable #else | REWRITE | delete the asm branches | -
src/except.cc:255-310,441-453 | x86 register/stack dump; x64 prints only RIP/RSP/RBP | REWRITE | drop x86; StackWalk64 or MiniDumpWriteDump | BUG-0021
src/lprint.cc:113,441 | object address printed through u_int, truncating the 64-bit pointer (not in audit TSV) | REWRITE | print uintptr_t / %p | -
src/pane.cc:538 and src/xpi.h and src/xpi.cc | si:plugin-arg returns function-table address through 32-bit long; xpi plugins are 32-bit DLLs | REWRITE | make_pointer_integer; plugins must be x64 | BUG-0015
src/targetver.h:11 and projects/xyzzy.vcxproj:24,31 | _WIN32_WINNT 0x0502 (XP) and CharacterSet NotSet (all unsuffixed Win32 calls are A) | REWRITE | 0x0A00; UNICODE and W APIs | -
src/sysdep.cc:10-11 and src/privctrl/privctrl.cc:70 and src/xyzzycli.cc:18 | GetVersionEx without manifest reports 6.2 on Win11 (xdde.cc:141 goes with DDE) | REWRITE | manifest supportedOS plus RtlGetVersion, or drop checks | -
src/gen-syms.cc:3054 and src/random.cc:122 | time(0) kept in int dump_version and long seed (2038) | REWRITE | int64 / uint32 | BUG-0025
src/arc-if.h:127-211 and src/comm-arc.h:14-81,360 and src/archiver.cc:974-986 and src/ctl3d.cc:25 | 32-bit archiver DLL ABI: pack(1) structs, long time_t, 32-bit DLL names; ctl3d32 | DECIDE | archiver policy (ctl3d32 is plain removal) | BUG-0004,BUG-0005,BUG-0006

## Not found / checked clean
- No GetWindowLong/SetWindowLong (non-Ptr) or DWL_/GCL_ index uses remain in src.
- lisp/condition.l has no c-callable or FFI condition entries.
- src/gen/ is git-ignored (generated); msgcode.h, symtable.cc follow from msgdef.h/gen-syms.cc.
- The audit TSV (bugs/audit/x64-pointer-width-2026-11-09.tsv) has no row for lprint.cc:441.
