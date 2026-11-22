# BUG-0053 符号に定義のないバイトが、別の文字（半角カナ・U+30FB・私用領域）として読まれる

- 状態: 未修正（P1 の変換器は生のバイトの文字にして書き戻す。今の読み書きの経路は P1 で切り替えていない）
- 対象: feature/x64-only。x64 固有ではない
- 起票: 2026-11-22
- 出所: P1（変換器）で全符号の表を今の読みと突き合わせたとき。設計書 §5.3

## 症状
内部の 0x80〜0xFF が cp932 の 1 バイト（半角カナ）の値なので、符号に定義のないバイトを読むと、別の文字になる。
- windows-1253 の 0xAA・0xD2・0xFF は U+FF6A・U+FF92・U+F8F3。windows-1257 の 0xA1・0xA5 は U+FF61・U+FF65（半角カナ）。
- ISO-8859-x と windows-125x の 0x80〜0x9F の多くは、Unicode のない文字（`char-unicode` が U+30FB）。
- big5 で先頭バイト 0xA1 の後が後続バイトの範囲外（例 0xA1 0x7F）のとき、先頭の 0xA1 が U+FF61（半角カナ）になる。
- sjis の 0xA0、0xFD〜0xFF は U+F8F0〜U+F8F3。

## 実測（2026-11-22）
| 式 | 結果 |
|---|---|
| `(map 'list #'char-unicode (convert-encoding-to-internal *encoding-windows-greek* (map 'string #'code-char '(#xaa #xd2 #xff))))` | `(65386 65426 63731)` |
| `(map 'list #'char-unicode (convert-encoding-to-internal *encoding-windows-baltic* (map 'string #'code-char '(#xa1 #xa5))))` | `(65377 65381)` |
| `(map 'list #'char-unicode (convert-encoding-to-internal *encoding-iso-8859-1* (map 'string #'code-char '(#x81 #x85))))` | `(12539 12539)` |
| `(map 'list #'char-unicode (convert-encoding-to-internal *encoding-big5* (map 'string #'code-char '(#xa1 #x7f))))` | `(65377 127)` |

## 再現
上の式を評価する。

## 場所
- src/encoding.cc の `windows_codepage_to_internal_stream::refill_internal`（表が -1 のバイトはそのまま内部の値にする）、`iso8859_to_internal_stream::refill_internal`、`big5_to_internal_stream::refill_internal`、`sjis_to_internal_stream`。

## 期待
- 利用者が決める。設計書は、定義のないバイトを生のバイトの文字にして元のバイトを書き戻す。

## R4（2026-11-22）
- ファイルとソケットのストリーム・load は、1 の側で P1 の変換器に切り替え済み（src/stream.cc の stream_conv）。生のバイトの文字は元のバイトで書き戻る。0 の側の動き（バッファの読み書き）は変えていない。V で閉じる。
