# BUG-0057 ISO-2022-CN の書きが、CNS 11643 に無い Big5 の文字で、でたらめなバイト列を出す

- 状態: 修正済み（2026-11-22、S4 の段 2「W」）
- 対象: x64 版（feature/x64-only、5e40677）。x64 固有ではない（Win32 の時代から）
- 起票: 2026-11-22（W の前後で全部の符号の書きを比べて見つけた）

## 症状
- `*encoding-iso-2022-cn*`・`*encoding-iso-8bit-cn*` で保存するとき、内部の符号が Big5 の領域にある文字のうち、CNS 11643 の表（`big5cns_table`）に無いものは、でたらめなバイト列になる。
- たとえば半角の「､」（U+FF64、内部の符号 0xA4）1 文字を `*encoding-iso-2022-cn*` で書くと、`ESC $ ( 1` の後ろに 0xE5 0x3D が出る（7bit の符号なのに 0x80 以上のバイトが出る）。正しくは Big5 の文字として `ESC $ ( 0`（または `1`）の後ろに 7bit の 2 バイトが出る。
- 同じ種類の文字が、内部の符号の別の行（0x0100〜 など）にあっても同じ。`*encoding-iso-8bit-cn*` も同じ。

## 原因
- src/encoding.cc の `internal_to_iso2022_stream::refill` の `case ccs_big5`。`cc = big5cns_table[cc - CCS_BIG5_MIN];` で、元の `cc`（Big5 の符号）を表の結果で上書きする。表に無い（`CHAR_NONE`）ときは、その `CHAR_NONE` を `int_to_big5 (cc, c1, c2)` に渡してしまう。
- 16bit の Char のときは 0xFFFF として、32bit の Char のときは 0xFFFFFFFF として計算されるので、でたらめな値の違い方が W の前後で違った。

## 修正
- 表の結果は別の変数（`cns`）に受けて、表に無いときは元の `cc` を `int_to_big5` に渡す（Big5 の文字として書く）。
- 試験: unittest/char-width-tests.l の `char-width-iso-2022-cn-big5-char-without-cns`（出力が 7bit のバイトだけ）。
