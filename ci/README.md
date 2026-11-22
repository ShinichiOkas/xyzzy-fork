# CI（GitHub Actions、この PC で走らせる）

- 定義: `.github/workflows/test.yml`。`feature/x64-only` と `develop` への push、または手動（Actions の画面の Run workflow）で走る。
- 中身: `ci/ci-test.ps1` = `install.bat` → 一度起動してダンプ（xyzzy.wxp）を書かせる → `run-tests.bat`。ログは成果物 `test-logs`（`ci-out/`）。
- 走る場所: この PC の自己ホストのランナー（ラベル `self-hosted, windows, x64, xyzzy`）。GitHub のクラウドのマシンは使わない。
- CI は統合の門。統合のブランチだけで走る（2026-11-22 改め。作業用のブランチ `worktree-agent-*` では走らない）。役は自分の試験をこの PC の別のデスクトップで流し（`tools/gui-verify/deskrun.exe` から `ci-test.ps1 -Tests …`）、親が小片をまとめて統合して CI を 1 回流す。文書だけの変更（`*.md`・`docs/`・`bugs/`・`.pair-agent/`）と `[skip ci]` を書いたコミットでも走らない。
- 全件の実行（`-Tests` なし）の最後に、値の切り替え `XYZZY_CHAR_UNICODE=1` でビルドとリンクが通るかを確かめる（[4/4]）。
- 同じブランチに新しく push すると、そのブランチの古い実行は取り消される。ランナーは 1 台なので、別のブランチの実行は順番待ちになる。

## 作業用のブランチでの確かめ（役）

役のブランチでは CI は走らない。役の門は手元の 2 つ:

1. 0 の側の差分ビルド: `build.bat Release Build minimal`
2. 自分の試験ファイルだけを、別のデスクトップで（PowerShell から。Git Bash からは呼ばない。pwsh 7 を使う）:

       & tools\gui-verify\deskrun.exe 1200 'C:\Program Files\PowerShell\7\pwsh.exe' -NoProfile -ExecutionPolicy Bypass -File <作業ツリー>\ci\ci-test.ps1 -Tests unittest\a-tests.l,unittest\b-tests.l

   deskrun の最後の行が `windows of the child tree seen on Default desktop: 0` と `foreground window ever owned by the child tree: False` であること。全件とクリップボードを使う試験は手元で流さない。deskrun を通さずに `run-tests.bat`・`install.bat`・`xyzzy.exe` を走らせない（窓が利用者の画面に出て、前面やキーボードを奪う）。

## 統合のブランチで結果を待つ（親）

    git push origin feature/x64-only        # 文書だけの変更なら: gh workflow run test.yml --ref feature/x64-only
    powershell -NoProfile -ExecutionPolicy Bypass -File ci/wait-run.ps1

- `wait-run.ps1` は HEAD のコミットの実行を待ち、結果・失敗した試験・ログの末尾を出す。ログは `ci-out/run-<番号>/` に落ちる。
- 終了コード: 0 合格、1 不合格か取り消し、2 まだ待ち行列か実行中（もう一度呼ぶ）、3 そのコミットの実行が無い。
- 既定では 9 分で待つのをやめる（エージェントのシェルのコマンドの上限 10 分より短い）。`-Minutes` で変えられる。
- 最後の門の前に、ランナーに他の実行が無いことを確かめる（`gh run list`）。ランナーは 1 台。

## ランナーの登録（1 回だけ）

管理者の PowerShell で:

    powershell -ExecutionPolicy Bypass -File ci\install-runner.ps1

- ランナーは Windows のサービスとして入る。サービスは利用者の画面と別の、見えない画面で動くので、CI の試験が窓を出したりマウスや前面の窓を奪ったりしない。
- 置き場所: `-InstallDir`、なければ環境変数 `AGENT_RUNNER_DIR`、なければ `%ProgramData%\actions-runner\<リポジトリ名>`。利用者のフォルダ（`C:\Users\…`）の下には置かない（サービスのアカウント NETWORK SERVICE が上の階層を読めず、起動できない）。
- 登録の鍵は `gh`（ログイン済み）で GitHub から取る。`-Token` で渡してもよい。

## 安全

- リポジトリは公開なので、`pull_request` では走らせない（他人の fork の PR のコードがこの PC で動くため）。push できるのは共同作業者だけ。
- 外す: ランナーのフォルダで `config.cmd remove --token <削除の鍵>`。サービスも消える。
