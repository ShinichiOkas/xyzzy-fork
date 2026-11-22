# BUG-0062 run-tests.bat に無い試験ファイルを渡すと、終わらずに止まる

- 状態: 未修正（2026-11-22。見つけたのは S4 段 3 R8 の w2s の片）

## 見たこと
- `ci/ci-test.ps1 -Tests` に、存在しない試験ファイル（`unittest\pathname-tests.l`・`filer-tests.l`・`process-tests.l`）を含めて渡した。
- 試験の xyzzy が 8 分たっても終わらなかった。外から止めた（TaskStop。deskrun が子のプロセスを片付けた）。
- 存在するファイルだけを渡した次の実行は、168 件が合格して普通に終わった。
- 期待: 無いファイルは「見つからない」と出して失敗で終わる。
- 関連: BUG-0060（試験が終わらないと run-tests が止まる）。
