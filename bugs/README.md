# バグ票（x64 版の残件）

対象: feature/x64-only。見たことだけを書く（症状・実測・再現手順・場所）。修正は修正役が行い、票の状態を更新する。

監査の分類表（565 行、ポインタ幅の縮小警告の判定）: [audit/x64-pointer-width-2026-11-09.tsv](audit/x64-pointer-width-2026-11-09.tsv)

| 票 | 題 | 状態 |
|---|---|---|
| [BUG-0001](BUG-0001-ffi-x64-unsupported.md) | FFI（DLL 関数呼び出し・c-callable）が x64 版で未対応 | 修正済み（2026-11-10、S1。c-callable を削除） |
| [BUG-0002](BUG-0002-ole-scriptcontrol-not-registered.md) | ole-tests: ScriptControl が x64 プロセスから作成できず失敗 | 修正済み（2026-11-22、OLE Automation を削除） |
| [BUG-0003](BUG-0003-den8view-32bit-dll-and-decorated-names.md) | den8view が 32bit の etc/den8util.dll と x86 の装飾名に依存する | 修正済み（2026-11-10、S1） |
| [BUG-0004](BUG-0004-archiver-dlls-32bit.md) | アーカイバー DLL（UNLHA32 など）は 32bit で、x64 プロセスから読み込めない | 検証済み（7-Zip の CLI の条件を確認。LZH の作成・読みと、同梱の扱いは判断待ち。2026-11-10 の追記を参照） |
| [BUG-0005](BUG-0005-verup-uses-32bit-extract-exe.md) | 自己更新（verup）の展開が 32bit の etc/extract.exe と UNLHA32.DLL に依存する | 未検証 |
| [BUG-0006](BUG-0006-extract-vcxproj-unlha32-lib.md) | projects/extract.vcxproj が UNLHA32.LIB を要求し、リンクできない | 保留（オーナーの判断待ち） |
| [BUG-0007](BUG-0007-update-zlib-bat-stale.md) | misc/update_zlib.bat が現在の環境で使えない | 未修正 |
| [BUG-0008](BUG-0008-etc-sln-win32-configurations.md) | projects/etc.sln に Win32 の構成が残り、ビルドできない | 修正済み |
| [BUG-0009](BUG-0009-intdir-shared-across-platforms.md) | 中間ディレクトリが構成とプラットフォームで共有され、x86 と x64 の obj が混在する | 修正済み |
| [BUG-0010](BUG-0010-run-tests-bat-no-console-output.md) | run-tests.bat の結果がコンソールに出ず、エディタのバッファにしか出ない | 修正済み（2026-11-10、S0） |
| [BUG-0011](BUG-0011-foreign-test-not-collected.md) | unittest/foreign-test.l が全テスト実行の対象になっていない | 修正済み（foreign テストを全テストに含めた。c-callable の試験は S1 で削除） |
| [BUG-0012](BUG-0012-eval-in-another-xyzzy-reader-error.md) | テスト基盤: 別プロセスの評価がエラーを返すと、結果の読み戻しで reader-error になる | 修正済み（2026-11-10、S0） |
| [BUG-0013](BUG-0013-win9x-only-branches-remain.md) | x64 版にも Win9x 専用の分岐が残っている | 修正済み |
| [BUG-0014](BUG-0014-listen-stream-pipe-check-changed.md) | listen-stream の入力待ち判定を変えたが、パイプ入力の挙動は未検証 | 要検証 |
| [BUG-0015](BUG-0015-pane-address-truncated.md) | pane.cc の戻り値が 64bit のアドレスを 32bit に切り詰める | 要検証 |
| [BUG-0016](BUG-0016-zlib-libs-tracked-in-git.md) | src/zlib の zlib.lib / zlib.pdb が Git に追跡されている | 保留（オーナーの判断待ち） |
| [BUG-0017](BUG-0017-etc-binaries-tracked-overwritten.md) | etc/ の DLL・EXE が Git に追跡され、ビルドで上書きされる | 保留（オーナーの判断待ち） |
| [BUG-0018](BUG-0018-winhelp-htmlhelp-dword-arg.md) | winhelp.cc: HtmlHelp の第4引数が DWORD のため、64bit のスタックアドレスが切り詰められる | 修正済み |
| [BUG-0019](BUG-0019-ldialog-combobox-index-ownerdraw.md) | ldialog.cc: コンボボックスの :index 経路で、owner-draw の項目データを 32bit に切り詰める | 要判断（設計） |
| [BUG-0020](BUG-0020-privctrl-subclass-parent-int.md) | privctrl: subclass_parent が int を返し、旧 WNDPROC（64bit）を切り詰める | 修正済み |
| [BUG-0021](BUG-0021-except-crash-report-pointer-format.md) | クラッシュレポートの書式 %08x に 64bit のポインタを渡している（表示のみ） | 修正済み |
| [BUG-0022](BUG-0022-wheel-timer-id-uint.md) | wheel.cc: SetTimer の戻り値（UINT_PTR）を UINT に格納している | 修正済み |
| [BUG-0023](BUG-0023-kbd-hkl-roundtrip.md) | kbd.cc: HKL を fixnum に変換して戻すとき、上位 32bit の扱いが未確認 | 要確認（この PC には問題になる形の HKL が登録されていない） |
| [BUG-0024](BUG-0024-format-mismatch-varargs.md) | 書式指定子と可変引数の型が一致しない箇所（C4477 / C4313 / C4309 / C4319） | 修正済み |
| [BUG-0025](BUG-0025-gen-syms-dump-version-int.md) | gen-syms: 画像の版番号 dump_version が int（time(0) 由来）で、2038 年に折り返す | 修正済み（2026-11-22、S3） |
| [BUG-0026](BUG-0026-etc-targets-win32-condition.md) | etc.targets の後処理コピーが Release\|Win32 の条件のままで、x64 のビルドでは etc/ が更新されず、etc/ の実行ファイルは x86 のまま | 保留（オーナーの判断待ち。BUG-0017 と連動） |
| [BUG-0027](BUG-0027-duplicate-projectguid.md) | ProjectGuid {2C956011-ED51-4699-BAC1-B7687910AF1D} が extract.vcxproj と xyzzyenv.vcxproj で重複している（現状は害なし） | 未修正（低重大度） |
| [BUG-0028](BUG-0028-foreign-size-t-ptrdiff-t-time-t-4byte-on-x64.md) | lisp/foreign.l の size_t・ptrdiff_t・time_t が x64 で 4 バイトに写像されている（MSVC の x64 では 8 バイト） | 修正済み（2026-11-22、S3。64c1eb0 で 8 バイトにし、往復の試験を追加） |
| [BUG-0029](BUG-0029-editor-config-path-tests-intermittent.md) | 空の環境変数 XYZZYINIFILE・XYZZYCONFIGPATH が「設定あり」として扱われ、設定場所の試験が失敗する | 試験の実行環境は修正済み（S0）。本体の扱いは未修正（S6） |
| [BUG-0030](BUG-0030-wip-winapi-x64-type-widths.md) | lisp/wip/winapi.l の WPARAM・LPARAM・LRESULT と MSG が x64 で 4 バイトに写像されている（MSG は Win64 より小さく、GetMessage が超えて書く） | 修正済み（2026-11-10、S1。lisp/wip を削除） |
| [BUG-0031](BUG-0031-define-c-struct-alignment-x64.md) | lisp/foreign.l の define-c-struct の整列が先頭のメンバーだけで決まり、入れ子の構造体の整列も誤る（x64 の配置がずれる） | 修正済み（2026-11-22、S3。配置の計算は 64c1eb0 で C の規則にしてあった。S3 で MSVC x64 の実測と突き合わせる試験を追加し、確定） |
| [BUG-0032](BUG-0032-stale-menu-in-dump-image.md) | ダンプイメージに残ったメニューが次の起動で無効になり、ファイル付きの起動でエラーになる | 修正済み（2026-11-10、S1 の統合で発見） |
| [BUG-0033](BUG-0033-expt-huge-exponent-never-returns.md) | `(expt 2 4294967296)` が返らない（巨大な指数でループし続ける） | 修正済み（2026-11-22、d549c5a） |
| [BUG-0034](BUG-0034-sxhash-negative.md) | sxhash が負の値を返すことがある（CL では非負の fixnum であるべき） | 修正済み（2026-11-22、baad60f） |
| [BUG-0035](BUG-0035-ash-zero-huge-count.md) | `(ash 0 <long を超える数>)` が 0 を返さず bignum-overflow を出す | 修正済み（2026-11-22、8a01f16） |
| [BUG-0036](BUG-0036-radix-hex-case-mismatch.md) | `write-to-string :radix t` は 16 進の英字を大文字で印字し、`~X` や `*print-base*` は小文字（不揃い） | 修正済み（2026-11-22、小文字に揃えた） |
| [BUG-0037](BUG-0037-foreign-c-pointer-type-dead-branch.md) | lisp/foreign.l の `c-pointer-type` に、`:x64` が無いとき `:uint32` に落ちる死んだ分岐が残っている | 修正済み（2026-11-22、c9ccc76） |
| [BUG-0038](BUG-0038-expt-huge-exponent-complex-float.md) | 巨大な整数の指数の expt が、有理複素数の底では返らず、浮動小数の底で負の指数だと誤ったエラーになる | 修正済み（2026-11-22） |
| [BUG-0039](BUG-0039-auto-encoding-corrupts-stray-bytes.md) | 文字コードを自動判定で開くと、はぐれたバイト（単独の A0、E0 の後に ASCII など）が big5 / euc-jp と判定され、保存でバイト列が変わる | 未修正（2026-11-22、S4-0 の網。試験は機能フラグで切ってある） |
| [BUG-0040](BUG-0040-explicit-encoding-normalizes-mixed-line-ends.md) | 符号を指定して開くと、CRLF で始まり LF・CR が混ざるファイルが、保存で全行 CRLF に変わる（自動判定では変わらない） | 未検証（仕様かもしれない。2026-11-22、S4-0 の網。試験は機能フラグで切ってある） |
| [BUG-0042](BUG-0042-regexp-syntax-wide-char-table-index.md) | 正規表現の \w・\s? と skip-syntax-spec が、256 以上の文字で構文表を `cc >> 8` で引き、内部の値が 0x0100〜0x7FFF の文字（ISO-8859 など）の構文を誤る | 修正済み（2026-11-22） |
| [BUG-0050](BUG-0050-latin-word-char-forward-word-vs-regexp.md) | ラテン文字（ISO-8859 など）が、forward-word・word-char-p では語の文字で、正規表現の \w では語の文字でない | 未修正（2026-11-22、BUG-0042 の修正後に残った差） |
