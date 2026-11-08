# BUG-0004 アーカイバー DLL（UNLHA32 など）は 32bit で、x64 プロセスから読み込めない

- 状態: 方針待ち（代替アーカイバーの選定）
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09

## 症状
x64 版では、外部のアーカイバー DLL を読めないため、アーカイバー操作（`extract-archive`、`create-archive`、`list-archive`、`delete-file-in-archive`、`convert-to-SFX`、`archiver-dll-version`）が動かない。対象の DLL は lisp/defs.l の `*archiver-dll-list*` にある 12 種。

## 実測（2026-11-09）
- 64bit の PowerShell から、32bit の DLL を `LoadLibrary` すると失敗し、`GetLastError` は 193（ERROR_BAD_EXE_FORMAT）になる。
- 同じ条件で、64bit のシステム DLL（`%windir%\System32\kernel32.dll`）は読み込めた。
- リポジトリにも etc/ にも、アーカイバー DLL は含まれていない（git ls-files で確認）。dist.bat は etc/ をそのままコピーする。
- ユーザー環境の DLL の bitness は、この PC では確認していない。

## 再現手順
64bit の PowerShell で `LoadLibraryW` を、32bit の DLL のパスに対して呼ぶ。

## 参照箇所
- src/arc-if.cc:44（GetProcAddress）、src/arc-if.cc:54（LoadLibrary）
- src/arc-if.h:127-211（DLL 名）
- lisp/defs.l:151-166
- src/archiver.cc:531-534（Lha::create、`a -d1n`）

## 補足
LZH の作成（Lha::create）に当たる x64 の代替は、この時点では見つかっていない（外部調査の結果）。

