# BUG-0008 projects/etc.sln に Win32 の構成が残り、ビルドできない

- 状態: 未修正
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09

## 症状
projects/etc.sln の Release|Win32 をビルドすると、構成が見つからずエラーになる。

## 実測（2026-11-09）
    error MSB8013: このプロジェクトには、Release|Win32 の構成とプラットフォームの組み合わせが含まれていません。

- etc.sln には Win32 の構成が 18 件、x64 は 0 件（grep で確認）。
- 参照先の vcxproj（den8util、extract、jdwrap、tail-f）は x64 の構成だけになっている。

## 参照箇所
- projects/etc.sln

