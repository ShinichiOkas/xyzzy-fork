# BUG-0001 FFI（DLL 関数呼び出し・c-callable）が x64 版で未対応

- 状態: 方針待ち
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09

## 症状
x64 版では、DLL 関数の呼び出し（`c-function` / `define-dll-entry`）と、C から呼べる Lisp 関数（`c-callable` / `defun-c-callable`）を使うと次のエラーになる。

    x64 版では DLL 関数の呼び出しと c-callable は未対応です

## 実測（2026-11-09、x64 Release の xyzzy）
- run-tests 全体は 533 件中 4 件が失敗。このうち FFI 由来は 3 件。
  - `c-callable-typep`（unittest/typespec-tests.l）
  - `c-callable-check-type-1`（unittest/typespec-tests.l）: 上記エラーで失敗
  - `fix-for-FFI-c-function-return-doubl/float-00`（unittest/nanri-master-change-tests.l）
- `(load-file "lisp/den8view.l")` が上記エラーで失敗する（トップレベルの `defun-c-callable` が読み込み時に評価されるため）。
- `(load-file "unittest/foreign-test.l")` も同じエラーで失敗する。

## 再現手順
1. x64 版の `xyzzy.exe` で `(load-file "unittest/foreign-test.l")` を評価する。
2. `run-tests.bat unittest/typespec-tests.l` を実行する（c-callable の 2 件が失敗する）。

## 参照箇所（場所のみ）
- src/dll.cc:347-429（funcall_dll。x64 の分岐は 426-427 行）
- src/dll.cc:588-640（init_c_callable。x64 では未対応のエラーを返す）
- src/msgdef.h（Ex64_ffi_unsupported）

