---
name: xyzzy の D&D・ショートカットは、人の目を使わずスクリプトで確かめる
description: D&D の登録は起動したウィンドウの OleDropTargetInterface プロパティを PowerShell の GetProp で見る。ショートカットは Lisp の -l スクリプトで作成→解決の往復を見る
type: domain
maturity: forming
proposed_by: ai
created_at: 2026-11-22
provenance: xyzzy-fork S4-15（OLE Automation 削除後の B/C の確かめ）
---

## 手順

- **D&D（`src/DnD.cc`）**: `xyzzy.exe` を起動し、メインウィンドウのハンドルに対して PowerShell から `GetProp` で `OleDropTargetInterface` を読む。付いていれば `OleInitialize` と `RegisterDragDrop` が効いている（S4-15 で確認。付かなければ初期化を消し過ぎた疑い。これは推定で、付かない例はまだ見ていない）。実際にドロップを撃つ代わりに、登録の有無で足りる範囲を確かめる
- **ショートカット（`src/com.cc`）**: `-l` で渡す Lisp スクリプトで `*create-shortcut` を呼んで作り、`resolve-shortcut` で読み戻す。行き先と説明が一致すれば往復が通っている

## 師匠のマウス・キーボード・前面を奪わない（絶対）

師匠の言葉（原文 2026-11-22）:

> テストするのはいいんだが、マウスカーソルを持っていかれると困る。PCが全く使えないで誤操作の山になる

- **使わない**: `SendInput`・`SetCursorPos`・`mouse_event`・`keybd_event`・`SetKeyboardState`・`BlockInput`、`SetForegroundWindow`・`AllowSetForegroundWindow`・`SwitchToThisWindow`・活性化を伴う `ShowWindow`
- **入力**: 自分で起動した窓（自分の PID の窓）にだけ `PostMessage`／`SendMessage` で送る
- **撮影**: `PrintWindow`（`PW_RENDERFULLCONTENT`）。窓を前に出さない
- **起動**: 画面の外の座標か、活性化しない出し方（`SW_SHOWNOACTIVATE`・`SW_SHOWMINNOACTIVE`、`STARTUPINFO` の `wShowWindow`）
- **限界**: 本物のマウスでの OLE の `DoDragDrop`、IME の実機などは確かめられない限界として記録し、師匠が許したとき以外はやらない。D&D は上の `GetProp` で登録の有無を見る範囲で足りるとする
- 役同士の画面のロック（`gui.lock`）は順番を決めるだけで、奪うことの許可ではない
- 一般の規則はペア固有 Skill `verify-gui-without-taking-the-users-input`
- 実際に動く道具（別デスクトップ・`tools/gui-verify/`）は `xyzzy-verify-gui-on-a-separate-desktop.md`

## Why

師匠の宣言（2026-10-27、`CLAUDE.md` 末尾）: 確かめは AI が自分で通す。人の目でしか確かめられない項目は検証の仕組みの欠陥。S4 の TODO（ステップ 15）は「手で filer を開く・D&D する」と書いていたが、上の 2 手順で人の手を使わずに済んだ。

## 取り消し

AI の観測（1 スプリント）。取り消すときはこのファイルを消す。
