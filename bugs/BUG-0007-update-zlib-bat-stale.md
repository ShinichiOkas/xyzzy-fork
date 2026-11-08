# BUG-0007 misc/update_zlib.bat が現在の環境で使えない

- 状態: 未修正
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09

## 症状
misc/update_zlib.bat は、現在の環境では動かない。

## 実測（2026-11-09）
- 取得先 `http://zlib.net/zlib128.zip` は `curl -sI` で `HTTP/1.1 404 Not Found` を返した。
- スクリプトは `call "%VS120COMNTOOLS%\vsvars32.bat"` を使う。VS2013（32bit）専用の環境変数で、この PC には無い。

## 補足（今回の作業）
x64 版の zlib.lib は、同じ版（zlib 1.2.8。src/zlib/zlib.h と一致を確認）の fossils のソースを、x64 の vcvars64 と nmake でビルドして作った。このスクリプトは使っていない。

## 参照箇所
- misc/update_zlib.bat:8-17

