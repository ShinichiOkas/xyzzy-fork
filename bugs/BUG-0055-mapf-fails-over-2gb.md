# BUG-0055 mapf（mman.cc）が 2GB を超えるファイルを開けず、2〜4GB のファイルの compare-file が失敗する

- 状態: 修正済み（2026-11-22）
- 対象: x64 版（feature/x64-only、5e40677）。S4-c（W 版のファイル処理）で入った退行
- 起票: 2026-11-22（S4-c の役の報告より）

## 症状
- `mapf::open`（src/mman.cc）は、サイズを `wfile::get_file_size`（GetFileSizeEx）で取り、2GB（0x7fffffff）を超えると `ERROR_FILE_TOO_LARGE` で失敗する。サイズを持つ `mf_size` と `size ()` が DWORD だった。
- `compare-file` は両方のファイルを `mapf` で開くので、2〜4GB のファイルの比較が file-error になる。以前（GetFileSize の下位 32bit）は、4GB 未満なら正しいサイズで開けていた。
- x64 ではアドレス空間に余裕があり、2GB を超えるファイルも写像できるはず。

## 再現
- 疎ファイルを 2 つ作り（`fsutil sparse setflag`、末尾 1 バイトだけ書く。`unittest/wfile-tests.l` の `wfile-make-sparse-file`）、`(compare-file a b)` する。
- 3GB（3221225473 バイト）と 5GB（5368709121 バイト）のどちらも `file-error`（「ファイル サイズが、許可された制限を超過しているため、保存できません。」= ERROR_FILE_TOO_LARGE）。

## 修正
- `mapf` の `mf_size`・`size ()` を `size_t` にし、2GB の上限の分岐は外した（size_t に収まらない場合だけ ERROR_FILE_TOO_LARGE。x64 では起きない）。写像が本当にできないとき（アドレス空間・メモリ不足）は CreateFileMapping / MapViewOfFile の失敗で今までどおり open が 0 を返す。`init ()` で `mf_size` も 0 に戻す。
- 利用者のうち `compare-file`（memcmp の長さ）は size_t のまま 64bit で通る。内部の長さが int の読み込み（src/fileio.cc の `Buffer::read_file_contents`・`readin_chunk`）は、読む範囲が 2GB（INT_MAX）を超えるときに、今までと同じく ERROR_FILE_TOO_LARGE（RFCS_OPEN）で失敗させる。`detect_encoding`・`detect_eol_code` の長さの計算は size_t に合わせた。
- 試験: `unittest/wfile-tests.l` の `wfile-compare-file-3gb`・`wfile-compare-file-5gb`（同じ内容は t、末尾だけ違うと nil。後始末は unwind-protect）。
- 試験の時間: 3GB の疎ファイル 2 つの比較は 1 回約 3 秒（5GB は約 5 秒）。memcmp が両方の写像の全ページを読み込む（疎ファイルは 0 のページが返るだけだが、読むこと自体は 3GB 分かかる）ので一瞬ではなく、2 つの試験で約 27 秒。2GB・4GB の境界を確かめるには境界の上の大きさが要り、縮めても 25% ほどしか減らないので、3GB と 5GB のままにした。
- 確かめ: 3GB の疎ファイルを `insert-file-contents` すると file-error（ERROR_FILE_TOO_LARGE）、範囲を指定した `insert-file-contents`（先頭 10 バイト）は成功。利用者の見直し: doc.cc・dockbar.cc・edict.cc は size_t との比較だけで変更不要（edict の `long (dic.size ())` は辞書ファイルが 2GB 未満の前提のまま）。ビルドの警告は増えていない。全件 813 件合格。
