---
name: xyzzy の確かめのスクリプトは `-q -trace -l` で走らせ、`(require ...)` は別の toplevel に置いて `c::` で参照する
description: 読み取りエラー（外部でないシンボル c:size_t など）は toplevel を落としてダイアログで止まる。(require ...) を別の toplevel にしておけば、後の toplevel は require の後に読まれる
type: domain
maturity: forming
proposed_by: ai
created_at: 2026-11-22
provenance: xyzzy-fork S3（BUG-0028・0031 の実測と foreign の試験）。師匠の原文の裏付けは無い（AI の観測）
---

## 手順

- プローブ（一回きりの実測スクリプト）は `xyzzy.exe -q -trace -l script.l` で走らせる（`run-tests.bat` も同じ形）。GUI を人が見なくても、標準出力と trace で結果が取れる
- **読み取りエラーは toplevel を落とし、ダイアログで止まる。** 例: `c:size_t` のように外部シンボルでないものを、その名前空間が読み込まれる前に 1 つの toplevel で読むと、読み取りの段階で落ちる
- 対策: **`(require "foreign")` などを別の toplevel に置き**（1 つ目）、参照する式は次の toplevel に書く。シンボルは **`c::size_t`**（内部のシンボルの参照）で書くと、外部でなくても読める
- ダイアログで止まったプローブは人が閉じるまで走り続ける。夜間や並列の役が走らせるものは、読み取りエラーが起きない書き方にしておく

## Why

S3 で BUG-0028（8 バイトの値が引数・戻り値で落ちる）・BUG-0031（構造体の整列）を実測するプローブが、読み取りエラーのダイアログで止まった。require を別の toplevel に分け、`c::` で参照する形にして通った。

## 関連

[[xyzzy-verify-gui-features-without-a-human]]（人の目を使わない確かめの別の手段）。票の症状が前のコミットで既に直っていたことは、着手前の測り直し（ペア固有 `bug-ticket-must-be-remeasured-before-fixing`）で見つけた。

## 取り消し

AI の観測（1 スプリント）。取り消すときはこのファイルを消す。
