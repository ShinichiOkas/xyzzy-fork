# BUG-0039 文字コードを自動判定で開くと、はぐれたバイトが big5 / euc-jp と判定され、保存でバイト列が変わる

- 状態: 修正済み（2026-11-22、S4-0。試験は通常の試験に戻した）
- 対象: x64 版（feature/x64-only、f2342cb）。x64 固有ではないと見られる
- 起票: 2026-11-22

## 症状
- cp932 として不正（後続の無い先行バイト、単独の A0 など）なバイトを含むファイルを、符号を指定せずに開いて（自動判定）保存すると、バイト列が変わる。判定が sjis にならず、big5 または euc-jp になるため。
- 符号に sjis を指定して開くと、同じファイルのバイト列は変わらない（不正バイトを含む 49 件すべて）。

## 実測（2026-11-22、`ed::find-file-internal`（符号は nil＝自動判定）で開き、`save-buffer` で保存して、バイト列を読み戻した。abc = 61 62 63、行末は CRLF）
| 元のバイト列 | 自動判定 | 保存後のバイト列 |
|---|---|---|
| `abc E0 CRLF` | big5 | `abc 3F CRLF` |
| `abc E0 39 abc` | big5 | `abc 3F 39 abc` |
| `abc A0 abc` | big5 | `abc 3F abc` |
| `A0 61` | big5 | `3F 61` |
| `abc A0`（ファイル末尾） | big5 | `abc 3F` |
| `abc A0 40 abc` | big5 | `abc 3F 40 abc` |
| `abc FE`（ファイル末尾） | euc-jp | `abc 8E FE` |

同じ自動判定でバイト列が変わらないもの（sjis と判定される）: 後続の無い先行バイト（81・9F・E0・EA・F0・F9・FC、ファイル末尾と行末の前）、`81 20`・`81 3F`・`81 7F`・`81 FD`・`81 FE`・`81 FF`、`FD`・`FE`・`FF` の単独（`abc FE` のファイル末尾を除く）、`EB 40`・`EF FC`、定義の無い 2 バイト（`81 AD`・`85 40`・`87 76`・`84 BF`・`98 73`・`EA A5`）、`FF FF FF`。big5 と判定されても変わらないもの: 単独の `80`、`80 80 80`、`80 40`。
有効な cp932 の各領域（JIS X 0208、半角カナ、NEC 13 区、NEC 選定 IBM 拡張、IBM 拡張、利用者定義）は、どれも sjis と判定されて変わらない。ただし半角カナだけの短いファイル（`B1 B2`）は euc-jp と判定される（バイト列は変わらない）。

## 再現
1. 上の表のバイト列を、バイナリ（`:encoding :binary`）で一時ファイルに書く。
2. `(ed::find-file-internal ファイル t nil nil t)` で開く（符号は nil）。
3. `(buffer-fileio-encoding)` を見る（big5 / euc-jp）。`(set-buffer-modified-p t)` のあと `(save-buffer)` する。
4. ファイルをバイナリで読み戻して比べる。

試験: unittest/cp932-roundtrip-tests.l の `cp932-roundtrip-invalid-bytes-auto-known-bug`（`(push :cp932-roundtrip-known-bugs *features*)` のあとで読み込むと走る）。

## 期待
- 自動判定でも、保存でバイト列が変わらない（変換できないバイトは保存で元のバイトに戻る）。または、変換できないバイトがあるときは big5・euc-jp にしない。

## 原因と直し方（2026-11-22）
- 原因: src/guess.cc の `Fguess_char_encoding` は、候補が 1 つだけ生き残ると、そのバイトを読ませる前に（ほかの候補を殺したバイトを無視して）その候補を返していた。A0 や E0 の後ろの ASCII は sjis・euc-jp・utf8 を殺すが、big5 は検査されないまま返された。また、データの終わりで文字の途中にいる候補（`abc FE`）も残っていた。
- 直し方: すべての候補にすべてのバイトを読ませる。全滅したときは下の「見直し」のとおり。データ全体が渡されたときは、文字の途中で終わる候補を外す。判定窓だけが渡されたとき（ファイルの先頭 `*detect-char-encoding-buffer-size*` バイトだけ）は、切れ目が文字の途中でも外さない（kanji.cc の `detect_char_encoding_libguess`、size < real_size）。
- 見直し（2026-11-22）: 全候補が全バイトを読んで全滅したら nil（既定の sjis）にする形は、ほとんど正しい utf-8 に不正なバイトが 1 つあるファイルを sjis と判定する退行になった（以前は sjis が先に死んだ時点で utf-8 を返していた）。実測（数 KB の日本語 utf-8 に FF を後ろ・途中・先頭近く・末尾に 1 つ、A0 を後ろに 1 つ）で、修正前は utf8n、最初の直しは nil（sjis）、見直し後は utf8n。sjis の日本語に不正なバイトを 1 つ入れたもの（A0・80・`81 20`・FF）は、修正前・見直し後とも sjis。
- 見直しの直し方: 全滅したときは、最も後ろまで読んで死んだ候補を返す。同じ位置で死んだ候補が 2 つ以上なら（先頭や ASCII の部分に不正なバイトがあるなど、材料が無いとき）nil（呼ぶ側の既定の符号 sjis で、バイトは保たれる）。`abc E0 CRLF`・単独の A0・末尾の FE などの試験の場合は、どの候補も同じ位置で死ぬか sjis が残るので、sjis のまま保たれる。
- 見直しで残る限界: utf-8 と判定されたファイルの不正なバイトは、utf-8 の読みの限界で保存でバイト列が変わる（BUG-0052）。
- 試験: unittest/cp932-roundtrip-tests.l の `cp932-roundtrip-invalid-bytes-auto-lossy`、`cp932-detect-nearly-utf8-stray-byte`、`cp932-detect-nearly-sjis-stray-byte`。
