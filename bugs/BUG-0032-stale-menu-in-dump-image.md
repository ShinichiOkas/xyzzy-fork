# BUG-0032 ダンプイメージに残ったメニューが次の起動で無効になり、ファイル付きの起動でエラーになる

- 状態: 修正済み（2026-11-10、S1 の統合で発見。lisp/app-menu.l を修正）
- 対象: x64 版（feature/x64-only）。x64 固有ではない
- 起票: 2026-11-10

## 症状
- ファイルや行を指定して起動すると（例: `xyzzy.exe -q unittest\editor-tests.l -g10`）、「初期化されていないメニューです」のダイアログが出て、子プロセスが止まる。
- 全件テストで、コマンドライン引数の試験 8 件が「eval-in-another-xyzzy timeout」で失敗し、テストの所要時間が 82 秒から 555 秒に伸びた。
- 引数なしの起動、`find-file` は動く。

## 実測
- `(ed::init-app-menus)` が「初期化されていないメニューです」で失敗する。`*app-menu*` の定義が途中で失敗し、`(ed::set-menu ed::*app-menu*)` も同じ理由で失敗する。
- 失敗の原因は `(:menu nil "ツールバー(&Q)" *command-bar-popup-menu*)`。`*command-bar-popup-menu*` の値が、ハンドルの無いメニューだった。
- `*command-bar-popup-menu*`、`*recent-file-popup-menu*`、`*recent-session-popup-menu*` を nil に戻してから `init-app-menus` を呼ぶと、成功する。

## 原因
- `init-app-menus` は、三つのメニューを `(or 変数 (setq 変数 (create-popup-menu ...)))` で作る。変数が非 nil なら作り直さない。
- ダンプイメージ（xyzzy.wxp）に、`init-app-menus` を実行した後の値が入っていた。Windows のメニューのハンドル（HMENU）はプロセスをまたいで無効になるので、読み込んだ値は使えない。
- 起動時の `(ignore-errors (init-app-menus))` がエラーを握りつぶすので、メニューが作られないまま起動が続き、後でファイルを開いたときに別の箇所で失敗していた。

## 修正
- lisp/app-menu.l: 三つの `(or ...)` を、毎回作り直す `setq` に変えた。
- 確認: 子プロセスの起動（ファイルと `-g10`）で、行の位置が 10 になる。全件テストは 573 件中 572 成功・1 失敗（既知の OLE の 1 件）。

## 残り（別の票にする）
- `ignore-errors` が `init-app-menus` の失敗を隠している。失敗は起動の中で見えるようにする（S6 の起動の整理で扱う）。
- ダンプイメージに、ハンドルを持つ値が入ること自体が危うい。ダンプの時点で、メニューの値を外す（S6 で扱う）。

## 参照箇所
- lisp/app-menu.l（init-app-menus）
- lisp/estartup.l（startup の ignore-errors）
- lisp/startup.l（ダンプの作成）
