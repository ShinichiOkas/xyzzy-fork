# BUG-0022 wheel.cc: SetTimer の戻り値（UINT_PTR）を UINT に格納している

- 状態: 修正済み
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09（ポインタ幅の監査による）

## 症状（コード上）
- src/wheel.cc:153 の `UINT timer_id;` に、`SetTimer` の戻り値（UINT_PTR）を代入している（185 行、234 行）。
- 267 行で `msg.wParam == param.timer_id` と比較している（wParam は WPARAM、64bit）。
- 184 行と 280 行で `KillTimer (0, param.timer_id)` を呼んでいる。

## 実測（2026-11-09）
- x64 ビルドの警告: `src/wheel.cc(185)`、`(234)` に C4244（「'UINT_PTR' から 'UINT'」）。
- x64 で SetTimer が返す ID が 32bit を超えるかどうかは確認していない。

## 参照箇所
- src/wheel.cc:153、184-185、234、267、280

