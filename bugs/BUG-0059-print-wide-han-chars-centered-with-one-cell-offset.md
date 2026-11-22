# BUG-0059 印刷で、GB・KSC・Big5 の全角文字だけ、全角（2 マス）のずらしでなく半角（1 マス）のずらしで置かれる

- 状態: 未修正（2026-11-22、S4 段 3 R3 で見つけた。R3 は切り替え 0 の側で元の動きを保った）
- 対象: src/print.cc（0.2.2.235 の取り込みの時点から）

## 症状
- 印刷で、フォントの平均の幅（`tmAveCharWidth`）が 1 マスの幅と違うとき、文字を枠の中央に置くずらしを使う。半角の文字は `pe_offset[f].x`（差の半分）、全角の文字は `pe_offset2x[f]`（差の全部）が正しい。
- CP932 の全角（`paint_kanji`）と JIS X 0212（`paint_jisx0212`）は全角で `pe_offset2x` を使う。GB 2312・KS C 5601・Big5 の全角（`paint_full_width`）だけが、全角でも `pe_offset[f].x` を使う。フォントの平均の幅と 1 マスの幅が違う設定では、これらの文字が右へ寄らず、中央からずれる。

## 参照箇所
- 旧 src/print.cc の `paint_kanji`・`paint_jisx0212`・`paint_full_width`。今は `print_engine::paint_char`（切り替え 0 の側だけ、GB・KSC・Big5 の枠は半角のずらしを使う。切り替え 1 の側は全角に `pe_offset2x`）
