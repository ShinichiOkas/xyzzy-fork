---
name: xyzzy の画面の確かめは、別デスクトップで走らせて師匠の画面・マウス・前面に触れない
description: tools/gui-verify の deskrun.exe が CreateDesktop で作った別デスクトップに xyzzy を起動し、PostMessage/SendMessage と PrintWindow だけで操作・撮影する。師匠の画面に窓が出ず、マウス・キーボード・前面を奪わない
type: domain
maturity: confirmed
proposed_by: user
created_at: 2026-11-22
provenance: 師匠の宣言（2026-11-22「テストするのはいいんだが、マウスカーソルを持っていかれると困る。PCが全く使えないで誤操作の山になる」）を受け、画面の確かめの役が作って実証した（5 回の実行で前面は子の系統のものにならず、Default デスクトップに子の窓 0 件、撮影は黒くない）
---

## いつ使う

xyzzy の画面を伴う確かめ（ダイアログ・filer・キー入力・ウィンドウメッセージ・WM_DROPFILES・撮影）をするとき。**師匠の画面・マウス・キーボード・前面の窓に触れない**ことが条件で、これが唯一の実行の仕方。`.pair-agent/skills/xyzzy-verify-gui-features-without-a-human.md` の「奪わない」の規則を、実際に動く道具にしたもの。実体は `tools/gui-verify/`（README.md に詳細）。

師匠の言葉（原文 2026-11-22）:

> テストするのはいいんだが、マウスカーソルを持っていかれると困る。PCが全く使えないで誤操作の山になる

## 実行の仕方

1. 道具を作る（初回と `.cs` を直したとき）: `powershell -NoProfile -File tools\gui-verify\build.ps1`（`deskrun.exe`・`dropfiles.exe` ができる。exe と png はコミットしない）
2. 試す xyzzy の作業ツリーを `$env:GV_WT` に入れる（無ければこのリポジトリの根）
3. 別デスクトップで台本を走らせる:
   `tools\gui-verify\deskrun.exe 240 "C:\Program Files\PowerShell\7\pwsh.exe" -NoProfile -File tools\gui-verify\proof.ps1`
4. 台本は `. tools\gui-verify\lib.ps1` を読み、`Start-Xyzzy` → `Get-Top` → `Key`（WM_KEYDOWN/UP を PostMessage）→ `Shot`（PrintWindow）で組む。Lisp 側の動作は `tools/gui-verify/gui.l` にキーを足して `work\log.txt` に書かせ、読み戻す。新しい確かめは `proof.ps1`・`r10.ps1` を写して作る
5. 出力（log・トランスクリプト・png）は `tools/gui-verify/work/`（コミットしない）

## 何が示せるか（合格の読み方）

`deskrun.exe` の最後の 2 行で見る:
- `windows of the child tree seen on Default desktop: 0`（子の系統の窓が師匠の画面に 1 つも出ていない）
- `foreground window ever owned by the child tree: False`（前面が子の系統のものになっていない）

`foreground before/after` の差は師匠自身の操作でも変わるので見ない。撮影は `distinct colours sampled` が 1 より大きければ黒くない。実証は 5 回の実行すべてでこの 3 点が通った。

## 限界（記録する。回避しない）

