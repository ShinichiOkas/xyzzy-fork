# BUG-0017 etc/ の DLL・EXE が Git に追跡され、ビルドで上書きされる

- 状態: 方針待ち
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09

## 症状
etc/ には DLL と EXE が追跡されている。projects/etc.sln のビルドで、これらが上書きされて差分になる。

## 実測（2026-11-09）
- `git ls-files etc` に、etc/den8util.dll、etc/extract.exe、etc/gendic.exe、etc/gendicnm.exe、etc/jdwrap.exe、etc/tail-f.exe、etc/libxpi.lib が含まれる。
- etc.sln のビルドの後、git status に `M etc/den8util.dll`、`M etc/jdwrap.exe`、`M etc/tail-f.exe` が出た（この日の作業中に実測）。

## 参照箇所
- projects/etc.targets（EtcDir へのコピー）

