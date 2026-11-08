# BUG-0018 winhelp.cc: HtmlHelp の第4引数が DWORD のため、64bit のスタックアドレスが切り詰められる

- 状態: 修正済み
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09（ポインタ幅の監査による）

## 症状（コード上）
- src/winhelp.cc:400 の `HTMLHELPPROC` は、第4引数を `DWORD` と宣言している。
- 444 行で `HtmlHelp (..., (DWORD_PTR)&link)`、448 行で `HtmlHelp (..., (DWORD_PTR)&err)` を呼ぶ。この時点で 64bit のアドレスが `DWORD` に変換される。

## 実測（2026-11-09）
- x64 ビルドの警告: `src/winhelp.cc(444)` と `src/winhelp.cc(448)` に C4244「'DWORD_PTR' から 'DWORD' への変換」。
- インストール済みの Windows SDK（10.0.26100.0）の `um/HtmlHelp.h:375-380` では、`HtmlHelpA` の第4引数は `DWORD_PTR`。
- 未実施: 実行時の動作（hhctrl.ocx に渡った値）の確認。

## 参照箇所
- src/winhelp.cc:400、430-431、440-448

