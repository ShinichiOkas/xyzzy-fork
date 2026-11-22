# BUG-0051 std- の UTF（std-utf8n など）が、ASCII のバックスラッシュとチルダを ¥（U+00A5）と ‾（U+203E）で書き、読みでは U+005C を全角の＼（U+FF3C）にする

- 状態: 未修正（P1 の変換器は別の扱いにしてある。今の読み書きの経路は P1 で切り替えていない）
- 対象: feature/x64-only。x64 固有ではない
- 起票: 2026-11-22
- 出所: P1（変換器）で std- の書きを今の変換器と突き合わせたとき。設計書 docs/refactoring/s4-char-design.md §4.6・§5.5

## 症状
- `*encoding-std-utf8n*`（`:windows` なし）で書くと、ASCII の `\`（0x5C）が C2 A5（U+00A5 ¥）に、`~`（0x7E）が E2 80 BE（U+203E ‾）になる。`*encoding-utf8n*` は 5C・7E のまま。
- `*encoding-std-utf8n*` で U+005C を読むと、内部の 0x815F（全角の＼）になる。
- 保存すると、バッファの `\` が別の文字に変わる。

## 実測（2026-11-22）
| 式 | 結果 |
|---|---|
| `(map 'list #'char-code (convert-encoding-from-internal *encoding-std-utf8n* "\\"))` | `(194 165)`（C2 A5） |
| `(map 'list #'char-code (convert-encoding-from-internal *encoding-std-utf8n* "~"))` | `(57984 190)`（E2 80 BE。先頭の 2 バイトが 1 つの Char にまとまって返る） |
| `(map 'list #'char-code (convert-encoding-from-internal *encoding-utf8n* "\\~"))` | `(92 126)` |
| `(map 'list #'char-code (convert-encoding-to-internal *encoding-std-utf8n* (map 'string #'code-char '(92))))` | `(33119)`（0x815F） |

## 再現
上の式を評価する。

## 場所
- src/encoding.cc: `internal_to_utf_stream::getw`（`utf_internal2shiftjis_hash`）、`utf_to_internal_stream::putw_jp`（`utf_shiftjis2internal_hash`）。表は src/gen-utf2sjis.cc が作る 9 組（0x5C・0x7E・0x815F・0x8160・0x8161・0x817C・0x8191・0x8192・0x81CA）。

## 期待
- 利用者が決める（JIS X 0201 の円記号・オーバーラインを std- の意味とするのは設計の意図かもしれない）。P1 の変換器は、std- の書きで寄せるのを 6 文字（U+FF5E→U+301C、U+2225→U+2016、U+FF0D→U+2212、U+FFE0→U+00A2、U+FFE1→U+00A3、U+FFE2→U+00AC）にして、ASCII の 2 文字と U+FF3C は寄せない。

## R4（2026-11-22）
- ファイルとソケットのストリーム・load は、1 の側で P1 の変換器に切り替え済み（src/stream.cc の stream_conv）。生のバイトの文字は元のバイトで書き戻る。0 の側の動き（バッファの読み書き）は変えていない。V で閉じる。