- **クリップボード**は WinSta0 の中で師匠と共有。ローカルでは確かめない（書かない）
- **OLE の `DoDragDrop`（本物のマウスのドラッグ）はできない**。OLE 登録の落とし先（filer の枠・バッファバー）は確かめられない限界として記録する。メインウィンドウは WM_DROPFILES の宛先なので確かめられる
- **WM_DROPFILES は `SendMessage` で送る**（`PostMessage` は ERROR_INVALID_HANDLE で拒否される）。HDROP のメモリは **xyzzy の中で確保**する（`dropfiles.exe` が CreateRemoteThread の小さな stub で GlobalAlloc させる）。xyzzy は Explorer からの落としと同じに DragQueryFileW で読む
- **PrintWindow は活性でないプロパティシートのタブの列を描かない**（ダイアログの上端が空く）。タブの列の見た目は撮影で示せない。コントロールの列挙（`[G]::Kids`）の座標・文字列で補う
- IME（IMM32）はこの PC では確かめられない
- **印刷プレビューは、この PC のプリンタ（Canon TS5400）が通る経路しか示さない。** GB・KSC・Big5 の文字はこのプリンタでは比例幅の経路（`pe_fixed_pitch` が 0）に入り、固定幅の経路の直し（BUG-0059 の全角のずらし）を通らないので、プレビューの撮影では確かめられなかった（BUG-0058 は同じ方法で確かめられた）。印刷の直しを確かめる前に、その文字がこのプリンタでどちらの経路に入るかを見る（2026-11-22 段 3。AI の観測）
- **新しいビルドはダンプイメージ（`xyzzy.wxp`）を最初の 1 回書く**。その 1 回も deskrun 越しに走らせる（xyzzy を起動して殺す台本）。書かれた後の台本を流す
- **ラベルの前に、その exe がどのビルドかを確かめる**（`XYZZY_CHAR_UNICODE` が 0 か 1 か）。0 は旧内部コード（あ = 0x82A0）、1 はコードポイント（あ = 0x3042）。結果を「合格・不合格」と書く前に `r10.ps1` の F5（バッファの文字コード）などで識別する
- **変更の前後の画面は、同じ窓を PrintWindow で撮って画素で比べる**（`r3.ps1` で 2 つのビルドを同じ場面で撮り、`cmp.exe <dir> <tagA> <tagB>` が帯ごとに差の画素数と範囲を出す。before を 2 回撮って before == before になること（雑音の床）を先に確かめる）。キャレットは PrintWindow に写って点滅するので、`r3.ps1` が書く `.mask`（GetGUIThreadInfo の矩形）を `cmp.exe` が除く。印刷プレビューの比較は倍率を 200% に固定する（既定の「窓に合わせる」は同じ exe でも 2 回で変わる）。手順は `tools/gui-verify/README.md`。0 と 1 の見分けは `r10.ps1` で WM_CHAR 0x82 0xA0 を送る（0 は 33440、1 は 12354。`(char "あ" 0)` だけでは見分けられない）

## 役が自分の試験を手元で流す（`ci-test.ps1 -Tests` を deskrun 越しに）

AI の観測（2026-11-22 段 3。forming 相当）。この節は、上の確定した規則（師匠の画面・入力・前面に触れない）を守ったまま、役が自分の試験を秒〜分で回すための運用。段 3 の 21:15 から、役の門は「増分ビルド + 自分の試験をこの形で流す」になった（`xyzzy-run-tests-via-ci-on-this-pc` の「役は CI を待たない」）。

形（詳細は `ci/ci-test.ps1` の先頭の注釈）:

    & <リポジトリ>\tools\gui-verify\deskrun.exe 2400 'C:\Program Files\PowerShell\7\pwsh.exe' -NoProfile -ExecutionPolicy Bypass -File <作業ツリー>\ci\ci-test.ps1 -Tests unittest\a-tests.l,unittest\b-tests.l

つまずいた点:
- **pwsh 7 を使う。** Windows PowerShell 5.1 は別デスクトップで起動に失敗する
- **PowerShell から起動する。Git Bash から呼ばない。** Git Bash は `/c` などの引数をパスに書き換え、`\r` を改行にして壊す
- **`-Tests` の複数のファイルはカンマで区切る。** `-File` で渡すと 1 つの文字列になる。`ci-test.ps1` がカンマで割る
- **全件（`-Tests` なし）とクリップボードに触る試験は、この形で流さない。** クリップボードは WinSta0 の中で師匠と共有。全件は親の統合の CI で流す
- **`-Tests` に無いファイルを含めると、実行が終わらない**（BUG-0062）。渡す前にファイルがあるかを見る。子の xyzzy が終わらない試験も実行を止める（BUG-0060）。deskrun の時間切れ（終了コード 99）で止まったら、残ったプロセスのうち自分が起こした PID だけを止める
- **手元のビルドは同時に 3 つまで。** 並列の役のビルドが重なってメモリが足りなくなり、処理が止められたことがある。親が依頼文で渡す枠（`build-slot-1`〜`3` のフォルダ。`mkdir` できたら取れた）を取ってからビルドする
- **段 3 の結果**: この形にした後、R5 の部品は 1 件を起こしてから統合まで約 10〜15 分で回った（R5-A は約 10 分。親の記録）

## Why

画面の確かめは AI が自分で通す（師匠の宣言 2026-10-27）が、通すのに師匠のマウス・キーボード・前面を使えば、師匠は PC を使えず誤操作を招く。別デスクトップなら画面ごと別なので、確かめを速く回しても師匠の作業を止めない。

## 取り消し

師匠の明示の規則（confirmed）。道具を置き換えるときも、「師匠の画面・入力・前面に触れない」条件は残す。
