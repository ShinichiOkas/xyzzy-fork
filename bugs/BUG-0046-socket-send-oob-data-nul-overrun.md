# BUG-0046 socket-stream-send-oob-data が、確保した領域を 1 バイト超えて書く

- 状態: 修正済み（2026-11-22）
- 対象: x64 版（feature/x64-only、59f7cfe）。x64 固有ではない（Win32 の時代から）
- 起票: 2026-11-22（S4-c の設計の調査 docs/refactoring/s4-char-design.md §12 で見つけた）

## 症状
- `socket-stream-send-oob-data` を呼ぶと、スタック（alloca）に確保した領域の末尾から 1 バイトはみ出して書く。

## 原因（コード上の推測）
- src/stream.cc の `Fsocket_stream_send_oob_data` が `alloca (w2sl (string))` で確保し、`w2s (s, string)` で書く。`w2s` は文字列の後ろに NUL を 1 バイト書くので、`w2sl` の値では 1 バイト足りない。

## 参照箇所
- src/stream.cc の `Fsocket_stream_send_oob_data`
- src/string.cc の `w2s (char *, const Char *, size_t)`（末尾で `*b = 0`）

## 修正
- `alloca (l + 1)` にして、NUL の 1 バイトぶんを確保する（送るのは `l` バイトのまま）。
- 試験は足していない。OOB データの送信には接続済みのソケットと、受け側で OOB を受ける相手が要り、スタックの 1 バイトのはみ出しは値として観測できない（試験の仕組みでは確かめられない）。