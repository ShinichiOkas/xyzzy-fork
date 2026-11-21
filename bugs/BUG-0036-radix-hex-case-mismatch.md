# BUG-0036 `write-to-string :radix t` は 16 進の英字を大文字で印字し、`~X` と `*print-base*` は小文字（不揃い）

- 状態: 未修正
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
