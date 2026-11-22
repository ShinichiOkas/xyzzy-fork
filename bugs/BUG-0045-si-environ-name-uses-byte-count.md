# BUG-0045 si:environ が環境変数の名前を切り出すとき、バイト数を文字数として渡している

- 状態: 修正済み（2026-11-22）
- 対象: x64 版（feature/x64-only、59f7cfe）。x64 固有ではない（Win32 の時代から）
- 起票: 2026-11-22（S4-c の設計の調査 docs/refactoring/s4-char-design.md §12 で見つけた）

## 症状
- 名前に 2 バイト文字（全角など）を含む環境変数があると、`(si:environ)` が返す名前の組が正しくならない。名前の後ろ（`=` とその後ろの値の一部）まで名前に入る。

## 原因（コード上の推測）
- src/environ.cc の `Fsi_environ` が `make_string (*e, eq - *e)` を呼ぶ。`make_string (const char *, size_t)` の第 2 引数は Char（文字）の数で、バイト数ではない。`eq - *e` はバイト数なので、名前に 2 バイト文字が 1 つあるたびに 1 文字ずつ長く切り出される。

## 参照箇所
- src/environ.cc の `Fsi_environ`
- src/string.cc の `make_string (const char *, size_t)`（`s2w (b, size, &string)` は size 文字ぶん読む）

## 修正
- 名前の文字数を `s2wl (*e, eq, 0)`（`=` の手前までの文字数）で数えて `make_string` に渡す。
- 試験: unittest/environ-tests.l の `test-environ-double-byte-name`（日本語の名前の環境変数を si:putenv で作り、si:environ の名前と値、si:getenv を確かめる）。修正前の本体では失敗（名前が nil）、修正後は通る。