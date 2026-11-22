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
- 待つ: `ci/wait-run.ps1`（0 合格・1 不合格・2 まだ・3 実行なし）。**`gh run watch` や gh を繰り返し呼ぶ待ち方は使わない。** GitHub の API は 1 時間 5000 回でアカウントの共有。2026-11-22 に、役たちが数秒ごとに問い合わせて使い切り、全員の待ちが止まった。wait-run.ps1 は 1 分に 1 回、1 回の呼び出しで問い合わせ、上限に当たると戻る時刻まで待つ（戻る時刻は `gh api rate_limit` で見る。これは回数に数えられない）
- ログ・成果物: `gh run download <id> -n test-logs`
- 手動で起こす: `gh workflow run test.yml --ref <ブランチ>`

## 役（サブエージェント）の試験も CI で流す（師匠の指示 2026-11-22）

- `test.yml` は `worktree-agent-*`（エージェントの作業ツリーのブランチ）への push でも走る。文書だけの変更（`*.md`・`docs/`・`bugs/`・`.pair-agent/`）と `[skip ci]` では走らない
- 役の手元でしてよいのは**コンパイルだけ**（`build.bat Release Build minimal`。`XYZZY_CHAR_UNICODE=1` の確かめは `set CL=/DXYZZY_CHAR_UNICODE=1` と `Rebuild`）。`install.bat` は中の byte-compile で xyzzy.exe を起動するので手元で流さない。探りの Lisp も試験（`unittest/`）として書いて CI で流す
- 試験の流し方: コミット → `git push -u origin HEAD` → `powershell -NoProfile -ExecutionPolicy Bypass -File ci/wait-run.ps1`（終了コード 0 合格・1 不合格・2 まだ〈もう一度呼ぶ〉・3 実行なし）。ログは `ci-out/run-<番号>/`
- ランナーは 1 台。押すのは試験の結果が要る区切りだけにする（途中の保存の push は `[skip ci]`）。同じブランチの新しい push は古い実行を取り消す

## 役は CI を待たない（師匠の指摘 2026-11-22・21:15 から適用）

師匠の言葉（原文 2026-11-22）:

> ３時間以上はあまりにも遅すぎる。進め方の設計が根本的に悪い

- 段 3 で、役が全件 CI の待ちを抱えて 1 スライス 3〜3.5 時間かかった。**役は CI を待たない。** 上の「役（サブエージェント）の試験も CI で流す」は、この節で置き換える
- 役の門は手元の周: **増分ビルド + 自分の試験だけ**を、別デスクトップ（deskrun。`xyzzy-verify-gui-on-a-separate-desktop`）で流す。秒で回す。`XYZZY_CHAR_UNICODE=1` 側の Rebuild も役はしない
- **CI は統合の門。** 親が部品を束ねて統合し、統合の CI を 1 回流す（1 側のビルドもここで確かめる）。落ちたら落ちた部品だけを差し戻す
- 仕事は 30 分以内の部品（関数 1 つかファイル 1 つ）に割って並列に起こす。一般の規則はペア固有 Skill `small-parallel-parts-fast-local-loop-ci-only-at-integration`

## つまずいた点（再発させない）

- ランナーを `C:\Users\<名前>\…` の下に置くとサービスのアカウント（NETWORK SERVICE）が上の階層を読めず起動しない。`%ProgramData%` 側に置く
- サービスの初期化がイベントログの発行元を作った直後に書いてアクセス拒否になる。発行元を先に作る（`install-runner.ps1` が対応済み）
- サービスの TEMP は `NETWOR~1` のような 8.3 の名前を含みうる。試験は一時フォルダの 8.3 の名前に依らない形にする（`wfile-long-path-truename` を直した）

## 取り消し

師匠の明示の指示（confirmed）。取り消すときはこのファイルを消す。
