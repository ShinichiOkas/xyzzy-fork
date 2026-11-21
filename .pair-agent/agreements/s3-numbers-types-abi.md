# S3: 数値と型・ABI（残り）

- status: executing
- sprint_id: 0f5f23e7-6c74-4a48-b0a5-9fc395a696dd
- created: 2026-11-22
- 計画: docs/refactoring/plan.md §4.3・§6 の S3（規模 M。fixnum を含めると L 寄り）
- 師匠の指示: 「S3の残りから始めて」（2026-11-22）
- 前段: `64c1eb0`（foreign.l の size_t・time_t・ptrdiff_t を 8 バイトへ。dump_version の変数を int64_t へ）

## ゴール

x64 の数値と型・ABI を正しくする。計画の S3 の完了の基準を満たす。

## 範囲と分担（並列）

| 片 | 中身 | 担当 | 主に触るもの |
|---|---|---|---|
| A FFI | BUG-0028 を実測で確かめて閉じる（8 バイトの値が引数・戻り値で落ちない）。BUG-0031 構造体の整列をメンバーの整列の最大値で計算する（入れ子も）。型表を一か所にし、C++ の sizeof・alignof と Lisp 側の表を突き合わせる試験を置く | 役（worktree） | lisp/foreign.l、unittest/foreign-tests.l、src/dll.cc、型表の検査 |
| B fixnum | fixnum を 62bit にする（判断 2）。即値の整数を pointer 幅で持ち、`most-positive-fixnum` は 2^61-1 | 役（worktree） | src/number.h・number.cc・bignum.*・lisp.h とその呼び出し側 |
| C dump_version | BUG-0025: `dump_header.version` を int64_t にする（今は long で切り詰めている） | 親 | src/data.cc |

## 完了の基準（計画より）

- foreign の試験が新しい期待値（8、24）で通る。型表の検査が通る
- install.bat が通る（ビルド 0 エラー）。全件テストが合格（今の基準は 558 件全件合格。増えた試験は内訳を報告する）
- `most-positive-fixnum` が 2305843009213693951（2^61-1）。fixnum と bignum の境目の算術・読み書き・比較・ハッシュが正しい
- BUG-0025・0028・0031 を閉じる。計画書の S3 に完了を書く

## リスク

- fixnum: `make_fixnum` 480 箇所・`fixnum_value` 213 箇所が 32bit の long を前提にしている。黙った切り詰め（C4244）が入りうる。境目の試験で押さえる
- ダンプイメージの形式が変わる（作り直すので可。計画 §9）
- A と B はどちらも data.cc の別の箇所を触る可能性がある。統合は親が行う
