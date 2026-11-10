# x64 版 リファクタリング計画（目標の設計と移行）

- 状態: 案。オーナーの判断待ちの項目がある（「8. 判断してほしいこと」）。
- 作成日: 2026-11-10
- 対象: feature/x64-only（HEAD 8dc9230 時点の棚卸し）
- 根拠: [決定事項](decisions.md)、棚卸し 9 件（[inventory/](inventory/)）、既存の票（bugs/）

## 1. 目的

「動くようにする」ではなく、今ゼロから設計するならこうなる、という姿を決め、そこへ段階的に移す。

- 対象は x64 の Windows 11 だけ。32bit の名残を持たない。
- AI コーディングツールとの連携を、目的の一つにする。そのために、外から機能を足す仕組み（拡張）と、外部からの操作の窓口が要る（4.13）。
- エディタを起動して編集するのに要らない機能は持たない（オーナーの方針）。
- Windows の作法に沿う: マニフェスト、UTF-16 の API（W 版）、ユーザーのフォルダへの書き込み、DPI 対応、長いパス。
- どの段階でも「ビルドできる・起動できる・試験が通る（期待どおりの失敗を除く）」状態を保つ。

## 2. 決定事項（すでに決めたこと）

詳細は [decisions.md](decisions.md)。

1. 対象は x64 のみ。Windows 11 を基準にする。Windows 10 は保証しない。
2. 文字は内部で UTF-16（W 版 API）に統一する。ファイルは今までどおり cp932・UTF-8 を読み書きする。
3. マニフェストで Per-Monitor DPI v2 と長いパス対応を宣言する。
4. IME は当面 IMM32 のまま。IME との連携は一つの層にまとめる。
5. VC++ ランタイムは静的リンク（/MT）。ビルドは Visual Studio 2022。
6. 32bit の拡張 DLL は対象外。
7. Den8 関係（den8view、den8util、jdwrap）を削除する。
8. DDE を廃止する（起動時に作る窓口を含む）。WinHelp の表示も廃止する。
9. c-callable を廃止する。wip のサンプルは削除、または samples/ へ移す。
10. 設定ファイルは `~/.xyzzy/` に置く。`-config` と `XYZZYCONFIGPATH` による指定は残す。

## 3. 今の状態（棚卸しの要点）

| 領域 | 今の状態 | 主な根拠 |
|---|---|---|
| 文字 | 内部の文字は独自の多言語符号（16bit、CP932 などを内部の値で持つ）。Win32 は全部 A 版（ANSI）で呼ぶ。ソースは cp932。 | inventory-numbers, inventory-build |
| ビルドの前提 | 日本語の Windows（ACP 932）でしか通らない（/source-charset がない）。 | inventory-build |
| 数値 | fixnum は 30bit。size_t・time_t・ptrdiff_t は 4 バイト（BUG-0028）。dump_version は int（BUG-0025）。 | inventory-numbers, inventory-ffi |
| FFI | c-callable が約 20 箇所に散らばる（32bit の thunk を含む）。64bit かどうかをホストの CPU で判定している。 | inventory-ffi |
| 構造体 | 整列を先頭メンバーだけで決める（BUG-0031）。 | inventory-ffi |
| Win32 の境界 | GetVersionEx に頼る版判定（Win9x/NT4 の死んだ分岐が 20 箇所以上）。マニフェストが無い。_WIN32_WINNT が XP。固定の 260/1024 バッファが約 120 箇所。4GB 超のファイルを誤ってマップする。 | inventory-system, inventory-ui |
| 画面 | 固定ピクセル（DPI 非対応）。古い形式のダイアログ。コンボの項目データ（BUG-0019）。comctl32 の内部メモリを直接読む（dockbar、高リスク）。 | inventory-ui |
| IME | IMM32 の呼び出しが層の外にも 4 箇所。Active IMM（COM）。ANSI の合成経路。 | inventory-kbd |
| 設定と書き込み | 設定は `<install>\usr\<user>\wxp`（インストール先の中）。自動保存・クラッシュログ・ダンプイメージがインストール先に書かれる。ホームの場所を ini から読むが、ini の場所は、そのホームが決まる前に決まっている（順番の矛盾）。空の環境変数が「設定あり」になる（BUG-0029）。 | inventory-system |
| アーカイバー | 32bit の DLL 12 個をプロセス内で読む。Common Archiver API の偽装。7-Zip 呼び出しに誤り（-hide など）。Windows 標準の tar.exe で ZIP・7z・LZH の読みができるのに使っていない。 | inventory-archiver |
| 同梱物 | etc\ の 32bit の exe・DLL が追跡されている。作り方の無い道具（gendic*）。参照されない 32bit のプラグイン（XPI、dxflush、libxpi）。zlib 1.2.8 のライブラリが追跡されている。etc.targets は Win32 の条件のまま。 | inventory-archiver, inventory-build |
| 古い機能 | DDE サーバー（起動時）、WinHelp、den8view、wip（c-callable のサンプル）、ScriptControl と IE のテスト。 | inventory-kbd, inventory-lisp, inventory-tests |
| Lisp | 初期化ファイル `~/.xyzzy` の名前が、新しい設定フォルダ `~/.xyzzy/` と衝突する。WinHelp の変数が起動の経路（loadup、optprop、paths）に入っている。featurep による古い版判定。 | inventory-lisp |

