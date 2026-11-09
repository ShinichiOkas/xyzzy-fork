# BUG-0029 editor-tests の設定場所の試験（xyzzy-ini-path・user-config-path-*）が断続的に失敗する

- 状態: 未修正（断続的。原因の仮説あり、未確定）
- 対象: x64 版（feature/x64-only）。x64 固有ではない（推定）
- 起票: 2026-11-10（BUG-0001 の全件テストで見つけた）

## 症状
- 全件テストで、次の 4 件が失敗することがあった。unittest/editor-tests.l:1028-1096。
  - xyzzy-ini-path
  - user-config-path-startup-option
  - user-config-path-XYZZYCONFIGPATH
  - user-config-path-startup-option-and-XYZZYCONFIGPATH
- xyzzy-ini-path: `(path-equal (merge-pathnames "xyzzy.ini" (user-config-path)) (xyzzy-ini-path))` が nil になった。
- user-config-path-*: 別プロセス（eval-in-another-xyzzy）の後で、`(delete-file "xyzzy.ini")`（一時フォルダ内）が file-not-found になった。

## 実測（2026-11-10）
- 全件テストは 5 回中 2 回（最初の 2 回。その間、独立レビューの役が並列で動いていた）で、この 4 件が失敗した。残りの 3 回は通った。
- editor-tests.l 単独では、124 件すべて通った。
- foreign-tests.l を外した全件では通った（530/533。失敗は c-callable 2 件と、ole の環境依存 1 件）。
- 次の組み合わせでも通った: editor-tests.l と foreign-tests.l、先頭側（array・common-lisp・complete・defun-builtin・environ・eval）を加えた組み合わせ、後半（glob・lisp・nanri・num-arith・ole・pathnames・random・sequence・stream・system）を加えた組み合わせ。typespec-tests.l を含む全件は、3 回のうち 2 回だけ失敗した。組み合わせの原因は特定できていない。
- 本体の値は、診断用の起動では一致した（user-config-path = <repo>/usr/shini/wxp/、xyzzy-ini-path = <repo>/usr/shini/wxp/xyzzy.ini）。

## 仮説（コードで確認した部分）
- src/pathname.cc:810 の Fpath_equal は same_file_p（src/fileio.cc:442）を使う。文字列が一致しても、ファイルが存在しなければ nil を返す（GetFileAttributes(path1) != -1 の判定）。
- つまり xyzzy-ini-path の試験は、<repo>/usr/shini/wxp/xyzzy.ini が存在する間だけ通る。失敗した回では、そのファイルが無かった可能性が高い。ただし、なぜ無くなったかは確かめていない。
- user-config-path-* の失敗は、子プロセスが一時フォルダに xyzzy.ini を作らなかったことを示す。手で同じ起動（-config を付けて -e で評価）をすると、xyzzy.ini は作られた。子プロセスが ini を書く条件は確かめていない。

## 直す方向（案）
1. 試験を、ファイルの存在に依存させない（パスを文字列として比べる）。path-equal は、存在しないファイルでは使えない。
2. xyzzy.ini が消える経路を調べる（本体の起動・終了時の書き出し、子プロセスの終了の仕方）。
3. 負荷をかけた状態で 4 件を繰り返し実行し、再現させてから直す。

## 参照箇所
- unittest/editor-tests.l:1028-1096
- unittest/simple-test.l:1036-1058（eval-in-another-xyzzy）
- src/pathname.cc:810（Fpath_equal）、src/fileio.cc:442（same_file_p）
- src/init.cc:171-237（user-config-path と xyzzy-ini-path の決め方）
