# BUG-0011 unittest/foreign-test.l が全テスト実行の対象になっていない

- 状態: 修正済み（foreign テストを全テストに含めた。c-callable の試験は S1 で削除）
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09
- 更新: 2026-11-10

## 症状（修正前）
unittest/foreign-test.l の 44 件の deftest は、全テスト実行（run-all-tests）の結果に現れない。Win32 版の基準（535 件）でも同じ。

## 実測（2026-11-09）
- 全テストの収集は `:wild "*-tests.l"` で行われる（unittest/run-tests-helper.l:13）。
- foreign-test.l は `*-tests.l` に一致しない（`*-test.l` の名前）。
- 結果の名前と突き合わせると、foreign-test.l の 44 件は 0 件だった。

## 修正（2026-11-10）
- 改名: unittest/foreign-test.l → unittest/foreign-tests.l（`*-tests.l` の収集に入る）。
- 読み込みを止めていた、トップレベルの c:defun-c-callable（2 件）は、それぞれの試験の中で eval して定義するよう変えた。読み込みは通り、x64 では、その試験が c-callable 未対応のエラーで失敗する。
- x64 で正しくない期待値を、x64 の値に合わせた。
  - c:c-size-of の void*・char*・string は 8 バイト（固定値 8）。
  - expand-c-vaargs と vaargs-macro の c:string は :uint64（固定値）。期待値を、実装と同じ判定（*features* の :x64）で作ると、判定が誤っても気づけないので、固定値にした。
- 既知の問題（BUG-0028、BUG-0031）に当たる期待値は、コメントで明記した（まだ誤った値のまま。票の修正で直す）。
- 全件の再実行で、設定場所の試験が断続的に失敗したことがあった（BUG-0029）。この修正とは別の問題。

## 実測（2026-11-10、x64 Release の xyzzy）
- 全件: 578 件（修正前の 533 件に、foreign の 45 件を足した）。573 件が成功、5 件が失敗（想定どおり。c-callable の 4 件と、環境依存の ole 1 件）。
- foreign の 45 件のうち、43 件が成功。失敗は c-callable の 2 件（defun-c-callable-stdcall、defun-c-callable-cdecl）。

## 参照箇所
- unittest/run-tests-helper.l:13
- unittest/foreign-tests.l
