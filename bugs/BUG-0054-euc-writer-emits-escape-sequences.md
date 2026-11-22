# BUG-0054 EUC-JP の書きが、EUC にない文字で ISO-2022 のエスケープ列を出す。EUC-KR の書きが é を e にする

- 状態: 未修正（P1 の変換器は、EUC（エスケープなし）では符号にない文字を `?` にする。今の読み書きの経路は P1 で切り替えていない）
- 対象: feature/x64-only。x64 固有ではない
- 起票: 2026-11-22
- 出所: P1（変換器）で今の書きと突き合わせたとき

## 症状
- `*encoding-euc-jp*` で書くとき、EUC-JP にない文字（例: U+00A4 ¤）が、ESC , A $ ESC ( B のようなエスケープ列を含むバイトになる。EUC-JP のファイルにエスケープ列が入る。
- `*encoding-euc-kr*` で é（U+00E9）を書くと、バイトが `e`（0x65）になる。

## 実測（2026-11-22）
| 式 | 結果 |
|---|---|
| `(map 'list #'char-code (convert-encoding-from-internal *encoding-euc-jp* (string (unicode-char #xa4))))` | `(27 44 65 36 27 40 66)` |
| `(map 'list #'char-code (convert-encoding-from-internal *encoding-euc-kr* (string (unicode-char #xe9))))` | `(101)` |

## 再現
上の式を評価する。

## 場所
- src/encoding.cc `internal_to_iso2022_stream`（`no-escape` の符号も同じ書き手を使う。`select_designation` が、指定できる集合として「何でも」を持つ G0 に 96 文字集合を指定する）。é が e になる所は未調査。

## 期待
- 利用者が決める。
