---
name: xyzzy の試験・プローブを書く前に、使う CL の関数が xyzzy にあるかを確かめる。deftest の期待値は評価されない
description: xyzzy の Lisp は CL のつもりで書くと外れる。段 3 では ecase・拡張の loop・write-string・search・make-string が無いか別物で、役の試験が落ちて直しの往復を食った。使う前に src/gen-syms.cc と lisp/ を引く。deftest の「=> 値」と :compare は評価されないので、式で書くなら #.(...)
type: domain
maturity: forming
proposed_by: ai
created_at: 2026-11-22
provenance: xyzzy-fork S4 段 3。役の試験の直しのコミット（853357c R1「ecase・loop for が無い」、94a2d62 R4「write-string は xyzzy に無い」、071f2c4 R6「search（xyzzy に無い）」、R9「make-string は無い」）。deftest の期待値は unittest/README.md に書いてある。師匠の原文の裏付けは無い（AI の観測）
---

## いつ使う

`unittest/` に試験を書くとき、`-l` で流すプローブを書くとき。役に試験を頼むときは、下の表を依頼文に貼る。

## 無い・別物（2026-11-22 に役の試験を落としたもの）

| CL の書き方 | xyzzy では | 代わりに書く |
|---|---|---|
| `ecase` | 無い | `case` に `(t (error ...))` の節を足す（R1 は `case` に直した） |
| `(loop for ...)`（拡張の loop） | 既定の `loop` は `lisp/evalmacs.l` の単純な形（本体を繰り返すだけ）。`for` などの語は、繰り返す本体の式（変数）として評価される | `dolist`・`dotimes`・`do`。拡張の loop は `lisp/cmu_loop.l` にあるが、`require` すれば使えるかは確かめていない |
| `write-string` | 無い | `princ`（R4 が直した） |
| `search` | 無い | 文字列の中を探すなら `(string-match (regexp-quote 語) 文字列)`（どちらも `src/gen-syms.cc` にある。R6 が何に替えたかは見ていない） |
| `make-string` | 無い | `map 'string`（R9 が直した） |

この表は段 3 で当たったものだけ。ほかにも無いものがある前提で書く。

## 使う前の確かめ方（秒で済む）

- `src/gen-syms.cc` を `DEFUN3 (名前,` で引く（C++ で作った関数。`CL_DEFUN3`・`SI_DEFUN3` の行もこの語で当たる）
- `lisp/*.l` を `(defun 名前 `・`(defmacro 名前 ` で引く
- `lisp/` にあっても、起動時に読まれているとは限らない（`cmu_loop.l` のように `require` が要るものがある）。怪しければ、自分の試験を別デスクトップで 1 回流して落ちないかを見る（`xyzzy-verify-gui-on-a-separate-desktop`）

## deftest の期待値は評価されない

`unittest/README.md`（「細かい説明」の節）に書いてある。要点だけ:

- `=> 値` の値は評価されない。`=> (code-char 12354)` は、リスト `(code-char 12354)` と比べることになる
- `:compare` の比較関数も評価されない。関数名か、`#'` を付けないラムダ式で書く
- 式で期待値を作るなら `#.(...)`（読み込みのときに評価される）。使う補助関数は、同じファイルのそれより前の toplevel で定義しておく（例: `unittest/ini-tests.l` の `ini-t-text` は 30 行目で定義し、後ろの試験で `=> #.(ini-t-text ...)` と書いている）

## Why

段 3 の役は CL のつもりで試験を書き、関数が無いだけで落ちた。当時は確かめが全件の CI だったので、こうした小さな誤りの 1 件ごとに 5〜60 分の往復を食った（corrections `20261122-stage-design-too-slow-3h-per-slice`）。書く前に引けば、往復は要らない。

## 関連

[[xyzzy-probe-with-q-trace-l-and-keep-require-in-its-own-toplevel]]（プローブの読み取りエラー）/ ペア固有の `small-parallel-parts-fast-local-loop-ci-only-at-integration`（小さな誤りは手元の周で秒で落とす）

## 取り消し

AI の観測（1 スプリント）。取り消すときはこのファイルを消す。
