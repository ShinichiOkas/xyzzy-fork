# BUG-0029 空の環境変数 XYZZYINIFILE・XYZZYCONFIGPATH が「設定あり」として扱われ、設定場所の試験が失敗する

- 状態: 未修正（原因を確定。引き金は試験の実行環境の設定。本体の扱いが誤り）
- 対象: x64 版（feature/x64-only）。x64 固有ではない
- 起票: 2026-11-10（BUG-0001 の全件テストで見つけた。最初は断続的に見えたが、原因は環境変数の設定の違い）

## 症状
- 全件テストで、unittest/editor-tests.l の次の 3 件が失敗する（4 件のうち 1 件が xyzzy-ini-path、残りの 3 件が user-config-path-*）。
  - xyzzy-ini-path
  - user-config-path-startup-option
  - user-config-path-XYZZYCONFIGPATH
  - user-config-path-startup-option-and-XYZZYCONFIGPATH
- xyzzy-ini-path: (xyzzy-ini-path) が <repo>/usr/shini/wxp/（フォルダ）を返し、(merge-pathnames "xyzzy.ini" (user-config-path)) と一致しない。
- user-config-path-*: 子プロセスが一時フォルダに xyzzy.ini を作らず、delete-file が file-not-found になる。
- XYZZYINIFILE を明示する 3 件（xyzzy-ini-path-startup-option、xyzzy-ini-path-XYZZYINIFILE、xyzzy-ini-path-startup-option-and-XYZZYINIFILE）は通る。

## 原因（確認済み）
- 失敗した 2 回の全件実行は、PowerShell で `$env:XYZZYINIFILE = ''` と `$env:XYZZYCONFIGPATH = ''` を設定して起動していた。このとき、子プロセスには値が空の変数が存在した（[Environment]::GetEnvironmentVariable は ""）。
- 本体の扱い（src/init.cc）: init_user_inifile_path は getenv("XYZZYINIFILE") が NULL でなければ、値が空でもその値を使う（init.cc:211-236）。空の値では ini のファイル名が空になり、設定場所のフォルダそのものが ini の場所になる。init_user_config_path も、空の XYZZYCONFIGPATH を先に評価する（init.cc:173-175）。
- 再現（diag-config.l）: この設定で起動すると、xyzzy-ini-path は <repo>/usr/shini/wxp/（末尾が / のフォルダ）になった。XYZZYINIFILE と XYZZYCONFIGPATH は、どちらも ""。
- それ以降の全件実行は `Remove-Item Env:` で変数を消して起動したので、通った。断続的だったのではなく、起動の設定が違っていた。
- 公式の実行経路（run-tests.bat）は cmd の `set VAR=` を使う。cmd では、これは変数を消すので、この不具合は起きない。
- 補足: path-equal（src/fileio.cc:442 の same_file_p）は、文字列が一致しても、ファイルが存在しなければ nil を返す。これは本件の原因ではないが、この試験は ini ファイルの存在に依存している。

## 直す方向
1. src/init.cc: 値が空（""）の XYZZYINIFILE と XYZZYCONFIGPATH を、未設定として扱う（init_user_inifile_path と init_user_config_path）。
2. 試験を足す: 空の XYZZYINIFILE を子プロセスに渡しても、xyzzy-ini-path が既定の xyzzy.ini を返す（unittest/editor-tests.l）。
3. 全件テストの起動は、変数を消して行う（run-tests.bat は既にそうしている）。PowerShell で `$env:X = ''` を使わない。

## 参照箇所
- src/init.cc:171-237（init_user_config_path、init_user_inifile_path）
- src/fileio.cc:442（same_file_p）、src/pathname.cc:810（Fpath_equal）
- unittest/editor-tests.l:1028-1096
