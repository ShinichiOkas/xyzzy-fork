# BUG-0001 FFI（DLL 関数呼び出し・c-callable）が x64 版で未対応

- 状態: 一部修正済み（DLL 関数の呼び出しは x64 で動く。c-callable は未対応のまま）
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09
- 更新: 2026-11-10（呼び出し側を x64 で実装。方針「呼び出し側を x64 で実装。c-callable は保留」に従う）

## 症状（修正前）
x64 版では、DLL 関数の呼び出し（`c-function` / `define-dll-entry`）と、C から呼べる Lisp 関数（`c-callable` / `defun-c-callable`）を使うと次のエラーになる。

    x64 版では DLL 関数の呼び出しと c-callable は未対応です

## 修正（2026-11-10）
- src/dll.cc（x64 の funcall_dll）: 引数は、どれも 64 bit の値（float は下位 32 bit）を double の型の値として、可変長引数の関数ポインタ経由で呼ぶ。MSVC の x64 では、可変長の呼び出しで浮動小数点の引数を整数のレジスタにも複製するので、整数の引数も浮動小数点の引数も、位置にかかわらず届く。
- 戻り値: 整数（RAX）、double（XMM0）、float（XMM0 の下位 32 bit）。
- 制限: 引数は 16 個まで（超えると「x64 版では DLL 関数の引数は 16 個までです」）。
- lisp/foreign.l: ポインタの型を、x64 では 8 バイト（:uint64）にした（c-pointer-type）。以前は void* が long（4 バイト）に固定され、アドレスが切り詰められていた。
- src/msgdef.h: x64 のメッセージを、c-callable だけが未対応という文言にし、引数の上限のメッセージを足した。
- 確かめ:
  - MSVC の x64 で、レジスタとスタックの両方の引数、int・double・float の戻り値を確かめる小さなプログラムで、一致を確かめた（リポジトリには含めない）。
  - 全件テストで、foreign-tests.l の 43 件と、strtod（double の戻り値。nanri-master-change-tests.l）が通る。
  - float の戻り値は、Lisp の試験がまだ無い（上のプログラムでの確認のみ）。
  - 独立レビュー（読み取り専用）で、呼び出し経路（dll.cc）に指摘は無かった。

## 残り
- c-callable（init_c_callable、x64）は未対応（Ex64_ffi_unsupported）。
- 想定どおり失敗する試験（4 件）: typespec-tests.l の c-callable-typep、c-callable-check-type-1。foreign-tests.l の defun-c-callable-stdcall、defun-c-callable-cdecl。
- 関連する型幅の問題: BUG-0028（size_t・ptrdiff_t・time_t）、BUG-0030（lisp/wip/winapi.l の WPARAM・LPARAM・LRESULT・MSG）、BUG-0031（define-c-struct の整列）。

## 実測
- 修正前（2026-11-09、x64 Release の xyzzy）: 全件は 533 件中 4 件が失敗。FFI 由来は 3 件。
  - `c-callable-typep`（unittest/typespec-tests.l）
  - `c-callable-check-type-1`（unittest/typespec-tests.l）: 上記エラーで失敗
  - `fix-for-FFI-c-function-return-doubl/float-00`（unittest/nanri-master-change-tests.l）
  - 他に、ole-method-null-and-empty（環境依存）が 1 件。
- 修正後（2026-11-10、x64 Release の xyzzy、foreign テストを含む全件）: 578 件中 573 件が成功、5 件が失敗（想定どおり。上の「残り」の 4 件と、環境依存の ole 1 件）。strtod の試験は通るようになった。
- `(load-file "lisp/den8view.l")` は、トップレベルの `defun-c-callable` が読み込み時に評価されるため、上記エラーで失敗する（未修正。c-callable の対応で直す）。

## 参照箇所（場所のみ）
- src/dll.cc（funcall_dll の x64 の分岐、x64_arg_bits、call_proc_x64_int、call_proc_x64_double。init_c_callable の x64 の分岐）
- src/msgdef.h（Ex64_ffi_unsupported、Ex64_ffi_too_many_args）
- lisp/foreign.l（c-pointer-type、c-typespec-p、void* の定義）
