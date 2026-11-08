# BUG-0006 projects/extract.vcxproj が UNLHA32.LIB を要求し、リンクできない

- 状態: 未修正
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09

## 症状
projects/extract.vcxproj を x64 の Release でビルドすると、リンクが失敗する。

## 実測（2026-11-09）
    LINK : fatal error LNK1181: 入力ファイル 'UNLHA32.LIB' を開けません。

## 再現手順
`msbuild projects\extract.vcxproj /p:Configuration=Release /p:Platform=x64`

## 参照箇所
- projects/extract.vcxproj:67、83（AdditionalDependencies に UNLHA32.LIB）
- リポジトリに UNLHA32.LIB は無い（git ls-files で確認）