| テスト | 子プロセスの 36 件が、実際の設定を使う。結果は画面のバッファにしか出ない。終了コードがない。32bit の値を期待している。 | inventory-tests |
| GC・メモリ | スタックの走査が setjmp の配置に依存する。initial_stack がスタックの底ではない。読み取り範囲外のアクセス（utils.cc）。 | inventory-numbers, inventory-ffi |

## 4. 目標の設計

### 4.1 ビルドと基盤

- VS2022、x64 のみ、Release と Debug。/MT（静的ランタイム）。
- 共通の設定は Directory.Build.props に集める（今は 9 つの project が同じ設定を繰り返している）。
- マニフェストを埋め込む（src\xyzzy.manifest）: Per-Monitor DPI v2、longPathAware、supportedOS（Windows 10/11）、comctl32 v6。
- _WIN32_WINNT は 0x0A00（Windows 10 以降）。
- 安全の設定: /guard:cf、/DYNAMICBASE、/HIGHENTROPYVA、/LARGEADDRESSAWARE、/GS。DEBUG_GC は Debug だけ。
- zlib はソースから作る（1.3.x。今は 1.2.8 の追跡ライブラリ）。
- install.bat を作る（プロジェクトの規則）。VS2022 の確認、zlib の用意、ビルドを一度に行う。
- ソースは UTF-8 にし、/utf-8 でビルドする（判断 9）。

### 4.2 文字の扱い

- 内部の文字は Unicode のコードポイントにする（判断 1）。Win32 とは UTF-16 でやり取りする（決定 2）。
  - 代わりに UTF-16 の単位（16bit）で持つ案もあるが、サロゲートペアの扱いが文字列の全箇所に入る。
- 文字の変換（cp932、UTF-8、UTF-16 の BOM つき）は、一つのエンコーダ層に集める。ファイル、ini、履歴、Lisp のファイル読み込みは、すべてこの層を通す。
- 文字の型とキーの符号を分ける（chtype.h の CC_META_BIT や CCF_* は、文字と別の型へ）。
- 既存の cp932 のファイルは、開いて保存したときにバイト列が変わらないこと（試験で確かめる）。
- 注意: 文字の値（`char-code` の値）は変わる。Lisp で特定の値に依存しているコードは直す必要がある。

### 4.3 数値と ABI（FFI）

- 型の表を一か所に作る。C++ 側のサイズを静的に検査し、Lisp 側の表と突き合わせる試験を置く（BUG-0028 の再発防止）。
- ポインタは 64bit。size_t・ptrdiff_t・time_t は 8 バイト。long は 4 バイト（Windows の規則）。
- 構造体の整列は、型ごとの自然な整列で計算する（BUG-0031）。
- 64bit かどうかはビルド時に決める（_M_X64）。ホストの CPU を調べる方法は使わない。
- 呼び出しの経路は、今の x64 の方式を残す（可変長の double 方式。動作と試験が済んでいる）。引数 16 個の上限は当面残す。
- c-callable は削除する（決定 9）。
- fixnum は 62bit にする（判断 2）。

### 4.4 Win32 の境界（ファイル・プロセス・版）

