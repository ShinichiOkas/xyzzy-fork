# BUG-0049 IME の文脈の問い合わせ（document feed）で、w2s (lisp) が返した領域を解放していない

- 状態: 修正済み（2026-11-22、S4-k）
- 対象: x64 版（feature/x64-only、59f7cfe）。x64 固有ではない（Win32 の時代から）
- 起票: 2026-11-22（S4 の設計の調査 docs/refactoring/s4-char-design.md §12 の F9）

## 症状
- IME が前後の文脈を問い合わせるたび（`Vime_documentfeed_helper` のフックが設定されているとき）、メモリが漏れる（問い合わせごとに 2 つの領域）。

## 原因（コード上の推測）
- src/kbd.cc の `kbd_queue::documentfeed` が `char *content = w2s (c); char *before = w2s (b);` で `xmalloc` された領域を受け取り、どの出口でも `xfree` しない。
- 設計書の F9 は場所を `reconvert` と書いていたが、`content`・`before` は `documentfeed`（再変換 `reconvert` の隣の関数）にある。`reconvert` の領域は `safe_ptr` で解放される。

## 参照箇所
- src/kbd.cc の `kbd_queue::documentfeed`
- src/string.h の `w2s (lisp)`（`xmalloc` した領域を返す）

## 修正
- 2 つの領域を、スコープを出るとき（どの return でも）`xfree` する小さな入れ物に持たせた。
- 試験は足していない。IME からの問い合わせは OS が呼ぶ経路で、漏れたメモリの量はスクリプトから観測できない。
