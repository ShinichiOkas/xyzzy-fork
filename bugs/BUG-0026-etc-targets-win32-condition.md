# BUG-0026 etc.targets の後処理コピーが Release|Win32 の条件のままで、x64 のビルドでは etc/ が更新されず、etc/ の実行ファイルは x86 のまま

- 状態: 方針待ち（etc/ の扱いの判断。BUG-0017 と連動）
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09（BUG-0008 の修正中に見つけた）

## 症状（コード上）
- projects/etc.targets:7 の Copy（$(TargetPath) を $(EtcDir) へ）の条件は `'$(Configuration)|$(Platform)'=='Release|Win32'`。
- x64 の構成では条件が成立せず、etc/ へのコピーは走らない。

## 実測（2026-11-09）
- x64 Release で projects\etc.sln をビルドした後も、git status に etc/ の変更は出なかった（修正役の検証）。
- etc/ の追跡されている実行ファイルの COFF Machine は、いずれも 0x014C（x86）: etc/den8util.dll、etc/extract.exe、etc/jdwrap.exe、etc/tail-f.exe。
- 影響: lisp/den8view.l が読む etc/den8util.dll は x86 のまま。x64 の xyzzy からは読めない（BUG-0001、BUG-0003 も参照）。

## 判断が要る点
- 条件を x64 に直すと、x64 のビルドで etc/ の追跡ファイルが上書きされる（BUG-0017 と同じ差分が出る）。etc/ を x64 の成果物に入れ替えるかは、リポジトリの方針の判断。

## 参照箇所
- projects/etc.targets:7
- projects/etc.sln、projects/den8util.vcxproj（x64 の構成あり）
- etc/den8util.dll、etc/extract.exe、etc/jdwrap.exe、etc/tail-f.exe
