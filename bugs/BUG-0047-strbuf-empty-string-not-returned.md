# BUG-0047 StrBuf::make_string と make_substring が、長さ 0 のときに作った空の文字列を返さない

- 状態: 修正済み（2026-11-22）
- 対象: x64 版（feature/x64-only、59f7cfe）。x64 固有ではない（Win32 の時代から）
- 起票: 2026-11-22（S4-c の設計の調査 docs/refactoring/s4-char-design.md §12 で見つけた）

## 症状
- 長さ 0 のとき、`make_string_simple ("", 0)` で文字列を作るだけで捨て、そのあと 0 バイトの確保と別の文字列の作成に進む。空の文字列用の分岐が働いていない（捨てた文字列の分の確保がむだになる。0 バイトの確保の扱いにも頼っている）。

## 原因（コード上の推測）
- src/StrBuf.cc の `StrBuf::make_string` の `if (!l)` と `StrBuf::make_substring` の `if (l <= 0)` が、`make_string_simple ("", 0)` の結果を return していない。

## 参照箇所
- src/StrBuf.cc の `StrBuf::make_string`、`StrBuf::make_substring`

## 修正
- 2 か所とも `return make_string_simple ("", 0);` にした。
- 試験: unittest/stream-tests.l の `test-strbuf-empty-string`（princ-to-string・format nil・with-output-to-string・get-output-stream-string・`#:` の空の名前が、空の文字列になる）。修正の前後で結果は変わらない（旧コードも最後に長さ 0 の文字列を返していた）ので、通ることの確かめであり、修正の検出ではない。
- `make_substring` の長さ 0 の分岐は、Lisp から届く経路が見つからなかった（リーダーの `::foo`・`foo::`・`:` は、その手前でエラーになる）。試験は無い。