# BUG-0044 sjis の読みで、先行バイトのすぐ後の改行（81 0A など）が 1 つの Char に組まれ、行として数えられない

- 状態: 修正済み（2026-11-22、S4-0）
- 対象: x64 版（feature/x64-only）。x64 固有ではないと見られる
- 起票: 2026-11-22（BUG-0039 の調査中に見つけた）

## 症状
- sjis（cp932）として読むとき、先行バイト（0x81〜0x9F・0xE0〜0xFC）の次のバイトが 2 バイト目の範囲（0x40〜0x7E・0x80〜0xFC）でなくても、先行バイトと次のバイトを 1 つの Char（例 `81 0A` -> 0x810A）に組んでいた。次のバイトが改行（LF）のとき、その改行は行として数えられない。

## 実測（2026-11-22、修正前）
- バイト列 `61 81 0A 62 0A` を、符号を指定して（sjis）開いたときの、バッファの終わりの行番号: 2（期待は 3）。
- 同じファイルをファイルストリームで `read-line` したとき: 1 行目が 3 文字（`61`、0x810A、`62`）で、2 行目は nil（期待は `61 81` と `62` の 2 行）。
- 保存で戻るバイト列は変わらない（組んだ Char は 2 バイトで書き戻される）。

## 原因と直し方（2026-11-22）
- 原因: src/encoding.cc の `sjis_to_internal_stream::refill_internal`・`fast_sjis_to_internal_stream::refill_internal`、src/stream.cc の `readc_stream`（ファイル・ソケット）が、先行バイトの次のバイトを無条件に 2 バイト目として取っていた。
- 直し方: 次のバイトが 2 バイト目の範囲（`SJIS2P`）でなければ、先行バイトを単独のバイトとして出し、次のバイトは普通に読む（改行は改行として数える）。保存では単独の先行バイトが元の 1 バイトに戻る。
- 書き側の合わせ: 先行バイトと 2 バイト目でない値の 2 バイトの Char を、リーダーはもう 1 つに組まない。src/lprint.cc の `print_char`・`print_string` は、そのような Char を生のバイトで書かず、`\X8120` の形で書く（byte compile の出力 .lc に、こうした 2 バイトが約 4600 箇所あり、直さないと .lc を読んだ起動が止まった）。
- 試験: unittest/cp932-roundtrip-tests.l の `cp932-roundtrip-lead-line-end`・`cp932-lead-line-end-is-line-end`・`cp932-lead-line-end-file-stream`。
- 残り: src/byte-stream.cc の `byte_output_wstream::sflush`（バイト列から文字列を作る側）にも同じ組み方がある。行は数えないので手を付けていない。
