# BUG-0010 run-tests.bat の結果がコンソールに出ず、エディタのバッファにしか出ない

- 状態: 未修正
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09

## 症状
`run-tests.bat` を実行しても、結果は標準出力に出ない。結果はエディタ内の `*Test Results*` バッファにだけ出る。

## 実測（2026-11-09）
- `run-tests.bat` の標準出力をファイルに取ると、空だった。
- 結果の確認は、`*Test Results*` バッファの内容をファイルに書き出すスクリプトで行った（リポジトリには無い）。

## 参照箇所
- run-tests.bat
- unittest/run-tests-helper.l
- unittest/simple-test.l（get-report-buffer-stream。`*Test Results*`）

