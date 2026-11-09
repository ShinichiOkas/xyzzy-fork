# Inventory: tests and harness (area: tests)

Format: file:line | what it is | category | modern replacement | BUG ids
Paths are relative to S:\work\develop\xyzzy-fork. Read-only survey; nothing was run.
Test files: 21 under unittest\ (about 8850 lines, 563 deftest). insdel.l is not named *-tests.l, so run-tests-helper.l never loads it.

unittest/simple-test.l:986 | child command line built with unquoted xyzzy.exe path and no -q/-config | REWRITE | quote the path; always pass -config <temp dir>; never touch user config | BUG-0029
unittest/simple-test.l:1036 | eval-in-another-xyzzy passes :environ only; the parent environment (incl. empty XYZZY* vars) is inherited | REWRITE | clear XYZZYINIFILE/XYZZYCONFIGPATH for the child unless given; force a temp config dir | BUG-0029
unittest/simple-test.l:1001 | read-results: reads error struct back with READ; fails with reader-error on format-string conditions | REWRITE | send condition type + message string (not the struct) over the file | BUG-0012
unittest/simple-test.l:688 | results go only to the *Test Results* buffer (get-report-buffer-stream); nothing on stdout | REWRITE | also write each result and a summary to stdout/a log file | BUG-0010
unittest/simple-test.l:826 | end of run only pops the buffer ("Report and die" comment, no exit); no exit code | REWRITE | kill-xyzzy with exit code 0/1 when run from the command line | BUG-0010
unittest/simple-test.l:853 | run-tests opens a modal message-box if a previous session is unfinished | REWRITE | no GUI prompt in the batch path; reset the session | BUG-0010
unittest/run-tests-helper.l:1 | batch driver: adds --run-all-tests, loads tests, calls run-all-tests, never exits | REWRITE | print summary, then (kill-xyzzy exit-code); write a log file | BUG-0010
run-tests.bat:17 | sets XYZZYHOME, clears XYZZYINIFILE/XYZZYCONFIGPATH with cmd "set X=" (correct: unsets); no -config | CONFIG-MOVE | add -config %TEMP%\xyzzy-test-<n>; delete the folder afterwards | BUG-0029
run-tests.bat:21 | runs xyzzy.exe -q -trace -l helper; the parent test process itself writes xyzzy.ini and .xyzzy.history to usr\<user>\wxp\ (seen: usr\shini\wxp\ files dated 2026-11-10) | CONFIG-MOVE | run the whole suite with a temp config folder (parent and children) | BUG-0029
run-tests.bat:1 | comments are in cp932 and print as mojibake; README.md:88 names run-tests-all.bat and unittest/README.md:15 names run-all.l (neither exists) | REWRITE | make comments ASCII/UTF-8; fix the docs | -
unittest/editor-tests.l:1028 | xyzzy-ini-path: compares with (user-config-path)/xyzzy.ini of the real config; needs the file to exist (same_file_p) | CONFIG-MOVE | run in a temp config dir; add test: empty XYZZYINIFILE is treated as unset | BUG-0029
unittest/editor-tests.l:1063 | user-config-path-startup-option and -XYZZYCONFIGPATH (3 tests): rely on child creating xyzzy.ini in temp dir, then delete-file | CONFIG-MOVE | keep (-config and XYZZYCONFIGPATH stay); add empty-value cases; tolerate no ini | BUG-0029
unittest/editor-tests.l:1102 | image-startup-option: child with -image, -q (no -config) | KEEP | add -config temp | -
unittest/editor-tests.l:1176 | about 24 process-command-line--* tests (to line 1345): children started without -q and without -config, so each loads and writes the real user config and site-lisp | CONFIG-MOVE | prepend -config <temp dir> in eval-in-another-xyzzy by default | BUG-0029
unittest/editor-tests.l:835 | kill-xyzzy-exit-code: 4 child launches without -q/-config, unquoted exe path | CONFIG-MOVE | add -q -config <temp dir>, quote the path | BUG-0029
unittest/editor-tests.l:1275 | process-command-line--kill: child without -q/-config (kill option); exe path unquoted | CONFIG-MOVE | add -q -config <temp dir> | BUG-0029
unittest/editor-tests.l:1303 | process-command-line--p opens the print dialog hook in a child | KEEP | informative: GUI-dependent; keep with temp config | -
unittest/editor-tests.l:756 | chdir-0..6 leave the cwd at system dirs; later children use relative paths (-Iunittest, unittest/editor-tests.l, lisp/foreign.l) that depend on child cwd | REWRITE | pass :exec-directory = system-root to children; restore cwd in tests (order dependence, unverified) | -
unittest/nanri-master-change-tests.l:46 | fix-saving-history-file: calls ed::load-history-file on the real user history (.xyzzy.history) | CONFIG-MOVE | load from a temp config dir only | BUG-0029
unittest/nanri-master-change-tests.l:395 | FFI float/double child test (strtod via msvcrt), -q, no -config; fails as reader-error when FFI errors | REWRITE | add -config temp; error text must come back readable | BUG-0012, BUG-0001
unittest/nanri-master-change-tests.l:753 | child test, -q, no -config (cdr circular printing) | CONFIG-MOVE | add -config temp | BUG-0029
unittest/nanri-master-change-tests.l:1293 | child test, -q, no -config (*load-pathname*; FIXME says it passes when no dump image) | CONFIG-MOVE | add -config temp; check against the dump image path | BUG-0029
unittest/foreign-tests.l:9 | defun-c-callable-stdcall with EnumWindows (c:defun-c-callable via eval) | REMOVE | delete test and EnumWindows entry (lines 3-7) | BUG-0001, BUG-0011
unittest/foreign-tests.l:25 | defun-c-callable-cdecl with qsort callback; qsort entry exists only for it | REMOVE | delete test and qsort entry (lines 25-32) | BUG-0001, BUG-0011
unittest/typespec-tests.l:120 | c-callable-typep, c-callable-check-type-1/2 use si:make-c-callable and si:c-callable type (6 tests to line 140) | REMOVE | delete with the c-callable typespec entry | BUG-0001
unittest/foreign-tests.l:677 | c-size-of-define-c-type expects size_t=4, time_t=4, ptrdiff_t=4 (pins the wrong x64 value) | REWRITE | expect 8 for size_t, time_t, ptrdiff_t after the foreign.l fix | BUG-0028
unittest/foreign-tests.l:699 | c-size-of-struct expects 20 for {int; double; char} (x64 C layout is 24) | REWRITE | expect 24 and 24 after the alignment fix | BUG-0031
unittest/foreign-tests.l:365 | GetModuleHandleA declared returning c:int (HMODULE is 64 bit); A-suffixed API in test | REWRITE | declare GetModuleHandleW, return c:intptr_t/void*; keep the access-violation check | -
unittest/foreign-tests.l:255 | MultiByteToWideChar via kernel32 with c:void * args (pointer as 64-bit chunk); last-win32-error | KEEP | already x64-clean; informative: only 64-bit pointer path covered here | -
unittest/foreign-tests.l:1 | whole file needs removal of c-callable parts; no test covers wip/winapi.l (MSG, WPARAM sizes) | DECIDE | add tests if lisp/wip moves to samples\ and winapi.l is kept | BUG-0030
unittest/ole-tests.l:4 | ole-method-null-and-empty uses ScriptControl (32-bit only COM class) | REWRITE | use another JScript host (e.g. WScript/JScript engine or htmlfile) or drop | BUG-0002
unittest/ole-tests.l:24 | with-ie and 5 tests (to line 106) use InternetExplorer.Application and google.co.jp (IE is gone on Win11; network needed) | REWRITE | use a local COM object or Edge/WebView; no network | -
unittest/ole-tests.l:17 | fix-ole-getmethod-immediate-array does a blocking GET to https://github.com/... | REWRITE | local file/loopback HTTP or drop; tests must not need network | -
unittest/ole-tests.l:118 | print-oledata branches on (featurep :windows-8) for the XMLHTTP type name | REWRITE | Win11 baseline: expect IServerXMLHttpRequest only; drop the feature test | -
unittest/lisp-tests.l:20 | os-version dispatch for Win2000/XP/Vista/7/8 only; also asserts (pathname-name (user-config-path)) = "w2k"/"wxp" (5 tests, dead on Win11 = 10.0) | REWRITE | replace with one Win11 test; user-config-path becomes ~/.xyzzy/ (name no longer "wxp") | -
unittest/lisp-tests.l:754 | fix-truename-5/-6 are #+wow64 (SysWOW64, Sysnative); wow64 feature is for 32-bit processes | REMOVE | delete both (x64 only, no WOW64 build) | -
unittest/lisp-tests.l:118 | machine-type test has #+x86 / #+x64 / #+ia64 branches | REWRITE | keep only "x64" | -
unittest/glob-tests.l:30 | directory-- wild test lists etc\*.exe (vacuous when none; today includes jdwrap.exe, extract.exe) | REWRITE | use a fixture folder under unittest\data, not etc\ | -
unittest/insdel.l:1 | test file not named *-tests.l, never collected by run-tests-helper.l (same pattern as foreign-tests before the fix) | REWRITE | rename to insdel-tests.l | BUG-0011

