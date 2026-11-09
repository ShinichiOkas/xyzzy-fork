# BUG-0019 ldialog.cc: コンボボックスの :index 経路で、owner-draw の項目データを 32bit に切り詰める

- 状態: 要判断（設計）
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09（ポインタ幅の監査による）

## 症状（コード上）
- src/ldialog.cc:715-720 の `if (idx)` の分岐は、`CB_GETITEMDATA` の戻り値（LRESULT、64bit）を `make_fixnum` に渡す。720 行で C4244（「'LRESULT' から 'long'」）が出る。
- owner-draw の項目データは Lisp オブジェクトのポインタ（src/ldialog.cc:623 で `LPARAM (xcar (init))` を登録）。
- 同じ owner-draw の別の分岐（723-728 行）は、`lisp (...)` で値をそのまま返す。

## 実測
- 実行しての確認はしていない。ソースの読み取りと、コンパイラ警告（ldialog.cc:720、C4244）による。

## 判断が要る点
- owner-draw と `:index` を同時に指定したときの戻り値を、どうするか（設計）。
- 推奨: owner-draw の分岐と同じく、値を切り詰めずに返す。ただし `:index` の意味を確かめてから決める。

## 参照箇所
- src/ldialog.cc:623、715-728

## 追記（2026-11-10）
- owner-draw の combobox を持つテンプレートは src/xyzzy.rc にある: MULTIFILEOPENORD（85 行）、FILEOPENORD（122 行）、IDD_DIRECTORY（151 行）。いずれも C++ 側のファイルを開くダイアログで、Lisp の dialog 仕様から `combobox_result` を通る経路は見つからなかった（grep）。
- lisp/ には、owner-draw の combobox を指定している箇所は見つからなかった（lisp/gentempl.l は様式の名前の対応表のみ）。
