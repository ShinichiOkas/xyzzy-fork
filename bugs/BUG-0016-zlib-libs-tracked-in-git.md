# BUG-0016 src/zlib の zlib.lib / zlib.pdb が Git に追跡されている

- 状態: 保留（オーナーの判断待ち）
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09

## 症状
src/zlib/Debug/ と src/zlib/Release/ の zlib.lib、zlib.pdb は Git に追跡されている。.gitignore の `[Dd]ebug/` の規則は、追跡済みのファイルには効かない。

## 実測（2026-11-09）
- `git ls-files src/zlib` は 4 件を返す。
- x64 の zlib をビルドして上書きすると、4 件とも `M`（変更）として表示された。

## 参照箇所
- .gitignore:79（`[Dd]ebug/`）