- ファイル: W 版 API の薄い層に作り直す。\\?\ で長いパスを扱う。8.3 の処理は削除する。GetFileSizeEx で 64bit のサイズを扱う。
- プロセス: CreateProcessW（Unicode の環境ブロック）。カレントディレクトリを変えない。タイムアウト付きで出力を読む。
- ネットワーク: Winsock 2（ws2_32）。
- 版判定はすべて削除する。Windows 11 を前提にする。機能が本当に無いときだけ、その機能の有無を実行時に確かめる。
- クラッシュ時の情報: x86 向けの表示を削除し、x64 の情報だけにする。クラッシュログは ~/.xyzzy に書く。
- スタックの走査（GC）: RtlCaptureContext で現在のレジスタを保存し、TEB からスタックの底を得る。IsBadWritePtr は使わない。

### 4.5 画面

- ウィンドウクラスは W 版（wWinMain、RegisterClassExW）。
- DPI: 固定ピクセルをすべて DPI に合わせる（GetDpiForWindow、WM_DPICHANGED）。ダイアログは DIALOGEX と DS_SHELLFONT を使う。
- 標準のダイアログ: ファイルを開く・保存は IFileDialog（W 版、長いパス対応）。フォルダ選択は FOS_PICKFOLDERS。独自のメッセージボックスは TaskDialog に替える（判断はしない。作業の中で決める）。
- 入力: WM_CHAR は UTF-16 を受け、文字に変換する（サロゲートペアに対応）。
- サブクラス化は SetWindowSubclass を使う（privctrl）。
- ドラッグ＆ドロップは DragQueryFileW と CF_UNICODETEXT。クリップボードは AddClipboardFormatListener。
- フォアグラウンドの切り替えは AllowSetForegroundWindow で行う（xyzzycli から）。
- dockbar の tab_bar::modify_spin（comctl32 の内部メモリを読んでいる）は、自前のアップダウンコントロールに替える（高リスク。単独の作業として扱う）。
- コンボボックスの owner-draw（BUG-0019）: 項目データに Lisp オブジェクトのポインタを入れない。番号で Lisp 側の表を引く。
- マウス: WM_MOUSEHWHEEL を使い、スクロール量は SPI_GETWHEELSCROLLLINES で取る。WH_MOUSE の座標比較は POINT で行う。
- 印刷は GDI のまま（PrintDlgEx への移行は後の課題）。

### 4.6 IME 層

- すべての IMM 呼び出しを gime の層を通す（dialogs.cc と filer.cc の直接の呼び出しを移す）。
- Active IMM（COM）と ANSI の合成経路は削除する。
- IMM32 は維持する（決定 4）。TSF への移行は別の課題として記録する。
- 限界: この PC の日本語入力は TSF の Microsoft IME で、IMM32 の経路は実機で確かめられない（BUG-0023 の注記）。

### 4.7 設定と起動（~/.xyzzy/）

- 場所の決め方（この順番で決める）:
  1. `-config <フォルダ>` の指定
  2. 環境変数 `XYZZYCONFIGPATH`（空の値は未設定として扱う）
  3. ユーザーのホームフォルダ（SHGetKnownFolderPath の Profile）の下の `.xyzzy`
- 起動の順番: 場所を決める → 設定ファイル（xyzzy.ini）を開く → Lisp の初期化（`~/.xyzzy/init.l`）。ini の中の homeDir は、場所が決まった後に読む。
- 初期化ファイルの名前: 今は `~/.xyzzy` というファイルを初期化ファイルとして読む。新しい設定フォルダも同じ名前になるため、移行のとき、このファイルを `~/.xyzzy/init.l` へ移してからフォルダを作る。
- 旧い場所（`<install>\usr\<user>\wxp`）の設定は、新しい場所に設定がないときだけ、一度だけコピーする。
- 配置:
  - `xyzzy.ini`、`init.l`、`history`、`resume.N`、`abbrev_defs`、`edict`、クラッシュログ、自動保存のファイル、xyzzycli.ini。
  - 履歴ファイルの 8.3 の名前は使わない。
- インストール先には書かない（読み取り専用でも動く）。ダンプイメージはビルドのときだけ作る（判断 3）。
- 旧い形式（レジストリの設定、LOGFONT のバイナリ）は、移行のときに読んで捨てる。
- テストは必ず一時の設定フォルダを使う（4.11）。

### 4.8 アーカイバー（判断 5 に従う）

