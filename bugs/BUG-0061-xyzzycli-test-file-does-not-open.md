# BUG-0061 xyzzycli の試験で、動いている xyzzy にファイルを渡しても開かない

- 状態: 未修正（2026-11-22。見つけたのは S4 段 3 R8）
- 試験: `unittest/xyzzycli-tests.l.wip`（拾われない名前で置いてある。直ったら `-tests.l` に戻す）

## 見たこと
- 試験は、試験の xyzzy が動いているところで xyzzycli.exe に日本語の名前と ASCII の名前のファイルを渡し、`get-file-buffer` で開いたかを見る。2 件とも不合格（0 合格・2 不合格）。
- xyzzycli.exe は 0 で終わる。新しい xyzzy は起動しない（xyzzy.exe は 1 つだけ）。
- `*process-command-line-hook*` に入れた関数は、渡したパスで呼ばれる。日本語の名前も正しい文字で届く（受け渡しの "xyzzy-server v3"・UTF-8 は動いている）。
- その後、そのファイルのバッファができない。バッファの一覧は `*Minibuf0*`・`*Test Results*`・`*Trace Output*`・`*scratch*` と試験のバッファだけ。`*Trace Output*` は空。
- 日本語の名前でも ASCII の名前でも同じ。一時フォルダの 8.3 の名前のせいでもない（バッファ名で探しても見つからない）。
- 走らせた場所: この PC の別のデスクトップ（tools/gui-verify/deskrun）。ブランチ worktree-agent-a535898288dc0d35f（a401051）に調べの版の試験がある。

## 次の一手（R8 の調べの役の提案）
- フックの中で `(handler-case (find-file path) (error (c) (si:*print-condition c) c))` にして t を返し、誤りを結果に入れる。`*xyzzycli-helper`／`find-file` の中の誤りか、まわり（`with-set-buffer`・`si:*show-window-foreground`）を疑う。
