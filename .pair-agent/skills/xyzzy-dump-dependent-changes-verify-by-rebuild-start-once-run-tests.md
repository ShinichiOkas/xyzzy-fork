---
name: xyzzy のダンプ・.lc に依存する変更の確かめは、install.bat → 1 回起動してダンプを書かせる → run-tests の順で通す
description: bytecompile.bat は .lc とダンプを消して作り直す。ダンプを書かせる 1 回の起動を挟み、run-tests の前後で wxp の時刻が変わらないことで「ダンプを読んで走った」ことを確かめる
type: domain
maturity: forming
proposed_by: ai
created_at: 2026-11-22
provenance: xyzzy-fork S3 の閉じ（fixnum の変更でダンプの形式が変わった。クリーンインストールの後に 626/626 を確認）。師匠の原文の裏付けは無い（AI の観測）
---

## 手順

1. `install.bat`（中で `bytecompile.bat` が `.lc` とダンプを消して作り直す。古いものが残ると古いコードが読まれる〈`docs/refactoring/plan.md` §9〉）
2. `xyzzy.exe` を **1 回起動**して、ダンプ（`xyzzy.wxp`）を書かせる
3. `run-tests.bat` を走らせる。**走らせる前後で `xyzzy.wxp` の更新時刻が変わっていない**ことを確かめる（変わっていれば、ダンプを読まずに作り直して走った＝ダンプ経由の確かめになっていない）

## Why

S3 で fixnum を 62bit にしてダンプイメージの形式が変わった。ダンプに焼かれた値（fixnum の即値）が新旧で食い違うと、`.lc` やダンプが古いまま残る環境でだけ壊れる。クリーンインストールの後、ダンプを読んだ実行で 626/626（558 + foreign 14 + fixnum 54）合格を確認してスプリントを閉じた。wxp の時刻は「ダンプを読んだ」ことの機械的な印になる（見た目では区別できない）。

## 関連

[[xyzzy-probe-with-q-trace-l-and-keep-require-in-its-own-toplevel]] / ペア固有の `fixed-but-nothing-changed-suspect-stale-process`（古いものが読まれる系統）

## 取り消し

AI の観測（1 スプリント）。取り消すときはこのファイルを消す。
