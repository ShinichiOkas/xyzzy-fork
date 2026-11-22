# BUG-0041 code-char が 16bit に切り詰める

- 状態: 修正済み（2026-11-22、S4-k）
- 対象: x64 版（feature/x64-only、59f7cfe）。x64 固有ではない（Win32 の時代から）
- 起票: 2026-11-22（S4 の設計の調査 docs/refactoring/s4-char-design.md §12 の F1）

## 症状
- `(code-char #x10041)` がエラーにも nil にもならず、`#\A` を返す（下位 16bit だけを使う）。
- `(code-char -1)` は `#\xffff` を返す。
- バイトコードにコンパイルした `(code-char n)` も同じ（`BCcode_char`）。

## 原因（コード上の推測）
- src/char.cc の `Fcode_char` が `make_char (Char (fixnum_value (code)))` で、`Char`（16bit）に切り詰めてから文字を作る。src/bytecode.cc の `BCcode_char` も同じ式を持つ。

## 参照箇所
- src/char.cc の `Fcode_char`
- src/bytecode.cc の `BCcode_char`

## 修正
- `code-char` は、文字の値の範囲を検査してから作る。範囲外は **nil**（CL の規則: 実装に無い文字は nil。同じ char.cc の `unicode-char` も範囲外は nil）。
  - 0 以上 CHAR_LIMIT 未満はテキストの文字。
  - KEY_BASE から始まる値（キー。S4-k）は、実在するキー（メタ＋ASCII、機能キー、メタ＋機能キー）だけ。KEY_BASE に修飾の無い文字を足した値や、割り当ての無い番号は nil。
  - 負の値は nil。long（32bit）を超える値は、これまでどおり range-error（unittest/fixnum-tests.l の fixnum62-code-char-out-of-long-range が決めている）。整数でないものは type-error。
- `Fcode_char` とバイトコードの `BCcode_char` は同じ関数 `code_char (int64_t)`（src/char.h）を通す。
- 試験: unittest/keycode-tests.l の keycode-code-char-out-of-range、keycode-code-char-out-of-range-compiled ほか。
- 設計書 §3.6 は「範囲外のエラー」と書いていたが、§8.1（V の後）と `unicode-char` に揃えて nil にした。