- 既定の道具は Windows 標準の `tar.exe`（`C:\Windows\System32\tar.exe`。Windows 10 1803 以降と Windows 11 に入っている）。内部は libarchive（bsdtar）。
- 2026-11-10 に確かめた範囲:
  - ZIP の作成と展開（日本語の名前も正しく扱われた）。
  - 7z の作成（7-Zip の 7za で読めることを確認）。
  - LZH の一覧（上流のサンプルで確認）。
  - tar・gz・bz2・xz・zstd は、ヘルプに対応が出ている（実際の試験はしていない）。
- 7-Zip は使わない（RAR などが必要になったら後で任意に追加する）。同梱しない。
- LZH の作成はしない（Windows の道具では作れない）。RAR は標準では扱えないと見ている（サンプルがなく未確認）。
- 操作: 一覧は `tar -tvf`、展開は `tar -xf <書庫> -C <フォルダ>`、作成は `tar -a -c -f <書庫> -C <フォルダ> .`。出力の形式と文字コードは、実装のときに確かめる。
- エクスプローラーの「送る → 圧縮（zip）フォルダ」は Windows の機能（zipfldr.dll）なので、xyzzy が ZIP を作る仕組みは必須ではない。
- 自己更新（verup）は削除する（判断 4）。
- DLL の層（arc-if、comm-arc.h、12 の DLL 名、About の表示）は削除する。

### 4.9 同梱のツール（etc\）

- 残す: tail-f（x64 で作り直し、追跡から外す）。または Lisp で実装する。
- 削除する: extract（verup の依存）、gendic・gendicnm（元のソースがない）、jdwrap・den8util（den8 と一緒に）、dxflush.dxa・libxpi.lib・xpi（どこからも参照されない）。
- ビルドの成果物は追跡しない。配布のときだけ集める。

### 4.10 Lisp ライブラリ

- 削除: den8view、WinHelp（winhelp.l と dexplorer.l、起動の経路の変数）、c-callable の関数と型、DDE の条件と関数、wip の winapi・fractal・hellowin・turtle（c-callable を使う）、ispell、niftylog、NEC・富士通のキー名（判断 7）。
- 移動: wip の oletest と ftp、遊びとデモ（hanoi、life、gomoku、c-curve、tarai、pi、Gates）は samples/ へ（判断 7）。
- 版判定: featurep の古い版（:windows-nt、:windows-vista、:windows-7、:windows-8）を削除し、:windows-10 と :windows-11 を追加する（ビルド番号で判定）。
- 公開の変更（文書に書く）: 文字の値（4.2）、ダンプイメージの形式、削除した関数の一覧。

### 4.11 テストと検証

- 子プロセスには必ず `-q` と `-config <一時フォルダ>` を付ける。親のテストも一時の設定で走らせる（S0）。
- 結果は画面・標準出力・ログに出し、終了コードを返す（0 か 1）。
- 期待値は、実装の計算ではなく MSVC の規則から決める（BUG-0028、BUG-0031 の教訓）。
- ネットワーク、IE、ScriptControl に依存する試験は、削除するか、依存のない形に替える。
- GUI の確認: AI が起動し、画面を撮って確かめる（起動、開く、保存、ダイアログ）。
- 検証できないもの（記録して残す）: 英語版の Windows、ARM64、IMM32 の IME、DPI の実機の表示。

### 4.12 配布

- zip は tar.exe（Windows 11 に標準）で作る。7-Zip は配布に使わない。
- etc\ には作り直した成果物だけを入れる。
- 署名はしない（判断 10、決定）。配布の案内に、警告が出る旨と起動の手順を書く。
- README の動作環境を Windows 11・VS2022 に書き換える。

### 4.13 拡張（プラグイン）と外部連携（判断 8）

目的: AI コーディングツールが、エディタの機能を使えるようにする。そのための二つの仕組みを設計する。

