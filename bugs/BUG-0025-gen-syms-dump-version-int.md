# BUG-0025 gen-syms: 画像の版番号 dump_version が int（time(0) 由来）で、2038 年に折り返す

- 状態: 未修正（低重大度）
- 対象: x64 版（feature/x64-only）。x64 固有ではない
- 起票: 2026-11-09（ポインタ幅の監査の修正中に見つけた）

## 症状（コード上）
- 生成器 src/gen-syms.cc:3054 が `printf ("int dump_version = %lld;\n", (long long) time (0));` を出す。
- 生成物（src/gen/dumpver.cc、Git の管理外）は `int dump_version = 1794163739;` の形になる。
- 値は int なので、2038-01-19 以降は負の値に折り返す。
- src/data.cc:2709（extern int）、2762（head.version = dump_version）、2800（版の比較）で使われる。

## 実測（2026-11-09）
- 生成物の値は 1794163739（ビルドごとに変わる）。折り返しは実測していない（コードからの推論）。
- 修正役が、生成器の printf の型を %d から %lld に直したが、変数の型（int）は変えていない（修正役の報告）。

## 影響（推論）
- 比較は同じ変数どうしで行うため、折り返しても版の不一致は起きにくいと見られる。実害は未確認。

## 参照箇所
- src/gen-syms.cc:3050-3055
- src/data.cc:2709、2762、2800
