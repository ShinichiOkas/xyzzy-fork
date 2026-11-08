# バグ票（x64 版の残件）

対象: feature/x64-only。見たことだけを書く（症状・実測・再現手順）。調査と修正は担当者が行う。

| 票 | 題 | 状態 |
|---|---|---|
| [BUG-0001](BUG-0001-ffi-x64-unsupported.md) | FFI（DLL 関数呼び出し・c-callable）が x64 版で未対応 | 方針待ち |
| [BUG-0002](BUG-0002-ole-scriptcontrol-not-registered.md) | ole-tests: ScriptControl が x64 プロセスから作成できず失敗 | 環境依存（方針待ち） |
| [BUG-0003](BUG-0003-den8view-32bit-dll-and-decorated-names.md) | den8view が 32bit の etc/den8util.dll と x86 の装飾名に依存する | 未対応（方針待ち） |
| [BUG-0004](BUG-0004-archiver-dlls-32bit.md) | アーカイバー DLL（UNLHA32 など）は 32bit で、x64 プロセスから読み込めない | 方針待ち（代替アーカイバーの選定） |
| [BUG-0005](BUG-0005-verup-uses-32bit-extract-exe.md) | 自己更新（verup）の展開が 32bit の etc/extract.exe と UNLHA32.DLL に依存する | 未検証 |
| [BUG-0006](BUG-0006-extract-vcxproj-unlha32-lib.md) | projects/extract.vcxproj が UNLHA32.LIB を要求し、リンクできない | 未修正 |
| [BUG-0007](BUG-0007-update-zlib-bat-stale.md) | misc/update_zlib.bat が現在の環境で使えない | 未修正 |
| [BUG-0008](BUG-0008-etc-sln-win32-configurations.md) | projects/etc.sln に Win32 の構成が残り、ビルドできない | 未修正 |
| [BUG-0009](BUG-0009-intdir-shared-across-platforms.md) | 中間ディレクトリが構成とプラットフォームで共有され、x86 と x64 の obj が混在する | 未修正 |
| [BUG-0010](BUG-0010-run-tests-bat-no-console-output.md) | run-tests.bat の結果がコンソールに出ず、エディタのバッファにしか出ない | 未修正 |
| [BUG-0011](BUG-0011-foreign-test-not-collected.md) | unittest/foreign-test.l が全テスト実行の対象になっていない | 未修正 |
| [BUG-0012](BUG-0012-eval-in-another-xyzzy-reader-error.md) | テスト基盤: 別プロセスの評価がエラーを返すと、結果の読み戻しで reader-error になる | 未修正 |
| [BUG-0013](BUG-0013-win9x-only-branches-remain.md) | x64 版にも Win9x 専用の分岐が残っている | 方針待ち（削除範囲の判断） |
| [BUG-0014](BUG-0014-listen-stream-pipe-check-changed.md) | listen-stream の入力待ち判定を変えたが、パイプ入力の挙動は未検証 | 要検証 |
| [BUG-0015](BUG-0015-pane-address-truncated.md) | pane.cc の戻り値が 64bit のアドレスを 32bit に切り詰める | 要検証 |
| [BUG-0016](BUG-0016-zlib-libs-tracked-in-git.md) | src/zlib の zlib.lib / zlib.pdb が Git に追跡されている | 方針待ち（リポジトリ方針） |
| [BUG-0017](BUG-0017-etc-binaries-tracked-overwritten.md) | etc/ の DLL・EXE が Git に追跡され、ビルドで上書きされる | 方針待ち |
