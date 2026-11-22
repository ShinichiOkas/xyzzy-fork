# BUG-0050 ラテン文字（ISO-8859 など）が、forward-word と word-char-p では語の文字で、正規表現の \w では語の文字でない

- 状態: 未修正（2026-11-22。BUG-0042 の修正で、256 以上を SCkanji にそろえたあとに残った差）
- 対象: x64 版（feature/x64-only）。x64 固有ではない
- 起票: 2026-11-22
- 出所: docs/refactoring/s4-char-design.md §7.3（非 ASCII の構文の既定）、BUG-0042

## 症状
- 内部の値が 256 以上のラテン文字（ISO-8859-1 の é など）は、`forward-word`・`word-char-p` では語の文字（word_state の Unicode の範囲表で WCword）。
- 同じ文字が、正規表現の `\w`・`\sw` と `skip-syntax-spec-forward "w"` では語の文字でない（構文表の既定どおり SCkanji）。

## 実測（2026-11-22、BUG-0042 の修正後）
| 式 | 結果 |
|---|---|
| バッファ「abécd efg」（é は ISO-8859-1。空白は cd と efg の間だけ）で `(forward-word 1)` を 0 から | é をまたいで「abécd」の末尾（点 5）まで進む |
| `(word-char-p é)` | t |
| `(string-match "\\w" (string é))` | nil |

## 再現
1. `(iso-code-char #xe9 :iso8859-1)` で é を作る。
2. 上の 3 式を実行する。

## 期待
- 利用者が決める。語の文字を 1 つの定義（設計書 §7.3 の `syntax_of` に、Unicode の語の文字の既定を載せる）にそろえるのが設計書の向き。