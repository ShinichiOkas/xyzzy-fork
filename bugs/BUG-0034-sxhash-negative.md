# BUG-0034 sxhash が負の値を返すことがある

- 状態: 未修正
- 対象: x64 版（feature/x64-only）。Win32 の時代から（long が 32bit のため）あったと見られる
- 起票: 2026-11-22（S3 の作業中に見つけた）

## 症状
- `sxhash` が負の整数を返すことがある。CL では非負の fixnum であるべき。

## 実測（2026-11-22）
- `(sxhash -1)` => -1
- `(sxhash 3000000000)` => -1294967296
- `(sxhash 2147483648)` => -2147483648
- `(sxhash 1.5d0)` => 1073217536（これは正）
- 3000 個の `(list i (* i 1000003))` のうち 1097 個、3000 個の double（`(* i 1.37d0)`）のうち 1427 個で負になった。

## 原因（コード上の推測）
- src/hash.cc の `Fsxhash` が `make_fixnum (sxhash_equal (object, 0))` を返す。`sxhash_equal` は `u_int` を返し、`make_fixnum` の引数は `long`（Windows では 32bit）なので、2^31 以上の値が負に変わる。

## 参照箇所
- src/hash.cc:554-557（Fsxhash）
- src/number.h:220（make_fixnum (long)）
