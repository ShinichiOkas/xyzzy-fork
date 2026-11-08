# BUG-0013 x64 版にも Win9x 専用の分岐が残っている

- 状態: 方針待ち（削除範囲の判断）
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09

## 症状（コード上）
x64 版のソースに、Win9x 専用の分岐がまだ残っている（VxD のコードは削除済み。commit 1609b02）。

## 実測（grep で確認、2026-11-09）
- src/process.cc:727、740-746（signal_win95 の呼び出し）、840-863（定義）
- src/fileio.cc:1048-1049（make_backup_file の `!sysdep.WinNTp ()` 分岐）
- src/kanji.cc:493（`!sysdep.WinNTp ()`）
- src/insdel.cc:1523-1525（Fclipboard_empty_p）
- src/conf.cc:682-685（レジストリ削除の分岐）
- src/kbd.cc:1393（`sysdep.Win95p ()`）

## 参照
x64 版は NT 系の Windows を対象にする。これらの分岐条件は NT 系では通らないと読める（実行しての確認ではない）。

