# BUG-0042 正規表現の \w・\s? と skip-syntax-spec が、256 以上の文字で構文表を `cc >> 8` で引き、誤った構文になる

- 状態: 修正済み（2026-11-22）
- 対象: x64 版（feature/x64-only、59f7cfe）。x64 固有ではない
- 起票: 2026-11-22
- 出所: docs/refactoring/s4-char-design.md §12 の F2（§7.3・7.4）

## 症状
- 内部の値が 0x0100〜0x7FFF の文字（ISO-8859、KSC、GB、JIS X 0212 など）で、`\w`・`\W`・`\sX`・`skip-syntax-spec-forward/backward` が、上位バイトを添字にした構文表の項目の構文になる。
- ISO-8859-1 の é（内部 #x169）は、表の 0x01 の項目（junk）になり、`\sx` に当たる。JIS X 0212 の漢字（#x6282 など）は、表の 0x62 の項目（`b`）になり、`\w` に当たる。
- JIS X 0208 の漢字・かな（#x88ea、#x82a0）は、たまたま上位バイトが kanji の項目なので、kanji（`\sj`）になる。
- 構文の走査（syntax.cc の `syntax()`、`update_normal`）は、256 以上を SCkanji に固定している。同じ文字で、正規表現と走査の答えが食い違う。

## 実測（2026-11-22、修正前。`string-match`・`skip-syntax-spec-forward`）
| 文字 | 内部の値 | `\w` | 当たった `\sX` | skip で進んだ spec |
|---|---|---|---|---|
| ISO-8859-1 é | #x169 | なし | x（junk） | x |
| ISO-8859-1 À | #x140 | なし | x | x |
| ISO-8859-2 ą | #x1b1 | なし | x | x |
| JIS X 0212 の漢字（0x3021） | #x6282 | あり | w | w |
| JIS X 0208 の漢字（0x306c） | #x88ea | なし | j | j |
| JIS X 0208 のかな（0x2422） | #x82a0 | なし | j | j |

## 再現
1. `(iso-code-char #xe9 :iso8859-1)` で é を作る。
2. `(string-match "\\sx" (string c))` が当たる。`(string-match "\\w" (string (iso-code-char #x3021 :jisx0212)))` が当たる。
3. 一時バッファに入れて `(skip-syntax-spec-forward "x")` が 1 文字進む。

## 原因
- regex.h の `Regexp::char_syntax` と search.cc の `skip_syntax_spec` が、256 以上の文字を `cc >> 8` にしてから表を引いていた。
- regex.cc の fastmap（`\w`・`\W`・`\sX`・`\S?` と記号の文字）は、表の 0〜255 の項目だけから作り、256 以上の文字の枠（上位バイト）に、表の同じ番号の項目を写していた。引く側を直しても、fastmap が先に落とす。

## 直し方
- syntax.h に `syntax_of (表, Char)` を足した（256 未満は表、256 以上は SCkanji）。syntax.cc の `syntax()`、regex.h の `char_syntax`、search.cc の `skip_syntax_spec` が同じ関数を呼ぶ。
- regex.cc の fastmap は、`syntax_fastmap` で作る。述語が SCkanji で真なら、上位バイトの枠 1〜255 を全部立てる（落とさない向き）。
- 設計書 §7.3 の将来の形（ASCII の密な表と、非 ASCII の疎な上書き）では、`syntax_of` の中身だけが替わる。

試験: unittest/regexp-syntax-tests.l（8 件）。