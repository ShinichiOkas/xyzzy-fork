# BUG-0048 ドロップしたパスとショートカットの名前の変換が CP_OEMCP で、ほかの箇所の CP_ACP と食い違う

- 状態: 修正済み（2026-11-22、S4-c）
- 対象: x64 版（feature/x64-only、59f7cfe）。x64 固有ではない（Win32 の時代から）
- 起票: 2026-11-22（S4-c。親からの連絡）

## 症状
- ファイラへのドロップで受け取る UTF-16 のパス、ショートカットから取り出す名前を、`WideCharToMultiByte (CP_OEMCP, ...)` で内部の narrow 文字列に直している。ファイル名を UTF-16 に直す側（`MultiByteToWideChar (CP_ACP, ...)`）は CP_ACP で、往復で使うコードページが違う。OEM と ANSI のコードページが違う環境では、同じ名前が往復で変わる。

## 参照箇所
- src/DnD.cc: `filer_drop_target::check_self (const wchar_t *, ...)`、`make_drop_file (const wchar_t *, ...)`、`process_drop` の base_path の取り出し（3 か所。`WideCharToMultiByte (CP_OEMCP ...)`）。親ディレクトリ名・項目名の UTF-16 化（`MultiByteToWideChar (CP_ACP ...)` 2 か所）
- src/com.cc: `Fget_special_folder_location` の STRRET_WSTR（`CP_OEMCP`）、`set_appid`・`Fcreate_shortcut`・`Fresolve_shortcut` の UTF-16 化（`CP_ACP`）
- src/filer.cc: ファイラの項目の ParseDisplayName 用の UTF-16 化（`CP_ACP`）

## 修正
- 変換を W 版のファイルの層（src/wfile.cc）の `narrow_to_wide`・`wide_to_narrow` に一本化した。これは今までの A 版の API の変換と同じ cp932 で、コードページはこの 2 関数だけが決める（Char の拡幅のときにここだけ替える）。
- DnD.cc の `#if 0` に残っていた古い変換の分岐は消した。
