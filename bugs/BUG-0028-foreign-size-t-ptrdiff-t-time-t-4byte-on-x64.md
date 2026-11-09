# BUG-0028 lisp/foreign.l の size_t・ptrdiff_t・time_t が x64 で 4 バイトに写像されている（MSVC の x64 では 8 バイト）

- 状態: 未修正（重大度: 中。実害は未確認）
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-10（BUG-0001 の修正中に見つけた）
- 関連: BUG-0001（FFI の x64 呼び出し側）

## 症状（コード上）
- lisp/foreign.l:129 `(*define-c-type u_int size_t)`（4 バイトの u_int に写像）
- lisp/foreign.l:130 `(*define-c-type long time_t)`（4 バイトの long に写像）
- lisp/foreign.l:131 `(*define-c-type int ptrdiff_t)`（4 バイトの int に写像）
- 上の型を引数に持つ define-dll-entry は、x64 で値の上位 32 bit を送れない。例: qsort の num と width（unittest/foreign-tests.l の defun-c-callable-cdecl）。

## 実測（2026-11-10、MSVC の cl・x64・/O2、VS2022 BuildTools）
- sizeof(void*) = 8
- sizeof(size_t) = 8
- sizeof(ptrdiff_t) = 8
- sizeof(time_t) = 8
- sizeof(off_t) = 4、sizeof(clock_t) = 4、sizeof(long) = 4、sizeof(int) = 4、sizeof(wchar_t) = 2（現在の写像と一致）
- 計測に使ったプログラムは作業用で、リポジトリには入れていない。

## テスト
- unittest/foreign-tests.l の c-size-of-define-c-type は、size_t・time_t・ptrdiff_t の期待値を 4 としている。x64 の値（8）と一致しないため、この期待値も直す対象。

## 影響（推論）
- x64 で size_t・ptrdiff_t・time_t を引数に使う DLL 関数は、値の上位 32 bit が失われる見込み。
- 戻り値として受けた場合も、上位 32 bit が落ちる見込み。
- 実際に誤った結果が出る例は、まだ確かめていない。

## 参照箇所
- lisp/foreign.l:129-131
- unittest/foreign-tests.l（c-size-of-define-c-type）

## 追記（2026-11-10、独立レビューでの推論。実行はしていない）
- x64 の DLL 呼び出しの経路（src/dll.cc の x64_arg_bits と戻り値の処理）に当てはめると、次のようになる。
  - size_t の引数に 4294967296 を渡すと、cast_to_long（32 bit）を通るため 0 になる。
  - ptrdiff_t の引数に 2147483648 を渡すと、-2147483648 になる。
  - size_t の戻り値 0x100000000 は、u_long（4 バイト）で読むため 0 になる。
- 試験の注記: unittest/foreign-tests.l の c-size-of-define-c-type に、この票の注記を入れた。期待値（4）は、直すまで誤ったまま。
- time_t（lisp/foreign.l:130）も同じ経路を通る。