- **拡張（プラグイン）**: x64 の DLL を、起動時に読み込む。
  - 読み込み元: インストール先の `plugins\`（読み取り専用でも読める）。利用者の追加分は `~/.xyzzy/plugins\`。
  - インターフェース: C の関数の表と、バージョンの番号を持つ構造体。今の XPI（ペインの追加）の機能を、x64 向けに作り直す。
  - 注意: 拡張は、エディタと同じプロセスで動きます。利用者が入れたものだけを読むのが原則です。
- **外部からの操作の窓口**: 名前付きパイプ（Windows の通信路）と、JSON の簡単な形式で行う。
  - 既定は無効。有効にしたときだけ使える。
  - 通信は同じ利用者だけが使えるように、パイプの権限を絞る。
  - 操作できる範囲（読む・書く・コマンドを実行する）は、段階を分けて広げる。Lisp の式を外から自由に実行する仕組みは、最初は入れない。
- **段階**: 拡張の仕組み（S7b）を先に作る。外部からの操作の窓口（S7c）は、その後に作る。
- **未決定（オーナーに確認する）**: AI コーディングツールに、どこまで操作させるか。拡張を置く場所。
- **未確認**: `etc\dxflush.dxa` のソースと用途。ソースが無ければ、作り直せるかどうかを判断できません。

## 5. 削除する一覧（まとめ）

| 対象 | 主な場所 | 段階 |
|---|---|---|
| Den8 一式 | lisp\den8view.l、src\etc\den8util.cc、projects\den8util.*、etc\den8util.dll、jdwrap 一式 | S1 |
| DDE | src\dde.cc、src\xdde.*、src\init.cc の起動時の呼び出し、signal・msgdef・gen-syms の定義、lisp の DDE 関数と条件、process.cc の DDE のエラー対応 | S1 |
| WinHelp | src\winhelp.cc（WinHelp の部分）、lisp\winhelp.l、lisp\dexplorer.l、loadup・optprop・paths の参照、xyzzy.rc の IDD_HELPDLG | S1 |
| HtmlHelp（CHM） | src\winhelp.cc（HtmlHelp の部分）、builtin の show-html-help（判断 6） | S1 |
| c-callable | dll.cc の c-callable と x86 の部分、dll.h、eval・pred・lprint・data（Tc_callable のタグ）、gen-syms、msgdef、foreign.l、typespec・help・etc\Lisp、テスト | S1 |
| wip の winapi・fractal・hellowin・turtle | lisp\wip | S1 |
| 32bit の XPI の部分 | etc\libxpi.lib、etc\xpi.h（32bit の道具）。仕組みは x64 で作り直す（判断 8、4.13） | S7b |
| 孤立したツール | etc\gendic.exe、etc\gendicnm.exe | S1 |
| extract | etc\extract.exe、src\etc\extract.cc、projects\extract.vcxproj | S9 |
| Win9x・NT4 の分岐 | 約 20 箇所の Win4p・Win5p・Win98p、ctl3d、sysdep の版判定 | S1・S2 |
| archiver の DLL 層 | arc-if.*、comm-arc.h、12 の DLL 名、About の表示 | S9 |
| 自己更新（verup） | lisp\verup.l | S9 |
| 古い試験 | defun-c-callable、c-callable の typespec、wow64、x86 の機械語名、OS 名の試験 | S1・S10 |
| ole の古い試験 | ScriptControl、IE、ネットワーク | S10 |

## 6. 移行の段階

各段階の共通の完了の基準: Debug と Release がビルドできる。全件テストを走らせ、期待どおりの結果になる。起動して開く・編集・保存ができる（AI が確かめる）。コミットして push する。票と決定事項の文書を更新する。

| 段階 | 内容 | 規模 | 前提 | この段階の完了の基準 |
|---|---|---|---|---|
| S0 安全網 | 子プロセスと親のテストを一時の設定で走らせる。結果を標準出力・ログに出し、終了コードを返す。（BUG-0010、0012、0029 の試験の部分） | S | なし | 全件を走らせても利用者の設定が変わらない（更新時刻で確認）。結果は現状と同じ（573 成功・5 失敗）。 |
| S1 不要な機能の削除 | Den8、DDE、WinHelp、HtmlHelp（判断 6）、c-callable、wip、XPI、孤立したツール、死んだ版判定（BUG-0013 の残り）。 | M | 判断 6・7・8 | 起動して開ける。全件テストの失敗は OLE の 1 件だけ。ダンプイメージを作り直して起動できる。（BUG-0001、0003、0006、0015、0027、0030 が閉じ、BUG-0017 は一部が閉じる） |
| S2 プラットフォームの土台 | マニフェストの埋め込み、_WIN32_WINNT 0x0A00、版判定の削除、64bit の判定を _M_X64 へ、保護の設定、install.bat。 | S | S1 | exe にマニフェストが入っている（確認）。起動する。DPI の宣言が効く（画面で確認）。 |
| S3 数値と型・ABI | 型表の一元化と静的検査、64bit の型（BUG-0028）、構造体の整列（BUG-0031）、dump_version（BUG-0025）、fixnum（判断 2）。 | M | S2 | foreign の試験が新しい期待値（8、24）で通る。型表の検査が通る。 |
| S4 文字の扱い | 4a エンコーダ層。4b 文字の型の作り直しと、キーの符号の分離。4c W 版のファイル処理（長いパス、8.3 の削除、4GB 対応）。4d ソースを UTF-8 に変換。OLE の文字列の変換（oledata.cc）も含む。 | XL | S3 | cp932 のファイルを開いて保存し、バイト列が同じ。非 cp932 の文字のファイル名を開ける。文字の試験が通る。 |
| S5 プロセスと通信 | CreateProcessW、カレントディレクトリの扱い、Winsock 2、xyzzycli の通信（BUG-0014 の検証）。 | M | S4 | 日本語の経路と引数で外部プロセスが動く。ソケットの試験。 |
| S6 設定と起動（~/.xyzzy/） | 起動の順番、空の値の扱い（BUG-0029 の本体）、初期化ファイルの移行、旧い場所からの移行、書き込み先の変更、ダンプイメージ。 | M | S2、S4 | 読み取り専用のフォルダに置いても起動して編集できる。旧い設定が移行される。 |
| S7 画面 | W 版のウィンドウ、DPI、標準のダイアログ、ドラッグ＆ドロップ、クリップボード、サブクラス化、dockbar の自前化、コンボの項目データ（BUG-0019）、ホイール（BUG-0022 の残り）。 | L | S4、S6 | 150% の表示で崩れない（画面で確認）。開く・保存・フォント・色のダイアログが動く。 |
| S8 IME 層 | 直接の Imm 呼び出しを層に集める。ANSI の経路と Active IMM を削除。HKL の往復を確かめる（BUG-0023）。 | S | S7 と並行可 | IME の呼び出しが全部、層を通る（コードで確認）。実機の確認の限界を記録する。 |
| S7b 拡張の仕組み（x64） | x64 の DLL の読み込みと、バージョンのある C の関数の表。XPI の機能（ペインの追加）を作り直す。BUG-0015 を直す。 | M〜L | S3、S7 | サンプルの拡張が起動時に読み込まれ、ペインを追加できる（画面で確認）。 |
| S7c 外部からの操作の窓口 | 名前付きパイプと JSON の形式。既定は無効。権限の絞り込み。 | M | S7b | 同じ利用者からだけ接続でき、既定では無効（確認）。 |
| S9 アーカイバー | tar.exe の子プロセス（一覧・展開・作成）、DLL の層の削除、LZH は読みだけ。SFX は廃止。verup は削除（判断 4）。 | M | S5、判断 4・5 | ZIP と 7z の作成・展開、LZH の一覧が動く（日本語の名前を含む）。試験を新しく作る。 |
| S10 試験の整理 | ネットワーク・IE・ScriptControl の試験の置き換えや削除（BUG-0002）、OS 名の試験の削除、insdel の名前（BUG-0011 の残り）。 | M | S0 以降いつでも | 全件テストがネットワークなしで通る。 |
| S11 Lisp と文書 | featurep の版判定の削除、:windows-10・:windows-11 の追加、文書の更新、公開の変更の記録。 | M | S1〜S9 | 削除した語が文書と実装に残っていない。 |
| S12 配布と追跡ファイル | zlib 1.3.x のソースビルド、追跡の整理（BUG-0007、0016、0026、0017）、tail-f の作り直し、dist.bat の更新。 | S | S2 | 追跡ファイルだけの clone で install.bat → ビルド → 配布が通る。 |

順番の理由:
- S0 を最初にする。以降の作業で、利用者の設定を壊さないため。
- S1 を早くする。削除は小さく確かな変更で、以降の作業の対象が減る。
- S4 が最も大きく、S6 と S7 の前提になる。

## 7. 既存の票との対応

| 票 | 題（短く） | 扱い | 段階 |
|---|---|---|---|
| BUG-0001 | FFI の x64（c-callable） | c-callable の削除で閉じる | S1 |
| BUG-0002 | ScriptControl の試験 | 試験を置き換える（判断なし） | S10 |
| BUG-0003 | den8view の 32bit DLL | 削除（決定 7） | S1 |
| BUG-0004 | アーカイバーの 32bit DLL | Windows 標準の tar.exe に置き換える（7-Zip は任意） | S9 |
| BUG-0005 | verup の 32bit extract | verup を削除（判断 4） | S9 |
| BUG-0006 | extract の UNLHA32.LIB | extract を削除 | S1・S9 |
| BUG-0007 | update_zlib.bat | zlib の作り方を替える | S12 |
| BUG-0008 | etc.sln の Win32 構成 | 修正済み。残りは削除で整理 | S1 |
| BUG-0009 | 中間ディレクトリ | 修正済み | — |
| BUG-0010 | run-tests の結果の出力 | 試験の出力を替える | S0 |
| BUG-0011 | foreign の試験の収集 | 修正済み。insdel の名前は S10 | S10 |
| BUG-0012 | 子プロセスの読み戻し | 試験の基盤で直す | S0 |
| BUG-0013 | Win9x の分岐 | 残りの版判定を削除 | S1・S2 |
| BUG-0014 | listen-stream の入力待ち | 検証する（xyzzycli の通信） | S5 |
| BUG-0015 | pane.cc のアドレス切り詰め | 拡張の仕組みを作り直すときに直す | S7b |
| BUG-0016 | zlib の追跡ライブラリ | ソースからビルドし、追跡から外す | S12 |
| BUG-0017 | etc\ の追跡バイナリ | 削除と作り直し | S1・S12 |
| BUG-0018 | HtmlHelp の引数 | 修正済み。WinHelp は削除、HtmlHelp は判断 6 | S1 |
| BUG-0019 | コンボの owner-draw | 項目データの設計を替える | S7 |
| BUG-0020 | privctrl のサブクラス化 | 修正済み。仕組みを SetWindowSubclass に替える | S7 |
| BUG-0021 | クラッシュ書式 | 修正済み。残りの x86 表示は削除 | S2・S6 |
| BUG-0022 | wheel のタイマー ID | 修正済み。残りは作り直し | S7 |
| BUG-0023 | HKL の往復 | 型を替え、往復の試験を置く（IME は不要） | S8 |
| BUG-0024 | 書式と可変引数 | 修正済み | — |
| BUG-0025 | dump_version の int | int64 に替える | S3 |
| BUG-0026 | etc.targets の Win32 条件 | etc の仕組みごと替える | S12 |
| BUG-0027 | GUID の重複 | extract の削除で消える | S1 |
| BUG-0028 | size_t などの 4 バイト | 型表で 8 バイトにする | S3 |
| BUG-0029 | 空の環境変数と設定の場所 | 試験の隔離（S0）と起動の作り直し（S6） | S0・S6 |
| BUG-0030 | wip の winapi の型 | wip を削除（決定 9） | S1 |
| BUG-0031 | 構造体の整列 | 型ごとの整列で計算する | S3 |

## 8. 判断してほしいこと

推奨のまま進めるなら「推奨で」と言ってください。

| # | 項目 | 推奨 | 別案と影響 |
|---|---|---|---|
| 1 | Lisp の文字の値 | 決定: Unicode のコードポイント（オーナーが 2026-11-10 に承認） | UTF-16 の単位（16bit）にする案もある。サロゲートの扱いが文字列の全箇所に入る。どちらも `char-code` の値が変わる。 |
| 2 | fixnum の幅 | 決定: 62bit（オーナーが 2026-11-10 に承認） | 30bit のまま。整数の上限が変わる。 |
| 3 | ダンプイメージ（起動の高速化） | 決定: ビルドのときだけ作る（オーナーが 2026-11-10 に承認）。無ければ普通に起動する | 利用者の領域にキャッシュを作る。起動の速さは変わる。 |
| 4 | 自己更新（verup） | 決定: 削除（オーナーが 2026-11-10 に承認。新版はオーナーが作るときだけ存在する） | 手順書による更新にする。 |
| 5 | アーカイバー | 決定: Windows 標準の tar.exe だけを使う（7-Zip は使わない）。LZH は作成しない。SFX は廃止（オーナーが 2026-11-10 に推奨で承認） | — |
| 6 | HtmlHelp（CHM 形式のヘルプ） | 決定: 削除（オーナーが 2026-11-10 に承認） | — |
| 7 | 古い Lisp モジュール（niftylog、ゲーム類、ispell、NEC・富士通の鍵配列） | 決定: 遊びとデモは samples/ へ移し、ispell・niftylog・NEC/富士通のキー名は削除（オーナーが 2026-11-10 に承認） | — |
| 8 | XPI（32bit のプラグイン） | 決定: x64 向けに作り直して残す（AI コーディングツールとの連携に必要。オーナーが 2026-11-10 に承認） | — |
| 9 | ソースの文字コード | 決定: UTF-8 に変換し、/utf-8 でビルドする（オーナーが 2026-11-10 に推奨で承認） | — |
| 10 | コード署名 | 決定: 署名しない（オーナーが 2026-11-10 に承認）。配布の案内に警告の注意書きを書く | 有料の証明書で署名する（警告が出なくなる）。 |

## 9. リスクと確認の方法

- S4（文字の扱い）が最大のリスク。段階を分け、各段階でバイト単位の試験を置く。
- Lisp の利用者への影響: 文字の値、削除した機能、ダンプイメージの形式。文書に書き、移行の案内を作る。
- ダンプイメージと .lc ファイルは、段階のたびに作り直す（古いものが残ると、古いコードが読まれる。今回の不具合の原因の一つ）。
- 検証の環境は、この PC（日本語・Windows 11）だけ。英語版や ARM64 では確かめられない。
- IMM32 の IME は、この PC では実機で確かめられない（BUG-0023）。
- DPI の確認は、画面の拡大率を変えて行う。AI がその設定を変えられるかは、S2 で確かめる。
- tar.exe の出力（一覧の形式、日本語の名前の文字コード）は、実装のときに確かめる。RAR は標準で扱えるかを確かめていない。
- 拡張（プラグイン）は、エディタと同じプロセスで動く。不具合やクラッシュがエディタ全体に及ぶ。利用者が入れたものだけを読む。
- 外部からの操作の窓口は、AI ツールに強い権限を与えることになる。既定で無効にし、権限を段階で広げる。
- `etc\dxflush.dxa` のソースが見つからない。作り直せない場合は、削除か、用途の確認が要る。

## 10. 検討した案

- A 段階的に置き換える（推奨）: 各段階で動く状態を保ち、試験で確かめながら進める。
- B 作り直す（新しいプロジェクトに Lisp の部分だけを移す）: 速く見えるが、今の動きの確認が失われ、危険が大きい。採らない。
- C 最小の修正だけ行う（今の設計のまま）: 32bit の名残と文字の問題が残る。目的に合わない。採らない。

## 11. Windows の用語（簡単な説明）

- マニフェスト: exe に埋め込む宣言書。DPI の扱い、長いパス、対応する Windows の版を Windows に伝える。
- DPI: 画面の拡大率。Per-Monitor DPI v2 は、モニターごとの拡大率に合わせて描く方式。
- W 版と A 版の API: W 版は UTF-16 で文字列を受ける。A 版は地域設定の文字コード（日本語なら cp932）で受ける。今は A 版で呼んでいる。
- UTF-16: Windows の内部の文字の形式。
- LLP64: Windows の 64bit の規則。ポインタは 8 バイト、long は 4 バイト。
- MSVC と VS2022: Microsoft の C++ コンパイラと、その開発環境。
- IMM32 と TSF: 日本語入力（IME）と通信する仕組み。IMM32 は古い方式で、TSF は新しい方式。
- DDE: 古いプロセス間の通信。今回は廃止する。
- 名前付きパイプ: 新しいプロセス間の通信の仕組みの一つ。将来、外部から操作する機能を作るときに使う。
- ダンプイメージ: 起動を速くするため、メモリの状態を保存したファイル（xyzzy.wxp）。

## 12. 棚卸しの場所

- [inventory-ffi.md](inventory/inventory-ffi.md)（FFI と ABI）
- [inventory-ui.md](inventory/inventory-ui.md)（画面とダイアログ）
- [inventory-kbd.md](inventory/inventory-kbd.md)（キーボード、IME、DDE、WinHelp、OLE）
- [inventory-system.md](inventory/inventory-system.md)（プロセス、ファイル、設定、起動）
- [inventory-numbers.md](inventory/inventory-numbers.md)（数値、文字、メモリ）
- [inventory-build.md](inventory/inventory-build.md)（ビルド、プロジェクト、スクリプト）
- [inventory-lisp.md](inventory/inventory-lisp.md)（Lisp、wip、サンプル）
- [inventory-tests.md](inventory/inventory-tests.md)（テストと試験の基盤）
- [inventory-archiver.md](inventory/inventory-archiver.md)（アーカイバーと etc\ の道具）
