# BUG-0037 lisp/foreign.l の c-pointer-type に、`:x64` の feature が無いとき `:uint32` に落ちる分岐が残っている

- 状態: 修正済み（2026-11-22、c9ccc76）
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-22（S3 の作業中に見つけた）

## 症状（コード上）
- lisp/foreign.l:80-81 の `c-pointer-type` は `(if (member :x64 *features*) ':uint64 ':uint32)` で、`:x64` が `*features*` に無いときは `:uint32` を返す。
- `:x64` は src/environ.cc:745 が起動時に `*features*` へ入れる。x64 専用の版では `:uint32` の分岐は通らない（死んだ分岐）。
- 使われている箇所: lisp/foreign.l:92（`c-typespec-p`）、273（define-c-struct の `*` スロット）。

## 参照箇所
- lisp/foreign.l:80-81、92、273
- src/environ.cc:745
