# S4: OLE Automation support removal

- status: executing
- sprint_id: e792e26f-cd81-4c6c-9cba-7fe31b38e9e4
- created: 2026-11-21
- decision reference: docs/refactoring/decisions.md (決定8の文脈: 「将来必要になったら、DDE や OLE ではなく、名前付きパイプと簡単な JSON 形式などのモダンな方式で新しく設計する」)

## ゴール

xyzzy の OLE Automation サポート（Lisp から COM 部品を呼び出す窓口）を削除する。

## 範囲

**削除する (A: OLE Automation のみ)**

- `src/oledata.cc` (1109 行)
- `src/oledata.h`
- `src/oleconv.h`
- `lisp/ole.l` (118 行) — `#{...}` 読み取り構文と `ole-for-each` マクロ
- `lisp/builtin.l` の `ole-*` 関数宣言 (15個程度):
  - `ole-create-object`, `ole-get-object`, `ole-putprop`, `ole-method`, `ole-method*`, `ole-getprop`
  - `ole-create-event-sink`, `set-ole-event-handler`
  - `ole-enumerator-create/next/reset/skip`
  - `ole-drop-files`
- `unittest/ole-tests.l` — 全削除
- `unittest/nanri-master-change-tests.l` の `fix-ole-method` テスト
- `unittest/editor-tests.l` 内の OLE 使用部分（`Scripting.FileSystemObject` を使っている箇所）
- `src/gen-syms.cc` の該当 DEFUN 宣言（S3 で部分的に修正済みだが OLE 関連は残っている）
- `src/data.cc`, `src/dataP.h`, `src/pred.cc`, `src/lprint.cc`, `src/lisp.h` などの OLE 型定義と述語
- `src/msgdef.h` の OLE 関連メッセージ
- `src/init.cc` の OLE 初期化（CoInitialize は B/C で残すので境界に注意）

**残す (B: Shell Link, C: Drag and Drop)**

- `src/com.cc` (IShellLink によるショートカット作成、AppUserModel_ID 設定)
- `src/DnD.cc` (IDataObject, IDropSource, IDropTarget による D&D)
- `CoInitialize` / `CoUninitialize` 自体 (B/C が使う)

## 影響

- `lisp/filer.l` の「送る」機能 (`ole-drop-files` 呼び出し, 1416行付近) — フォールバック経路 (`call-process`) が既にあるので、`ole-drop-files` 呼び出しを削除するか常に nil を返す形にすれば、自動的にフォールバックに流れる
- 失敗テスト `ole-method-null-and-empty` が無くなる
- ユーザーが OLE Automation を使う拡張スクリプト (COM 経由で ScriptControl/FileSystemObject などを呼ぶもの) は動かなくなる — 判断 8 の XPI / 将来の AI 連携窓口 (名前付きパイプ+JSON) で代替予定

## リスク

- OLE 関連の型定義とシンボルが広範囲 (src/ 配下 15 ファイル・219 箇所) に散らばっている。削除時に `src/gen-syms.cc` / `src/gen/symtable.cc` の整合を保つ必要
- `src/data.cc` の Tole_object (object tag) を削除すると、タグ番号が空く (S1 の Tvacant 方式で埋める)
- `CoInitialize` 自体は B/C で必要なので、OLE Automation の削除で消さないこと

## 作業の順序

1. `lisp/filer.l` の `ole-drop-files` 呼び出しを削除 (フォールバックに任せる)
2. `lisp/ole.l` を削除 / `lisp/loadup.l` から参照を削除
3. `lisp/builtin.l` から `ole-*` 関数宣言を削除
4. `src/gen-syms.cc` の該当 DEFUN を削除
5. `src/gen/symtable.cc` を再生成 (または対応する行を手で削る)
6. `src/oledata.cc`, `src/oledata.h`, `src/oleconv.h` を削除
7. `src/data.cc`, `src/dataP.h` の Tole_object を Tvacant 化 (S1 の方式に倣う)
8. `src/pred.cc`, `src/lprint.cc` の OLE 分岐を削除
9. `src/msgdef.h` の OLE 関連メッセージを削除
10. `src/init.cc` から OLE Automation 固有の初期化を削除 (CoInitialize 自体は残す)
11. `projects/xyzzy.vcxproj` と `.filters` から oledata.cc/h を除去
12. `unittest/ole-tests.l` を削除
13. `unittest/nanri-master-change-tests.l` の `fix-ole-method` を削除
14. `unittest/editor-tests.l` の OLE 使用部分を削除 (`fso` 変数など)
15. ビルド → bytecompile.bat → run-tests.bat で確かめる

## 事後条件

- ビルドが通る (0 エラー)
- テストが 1 件 (失敗していた `ole-method-null-and-empty`) 減って全件合格
- `ole-*` を含む Lisp 関数 / C++ シンボルがエクスポートに残っていない
- D&D とショートカット作成は変わらず動く
