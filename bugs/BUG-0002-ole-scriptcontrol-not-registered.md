# BUG-0002 ole-tests: ScriptControl が x64 プロセスから作成できず失敗

- 状態: 環境依存（方針待ち）
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09

## 症状
unittest/ole-tests.l の `ole-method-null-and-empty` が x64 版で失敗する。

## 実測（2026-11-09、x64 Release、この PC）
失敗メッセージ（要約）:

    クラスが登録されていません: "ScriptControl"

Win32 版の基準（535 件すべて通過の記録）では通過していた。

## 再現手順
`run-tests.bat unittest/ole-tests.l`（x64 版）。

## 補足
テストは JScript の ScriptControl を作成している。x64 プロセスからこのクラスが見つからないことだけを観察した。登録が 32bit と 64bit のどちらにあるかは未確認。

