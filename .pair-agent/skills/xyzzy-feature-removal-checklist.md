---
name: xyzzy の機能を削除するときの確かめ表（メッセージ・共有ヘッダー・生成物・型タグ・試験件数）
description: .cc を消したときの取り残しを機械で洗い出す手順。msgdef.h の MSG は「消したファイルでの使用 × 残りでの参照数」。共有ヘッダーは残す側が include していないか grep。src/gen/ は手で直さない。型タグは Tvacant 系で欠番にする。試験件数の減少は消した deftest 数と突き合わせる
type: process
maturity: forming
proposed_by: ai
created_at: 2026-11-22
provenance: xyzzy-fork S1（DDE 削除）と S4（OLE Automation 削除）
---

## 手順

1. **メッセージ（`src/msgdef.h` の `MSG (名前, ...)`）**: 消したファイルが使っていた名前を挙げ、それぞれ**残りのソースでの参照数**を数える。ゼロのものだけ `MSG` ごと消す。S4 では `oledata.cc` が使っていた 8 件（`Ecannot_convert_to_variant`・`Einvalid_idispatch` など）が、残りの参照ゼロで消せた。1 件でも残りが参照していれば消さない
2. **共有ヘッダー**: 消す側と残す側が両方 include するヘッダー（S4 では `oleconv.h`）は、残す側（`com.cc`・`DnD.cc`）が include していないかを `grep` で確かめてから消す。S4 では `com.cc` が `oleconv.h` を include し、`Fole_drop_files` の本体も残っていたので、先にそちらを削った。`com.h` の `safe_vidl` は `DnD.cc` が使うので残した。合意ドキュメントの削除対象に載っていなくても、grep で出てきたら扱う
3. **`src/gen/`**: git 管理外。`gen-src1` がビルド時に `gen-syms.cc` から再生成する。手で直さない。直すのは元の `src/gen-syms.cc`（DEFUN・MAKE_SYMBOL）と `lisp/builtin.l`・`lisp/typespec.l` の宣言
4. **型タグ（`src/lisp.h` の enum）**: 要素を消さず、`Tvacant`・`Tvacant2` のような欠番にして、以降の番号を保つ（S1: DDE handle → `Tvacant`、S4: oledata → `Tvacant2`）。2 回観測。`data.cc`（cleanup・GC マーク・dump/rdump）、`dataP.h`、`pred.cc`（`type-of`）、`lprint.cc` の分岐も一緒に消す
5. **VS プロジェクト**: `projects/xyzzy.vcxproj` と `.vcxproj.filters` から項目を消す。残すものとの境界（S4 では `CoInitialize`／`OleInitialize` は D&D とショートカットが使うので残す）は、合意ドキュメントの「残す」に書いてある通りに守る
6. **試験の件数**: 件数が減るのは削除の自然な結果。報告には、**減った件数を消した `deftest` の数で突き合わせて**書く（S4: 570 → 558 = `ole-tests.l` の 11 件 + `fix-ole-method` の 1 件）。突き合わせが合わなければデグレード（[[test-count-decrease-signals-degradation]]）

## 確かめ（ビルド後）

- `src/gen/*` と `xyzzy.exe` に、消した名前の文字列が残っていないこと（S4: `ole-create-object`・`ole-method`・`oledatap`・`ole-drop-files`・`#<oledata`）
- 残した機能が動くこと → [[xyzzy-verify-gui-features-without-a-human]]
- 編集の道具の注意 → [[xyzzy-cp932-crlf-source-editing]]

## 取り消し

AI の観測（2 スプリント）。師匠の原文の裏付けは無い。取り消すときはこのファイルを消す。
