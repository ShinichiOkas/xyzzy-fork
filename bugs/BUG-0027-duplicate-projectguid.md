# BUG-0027 ProjectGuid {2C956011-ED51-4699-BAC1-B7687910AF1D} が extract.vcxproj と xyzzyenv.vcxproj で重複している（現状は害なし）

- 状態: 未修正（低重大度）
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09（BUG-0008 の修正中に見つけた）

## 症状（コード上）
- 同じ GUID が projects/extract.vcxproj と projects/xyzzyenv.vcxproj の両方にある。
- ソリューションでは、xyzzy.sln は xyzzyenv を、projects/etc.sln は extract を含む。どちらも同じ GUID を参照している。

## 実測（2026-11-09）
- grep の結果: projects/extract.vcxproj、projects/xyzzyenv.vcxproj、projects/etc.sln、xyzzy.sln の 4 件に同じ GUID がある。
- 2 つのプロジェクトが同じソリューションに入ったときの動作は確かめていない。今の 2 つのソリューションは別々なので、ビルドには影響していない。

## 参照箇所
- projects/extract.vcxproj、projects/xyzzyenv.vcxproj、projects/etc.sln、xyzzy.sln
