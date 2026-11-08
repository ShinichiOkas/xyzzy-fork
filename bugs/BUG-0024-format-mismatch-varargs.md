# BUG-0024 書式指定子と可変引数の型が一致しない箇所（C4477 / C4313 / C4309 / C4319）

- 状態: 修正済み
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09（ポインタ幅の監査による）

## 症状（コンパイラ警告、2026-11-09 の x64 Debug ビルド）
| 場所 | 警告 | 内容 |
|---|---|---|
| src/process.cc:896 | C4477、C4313 | `sprintf` の `%u` に HANDLE。子プロセス（xyzzyenv）に渡す値で、受け取り側は src/xyzzyenv.cc で 32bit として読む |
| src/kbd.cc:1417 | C4477、C4313 | `sprintf` の `%08x` に HKL。レジストリのキー名（KLID の 8 桁） |
| src/except.cc:436、450、463 | C4477、C4313 | BUG-0021 と同じ |
| src/ColorDialog.cc:501、507 | C4477 | `sprintf` の `%d` に ULONG_PTR |
| src/archiver.cc:142 | C4477 | `printf` の `%d` に WPARAM（`#ifdef DEBUG` の中） |
| src/regex.cc:235 | C4477 | `printf` の `%5d` に `__int64` |
| src/gen-syms.cc:3054 | C4477 | `printf` の `%d` に `time_t`（生成器） |
| src/mouse.cc:254 | C4319 | `~button`（UINT）を WPARAM に零拡張 |
| src/bignum.cc:361 | C4309 | 定数値の切り捨て（この行の内容は未確認） |

## 注意
- 書式を変えると出力の文字列が変わる箇所がある。受け取り側（src/xyzzyenv.cc）と合わなくなるので、両側を確認すること（process.cc:896）。

## 参照箇所
- 上の表のとおり