## Group summaries
- KEEP (informative, dropped to stay within 40 lines): foreign-tests.l:100-250 and :321 (msvcrt x64 DLL calls, div() struct in RAX) pass on x64; num-arith-tests.l:3 and system-tests.l:739 (2^32 bignum bounds; chunk-data as raw 64-bit address, no test for the 64-bit range); ole-tests.l:110 (WMI); environ-tests.l:15 (si:environ vs cmd set; sensitive to BUG-0029 empty vars).
- Tests for decided removals: no test touches DDE, WinHelp, den8view, den8util, jdwrap, or lisp/wip. Only c-callable has tests (foreign-tests.l:9, :25; typespec-tests.l:120-140; 8 tests). foreign.l's `define-c-struct`/`c-size-of` tests stay.
- Child-process tests: 36 tests spawn xyzzy.exe (31 eval-in-another-xyzzy calls in editor-tests.l, 3 in nanri-master-change-tests.l, 2 call-process tests; kill-xyzzy-exit-code launches 4 processes). Only the 6 tests at editor-tests.l:1036-1096 pass a config choice. All the others use the real config folder (usr\<user>\wxp\ today, ~/.xyzzy after the move). One default in simple-test.l (child gets -config <temp>) fixes all.
- Windows-version-specific: lisp-tests.l:20-102 (5 os-platform/config tests, none run on Win11), ole-tests.l:118, ole IE tests.
- Not found in tests: __asm, _M_IX86, GetVersionEx, fixed pixel sizes, 32-bit DLL names (the only DLLs are msvcrt/kernel32/user32), MAX_PATH, time_t in int.
- Undecided items: archiver tests: only comments in nanri-master-change-tests.l (tar32.dll/lzma/xz at lines 288-350, ctl3d32.dll); the tests there use fake filer functions (C:/dir), no real archiver. Code signing: none.

## Notes on BUG-0029 (harness environment)
- Harness cause confirmed from reading: run-tests.bat uses cmd `set X=` (unsets), correct. The failing runs used PowerShell `$env:X = ''`. A harness-side guard is cheap: eval-in-another-xyzzy can pass :environ with the vars removed or set explicitly; make-process :environ cannot unset (it adds), so the child inherits whatever the parent has. If the parent is started with empty vars, only the src/init.cc fix or an explicit child value helps.
- Real config touched by a plain test run: usr\shini\wxp\xyzzy.ini and .xyzzy.history (git-ignored via usr/). Not under unittest\data.
