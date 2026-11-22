# CI（GitHub Actions、この PC で走らせる）

- 定義: `.github/workflows/test.yml`。`feature/x64-only` と `develop` への push、または手動（Actions の画面の Run workflow）で走る。
- 中身: `ci/ci-test.ps1` = `install.bat` → 一度起動してダンプ（xyzzy.wxp）を書かせる → `run-tests.bat`。ログは成果物 `test-logs`（`ci-out/`）。
- 走る場所: この PC の自己ホストのランナー（ラベル `self-hosted, windows, x64, xyzzy`）。GitHub のクラウドのマシンは使わない。

## ランナーの登録（1 回だけ）

管理者の PowerShell で:

    powershell -ExecutionPolicy Bypass -File ci\install-runner.ps1

- ランナーは Windows のサービスとして入る。サービスは利用者の画面と別の、見えない画面で動くので、CI の試験が窓を出したりマウスや前面の窓を奪ったりしない。
- 置き場所: `-InstallDir`、なければ環境変数 `AGENT_RUNNER_DIR`、なければ `%ProgramData%\actions-runner\<リポジトリ名>`。利用者のフォルダ（`C:\Users\…`）の下には置かない（サービスのアカウント NETWORK SERVICE が上の階層を読めず、起動できない）。
- 登録の鍵は `gh`（ログイン済み）で GitHub から取る。`-Token` で渡してもよい。

## 安全

- リポジトリは公開なので、`pull_request` では走らせない（他人の fork の PR のコードがこの PC で動くため）。push できるのは共同作業者だけ。
- 外す: ランナーのフォルダで `config.cmd remove --token <削除の鍵>`。サービスも消える。
