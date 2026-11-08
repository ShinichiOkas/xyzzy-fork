# BUG-0009 中間ディレクトリが構成とプラットフォームで共有され、x86 と x64 の obj が混在する

- 状態: 未修正
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09

## 症状
Win32 でビルドした中間ファイル（obj）が残った状態で x64 をビルドすると、リンクが失敗する。

## 実測（2026-11-09）
    Debug\xyzzy\alloc.obj : fatal error LNK1112: モジュールのコンピューターの種類 'x64' は対象コンピューターの種類 'x86' と競合しています。

Debug\xyzzy\ の中間ファイルを削除すると、x64 のビルドは通った。

## 参照箇所
- projects/xyzzy.vcxproj:47-48（OutDir と IntDir。IntDir は `$(SolutionDir)$(Configuration)\$(ProjectName)\` で、$(Platform) を含まない）

