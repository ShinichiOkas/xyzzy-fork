# S4 OLE Automation 削除スプリント — 残タスク TODO

- sprint_id: e792e26f-cd81-4c6c-9cba-7fe31b38e9e4
- 合意: `.pair-agent/agreements/remove-ole-automation.md`
- ブランチ: `feature/x64-only`
- 最終更新: 2026-11-21（上流 API 不調で中断）

## 済み（push 済み）

- [x] S4-1 `lisp/filer.l` の `ole-drop-files` 呼び出し削除（`8bc36d1`）
- [x] S4-2 `lisp/ole.l` 削除（`ed6e77d`）
- [x] S4-3 `lisp/builtin.l` の `ole-*`/`oledatap` 宣言 14 件削除（`ed6e77d`）
- [x] S4-4 `src/gen-syms.cc` の OLE DEFUN/MAKE_SYMBOL2Q 16 行削除（`d315a03`）
- [x] S4-5〜14（2026-11-22 再開）
  - S4-5 `src/gen/` は git 管理外。gen-src1 がビルド時に gen-syms.cc から再生成するので手作業なし
  - S4-6 `oledata.cc`/`oledata.h`/`oleconv.h` 削除
  - S4-7 `data.cc`（cleanup・GC マーク・dump/rdump）、`dataP.h` の `loledata` 削除。`lisp.h` は `Toledata` → `Tvacant2`（欠番）
  - S4-8 `pred.cc`（`type-of` の分岐・`Foledatap`）、`lprint.cc`（`print_oledata`）、`ed.h`（include）。`lisp/typespec.l` の `oledata` 行も削除
  - S4-9 `msgdef.h` の OLE 専用メッセージ 8 件削除（oledata.cc 以外の参照ゼロを確認）
  - S4-10 `init.cc` は `OleInitialize` だけで OLE Automation 固有の初期化なし（D&D が要るので残す）
  - 追加: `src/com.cc` に残っていた `Fole_drop_files` の本体と `oleconv.h` の include を削除（`com.h` の `safe_vidl` は DnD.cc が使うので残す）
  - S4-11 vcxproj・filters から 3 項目削除
  - S4-12〜14 unittest の OLE 部分削除
- [x] S4-15 確かめ（2026-11-22）
  - `install.bat`（ビルド＋バイトコンパイル）: 終了コード 0。ビルドは 0 エラー（警告は既存の C4244/C4267 のみ）
  - `run-tests.bat`: 558 件全件合格。S3 の 570 件（569 合格・1 失敗）から 12 件減 = `ole-tests.l` の 11 件 + `fix-ole-method`
  - `src/gen/*` と `xyzzy.exe` に `ole-create-object`・`ole-method`・`oledatap`・`ole-drop-files`・`#<oledata` の文字列なし
  - ショートカット: `*create-shortcut` → `resolve-shortcut` の往復で行き先・説明が一致
  - D&D: 起動したメインウィンドウに `OleDropTargetInterface` が付いている（`OleInitialize`＋`RegisterDragDrop` が効いている）

**S4 は閉じた。**

## 残 — 上流 API が落ち着いたら順に実行

合意ドキュメントの作業順序の続き（ステップ 5〜15）。

### ステップ 5: `src/gen/symtable.cc` の整合

- gen-src1/gen-src2 を回して自動再生成するのが正。手で削るなら以下を grep して該当行を削除:
  - `oledatap`, `oledata`, `ole-drop-files`, `ole-create-object`, `ole-get-object`,
    `ole-putprop`, `ole-method`, `ole-method*`, `ole-getprop`,
    `ole-create-event-sink`, `set-ole-event-handler`,
    `ole-enumerator-create`, `ole-enumerator-next`, `ole-enumerator-reset`,
    `ole-enumerator-skip`
- 併せて `src/gen/fns-decl.h`, `src/gen/msgcode.h`, `src/gen/msgdef.cc`, `src/gen/vars-decl.h` も
  OLE 関連シンボルが残っていないか確認

### ステップ 6: C++ 本体の削除

- `src/oledata.cc`（1109 行）を削除
- `src/oledata.h` を削除
- `src/oleconv.h` を削除

### ステップ 7: `src/data.cc`, `src/dataP.h` の Tole_object 整理

- Tole_object を S1 の Tvacant 方式で埋める（S1 コミット `7f54741` を参照）
- `src/dataP.h` の対応 enum/定義を調整

### ステップ 8: 述語・表示系の OLE 分岐削除

- `src/pred.cc` の OLE 関連分岐
- `src/lprint.cc` の OLE 関連分岐
- `src/lisp.h` の OLE 型定義
- `src/ed.h` の OLE 関連宣言

### ステップ 9: メッセージ定義

- `src/msgdef.h` の OLE 関連メッセージ削除
- 併せて `src/gen/msgcode.h`, `src/gen/msgdef.cc` の整合

### ステップ 10: 初期化処理

- `src/init.cc` の OLE Automation 固有初期化を削除
- **注意**: `CoInitialize`/`CoUninitialize` 自体は B（Shell Link: `src/com.cc`）と
  C（D&D: `src/DnD.cc`）が使うので残す

### ステップ 11: VS プロジェクトファイル

- `projects/xyzzy.vcxproj` から以下を削除:
  - `<ClCompile Include="..\src\oledata.cc" />` (L212)
  - `<ClInclude Include="..\src\oleconv.h" />` (L360)
  - `<ClInclude Include="..\src\oledata.h" />` (L361)
- `projects/xyzzy.vcxproj.filters` の対応エントリ (L204, L553, L556) も削除

### ステップ 12: unittest 削除

- `unittest/ole-tests.l` を全削除

### ステップ 13: unittest 部分修正

- `unittest/nanri-master-change-tests.l` の `fix-ole-method` テスト削除

### ステップ 14: unittest 部分修正

- `unittest/editor-tests.l` の OLE 使用部分削除:
  - L988: `(insert-and-eval "(load-library \"ole\")")`
  - L989: `(insert-and-eval "(setf fso (ole-create-object \"Scripting.FileSystemObject\"))")`
  - その周辺の `fso` 変数を使うコード

### ステップ 15: ビルドと試験

- VS2022 で x64 ビルドが 0 エラー
- `bytecompile.bat` 通る
- `run-tests.bat` で全件合格
  - 失敗していた `ole-method-null-and-empty` が「存在しない」側になる
- `ole-*` を含む Lisp シンボル / C++ シンボルがエクスポートに残っていないことを `nm`/`dumpbin` 等で確認
- D&D とショートカット作成 (B/C) が従来通り動くこと（手で filer を開く・D&D する）

## 事後条件（合意より）

- ビルドが通る（0 エラー）
- テストが 1 件（失敗していた `ole-method-null-and-empty`）減って全件合格
- `ole-*` を含む Lisp 関数 / C++ シンボルがエクスポートに残っていない
- D&D とショートカット作成は変わらず動く

## 中断理由（記録）

2026-11-21 のセッション中、Anthropic 側のストリーミング API の不調（"Streaming fallback
 triggered after 2 empty chunks" が頻発）と、auto mode 分類器の一時ブロックが重なり、
小刻みに止まる状態になった。師匠の指示で S4-4 まで push して TODO に落として中断。
上流が落ち着いたら S4-5 から順に再開する。
