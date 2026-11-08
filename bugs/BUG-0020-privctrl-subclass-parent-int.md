# BUG-0020 privctrl: subclass_parent が int を返し、旧 WNDPROC（64bit）を切り詰める

- 状態: 修正済み
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09（ポインタ幅の監査による）

## 症状（コード上）
- src/privctrl/privctrl.cc:31 の `int subclass_parent (HWND)` は、41 行で `SetWindowLongPtr` の戻り値（LONG_PTR、旧 WNDPROC のアドレス）を `int` として返す。

## 実測（2026-11-09）
- x64 ビルドの警告: `src/privctrl/privctrl.cc(41)` に C4244（「'LONG_PTR' から 'int'」）。
- 呼び出し元（src/privctrl/listviewex.cc:1004）は戻り値を使っていない（ソースの読み取り）。

## 潜在する問題
- 戻り値を使う将来の呼び出し元は、切り詰めた値を旧 WNDPROC として使うことになる。

## 参照箇所
- src/privctrl/privctrl.cc:31-41
- src/privctrl/privctlimpl.h:23
- src/privctrl/listviewex.cc:1004

