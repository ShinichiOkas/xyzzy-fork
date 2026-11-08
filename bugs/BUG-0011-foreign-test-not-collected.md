# BUG-0011 unittest/foreign-test.l が全テスト実行の対象になっていない

- 状態: 未修正
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09

## 症状
unittest/foreign-test.l の 44 件の deftest は、全テスト実行（run-all-tests）の結果に現れない。Win32 版の基準（535 件）でも同じ。

## 実測（2026-11-09）
- 全テストの収集は `:wild "*-tests.l"` で行われる（unittest/run-tests-helper.l:13）。
- foreign-test.l は `*-tests.l` に一致しない（`*-test.l` の名前）。
- 結果の名前と突き合わせると、foreign-test.l の 44 件は 0 件だった。

## 参照箇所
- unittest/run-tests-helper.l:13
- unittest/foreign-test.l

