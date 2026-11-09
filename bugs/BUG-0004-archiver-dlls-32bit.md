# BUG-0004 アーカイバー DLL（UNLHA32 など）は 32bit で、x64 プロセスから読み込めない

- 状態: 検証済み（7-Zip の CLI の条件を確認。LZH の作成・読みと、同梱の扱いは判断待ち。2026-11-10 の追記を参照）
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09

## 症状
x64 版では、外部のアーカイバー DLL を読めないため、アーカイバー操作（`extract-archive`、`create-archive`、`list-archive`、`delete-file-in-archive`、`convert-to-SFX`、`archiver-dll-version`）が動かない。対象の DLL は lisp/defs.l の `*archiver-dll-list*` にある 12 種。

## 実測（2026-11-09）
- 64bit の PowerShell から、32bit の DLL を `LoadLibrary` すると失敗し、`GetLastError` は 193（ERROR_BAD_EXE_FORMAT）になる。
- 同じ条件で、64bit のシステム DLL（`%windir%\System32\kernel32.dll`）は読み込めた。
- リポジトリにも etc/ にも、アーカイバー DLL は含まれていない（git ls-files で確認）。dist.bat は etc/ をそのままコピーする。
- ユーザー環境の DLL の bitness は、この PC では確認していない。

## 再現手順
64bit の PowerShell で `LoadLibraryW` を、32bit の DLL のパスに対して呼ぶ。

## 参照箇所
- src/arc-if.cc:44（GetProcAddress）、src/arc-if.cc:54（LoadLibrary）
- src/arc-if.h:127-211（DLL 名）
- lisp/defs.l:151-166
- src/archiver.cc:531-534（Lha::create、`a -d1n`）

## 補足
LZH の作成（Lha::create）に当たる x64 の代替は、この時点では見つかっていない（外部調査の結果）。（2026-11-10 の追記で更新）

## 追記（2026-11-10、7-Zip 26.06 x64 と UnLha64x の検証）
- 検証の方法: 公式配布物（7z2606-extra.7z の x64\7za.exe、SHA-256 dd3757f3…84ffa（計算値）。7z2606-x64.exe の展開物の 7z.exe）を sandbox で実行した。UnLha64x は実行していない（リポジトリの読みと配布物の一覧だけ）。
- 確認した終了コード（作業フォルダの外で実行）: `a -hide` は exit 7（Too long switch）、`a -ms=off` は exit 0、`-sccUTF8` は exit 7、`-sccUTF-8` は exit 0。

### 7-Zip の CLI（xyzzy が送る形のままでは動かない）
- `-hide` は無効。xyzzy の `ArchiverP::doit` が組み立てる文字列に含まれる（src/archiver.cc:211-223 付近）。
- 展開先は `-o{dir}` で渡す必要がある。xyzzy の形（出力先を裸で渡す。src/archiver.cc の copy_dir 経由）では、展開されない、またはカレントに書き出す（exit 0 のまま）。
- リスト（@ファイル）は cp932 で書かれている（src/archiver.cc:737-755）。7-Zip は既定で UTF-8 として読むので、`-scsWIN` が要る。cp932 のリストは、`-scsWIN` なしでは exit 7。

### 形式ごとの作成と読み
| 形式 | 作成 | 読み |
|---|---|---|
| 7z、ZIP、tar、gzip、bzip2、xz | できる（exit 0） | できる |
| LZH | できない（exit 2、実装されていません） | 7z.exe は読める（上流の LHa 標本 2 件。cp932 の名前は 7-Zip が cp932 として読む）。7za は読めない |
| CAB、ARJ、RAR | できない | 標本がなく確かめていない |

### UnLha64x（v1.08、最新は 2026-10-25）
- 作成（`a` コマンド）はあるが、作者は「新規に lzh 形式の圧縮ファイルを採用することは推奨されていません」と書いている（README.md 10-14 行）。
- 再配布の条件（README.md 242-263 行、LhaForUnix の lha.man）: バイナリだけの配布は許されない。同梱するなら、ソースと文書を添える必要がある。LICENSE ファイルはない。
- ビルドには `UNLHA32.H` が要る。リポジトリの Header/ にはなく、置き場所の説明だけがある。このままではビルドできない。
- xyzzy が呼ぶ関数のうち、UnlhaGetSubVersion は無い（版の下位の番号が出ない）。UnlhaConfigDialog はスタブ（設定画面が出ない）。ほかは出ている。

### 判断が要る点（オーナーの判断）
1. LZH を作成するか。推奨: 作成しない。7-Zip は作成できず、UnLha64x は再配布の条件とビルドの前提が未解決。
2. LZH を読むか。読むには 7z.exe（7z.dll を含む）が要る。7z.dll は RAR の復号を含み、unRAR の制限（インストーラの License.txt 131-144 行）が付く。推奨: 最初の版は 7za（LGPL と BSD だけ）で ZIP・7z・tar・gzip などを扱い、LZH の読みは判断を待つ。
3. 7-Zip の実行ファイルをリポジトリに同梱するか、利用者の PC のものを使うか。同梱するなら、LGPL の表示を添える。推奨: 同梱せず、設定でパスを指定する（配布物を増やさない）。

