# BUG-0052 UTF-8 の読みが、不正なバイトと途中で切れた列を黙って捨てる

- 状態: 未修正（P1 の変換器は、捨てずに生のバイトの文字にして書き戻す。今の読み書きの経路は P1 で切り替えていない）
- 対象: feature/x64-only。x64 固有ではない
- 起票: 2026-11-22
- 出所: P1（変換器）で今の読みと突き合わせたとき。設計書 §5.3（不正なバイトを保つ）。BUG-0044 との関係は未確認

## 症状
- UTF-8 として読むと、先頭として正しくないバイト（単独の 0x80..0xBF、0xC0・0xC1、0xF5..0xFF）と、末尾で切れた列（例 E3 81）が、文字にならず消える。保存するとそのバイトがなくなる。

## 実測（2026-11-22）
| 式 | 結果 |
|---|---|
| `(map 'list #'char-code (convert-encoding-to-internal *encoding-utf8n* (map 'string #'code-char '(#x80 #x41))))` | `(65)`（0x80 が消える） |
| `(map 'list #'char-code (convert-encoding-to-internal *encoding-utf8n* (map 'string #'code-char '(#x41 #xe3 #x81))))` | `(65)`（E3 81 が消える） |

## 再現
上の式を評価する。

## 場所
- src/encoding.cc `utf8_to_internal_stream::refill_internal`（`nbits` が 0 または 6 のときは何もしない。続きのバイトの検査もしない。`c == eof` で `return`）。

## 期待
- 利用者が決める。設計書は、どの符号でも読めないバイトを生のバイトの文字（0x110080〜）にして、どの符号で書いても元のバイトに戻す。
