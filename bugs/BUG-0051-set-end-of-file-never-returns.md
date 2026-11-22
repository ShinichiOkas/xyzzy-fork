# BUG-0051 `si:set-end-of-file` が :io のファイルストリームで返らない

- 状態: 未検証（2026-11-22、S4-c で見つけた。調査していない）
- 対象: x64 版（feature/x64-only、S4-c の作業ツリー 673dc50 の上）
- 起票: 2026-11-22（S4-c）

## 症状
- `-q -l` の起動のスクリプトで、次の式を実行すると `si:set-end-of-file` から返らない（xyzzy が応答しないまま残る）。小さいファイルでも同じ。
  ```
  (setq *f* (make-temp-file-name))
  (setq *s* (open *f* :direction :io :if-exists :overwrite :if-does-not-exist :create))
  (format *s* "abcdefghij")
  (file-position *s* 3)
  (si:set-end-of-file *s*)      ; 返らない
  ```
- エラーが出て、見えないダイアログで止まっているだけかもしれない（`handler-case` で包んでも何も出なかった）。

## 参照箇所
- src/stream.cc `Fset_end_of_file`（`SetEndOfFile (HANDLE (_get_osfhandle (_fileno (xfile_stream_output (stream)))))`）

## 補足
- S4-c の試験（unittest/wfile-tests.l）は、これを使わず、疎ファイルの末尾の 1 バイトを書いて 4GB を超えるファイルを作っている。
- S4-c の層（wfile）は `SetEndOfFile` を通らない（stream.cc の `Fset_end_of_file` は S4-c 以前のまま）。S4-c の変更で起きたかどうかは、S4-c の前の版での確かめをしていない。
