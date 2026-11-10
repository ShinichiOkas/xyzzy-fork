# BUG-0003 den8view が 32bit の etc/den8util.dll と x86 の装飾名に依存する

- 状態: 修正済み（2026-11-10、S1。den8view と etc/den8util.dll を削除。決定 7）
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09

## 症状
lisp/den8view.l は x64 版で読み込めない（読み込み時に BUG-0001 と同じ FFI のエラーになる）。

## 実測
- etc/den8util.dll の PE の Machine は 0x014c（32bit）。Git に追跡されている。
- projects/den8util.vcxproj には x64 の構成がある。
- lisp/den8view.l:457-465 の `define-dll-entry` は、`_den8_folder_info@8`、`_parse_den8_cache@8`、`_find_den8_msgid@8`、`_den8_flush_cache@0`、`_den8_cache_dirty_p@4` という装飾名を指定している。

## 参照箇所
- lisp/den8view.l:456-466、585-587
- etc/den8util.dll
- projects/den8util.vcxproj

