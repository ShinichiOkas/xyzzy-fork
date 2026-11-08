# BUG-0005 自己更新（verup）の展開が 32bit の etc/extract.exe と UNLHA32.DLL に依存する

- 状態: 未検証
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09

## 症状（コード上）
lisp/verup.l は、LZH の展開に etc/extract.exe を子プロセスとして起動する。extract.exe は UNLHA32.DLL を import している。

## 実測
- etc/extract.exe の PE の Machine は 0x014c（32bit）。Git に追跡されている。
- UNLHA32.DLL は配布物に含まれない。

## 未実測
x64 版で verup を実行していない。

## 参照箇所
- lisp/verup.l:17-21
- src/etc/extract.cc:3（Unlha の extern 宣言）
- BUG-0004 と同じ依存（UNLHA32）

