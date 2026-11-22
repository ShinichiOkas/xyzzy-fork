---
name: xyzzy の全件試験は、この PC の自己ホストの CI で流す（手元で run-tests.bat を流さない）
description: 手元で run-tests.bat を流すと xyzzy の窓が利用者の画面に出てマウス・前面を奪う。GitHub Actions の自己ホストのランナー（この PC・サービス）が見えない画面で流す
type: domain
maturity: confirmed
proposed_by: 師匠
confirmed_by: 師匠
confirmed_at: 2026-11-22
created_at: 2026-11-22
provenance: xyzzy-fork S4 後（師匠の指示 2026-11-22。一般の規則はペア固有 Skill run-tests-on-the-users-pc-in-an-invisible-session-via-a-ci-runner）
---

師匠の言葉（原文 2026-11-22）:

> テストするのはいいんだが、マウスカーソルを持っていかれると困る。PCが全く使えないで誤操作の山になる
> あと、GitHubのCIでテストするようにして。もちろんテスト環境そのものはこのPCね。

## 規則

- **手元で `run-tests.bat` を流さない。** xyzzy の窓が利用者の画面に出る。全件試験は CI で流す
- CI の実体はこの PC（自己ホストのランナー。名前 `ANCIENT-xyzzy-fork`、ラベル `xyzzy`、Windows のサービス）。見えない画面で動くので窓・マウス・前面を奪わない。環境はこの PC のまま

## ファイル

- `.github/workflows/test.yml`: `feature/x64-only` と `develop` への push と手動実行で走る。**公開リポジトリなので `pull_request` では走らせない**（第三者のコードがこの PC で動く）
- `ci/ci-test.ps1`: `install.bat` → ダンプを書く起動 → `run-tests.bat`
- `ci/install-runner.ps1`: この PC をランナーとして登録しサービスに入れる。管理者で 1 回。置き場所の既定は `%ProgramData%\actions-runner\<名前>`
- `ci/README.md`: 手順の詳細

## 結果の見方

- 一覧: `gh run list`
- 待つ（失敗で非 0）: `gh run watch <id> --exit-status`
- ログ・成果物: `gh run download <id> -n test-logs`
- 手動で起こす: `gh workflow run test.yml --ref <ブランチ>`

## 未決

- 役のブランチ（作業用ブランチ）での確かめの扱い。今は `feature/x64-only` と `develop` だけで走る。役のブランチを CI に載せるか、push 先を絞るかは未決（決まったらここと `test.yml` を直す）

## つまずいた点（再発させない）

- ランナーを `C:\Users\<名前>\…` の下に置くとサービスのアカウント（NETWORK SERVICE）が上の階層を読めず起動しない。`%ProgramData%` 側に置く
- サービスの初期化がイベントログの発行元を作った直後に書いてアクセス拒否になる。発行元を先に作る（`install-runner.ps1` が対応済み）
- サービスの TEMP は `NETWOR~1` のような 8.3 の名前を含みうる。試験は一時フォルダの 8.3 の名前に依らない形にする（`wfile-long-path-truename` を直した）

## 取り消し

師匠の明示の指示（confirmed）。取り消すときはこのファイルを消す。
