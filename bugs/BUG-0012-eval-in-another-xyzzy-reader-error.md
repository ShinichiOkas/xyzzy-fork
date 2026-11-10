# BUG-0012 テスト基盤: 別プロセスの評価がエラーを返すと、結果の読み戻しで reader-error になる

- 状態: 修正済み（2026-11-10、S0。子プロセスはエラーの文言を文字列で送る。確認: 子のエラーが親に届く）
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09

## 症状
`eval-in-another-xyzzy` で、別プロセスの評価がエラーになると、テストは期待値との比較ではなく、読み戻しのエラーで失敗する。

## 実測（2026-11-09、x64 版）
nanri-master-change-tests の `fix-for-FFI-c-function-return-doubl/float-00` の失敗表示:

    !! reader-error: ...: 2: 不正な構造体の構造です: (format-string x64 では ... format-arguments)

エラーの本当の内容（FFI 未対応）は、読み戻しに失敗して表示されない。

## 参照箇所
- unittest/simple-test.l（eval-in-another-xyzzy--make-command-string、eval-in-another-xyzzy--read-results）

