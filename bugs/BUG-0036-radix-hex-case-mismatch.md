# BUG-0036 `write-to-string :radix t` は 16 進の英字を大文字で印字し、`~X` と `*print-base*` は小文字（不揃い）

- 状態: 修正済み（2026-11-22。師匠の判断で案 A: 小文字に揃える。src/lprint.cc の print_integer が radix のときも小文字の表を使う）
- 対象: x64 版（feature/x64-only）。x64 固有ではない
- 起票: 2026-11-22（S3 の作業中に見つけた）

## 症状
- 同じ整数 255 の 16 進表記が、経路によって大文字と小文字に分かれる。

## 実測（2026-11-22）
- `(write-to-string 255 :radix t :base 16)` => "#xFF"
- `(format nil "~X" 255)` => "ff"
- `(let ((*print-base* 16)) (prin1-to-string 255))` => "ff"
- `(write-to-string 255 :radix t :base 8)` => "#o377"
- `(write-to-string 255 :base 16)` => "ff"

## 原因（コード上）
- src/lprint.cc:687 と 680 が、`radix` が真なら大文字の表（`upcase_digit_char`）、偽なら小文字の表（`downcase_digit_char`）を選ぶ。`~X`・`~O`・`~B`（Format::radix、integer）と `*print-base*` は radix 偽の経路なので小文字になる。
- この選択は xyzzy.src の取り込み時（983d32b）から同じ形で、意図した振る舞いと見られる。

## 既存の試験（両方の振る舞いを固定している）
- unittest/fixnum-tests.l の `fixnum62-print`（427-460 行付近）が、`(format nil "~X" most-positive-fixnum)` => "1fffffffffffffff"（小文字）と、`(write-to-string most-positive-fixnum :base 16 :radix t)` => "#x1FFFFFFFFFFFFFFF"（大文字）の両方を期待値にしている。

## 判断が要る点
どちらの大文字・小文字に揃えるか。揃えると、既存の試験の期待値のどちらかを変えることになる。
- 案 A: 小文字に揃える。`:radix t` / `*print-radix*` 真のときの出力（"#x1fffffffffffffff"）だけが変わる。既定（`*print-radix*` が nil）の出力、`~X`・`*print-base*` は変わらない。lisp/ に `:radix t` や `*print-radix*` を使う箇所は見つからなかった（grep。:radix は parse-integer の引数のみ）。試験の 1 行の期待値が変わる。
- 案 B: 大文字に揃える。`~X`・`~O`・`*print-base*` の既定の出力が変わる（利用者に見える）。多くの試験・利用箇所に波及する。
- 案 C: 揃えず、意図した非対称として残す（票を閉じ、radix 付きは大文字と文書に書く）。
- 推奨: 案 A（既定の出力を変えずに揃う）。ただし upstream 由来の意図的な選択でもあるため、決めてから直す。

## 参照箇所
- src/lprint.cc:674-695（print_integer）、src/bignum.cc:1378（to_ascii）
- unittest/fixnum-tests.l（fixnum62-format-radix）
