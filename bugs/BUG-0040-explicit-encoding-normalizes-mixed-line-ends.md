# BUG-0040 符号を指定して開くと、CRLF で始まり LF や CR が混ざるファイルが、保存で全行 CRLF に変わる

- 状態: 未検証（仕様かもしれない。S4-0 の網 unittest/cp932-roundtrip-tests.l で見つけた。試験は `:cp932-roundtrip-known-bugs` の機能フラグで切ってある）
- 対象: x64 版（feature/x64-only、f2342cb）。x64 固有ではないと見られる
- 起票: 2026-11-22

## 症状
- 行末が混ざったファイルを、符号に sjis を指定して開いて保存すると、最初の行末が CRLF のとき、残りの LF・CR も CRLF に変わる。
- 同じファイルを符号の自動判定で開いて保存すると、バイト列は変わらない（バッファの行末コードが 0 になり、CR がそのまま文字として残る）。

## 実測（2026-11-22、`ed::find-file-internal`・`save-buffer`、バイト列を読み戻した）
| 元のバイト列 | 符号を指定（sjis） | 自動判定 |
|---|---|---|
| `61 0D 0A 62 0A 63 0D 0A`（CRLF、LF、CRLF） | `61 0D 0A 62 0D 0A 63 0D 0A` | そのまま |
| `61 0D 0A 62 0D 63 0A`（CRLF、CR、LF） | `61 0D 0A 62 0D 63 0D 0A` | そのまま |
| `61 0A 62 0D 0A 63 0A`（LF で始まる） | そのまま | そのまま |
| `61 0D 62`（CR だけ） | そのまま | そのまま |
| `61 0D 0D 0A 62` | そのまま | そのまま |

## 再現
1. 一時ファイルに `61 0D 0A 62 0A 63 0D 0A` をバイナリで書く。
2. `(ed::find-file-internal ファイル t *encoding-sjis* nil t)` で開き、`(set-buffer-modified-p t)`、`(save-buffer)`。
3. バイナリで読み戻す。

試験: unittest/cp932-roundtrip-tests.l の `cp932-roundtrip-mixed-line-ends-explicit-known-bug`。

## 期待
- 利用者が決める。行末を最初の行で決めて全行をそろえるのが仕様なら、票を閉じて試験を仕様どおりの形に直す。バイト列を保つ方針なら、指定した符号でも自動判定と同じ扱いにする。
