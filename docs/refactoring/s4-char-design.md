# S4 文字の扱いの設計（キーの分離・エンコーダ層・文字の拡幅）

- 状態: 案。師匠の判断の項目が 2 つある（「13. 判断」）。判断の項目は推奨で進めてよい作りにしてある。
- 作成日: 2026-11-22
- 対象: feature/x64-only（f2342cb）
- 根拠: [計画](plan.md) §4.2・§4.4・§4.5・§6 の S4・§8 の判断 1・3・9・§9、[決定事項](decisions.md)、合意 `.pair-agent/agreements/s4-character-handling.md`
- 行番号は f2342cb のもの。S4-d・S4-k・S4-c で動くので、関数名で探すこと。
- 「この PC で確かめた」とある数は、2026-11-22 に日本語の Windows 11 で Win32 の API を呼んで数えたもの。

## 0. 推奨の一覧

| # | 項目 | 推奨 | 章 |
|---|---|---|---|
| 1 | 格納の幅 | Char は u_int32_t（UTF-32）。文字列とバッファのチャンクは今の形（1 文字 1 要素）のまま。本文のメモリは 2 倍になるが受け入れる（判断 1 で決定済み） | 2 |
| 2 | 値の空間 | 0〜0x10FFFF は Unicode。0x110080〜0x1100FF は生のバイト。0x01000000〜0x0100FFFF はキー。キーは文字列とバッファに入れない | 2.3 |
| 3 | キー（S4-k） | 16bit の Char のまま、キーの値を 0x01000000 の上へ移す。キーは Lisp の文字のまま。キーボードマクロは文字のベクタにする。C++ ではキーを別の型（keycode_t）にする | 3 |
| 4 | エンコーダ層（4a） | 「バイト列 ⇔ Char」の変換器を一か所（codec）に集め、今の char-encoding オブジェクトから引く。Lisp のファイルストリーム、load、ini、履歴も通す。表はビルドのときに、追跡しているマッピング表から作る | 4 |
| 5 | load の符号の判定 | BOM → `-*- encoding: … -*-` → ファイル全体が正しい UTF-8 なら UTF-8 → それ以外は cp932 | 4.5 |
| 6 | cp932 の往復 | 不正なバイトは生のバイトの文字で保つ。重複符号（398 個）はバッファごとに「どの符号で読んだか」を覚えて、同じ符号で書く（判断 A） | 5 |
| 7 | フォント | Unicode の文字種と表示の幅と、バッファの「漢字の言語」で枠を選ぶ。描画は ExtTextOutW に一本化する | 6 |
| 8 | Char で引く表 | UCD から作る 2 段の表（数十 KB）。ASCII の表は今のまま。構文表は ASCII の密な表と、非 ASCII の疎な上書き | 7 |
| 9 | Lisp から見える変化 | 文字の値、char-code-limit、`\X`・`#\x` の意味、キーボードマクロの型、文字集合の関数（8 章の表） | 8 |
| 10 | ダンプと .lc | ダンプはビルドのたびに作り直す（形式の版の定数を足す）。.lc は UTF-8 と版の印を持つ。版の印のない .lc は読まない（.l があれば .l を読む） | 9 |
| 11 | 段取り | S4-k ∥ S4-c ∥ 新しいファイルだけの準備 → W（幅だけ 32bit、値は今のまま）→ 準備の片（並列）→ V（値を Unicode へ、/utf-8）→ 後片付け（並列） | 10 |
| 12 | Win32 に残る A 版 | V で manifest に activeCodePage=UTF-8 を入れる。A 版の残りは UTF-8 で通る。W 版への移行は S5・S7 で続ける | 10.4 |

## 1. 今の作り（事実）

### 1.1 文字の持ち方

| もの | 今 | 場所 |
|---|---|---|
| 文字の型 | `typedef u_int16_t Char`。`CHAR_LIMIT 0x10000`。`lChar` は u_long で、EOF は `lChar (-1)` | cdecl.h |
| Lisp の文字 | 即値。`make_immediate (u_short type, u_short data)` なので 16bit しか持てない | lisp.h make_immediate・ximmediate_data、char.h make_char・xchar_code |
| 文字列 | Char の配列（`xstring_contents`）。最大の長さは `INT_MAX / sizeof (Char)` | string.h |
| バッファ | `struct Chunk` の `Char *c_text`。`TEXT_SIZE 4096`（1 チャンク 8KB）。領域は `fixed_heap Chunk::c_heap (sizeof (Char) * TEXT_SIZE)` | Buffer.h、Buffer.cc |
| `char-code-limit` | CHAR_LIMIT（65536） | init.cc |
| `code-char` | `make_char (Char (fixnum_value (code)))`。16bit に切り詰める（不具合 F1） | char.cc Fcode_char、bytecode.cc の BCcode_char |
| `Char (-1)` を「無い」の印に使う所 | 16 ファイル 69 箇所（encoding.cc 19、ucs2.cc 12、insdel.cc 8 など） | — |
| `sizeof (Char)` | 24 ファイル 60 箇所（data.cc 12、string.cc 7、regex.cc 6 など） | — |

### 1.2 内部の符号の配置（charset.h の先頭の注記）

| 範囲 | 中身 |
|---|---|
| 0000–007F | ASCII |
| 0080–00FF | JIS X 0201 のカナ。cp932 の 1 バイトの値をそのまま持つ（0x80、0xA0、0xFD–0xFF も入る） |
| 0100–057F | ISO-8859-1〜13（128 ずつ） |
| 0D00–16FF | UJP、ULATIN、グルジア文字、IPA、修飾記号（SMLCDM）。作った PC のフォントで半角・全角に分けた Unicode の文字 |
| 1700–39FF | KS C 5601 |
| 3A00–5CFF | GB 2312 |
| 5D00–7FFF | JIS X 0212 |
| 8000–80FF | メタ文字（0x8000 \| ASCII） |
| 8100–9FFF、E000–FCFF | cp932 の 2 バイト。Shift_JIS の値をそのまま持つ |
| A000–D5FF | Big5 |
| D600–D7FF | 内部に無い UTF-16 の値（2 つの Char で 1 文字） |
| D800–DFFF | サロゲート（2 つの Char で 1 文字） |
| FE00–FEFF | メタ＋機能キー |
| FF00–FFFF | 機能キー（F1〜F24、PageUp など、マウスのボタン、疑似の制御文字。0x40 が Shift、0x80 が Ctrl） |

- 内部 → Unicode は `internal2wc_table`（ucs2tab.h、128KB）。手で生成して追跡している。生成器 gen-ucs2tab.cc は、cp932・CP936・CP949・CP950 の部分を作った PC の `MultiByteToWideChar` から取った。
- ULATIN と UJP の配置は、作った PC のフォント（fontrange.h、gen-fontrange.cc）で決まった。
- Unicode → 内部は、起動のときに作る `wc2internal_table`・`wc2cp932_table`、初めて使うときに作る `wc2big5_table`・`wc2ksc5601_table`・`wc2gb2312_table`（ucs2.cc。各 64K×2 バイト）。

### 1.3 変換の経路

| 経路 | 今 | 場所 |
|---|---|---|
| バッファ ⇔ ファイル | encoding.h の流れ（`encoding_input_stream_helper`・`encoding_output_stream_helper`）。char-encoding オブジェクトで符号を選ぶ | fileio.cc、encoding.cc |
| Lisp のファイルストリーム（open・with-open-file） | 符号の変換をしない。読むときは先行バイトなら次のバイトと組にして 1 つの Char、書くときは DBCP なら上位と下位の 2 バイト | stream.cc readc_stream・putc_file_stream・putc_sock_stream |
| load・require・si:*load-library・autoload | 上の生のストリーム（:canonical）。符号の判定も `-*- -*-` の印も見ない。印を見るのは mc-load-file など mc- の関数だけ（バッファに読んでから load） | lread.cc load_file → stream.cc create_file_stream、misc.l mc-load-file |
| 履歴・resume・session・キーボードマクロのファイル | with-open-file（生）と `write :readably`。読みは load | history.l、session.l、kbdmacro.l |
| abbrev | 書くときは write-region（バッファの符号。既定は sjis）、読むときは load（生） | abbrev.l |
| .lc | 読みも書きも生のストリーム。先頭に版の印は無い | compile.l compile-file |
| xyzzy.ini | GetPrivateProfileString・WritePrivateProfileString（A 版。CharacterSet NotSet） | conf.cc read_conf・write_conf |
| Win32 の文字列 | s2w・w2s・make_string (const char *)（cp932 のバイト ⇔ Char） | string.cc |
| 画面 | ASCII・カナ・cp932 は ExtTextOutA（cp932 のバイト）。ほかは 1 文字ずつ ExtTextOutW (i2w (…)) | disp.cc paint_chars |
| キー入力 | WM_CHAR のバイトを `*kbd-encoding*` で復号する（kbd_queue::putc・puts）。IME の確定は ImmGetCompositionStringW の経路もある | kbd.cc、toplev.cc |

- 文字列の印字（lprint.cc print_string）: 上位バイトが SJIS の先行バイトでない DBCP は `\XNNNN`、値が先行バイトの 1 バイトは `\xNN`。文字の印字（print_char）は同じ規則で `#\xNNNN`。
- 読み（lread.cc の文字列の読みと parse_digit_char）: `\xNN`（2 桁）、`\XNNNN`（4 桁）、`#\xNNNN`、`#\NNN`（8 進）。値は内部の符号で、CHAR_LIMIT 未満。
- だから、履歴・resume・session・.lc には `\X`・`#\x` が内部の符号のまま書かれている。

## 2. 格納の幅（項目 1）

判断 1 で「Unicode のコードポイント、1 文字 32bit」に決まっている。この章はその中身を決める。

### 2.1 推奨

- `typedef u_int32_t Char`。char32_t ではなく u_int32_t にする（今の算術と printf の書式に合わせる）。
- 文字列とバッファのチャンクは、今の形のまま（1 文字 1 要素）。TEXT_SIZE は文字の数で 4096 のまま。1 チャンクは 16KB になる。
- Lisp の文字の即値は 32bit のデータを持つ。`make_immediate (u_short type, u_int32_t data)` にする（x64 なので、下位 16bit のタグの上に 32bit を置ける）。これは S4-k で先に行う（3 章）。
- メモリ: 文字列とバッファの本文は 2 倍。ASCII で 100MB のファイルは、今 200MB、拡幅の後は 400MB を使う。x64 だけなので受け入れる。拡幅の後に計測する（decisions.md の判断 1 の「未確認」）。

### 2.2 採らない案

| 案 | 採らない理由 |
|---|---|
| UTF-16 の単位で持つ | 判断 1 で採らないと決まった。サロゲートの扱いが文字列の全箇所に入る |
| バッファの中を UTF-8（可変長）にする | point が文字の番号である前提（insdel・move・regex・search）を全部作り直すことになる |
| チャンクごとに 16bit と 32bit を切り替える | `c_text` を直接触る 9 ファイル（Buffer・disp・fileio・insdel・move・msgbox・regex・search・syntax）が全部 2 通りになる |

### 2.3 値の空間（32bit の Char と Lisp の文字）

| 範囲 | 意味 | 文字列・バッファに入るか |
|---|---|---|
| 0x000000–0x10FFFF | Unicode。D800–DFFF は、UTF-16 の源（ファイル名、クリップボード、壊れた UTF-16 のファイル）から来た孤立したサロゲートだけ | 入る |
| 0x110080–0x1100FF | 生のバイト（RAW_BYTE_BASE 0x110000 + バイト）。どの符号でも解けなかったバイトを 1 つずつ持つ | 入る |
| 0x01000000–0x0100FFFF | キー（KEY_BASE 0x01000000 + 今の 16bit の配置）。メタ、機能キー、マウス、疑似の制御文字 | 入らない |
| 0x10000000、0x20000000 | キーの列（kbd_queue）の中だけで使う印（LCHAR_MOUSE・LCHAR_MENU） | 入らない。Lisp には出ない |
| 0xFFFFFFFF | lChar_EOF | — |

- `char-code-limit` は 0x110000 にする（V で）。生のバイトとキーはその上にある。
- 生のバイトは、どの符号で書いても元のバイトを 1 つ書く。UTF-8 で保存すると、元のファイルと同じ壊れた UTF-8 になる（元のバイトを保つことを優先する）。画面では `\x85` のように 4 マスで表示する。
- 「無い」の印は `Char (-1)` をやめて `CHAR_NONE`（0xFFFFFFFF）にする（W で。16bit の表に 0xFFFF で入っている所があるので、比べ方を揃える）。

## 3. キーの符号の分離（S4-k）

この章は S4-k の役がそのまま着手できる粒度で書く。

### 3.1 今の使われ方（事実）

キーは Lisp の文字（`(characterp #\F1)` は t）で、値は Char の空間の端にある。

| 使う所 | 中身 | 場所 |
|---|---|---|
| 定義 | CC_META_BIT 0x8000。CCF_* 0xFF00〜0xFF3E（PageUp〜Help、F1〜F24、マウスのボタン、XBtn）。疑似の制御文字 CCF_EXCLAM〜（0xFF30 系。Shift・Ctrl のビットを番号に流用し 36 個）。CCF_SHIFT_BIT 0x40、CCF_CTRL_BIT 0x80、CCF_META 0xFE00、CCF_EMPTY_CHAR 0xFFF8。述語と変換は meta_char_p・function_char_p・function_to_meta_function・meta_function_to_function・pseudo_ctlchar_p | chtype.h |
| キーの列 | `lChar cc[QUEUE_MAX]`。メニューは `LCHAR_MENU (0x20000) \| ID`、マウスは `c \| LCHAR_MOUSE (0x10000)` | kbd.h、toplev.cc（WM_COMMAND の putc）、mouse.cc |
| 仮想キー → キー | decode_vkeys・decode_ctlchars・decode_syskeys・decode_keys・decode_syschars。pre_decode_vkeys は `*extended-key-translate-table*`、lookup_translate_table は `*kbd-translate-table*`（長さ 128）を引く | kbd.cc |
| 実行 | dispatch (lChar) で `Char c = Char (cc)` に切り詰めてから keymap を引く。メタでも機能キーでもない非 ASCII の文字は `*default-input-function*` へ | toplev.cc dispatch |
| keymap | 密な keymap は長さ 380（128 + 4×63）。番号は full_keymap_index（ASCII はそのまま、機能キーは 128 + 下位 6bit に Shift・Ctrl の段を足す）。疎な keymap は `(文字 . 定義)` の連想リスト。メタは ESC の下の keymap に入れる | keymap.cc |
| キーボードマクロ | 記録は `Char saved[2048]`。end_macro が make_string で**文字列**にする。実行は execute_string。Fcommand_execute が `stringp (fn)` で分ける | kbd.h、kbd.cc end_macro・macro_char、toplev.cc execute_string、eval.cc Fcommand_execute |
| 最近のキー | get-recent-keys は copy_queue で**文字列**を返す | kbd.cc Fget_recent_keys |
| 読み | `#\C-x`（ASCII の制御文字。キーではない）、`#\M-x`（0x8000 \| ASCII）、`#\F1`、`#\S-C-F1`、`#\C-!`（疑似）、`#\M-F1`（0xFE00 系） | lread.cc number_backslash_reader、chname.cc |
| 印字 | 同じ名前で書く。非エスケープ（princ・`~C`）は Char をそのまま書く | lprint.cc print_char |
| その他 | set-meta-bit（char.cc Fset_meta_bit）、機能キーのバー（fnkey.cc）、メニューのキーの表示（menu.cc keyname）、機能キー → 仮想キー（toplev.cc）、ファイラ（filer.cc Filer::dispatch・read_char）、マウス（mouse.cc）、`*last-command-char*`（toplev.cc dispatch） | — |

Lisp の側（lisp/、samples/、unittest/ を grep した結果）:
- キーの値を数で書いている所はない（`(code-char #xff..)` も `#x8000` の算術もない）。メタは set-meta-bit だけ（cmds.l 2 箇所、keymap.l 1 箇所）。
- 名前で書いたキー: `#\M-…`・`#\C-M-…` が 22 ファイル 49 行、`#\F1`・`#\Left`・`#\LBtnDown` などが 21 ファイル 189 行。どれも値に依らない。
- keymap の長さ 380 を keymap.l が `*full-keymap-length*` に書いている。番号の付け方は変えないので、そのまま。
- 文字列に入ったキー: `*last-kbd-macro*`、`*saved-kbd-macro-alist*`（履歴の変数。履歴のファイルに `"\X8078…"` の形で書かれる）、save-kbd-macro-to-file・insert-kbd-macro の `~S`。

### 3.2 何が問題か

- Unicode では 0x8000–0x807F は漢字（U+8000〜）、0xFF00–0xFFFF は全角の英数（U+FF01 ！ など）で、キーと重なる。
- LCHAR_MOUSE 0x10000 と LCHAR_MENU 0x20000 は、U+10000 と U+20000 に重なる（BMP の外の文字を入力すると、マウスやメニューと取り違える）。
- キーが文字列に入る（キーボードマクロ、get-recent-keys）ので、文字列の値の空間からキーを外せない。

### 3.3 方針

1. キーの値を KEY_BASE（0x01000000）の上へ移す。下位 16bit の配置は今のまま（メタ 0x8000 \| ASCII、機能キー 0xFF00 \| 番号、メタ機能キー 0xFE00 \| 番号、0x40 が Shift、0x80 が Ctrl）。だから keymap の番号（下位 6bit と段）、疑似の制御文字の表（`pseudo_ctl2char_table[c & 0xff]`）、名前の表（chname.cc）は変えなくてよい。
2. キーは Lisp の文字のままにする（`(characterp #\F1)` は t）。別の Lisp の型にはしない。`#\F1` を使う 189 行と、`char=`・`case`・`eql`・`gethash` の比較をそのまま使うため。
3. 文字列とバッファにはキーを入れない。キーボードマクロと get-recent-keys は、文字の simple-vector にする。
4. Lisp の文字の即値を 32bit にする（lisp.h make_immediate・ximmediate_data。message.h の即値も同じ関数を使う）。Char（16bit）の文字列はそのまま。
5. キーの列の印を 0x10000000（LCHAR_MOUSE）、0x20000000（LCHAR_MENU）へ移す。Lisp には出ない。
6. C++ では、キーを別の型 `keycode_t`（中身が u_int32_t の enum class）にする。Char との混用はコンパイルで落ちる。キーの配置を知るのは新しい keycode.h だけにする。

### 3.4 型と関数

keycode.h（新規）:

```cpp
enum class keycode_t : u_int32_t {};
const u_int32_t KEY_BASE = 0x01000000;
// 今の CC_META_BIT・CCF_* は、KEY_BASE を足した値で keycode.h に移す（下位 16bit は同じ）
//   KEY_META_ASCII  = KEY_BASE | 0x8000   （| ASCII）
//   KEY_FUNCTION    = KEY_BASE | 0xFF00   （| 番号 | 0x40 Shift | 0x80 Ctrl）
//   KEY_META_FUNC   = KEY_BASE | 0xFE00
keycode_t keycode_from_char (Char);          // 修飾のない文字のキー（ASCII と非 ASCII の文字）
bool keycode_text_p (keycode_t);             // 文字のキーか（値 < KEY_BASE）
Char keycode_to_char (keycode_t);            // 文字のキーのときだけ
bool keycode_meta_ascii_p (keycode_t);       // 今の meta_char_p。(v & ~0x7F) == KEY_META_ASCII で判定する
bool keycode_function_p (keycode_t);         // 今の function_char_p
bool keycode_meta_function_p (keycode_t);
bool keycode_pseudo_ctl_p (keycode_t);
keycode_t keycode_set_meta (keycode_t, bool);  // set-meta-bit の中身
u_int keycode_keymap_index (keycode_t);      // 今の full_keymap_index
```

Lisp の文字との出し入れ（char.h）:

| 関数 | 中身 |
|---|---|
| `make_char (Char)` | テキストの文字。今と同じ |
| `make_key_char (keycode_t)` | キーの文字。文字のキーなら make_char と同じ値 |
| `make_char_value (lChar)` | 値から文字を作る（テキストでもキーでも）。readc_stream の結果を Lisp の文字にする所で使う |
| `xchar_code (lisp)` → Char | テキストの文字だけ。キーなら type-error（「キーは文字列やバッファに入れられません」） |
| `xchar_value (lisp)` → u_int32_t | どちらでも。char-code、char=・char< など、eql・equalp のハッシュ、印字で使う |
| `xkeycode (lisp)` → keycode_t | どちらでも（テキストの文字はその文字のキー）。keymap、キーの列、変換表で使う |

### 3.5 触るファイル

| ファイル | やること |
|---|---|
| chtype.h → keycode.h（新） | CC_META_BIT・CCF_*・述語・変換を keycode.h へ移し、KEY_BASE を足す。chtype.h には文字の分類だけを残す |
| lisp.h、char.h、message.h | 即値のデータを 32bit にする。make_key_char・xchar_value・xkeycode を足す。xchar_code にキーの検査を入れる |
| kbd.h、kbd.cc | キーの列の要素を「keycode_t ＋上位の印」にする。`saved[]` を keycode_t にする。end_macro と Fget_recent_keys はベクタを返す。macro_char はベクタ（と古い形の文字列）から読む。decode_*・pre_decode_vkeys・lookup_translate_table は xkeycode で |
| toplev.cc | dispatch を keycode_t で行う（`Char (cc)` の切り詰めをやめる）。execute_string はベクタも受ける。`*last-command-char*` は make_key_char。機能キー → 仮想キー（WM_PRIVATE_CALL_MENU の近く）も keycode.h で |
| eval.cc | Fcommand_execute: `stringp (fn)` に加えて、文字のベクタもキーボードマクロとして実行する |
| keymap.cc | full_keymap_index・parse_keymap・lookup_keymap・scan_key_slot・make_key_slot・search_key_slot・Fkeymap_index_char・Fkeymap_char_index・expand_keymap を keycode_t で |
| mouse.cc、fnkey.cc、fnkey.h、menu.cc（keyname） | CCF_* を keycode.h の名前に替える |
| filer.cc | Filer::dispatch・read_char・Vfiler_last_command_char の所だけ。S4-c とファイルが重なるが、関数が違う。取り込むときに合わせる |
| lread.cc | number_backslash_reader（キーの名前を keycode.h で組み立てる）。Fread_char などの `make_char (Char (c))` は、キーボードから来た値なら make_key_char。parse_digit_char は今のまま（数で書けるのはテキストだけ） |
| lprint.cc | print_char をキーとテキストに分ける。キーは princ でも名前で書く（`(princ #\M-x)` は `M-x`） |
| chname.cc | 名前の表の値を keycode_t にする |
| char.cc | 文字の関数は全部キーを受ける（xchar_value）。述語（alpha-char-p、digit-char-p、upper-case-p、graphic-char-p など）はキーなら nil、大文字小文字（char-upcase など）はキーならそのまま、比べる関数（char=〜char-not-lessp、char-equal〜）は値で比べる。今と同じ結果になる（例: cmds.l の process-prefix-args は `(digit-char-p #\F1)` を呼ぶ）。Fset_meta_bit を keycode.h で。Fcode_char は値の範囲を検査する（不具合 F1 を直す） |
| pred.cc | equalp の文字の比較（char_upcase (xchar_code …) の 2 箇所）を xchar_value で |
| eval.cc | Fcommand_execute（上の行）。Fcommandp は `stringp (object)` を t にしている所に、文字のベクタも足す（minibuf.cc の「C」の補完がこれを使う）。process_interactive_string の文字の比較を xchar_value で |
| lread.cc（キーボードの読み） | Fread_char・Fread_char_no_hang・Fpeek_char は、readc_stream の値（lChar。キーボードならキー）を、キーも作れる `make_char_value (lChar)` で文字にする（今は `make_char (Char (c))` で切り詰める）。Funread_char と Fpeek_char の peek-type は xchar_value。cmds.l の process-prefix-args が `(unread-char cc *keyboard*)` にキーを渡すため |
| kbd.cc（そのほか） | Fset_next_prefix_args（キーを受ける。cmds.l の process-prefix-args が渡す）、pre_decode_vkeys・lookup_translate_table（上の行） |
| toplev.cc（そのほか） | Fset_quit_char と、toplevel_wndproc で `app.lquit_char` を putc する所を xkeycode で |
| filer.cc（そのほか） | Filer::read_char の `app.lquit_char`、Ffiler_read_char（`make_char (Char (cc))` → make_char_value）、Ffiler_isearch（キーなら何もしない） |
| fnkey.cc | Fset_function_bar_label を xkeycode で |
| lisp/help.l | describe-function の `(stringp def)`（「a keyboard macro.」）に文字のベクタも足す |
| bytecode.cc | BCchar_code・BCcode_char・文字の比較の命令（CHAR_CASE_PRED など）を char.cc と同じにする |
| hash.cc | 文字のハッシュ（equalp の表で char_upcase (xchar_code …) を使う所）を xchar_value で |
| stream.cc | キーボードの流れ（st_keyboard）の読みで、lChar を make_key_char にする |
| lisp/kbdmacro.l | `(stringp def)` を「キーボードマクロか（文字列か文字のベクタ）」の判定に替える（name-last-kbd-macro・insert-kbd-macro） |
| lisp/help.l | view-lossage が get-recent-keys のベクタを扱う（`~:C` で印字しているので、列として回せばよい） |
| unittest/ | 新しい試験（3.7） |

- xchar_code の呼び出しは 27 ファイル 136 箇所ある。分け方: キーを受けるべき所（char.cc 43、keymap.cc 7、lprint.cc 4、hash.cc 4、kbd.cc 3、filer.cc 3、bytecode.cc 3、pred.cc 2、toplev.cc 2、fnkey.cc 1、eval.cc 1、lread.cc の Funread_char・Fpeek_char の 2）は上の表のとおり xchar_value・xkeycode に替える。それ以外は**変えない**。xchar_code が中で検査するので、キーが来れば type-error になる。変えない所は、文字列・バッファ・パスにテキストとして入れる所（array.cc、sequence.cc、string.cc、vector.cc、insdel.cc の Finsert・check_attrib、stream.cc の汎用入力の流れ、pathname.cc、syntax.cc、search.cc の scan_last_match、dialogs.cc、stdctl.cc、ldialog.cc の EM_SETPASSWORDCHAR、hashpjw.cc）、テキストだけの関数（move.cc の Fchar_columns・Fextended_alphabet_char_p、disp.cc の glyph_bmchar）、読みの表（lread.cc の parse_digit_char・call_macro_reader・call_dispmacro_reader・check_macro_char・check_dispmacro_char・Fread_delimited_list）。
- キーボードの流れから文字を受け取る Lisp の関数は全部キーを受ける: read-char、read-char-no-hang、peek-char、unread-char、set-next-prefix-args、set-quit-char、filer-read-char、`*last-command-char*`、`*filer-last-command-char*`。
- gen-ctab.cc の疑似の制御文字の表（pseudo_char2ctl_table・pseudo_ctl2char_table）は番号だけを持つので、そのまま。

### 3.6 Lisp から見える変化（S4-k）

| 式 | 今 | S4-k の後 |
|---|---|---|
| `(characterp #\F1)` | t | t |
| `(char-code #\F1)` | #xff0f | #x100ff0f |
| `(char-code #\M-x)` | #x8078 | #x1008078 |
| `(char-code #\C-x)` | #x18 | #x18（ASCII の制御文字。キーではない） |
| `(code-char #x100ff0f)` | 切り詰めて別の文字 | #\F1 |
| `(code-char #x10041)` | #\A（不具合 F1） | nil（BUG-0041。実在しないキーの値も nil。long を超える値は range-error のまま）。V の後は U+10041 |
| `(string #\M-x)`、`(insert #\F1)` | 内部の値がそのまま入る | type-error |
| `(princ #\M-x)`、`(format nil "~C" #\M-x)` | 内部の値の文字 | `M-x` |
| `*last-kbd-macro*`、`(get-recent-keys)` | 文字列 | 文字のベクタ |
| symbol-function が文字のベクタ | 実行できない | キーボードマクロとして実行する |
| 古い履歴にある文字列のマクロ（`"\X8078…"`） | — | 実行するとき、旧い値（0x8000–0x807F、0xFE00–0xFFFF）をキーに読み替える（execute_string の中だけ）。V の後は 9.4 の旧い形の読みで変換する |

### 3.7 試験と完了の基準（S4-k）

- 既存の全件（637 件）が通る。ダンプを作り直して起動できる。
- 新しい試験（unittest/keycode-tests.l）。値ではなく名前で書く（V の後もそのまま通るように）:
  - 全部の機能キーの名前（chname.cc の 54 個）と修飾（なし、S-、C-、M-、S-C-、C-M-、S-C-M-）の組で、`#\…` を読んで印字すると元の綴りに戻る。疑似の制御文字（`#\C-!` など 36 個）、メタの ASCII（`#\M-a`〜）も同じ。
  - `*keymap-char-index` → `*keymap-index-char`（editor）が 380 個全部で往復する。
  - `(set-meta-bit #\x t)` が `#\M-x`、戻すと `#\x`。機能キーも同じ。
  - `(char-code #\F1)` は char-code-limit 以上、`(char-code #\a)` は未満。
  - `(string #\M-x)` が type-error。`(princ-to-string #\C-M-F1)` が `C-M-F1`。
  - define-key と lookup-keymap が、密な keymap、疎な keymap、メタ（ESC の下）で今と同じ結果。
  - キーボードマクロ: キーの列に入れる口（試験用の内部関数。無ければ骨格から作り、中身が無い間は「未実装」を返す）でキーを流し、記録したマクロが文字のベクタで、command-execute で実行できる。古い形の文字列（`(make-string 1 :initial-element (code-char #x8078))` に相当するもの）も実行できる。
  - キーの列に U+1F600 相当の値を入れても、マウスやメニューにならない（V の前は印のビットが値と重ならないことを確かめる）。
- 画面で確かめる（AI が起動して撮影・操作）: F キー、Alt+x（M-x）、マウスのクリックとドラッグ、メニュー、ファイラのキー、キーボードマクロの記録と実行。

## 4. エンコーダ層（4a）

### 4.1 今サポートしている符号（全部）

C++ の型（ces.h の encoding_type）と、変数（C++ の 2 個と lisp/encoding.l の 115 個）:

| 型 | 変数（`*encoding-…*`） | 中身 |
|---|---|---|
| auto_detect | auto（C++） | 自動判定（detect_char_encoding。既定は libguess） |
| sjis | sjis（C++） | cp932 |
| iso2022（エスケープあり） | jis、jis-open、jis-necext、jis-ibmext、iso-2022-jp-2（4 種）、iso-8bit-jp（4 種）、jis-sio、jis8、iso-2022-cn、iso-8bit-cn、iso-2022-kr、iso-8bit-kr | ISO-2022 系 |
| iso2022_noesc | euc-jp（4 種）、euc-gb、euc-kr | EUC 系 |
| big5 | big5 | Big5 |
| iso8859 | iso-8859-1、-2、-3、-4、-5、-7、-9、-10、-13 | ISO-8859 |
| windows_codepage | windows-latin1、-latin2、-cyrillic、-greek、-turkish、-baltic、koi8r、koi8u | 1 バイトの Windows の符号 |
| utf8 | utf8、utf8n、std-utf8、std-utf8n、それぞれの -kr・-gb・-big5・-cn | UTF-8 |
| utf16 | utf16、utf16le-bom、utf16be-bom、utf16le、utf16be、std- の各版、-kr・-gb・-big5・-cn の各版 | UTF-16 |
| utf7 | utf7、imap4、std- の各版、-kr・-gb・-big5・-cn の各版 | UTF-7 |
| utf5 | utf5、std-utf5、-kr・-gb・-big5・-cn の各版 | UTF-5 |
| binary | binary | バイナリ |
| （別名） | default-utf16 など 10 個、default-euc、default-iso-2022（リスト） | 上の別名 |

- 4 種 = 素、-open（OSF/JVC）、-necext、-ibmext（NEC・IBM の拡張の扱い。ENCODING_ISO_VENDER_*）。
- `:cjk`（UTF）: Unicode の漢字を、内部のどの文字集合（JIS・KSC・GB・Big5）へ寄せるか。
- `:windows`（UTF）: cp932 の表（0x8160 ⇔ U+FF5E）か、JIS の表（⇔ U+301C。utf2sjis.h の 9 文字）か。既定の変数は :windows t、std- は nil。

### 4.2 層の形（推奨）

符号の層 = 「バイト列 ⇔ Char（Unicode）」の変換器の集まり。今の char-encoding オブジェクト（ces.h の lchar_encoding）を、変換器を選ぶ鍵として残す。Lisp の変数・関数・利用者の設定はそのまま使える。

```cpp
// src/codec.h（新規）
class char_decoder        // バイト → Char
{
public:
  virtual ~char_decoder () {}
  // src から読めるだけ読み、dst に書く。final のとき、末尾の途中の列は生のバイトにする
  virtual void decode (const u_char *&src, const u_char *se,
                       Char *&dst, Char *de, bool final) = 0;
};
class char_encoder        // Char → バイト
{
public:
  virtual ~char_encoder () {}
  virtual void encode (const Char *&src, const Char *se,
                       u_char *&dst, u_char *de) = 0;
  virtual bool can_encode (Char) const = 0;  // parse-char-encoding-region などが使う
};
struct codec_context;     // cp932 の重複符号の覚え（5.4）、UTF-16 のバイト順、漢字の言語（6.2）
char_decoder *make_decoder (lisp encoding, codec_context * = 0);
char_encoder *make_encoder (lisp encoding, codec_context * = 0);
lisp decode_bytes (lisp encoding, const void *, size_t);   // バイト → Lisp の文字列
```

- 改行（CRLF・LF・CR）の変換は、今の xread_stream・xwrite_stream の側に残す。変換器はバイトと文字だけを扱う。
- 今の xread_stream・xwrite_stream の派生（sjis_to_internal_stream など読み 18 個・書き 14 個）は、V で「変換器を 1 つ持つ流れ」に置き換える。encoding_input_stream_helper・encoding_output_stream_helper の名前と使い方（fileio.cc 3、insdel.cc 4、kanji.cc 2、kbd.cc 3、process.cc 2、string.cc 1 の 15 箇所）は変えない。

変換器の一覧:

| 変換器 | 受け持つ型 | 表の元 |
|---|---|---|
| cp932 | sjis | src/unicode/CP932.TXT（Unicode.org の Microsoft の表。新しく追跡する）。重複符号の正準形は Windows と同じ（5 章） |
| iso2022・euc | iso2022・iso2022_noesc | JIS X 0208（cp932 の表から）、JIS0212.TXT、GB2312.TXT、KSC5601.TXT、CNS11643.TXT、8859-*.TXT、eucJP-ibmext.txt（どれも追跡済み） |
| big5 | big5 | BIG5.TXT |
| 1 バイト | iso8859・windows_codepage | 8859-*.TXT、CP125x.TXT、KOI8-*.TXT |
| utf8・utf16・utf7・utf5 | utf* | 表なし（std- の書きだけ 4.6 の 9 文字） |
| binary | binary | バイト b ⇔ U+00bb。生のバイトの文字は元のバイト |

- 表は全部、ビルドのときに生成する（gen-codec.cc、新規。gen-src1 の AfterBuild に足す）。OS の符号ページは使わない（作る PC に依らない。inventory-numbers の指摘）。
- CP932.TXT は読み（バイト → Unicode）の表で、書きの正準形は決まらない（同じ種類の表から作った Python の変換器は、Windows と逆の形を選んだ。5.2）。だから書きの表は生成器が規則で作る: 同じ Unicode に複数の符号があるときは JIS X 0208 ＞ NEC の 13 区 ＞ IBM 拡張（FA〜FC）＞ NEC 選定 IBM 拡張（ED・EE）の順に選ぶ。この規則で、この PC の WideCharToMultiByte の 398 個と全部一致する（5.2 の表）。
- CP932.TXT に行が無いもの（利用者定義の F040–F9FC、単独の 0x80）は、生成器が規則で足す（F040–F9FC → U+E000–U+E757、0x80 → U+0080）。足したものも上の突き合わせの試験で確かめる。
- 生成した cp932 の表を、この PC の MultiByteToWideChar(932, MB_ERR_INVALID_CHARS) と、全部の 1 バイトと 2 バイトで突き合わせる試験を置く。この PC で確かめた数: 定義のある 2 バイトは 9604 個、先行バイトと後続バイトの範囲の中で定義のないものは 1676 個、往復で変わるものは 398 個。
- CP936・CP949・CP950 も同じように、OS の表と突き合わせて差を記録する（今の gen-ucs2tab.cc はこれらを OS から取っていた。どちらに合わせるかは、差を見てから V の役が決める）。

### 4.3 どの経路がこの層を通るか

| 経路 | 今 | 拡幅の後 |
|---|---|---|
| バッファの読み書き（insert-file-contents、write-region、save-buffer） | 層を通る | 同じ。変換器が Unicode を出す |
| プロセスの入出力、kbd の符号、convert-encoding-*、octet-length の :encoding | 層を通る | 同じ |
| Lisp のファイルストリーム（open・with-open-file） | 通らない（生の SJIS） | 通す。open に `:char-encoding` を足す（4.4） |
| ソケットのストリーム | 通らない | 通す。connect・accept-connection に `:char-encoding`。既定は 4.4 と同じ（中身の確かめは S5） |
| load・require・si:*load-library・autoload | 通らない | 通す（判定は 4.5） |
| 履歴・resume・session・キーボードマクロのファイル・abbrev | 生のストリーム | UTF-8 で書く。先頭に `;;; -*- Encoding: utf-8; Xyzzy-Data-Format: 2 -*-`。読みは load と同じ判定 |
| .lc | 生のストリーム | UTF-8 と版の印（9.2） |
| xyzzy.ini | GetPrivateProfileStringA | conf.cc に小さな ini の読み書きを作り、層を通す。読みは load と同じ判定（古い ini は cp932 になる）、書きは UTF-8 |
| Win32 の文字列（s2w・w2s・make_string (const char *)） | cp932 | 10.4 |
| ファイル名 | A 版 | S4-c の W 版の層。Char → UTF-16 の変換は 1 つの関数にまとめる（拡幅のときにそこだけ替える） |
| octet の関数（md5・sha・hmac・base64 など） | w2s で cp932 のバイト | 4.7 |

### 4.4 Lisp のファイルストリームの符号

- 今の `:encoding` は改行の扱い（:canonical・:text は CRLF ⇔ LF、:raw はそのまま、:binary は 2 バイトの組を作らない）。名前は紛らわしいが、互換のためそのまま残す。
- 文字の符号は、新しい `:char-encoding`（char-encoding オブジェクト）で指定する。

| :encoding | 文字の変換 | :char-encoding を省いたとき |
|---|---|---|
| :canonical・:text | 変換器を通す。改行は CRLF ⇔ LF | `*default-fileio-encoding*`（既定 sjis。今の生の SJIS と同じ結果） |
| :raw | 変換器を通す。改行はそのまま | 同上 |
| :binary | 1 バイト = 1 文字（binary の変換器。バイト b ⇔ U+00bb）。:char-encoding は無視 | — |

- :binary で `(char-code (read-char s))` がバイトの値になるのは今と同じ。imagehdr.l の `#\xff`・`"\xc0…"` は、V の後も U+00FF・U+00C0 として同じに働く。
- 書けない文字（cp932 に無い文字を sjis で書くなど）は、今と同じく `?`。生のバイトの文字は元のバイト。
- 入力の既定を自動判定にしない。open は先読みができず、今の結果とも変わるため。自動判定は load（4.5）と、バッファへの読み込み（`*expected-fileio-encoding*`）だけで行う。

### 4.5 load の符号の判定

順番:

1. BOM: EF BB BF → UTF-8、FF FE・FE FF → UTF-16。
2. 1 行目か 2 行目の `-*- … Encoding: <名前> … -*-`（`coding:` も）。名前は char-encoding の名前と `*character-set-alist*` で引く。書式は files.l の find-file-scan-params と同じ。
3. ファイル全体が正しい UTF-8 で、非 ASCII を含む → UTF-8。
4. それ以外 → cp932（`*encoding-sjis*`）。

- lisp/・unittest/ の今の cp932 のファイルは 4 で cp932 になる。後でファイルごとに UTF-8 に変えても、3 で読める。変換は V の後の片（10 章の C2）でまとめて行い、各ファイルの先頭の `-*- Mode: Lisp; … -*-` に `Encoding: utf-8` を足す。
- cp932 の日本語の文が、たまたま正しい UTF-8 になることは、ごく短い場合を除いてまず無い。短いファイルで迷うなら 2 の印を書く。
- 判定は C++ の load_file（lread.cc）に置く。load・require・si:*load-library・autoload・`-l`・`~/.xyzzy` が全部ここを通る。mc-load-file など mc- の関数は、中身を load を呼ぶだけにしてよい。
- load-path を探す所（pathname.cc Ffind_load_path。w2s と GetFileAttributesA）は、S4-c の W 版の層を使う。

### 4.6 符号の変数と関数の扱い

| もの | 扱い |
|---|---|
| `*encoding-std-*`（10 個）と `:windows` | 残す。読みは素の UTF と同じ（変換しない）。書きのときだけ、cp932 に特有の 9 文字を JIS の形に寄せる（U+FF5E → U+301C など。今の utf2sjis.h の対応） |
| `-kr`・`-gb`・`-big5`・`-cn` の UTF（40 個）と `:cjk` | 残す。文字の値は変わらない。読んだバッファの「漢字の言語」（フォント。6.2）を決めるだけ |
| `*unicode-to-half-width*` | 残すが、何もしない（内部の「半角の寄せ」が無くなるため） |
| `*vender-depend-code-mapping*` | 変換器の既定の vender として残す |
| map-sjis-to-internal、map-internal-to-ucs-2 など（encoding.l） | 名前を残し、中身を変換器で作り直す（内部 = Unicode） |
| insert-internal-char-table（encoding.l。0〜#xFFFF を回す） | insert-unicode-char-table と同じにする |
| detect-char-encoding・guess-char-encoding | そのまま（バイトを見る判定なので、内部の値に依らない） |
| parse-char-encoding-region・-string | 変換器の can_encode で作り直す（今は code_charset で内部の領域を見ている） |

### 4.7 octet の関数（文字列をバイトとして扱う所）

事実:
- 文字列 → バイト（byte-stream.cc の byte_input_string_stream・byte_input_streams_stream）: 256 未満の Char は 1 バイト、DBCP は上位と下位の 2 バイト。
- バイト → 文字列（byte_output_wstream::sflush）: 先行バイトなら次のバイトと組にして 1 つの Char、それ以外は 1 バイト 1 Char。つまり「内部の符号 = SJIS のバイト」の生の規則。
- この 2 つを使う関数（xstream_ibyte_helper・xstream_obyte_helper）: si:base64-decode・-encode、si:uudecode・uuencode、si:quoted-printable-decode・-encode、si:www-url-encode・-decode、si:binhex-decode、si:md5・si:sha-1〜512・si:hmac-*（encdec.cc）。:encoding は受けない。digest の `:binary t` は、1 バイト 1 文字の文字列（make_string_simple）を返す。hmac の鍵は w2s を通す。
- si:octet-length は :encoding が無ければ w2sl（内部の 2 バイト）で数え、あれば変換器で数える（string.cc Fsi_octet_length）。
- Lisp は「バイトの文字列」をこの規則で受け渡している: `(convert-encoding-to-internal charset (si:base64-decode …))`（encdec.l decode-mime-field）、map-char-encoding-region（kanji.l。バッファの文字を「元のバイト」とみなして別の符号で読み直す）、text-decode-region（encdec.l。復号したバイトをそのまま insert）、system-tests.l の HMAC と md5 の試験（`(code-char #xaa)` などで作ったバイトの列）。
- si:make-string-chunk・si:pack-string・si:unpack-string（chunk.cc）は、FFI で C の文字列を作るための w2s・s2w。これはバイトではなく Win32 の文字列（10.4 の ACP の規則に従う）。

推奨（「バイトの文字列」の規則。V で入れ、R7 で準備する）:
- バイト → 文字列: 1 バイト 1 文字。バイト b は U+00bb（Latin-1 と同じ）。`(char-code c)` がバイトの値になる（:binary のストリームと同じ。4.4）。
- 文字列 → バイト: `:encoding` を受ける関数はその符号で変換する（今 :encoding を受けない上の関数にも足す）。省いたときの既定の規則:
  - 全部の文字が U+00FF 以下か生のバイトなら、1 文字 1 バイト（バイトの文字列とみなす）。
  - それ以外は `*default-fileio-encoding*`（既定 sjis）で変換する。生のバイトの文字は元のバイト。
- この規則で、今と結果が変わらないもの: ASCII だけの文字列、`(code-char #xaa)` などで作ったバイトの列、digest の :binary の結果、日本語の文（cp932 のバイト。半角カナも 1 バイト）、si:octet-length の `"あ"` → 2、`"漢字"` → 4。
- 今と変わるもの:
  - base64-decode などの結果の文字列は、cp932 の日本語でも読める文字にならず、1 バイト 1 文字になる（今は SJIS の組で読める文字になっていた）。text-decode-region（encdec.l）は、insert の前に `*expected-fileio-encoding*` で読む形に直す。convert-encoding-to-internal を通している decode-mime-field は直さなくてよい。
  - ISO-8859 の文字（今は内部の 2 バイトという意味の無いバイト）は、Latin-1 の 1 バイトになる。
- map-char-encoding-region（kanji.l）は、「元のバイト」を作るときにバッファの符号（buffer-fileio-encoding）で書き戻してから読み直す形に直す（生のバイトと重複符号の覚えで、元のバイトに戻る）。
- 採らない案: バイト 0x80–0xFF を生のバイトの文字（0x1100xx）で表す。既定の規則は要らなくなるが、`(char-code (read-char s))` がバイトの値でなくなり、:binary のストリームを読む利用者のコード（imagehdr.l など）が全部壊れる。

## 5. cp932 の往復

S4-0 の役へ（先に読むこと）:
- 今のコードでは、どのバイト列も往復で変わらない（5.1）。S4-0 はこれを基準に置いてよい。
- 拡幅（V）の後は、判断 A のどちらを選んでも、「同じ文字を 2 つの符号で書いたファイル」の期待のバイト列は変わる。推奨の案（5.4）で往復を保つには、ED・EE の場合、FA〜FC の場合、NEC の 13 区の場合を**別のファイル**に置く。1 つのファイルに全部を入れる試験は、V の後に期待値を「最初に出た符号に揃う」へ直す。
- 不正なバイト、定義のない符号、利用者定義、末尾の先行バイトは、V の後も往復で変わらない（5.3）。

### 5.1 今（事実。コードを読んで確かめた）

- 読み（encoding.cc の sjis_to_internal_stream::refill_internal と fast_sjis_to_internal_stream）: 先行バイト（0x81–0x9F、0xE0–0xFC）なら、次のバイトを**検査せずに**組にして `(c1 << 8) + c2` の 1 つの Char にする。それ以外のバイトは値そのもの（0x80、0xA0、0xFD–0xFF も 0x0080 などの Char）。末尾の先行バイト 1 つは、そのバイトの Char。
- 書き（encoding.cc の internal_to_sjis_stream::refill）: ISO-8859・GB・KSC などの領域は Unicode を経て cp932 に変える。cp932 の領域と 0x0080–0x00FF は、値のバイトをそのまま書く。
- だから今は、どのバイト列も往復で変わらない（NEC の 13 区、ED・EE、FA〜FC、利用者定義、定義のない符号、先行バイト＋不正な後続、不正な単独のバイト、末尾の先行バイト）。変わるのは改行だけ（CRLF・LF・CR の判定と、混ざっているときの統一）。
- 先行バイト＋改行（例 0x81 0x0A）は 1 つの Char 0x810A になり、改行として数えられない。往復は変わらないが、行の数え方が変わる。

### 5.2 Unicode を経ると何が変わるか（この PC の Windows で確かめた）

MultiByteToWideChar(932) → WideCharToMultiByte(932) で、2 バイトの全符号を往復させた結果:

| 元の領域 | 往復の後 | 個数 |
|---|---|---|
| NEC 選定 IBM 拡張（ED40–EEFC） | IBM 拡張（FA–FC） | 373 |
| NEC 選定 IBM 拡張の ¬（EEF9） | JIS X 0208 の ¬（81CA） | 1 |
| NEC の 13 区の ≒≡∫√⊥∠∵∩∪（8790–879C の 9 個） | JIS X 0208 の記号（81E0 など） | 9 |
| IBM 拡張の Ⅰ〜Ⅹ・㈱・№・℡（FA4A–FA53、FA58–FA5A） | NEC の 13 区 | 13 |
| IBM 拡張の ¬・∵（FA54、FA5B） | JIS X 0208（81CA、81E6） | 2 |
| 計 | | 398 |

- 2 つ以上の符号を持つ文字は 396 個（符号で 794 個）。¬ と ∵ は 3 つ（81CA・EEF9・FA54、81E6・879A・FA5B）。
- 利用者定義（F040–F9FC、1880 個）は U+E000–U+E757 で往復する。
- 単独の 0x80 は U+0080 になり往復する。0xA0、0xFD–0xFF は、厳密な変換（MB_ERR_INVALID_CHARS）では誤り、緩い変換では U+F8F0–U+F8F3。
- 範囲の中で定義のない 2 バイト（例 0x85 0x40）と、先行バイト＋範囲外の後続（例 0x81 0x20）は、緩い変換では U+30FB（・）になり、元に戻らない。だから OS の変換は使えない。
- WideCharToMultiByte は既定で「近い文字」に寄せる（U+00A5 → 0x5C、U+00A2 → 0x8191 など）。
- Python の cp932 の変換器は、ED・EE と FA〜FC の正準形が Windows と逆（FA5C → ED40）。表は Windows（CP932.TXT）に合わせる。

### 5.3 推奨の仕組み（S4-0 の各場合に 1 対 1）

| S4-0 の場合 | 読み | 書き | 往復 |
|---|---|---|---|
| JIS X 0208 | 表で Unicode へ | 表 | 同じ |
| 半角カナ（A1–DF） | U+FF61–FF9F | 表 | 同じ |
| NEC の 13 区（定義 83 個のうち、下の 9 個を除く 74 個。うち 13 個は IBM 拡張と重なるが、こちらが正準） | 表 | 表 | 同じ |
| 重複符号（NEC の 13 区の 9 個、ED・EE の 374 個、FA〜FC の 15 個） | 表で Unicode へ。正準でない符号なら、バッファに「この文字はこの符号で読んだ」を覚える（5.4） | 覚えがあればその符号、なければ正準形（Windows と同じ） | 同じ（同じ文字を 2 つの符号で書いたファイルを除く） |
| 利用者定義（F0–F9） | U+E000–U+E757 | 表 | 同じ |
| 単独の 0x80 | U+0080（Windows と同じ） | 表 | 同じ |
| 不正な単独のバイト（0xA0、0xFD–0xFF） | 生のバイトの文字 | 元のバイト | 同じ |
| 定義のない 2 バイト（1676 個） | 先行バイトも後続バイトも生のバイトの文字 | 元のバイト | 同じ |
| 先行バイト＋範囲外の後続（例 0x81 0x0A、0x81 0x20） | 先行バイトだけ生のバイトの文字にし、次のバイトから読み直す（改行は改行になる） | 元のバイト | 同じ（行の数え方は今と変わる） |
| 末尾の先行バイト | 生のバイトの文字 | 元のバイト | 同じ |
| 改行 | 今と同じ | 今と同じ | 今と同じ（混在は統一される） |

- U+F8F0–U+F8F3（UTF-8 のファイルから来たもの）は、Windows と同じく 0xA0、0xFD–0xFF で書く。
- U+301C・U+2016 など JIS の形の 9 文字も、cp932 の符号で書く（5.5）。

### 5.4 重複符号の覚え（バッファごと）

- 読むとき: 正準でない符号 s を読んだら、その文字 u について「u は s で書く」をバッファの表に入れる。同じ u がもう入っていれば、最初のものを残す。
- 書くとき（そのバッファを cp932 で保存する、write-region する）: u の符号を表で引き、あればそれを、なければ正準形を書く。
- 表は小さい（最大 396 項目）。C++ の Buffer に持つ。revert と insert-file-contents は表に足す。Lisp からは si: の関数で見られるようにする（試験のため）。
- 文字をコピーして別のバッファへ貼ったときは覚えは移らない（正準形になる）。
- 限界: 同じ文字を 2 つの符号で書いたファイル（例: 髙 を EEE0 と FBFC の両方で書いた）は、後から出た方が最初の符号に揃う。S4-0 の試験は、ED・EE の場合と FA〜FC の場合と NEC の 13 区の場合を別のファイルに置く。全部を 1 つのファイルに入れる場合は、この揃い方を期待値にする。
- EUC-JP・ISO-2022-JP の -ibmext・-necext・-open は、今の vender の指定（ENCODING_ISO_VENDER_*）を変換器の表の選び方に移す。バイトの往復を保証するのは cp932 だけにする。

判断 A（13 章）: この「覚え」を作るか、Windows と同じ正規化を受け入れて完了の基準を定義し直すか。

### 5.5 WAVE DASH など、2 つの表

- 今: UTF の読み書きで :windows t（既定）なら cp932 の表（0x8160 ⇔ U+FF5E）、:windows nil（std-）なら JIS の表（utf2sjis.h の 9 文字。U+301C など）。内部では両方とも 0x8160 なので、既定の utf8 で U+301C を読んで保存すると U+FF5E に変わる。
- 拡幅の後:
  - cp932・EUC-JP・ISO-2022-JP の読みは、Windows の表（U+FF5E など）を正準にする（今の既定と同じ見え方）。
  - それらの書きは、両方の形を受ける（U+301C も U+FF5E も 0x8160）。
  - UTF の読みは変換しない（U+301C は U+301C のまま。上の「保存で変わる」が直る）。
  - std- の UTF の書きだけ、9 文字を JIS の形に寄せる（4.6）。

## 6. 文字種ごとのフォント

### 6.1 今（事実）

- フォントの枠は 9 個（font.h）: ASCII、JP、LATIN、CYRILLIC、GREEK、CN_SIMPLIFIED、CN_TRADITIONAL、HANGUL（Lisp の :ksc5601）、GEORGIAN。ほかに Lucida Sans Unicode を、U+02B0–036F を描くたびに作る（disp.cc paint_chars_lucida）。
- 選び方: `code_charset_table[cc >> 7]` → glyph_sbchar・glyph_dbchar → glyph の GLYPH_CHARSET（5bit、23 種）→ paint_chars の switch → 枠（disp.cc）。印刷（print.cc の paint_line・get_glyph_width）は同じ switch を別に持つ。IME の未確定の文字（kbd.cc kbd_encoding_font）は `*kbd-encoding*` から枠を 1 つ選ぶ。
- 画面の 1 マス（glyph_t、Window.h）は、文字 8bit＋文字集合 5bit＋種類 2bit。全角は 2 マスに上位と下位のバイトを入れ、描くときに組み立てる。コードポイントは入らない。
- BMP の外の文字と、Unicode に対応の無い内部の値は、1 Char ごとに空の箱を描く（BMP の外の文字は 2 つの箱）。
- 設定: set-text-fontset・get-text-fontset（font.cc）、lisp/font.l、xyzzy.ini の [Font]（キーは Ascii、Japanese、Latin、Cyrillic、Greek、GB2312、BIG5、KSC5601、Georgian。値は `高さ,"名前",文字集合`）、フォントのダイアログ（ChooseFont.cc が FONT_MAX で回す）。
- 既定の名前のうち MS Hei（GB2312）、GulimChe（KSC5601）、BPG Courier New U（Georgian）は、この PC に入っていない（SimSun、Microsoft JhengHei、Malgun Gothic、Segoe UI Symbol、Segoe UI Emoji は入っている）。

### 6.2 推奨

- 枠は今の 9 個を残す（設定と ini の互換のため）。「その他」の枠を 1 つ足す（FALLBACK。既定は Segoe UI Symbol）。
- 文字 → 枠は、Unicode の文字種（UCD の Scripts と Blocks）、表示の幅、バッファの「漢字の言語」で決める:

| 文字 | 枠 |
|---|---|
| U+0000–007F | ASCII |
| ひらがな、カタカナ、半角カナ、CJK の記号と句読点、全角の英数 | JP |
| 漢字（統合漢字、互換漢字、拡張 A〜） | バッファの漢字の言語で: jp → JP、kr → HANGUL、gb → CN_SIMPLIFIED、big5 → CN_TRADITIONAL（既定は jp） |
| ハングル | HANGUL |
| 東アジアの幅が「曖昧」な文字（ギリシア文字、キリル文字、罫線、○■※ など）で、幅を 2 にしているとき | 漢字の言語の枠（既定は JP） |
| 同じ文字で幅を 1 にしているとき、または曖昧でない文字 | ギリシア → GREEK、キリル → CYRILLIC、グルジア → GEORGIAN、それ以外のラテン・IPA・修飾記号 → LATIN |
| それ以外（アラビア文字、タイ文字、絵文字、BMP の外の記号など） | FALLBACK |
| 生のバイト | ASCII で `\x85` を 4 マス |

- 漢字の言語は、ファイルを読んだ符号から決める（euc-kr・iso-2022-kr・-kr の UTF → kr、euc-gb・iso-2022-cn・-gb の UTF → gb、big5・-big5 の UTF → big5、それ以外 → jp）。バッファ・ローカルの変数（例 `buffer-han-language`）で変えられる。今の「同じ漢字でも GB の文字と JIS の文字は別のフォント」を、バッファの単位で保つ。
- 幅が曖昧な文字の幅は、変数 `*east-asian-ambiguous-width*`（既定 2）。cp932 のファイルの ○ や α の見え方が今と同じになる（今は JIS X 0208 の文字なので全角）。ISO-8859 のファイルのギリシア文字は今は半角なので、ギリシア語の利用者は 1 にする。
- 描画は ExtTextOutW に一本化する（ASCII と JP の ExtTextOutA をやめる）。BMP の外の文字は UTF-16 の組で 1 回に描く。
- glyph_t（Window.h）は、コードポイント 21bit、枠 4bit、属性（今の色・種類）を持てる 64bit にする。全角の 2 マス目は「続き」の印にする。
- 文字 → 枠と幅は、uprops（7.2）の 1 つの関数で引く。印刷も同じ関数を使う（print.cc の switch をやめる）。
- 枠のフォントに字が無いときに FALLBACK へ切り替える仕組み（GetFontUnicodeRanges を使う）は後の課題にする。最初は表のとおりに描く。
- 既定のフォントの名前は、Windows 11 に入っているものへ見直す（S7 で画面を撮って決める）。

## 7. Char で引く表

### 7.1 今の表と扱い

| 表 | 大きさ | 作り方 | 拡幅の後 |
|---|---|---|---|
| internal2wc_table（ucs2tab.h） | 64K×2 バイト | 手で生成して追跡 | 削除。ただし旧い形の履歴などを読むための「旧内部 → Unicode」の表として、移行の間だけ残す（9.4） |
| wc2internal・wc2cp932・wc2big5・wc2ksc5601・wc2gb2312（ucs2.cc） | 各 64K×2 バイト | 起動時・初めて使うとき | 変換器の表（gen-codec の出力）に置き換え |
| cns11643・big5cns・windows_*_to_internal・17 個の wc2int_*_hash | 数 KB〜数十 KB | 起動時・静的 | 変換器の表に置き換え |
| code_charset_table | 512 バイト | 起動時 | 削除 |
| char_width_table | 8KB | gen-char-width（ビルド） | uprops の幅（UCD の EastAsianWidth） |
| jisx0212 の半角の表と hash、lucida-width.h、jisx0212-width.h、fontrange.h | 小さい | 追跡・ビルド（作った PC のフォントに依る） | 削除 |
| char_type_table（257）、大文字小文字の表（256×3）、char_numeric_table など | 1KB 程度 | gen-ctab（ビルド） | そのまま（ASCII とバイトの表） |
| syntax_table（type・match・comment 各 256） | 776 バイト／表 | Lisp | 7.3 |
| kanjitab.h（全角・半角の変換） | 1KB 程度 | gen-ktab（cp932 の文字列の値に依る） | Unicode で作り直す |
| ibmext.h、utf2sjis.h、iso2022state.h | 小さい | ビルド | 変換器の生成器へ移す |
| print.h の glyph_width.pixel[CHAR_LIMIT] | 64K×2 バイト（印刷ごと） | 実行時 | 256 文字のページごとに、必要なときだけ作る |
| 正規表現の charclass | 256×256 のビット | 正規表現のコンパイル | 7.4 |
| re_fastmap、rk_fastmap、BM の表、skip_chars | 256（`c >> 8` で畳む） | 実行時 | 7.4 |
| keymap（380）、readtable（128） | — | — | そのまま |

### 7.2 Unicode の属性の表（uprops、新規）

- 生成器 gen-uprops.cc（ビルドのとき）が、src/unicode/ucd/ に追跡する UCD のファイル（UnicodeData.txt、EastAsianWidth.txt、Scripts.txt、Blocks.txt。版を 1 つに固定する）から作る。
- 形は 2 段の表。上の段は 0x1100 個（コードポイント >> 8）、下の段は 256 個ずつで、同じ中身のページは共有する。1 項目は 1 バイト（幅の種類 2bit、フォントの群 4bit、語の文字か 1bit、図形文字か 1bit）。大きさは数十 KB の見込み。
- 引く関数: `uc_width (Char, int ambiguous)`、`uc_font_group (Char)`、`uc_word_char_p (Char)`、`uc_graphic_p (Char)`。生のバイトは別に扱う（幅 4）。キーは来ない。
- 大文字小文字: 今は ASCII だけ（char_upcase など、全部 ascii_char_p で絞っている）。S4 では変えない。Unicode の大文字小文字は後の課題として記録する。

### 7.3 構文表

- 今: 256 個。非 ASCII に構文を設定できない（set-syntax-* は NIL を返す）。非 ASCII（256 以上）の引き方が 3 通りある（SCkanji の固定、`table[cc >> 8]`、word_state の Unicode の範囲）。0x80–0xFF の既定は、SJIS の先行バイトの名残（kanji）と半角カナ（kana）。ISO-8859 の文字（内部 0x01xx）は `table[0x01]` を引くので junk になる。
- 推奨: ASCII の 128 個は今の密な表。非 ASCII は、疎な上書き（並べた配列）と既定の規則。引く関数を 1 つ（`syntax_of (table, Char)`）にして、3 通りをなくす。

| 文字 | 既定の構文 |
|---|---|
| U+0080–009F | junk |
| 半角カナ（U+FF61–FF9F） | kana（今の 0xA1–0xDF と同じ） |
| 生のバイト | junk |
| それ以外の非 ASCII | kanji（今の「256 以上は SCkanji」と同じ） |

- set-syntax-* は非 ASCII も受けて、上書きに入れる。今 NIL を返していた所が働くようになる（互換は壊さない）。
- ダンプは syntax_table を生のまま書いている（data.cc）。形が変わるので、ダンプの書きと読みも直す（ダンプはビルドのたびに作り直すので問題ない）。

### 7.4 正規表現と検索

- charclass（regex.cc の class charclass）は、上位 8bit×下位 8bit のビットの 2 段で、コンパイルした結果も Char の語に `(hi << 8) + all` の形で入れている。→ 範囲の並び（[lo, hi] の組）と ASCII の 128bit にする。コンパイルした形は「範囲の数、lo、hi、…」（Char が 32bit なので 1 語に 1 つの値）。
- `REPEAT_INFINITY (CHAR_LIMIT - 1)` と長さの検査（`b - p >= CHAR_LIMIT`）は、文字の上限ではなくパターンの語の上限。別の定数（RE_WORD_LIMIT。値は今の 0x10000 のまま）にする。
- fastmap、BM（search.cc の bm_compilef・bm_compileb）、skip_chars、kwdmatch の 256 の表: 畳み方の関数 `fold256 (Char)` を 1 つ置き（0x80 未満はそのまま、それ以外は上位のビットを混ぜて 0x80–0xFF に畳む）、コンパイルと照合で同じものを使う。BM のずらしは、同じ桶に入る文字の最小を取る（今も `c >> 8` で畳んでいるので考え方は同じ）。
- `\w`・`\s`・`\_`（regex.h Regexp::char_syntax が `table[cc >> 8]`）と skip_syntax_spec（search.cc）は、7.3 の syntax_of に替える。

## 8. Lisp から見える変化

### 8.1 文字の値と文字の関数（V の後。キーは S4-k の後）

| 関数・変数 | 今 | 拡幅の後 |
|---|---|---|
| char-code（テキスト） | 内部の符号（あ = #x82a0、é〈ISO-8859-1〉= #x169） | Unicode（あ = #x3042、é = #xe9） |
| char-code（キー） | #x8078、#xff0f など | #x1008078、#x100ff0f など（S4-k） |
| char-code-limit | 65536 | #x110000 |
| code-char | 16bit に切り詰める | 0〜#x10FFFF、生のバイト、キーの範囲を受ける。それ以外は nil |
| `#\xNNNN`、`#\NNN` | 内部の符号 | Unicode（桁の数の上限を外す。`#\x1f600`） |
| `"\xNN"`、`"\XNNNN"` | 内部の符号 | Unicode（\x は 2 桁、\X は 4 桁のまま） |
| `"\X{…}"`（新） | — | 1〜8 桁の任意の値。印字は、テキストでない値（生のバイト、孤立したサロゲート）だけをこの形で書く |
| 文字列の印字（prin1） | cp932 に無い文字を `\X` で書く | テキストの文字はそのまま書く。書けない文字の扱いは書く先の符号に任せる（履歴・.lc などは UTF-8 なので失われない） |
| unicode-char | 内部へ変換。2 文字の文字列を返すことがある | code-char と同じ（BMP の外も 1 文字）。範囲外は nil |
| char-unicode | i2w（BMP だけ） | テキストは char-code と同じ値。キーと生のバイトは nil |
| iso-char-code・iso-code-char・iso-char-charset | 内部の領域で | 変換器の表で引く。文字集合を選ぶ順は jisx0208、jisx0212、gb2312、ksc5601、big5、iso8859-n で固定。:vender は今と同じ意味 |
| kanji-char-p | 256 以上なら全部 t（ラテン、ハングルも） | 幅が 2 の文字（EastAsianWidth の W・F。曖昧は `*east-asian-ambiguous-width*` に従う） |
| kana-char-p | 1 バイトの 0xA1–0xDF | 半角カナ U+FF61–FF9F |
| dbc-first-byte-p・dbc-second-byte-p | cp932 のバイトの判定 | 同じ（バイトの話なので変えない） |
| extended-alphabet-char-p | i2w の範囲で | 同じ範囲を Unicode で直接 |
| char-columns | 内部の領域で 1 か 2 | EastAsianWidth（曖昧は変数）。生のバイトは 4、制御文字は 2（今と同じ） |
| word-char-p | 構文表を内部の値で | 7.3 の syntax_of |
| graphic-char-p | i2w が 0xFFFF でない | 一般カテゴリが Cc・Cs・Cn でない |
| map-to-half-width-*・map-to-full-width-* | cp932 の表 | Unicode で作り直す（kanji.cc、gen-ktab）。キーワード（:ascii :hiragana :katakana :greek :cyrillic）は同じ |
| `*unicode-to-half-width*` | unicode-char と UTF の読みで半角へ寄せる | 意味が無くなる（変数は残す） |
| si:octet-length（:encoding なし） | 内部の 2 バイトで数える | 4.7 の既定の規則で数える（"あ" は 2、"漢字" は 4 のまま） |
| si:base64-decode など復号の結果 | SJIS の組で読める文字列 | 1 バイト 1 文字（U+00bb）。文字にするには convert-encoding-to-internal を通す（4.7） |
| open の :char-encoding（新） | — | 文字の符号を指定する。省くと `*default-fileio-encoding*`（4.4） |
| 履歴・session・.lc の形式 | 生の SJIS と内部の符号の `\X` | UTF-8 と版の印（9 章） |
| キーボードマクロ | 文字列 | 文字のベクタ（S4-k） |

### 8.2 lisp/・samples/・unittest/ の中で直す所（grep の結果）

- code-char・char-code: 61 行。ASCII の範囲だけのもの 31 行（直さない）、内部の非 ASCII の値を書いたもの 5 行、計算しているもの 25 行。
- 内部の非 ASCII の値を書いたもの（5 行）: 全部 unittest/system-tests.l（76・77・94・113・129 行）。HMAC の RFC 4231 の鍵とデータ `(code-char #xaa)`・`#xdd`・`#xcd`。バイトとして使っているので、4.7 の規則で同じバイトになる（直さずに通ることを試験で確かめる）。
- 計算しているもの（25 行）と、その扱い:

| 場所 | 中身 | 扱い |
|---|---|---|
| charname.l 12〜25 行 | `(make-vector 65536)`、`(dotimes (i char-code-limit))` で全部の内部の値を回し、char-unicode と iso-char-charset で名前（`JISX0208_…` など）を作る | cnamedef.l の Unicode の値を直接回す形に書き直す。前置きの名前は変換器で付ける（R9） |
| cmds.l 774〜821 行（quote-char） | 内部の符号を 16 進で打ち込む（dbc-first-byte-p で 2 バイト目を待つ）。unicode-char が 2 文字の文字列を返す場合を扱う | 「内部の符号」の入力を Unicode の入力と同じにする。2 文字の場合の扱いを消す（V の後） |
| cmds.l 865〜867 行（what-cursor-position） | `Char: ~:c (0x~x / U+~4,'0X / 0x~2,'0X[~A])` で内部の符号と Unicode と ISO の符号を出す | 内部 = Unicode なので 1 つにする。`U+~4,'0X` は BMP の外で 5〜6 桁にする |
| compile.l 1826〜1853 行 | バイトコードの命令・PC・定数の番号を `(code-char n)` で文字列に入れる。命令の値は 0x81E7–0x8327、定数は 40000±1024 | 文字列のまま（Char が 32bit なので値は入る）。.lc の書き方は 9.2 |
| encoding.l 900〜914 行 | 0〜#xFFFF を回して表を挿入 | insert-unicode-char-table と同じにする |
| help.l 71〜74 行 | キーの表示で、連続する値を「a .. z」にまとめる | そのまま（キーの値も連続のまま） |
| imagehdr.l（10 行）、encdec.l 75 行 | :binary のストリームのバイトを文字で比べる（`#\xff`、`"\xc0…"`、`[^\000-\040:\177-\377]`） | そのまま（:binary は U+00bb = バイト。4.4） |
| samples/life.l、hanoi.l | ASCII の範囲だけ | そのまま |

- 文字の名前で書いた非 ASCII（`#\─` など）は boxdraw.l の 48 行だけ。比べるのは同じ文字かどうかだけなので、値に依らない。
- 文字の範囲（`(char< #\ぁ c #\ん)` など）と、非 ASCII の範囲を書いた正規表現は無い。
- `*encoding-*` の変数: 120 種、lisp/ の中で 658 回（encoding.l 611、keyboard.l 43〈うち 20 は注記〉、files.l 3、dialogs.l 1）。試験は editor-tests.l 114、system-tests.l 22、typespec-tests.l 2。変数は全部残すので、直さない。
- si:octet-length の試験（system-tests.l 357〜366 行）: `"あ"` が 2、`"漢字"` が 4。4.7 の規則で同じ値になる。

## 9. ダンプと .lc の形式

### 9.1 ダンプ（xyzzy.wxp）と etc/DOC

事実:
- Fdump_xyzzy（data.cc）は、オブジェクトを 1 つずつ書く（ヒープの丸ごとの写しではない）。ポインタは書き直し、即値は 8 バイトの値のまま書く。だから文字の即値の値（内部の符号）がダンプに入る。
- 文字列（lsimple_string・lcomplex_string・lstring_array）と正規表現のコンパイル結果は、`sizeof (Char)` で中身を書く。syntax_table は構造体を生のまま書く。
- 見出し（dump_header）は magic、version（int64）、大きさとその反転、型の数。version は gen-syms.cc が `time (0)` で作る dumpver.cc の値で、gen-syms の入力（gen-syms.cc、cdecl.h、function.h、symbol.h）が変わったときだけ作り直される。
- 合わないとき（版、大きさ、型の数、末尾）は黙って捨て、普通に起動して新しいダンプを作る（startup.l）。
- ビルドのたびに xyzzy.wxp を消す（projects/xyzzy.vcxproj の DeleteXyzzyDumpFile）。bytecompile.bat も消す。`-image` で別の場所に置いたダンプは消されない。
- etc/DOC（doc.cc）は magic 'CODX' の後に `long off, length, Char[]` を並べる。`sizeof (Char)` が入り、版は無い。ダンプを作るときに書き直す。
- パッケージの表のハッシュ（hashpjw、Char の列から計算）は、読み込みの後に作り直さない。同じ exe で書いて読むので問題ない。

推奨:
- ダンプの形式は変わってよい（判断 3）。W と V のどちらでも cdecl.h が変わるので版は自動で変わる。
- そのうえで、明示の形式の定数 `DUMP_FORMAT`（data.cc）を見出しに足し、W と V で上げる。cdecl.h の外（syntax.h、Window.h など）の形の変更と、`-image` で残ったダンプを確実に捨てるため。
- etc/DOC の magic を W で変える（'CODX' → 例 'CDX4'）。古い DOC を読まないため。
- write-registry-literally（environ.cc。REG_BINARY に `sizeof (Char) * 長さ` を書く）は、W の後は古い値を読めない。lisp/ に使う所は無い。公開の変更として記録するだけにする。

### 9.2 .lc

事実:
- compile-file（compile.l）は、読みも書きも生のストリーム。トップレベルの式を `write :escape t :circle t` で並べる。先頭に版の印は無い。
- バイトコードは `(si:*byte-code "<文字列>" #(定数))`。文字列の各要素は命令・PC・定数の番号（0〜65535）で、命令の値の多くは cp932 の先行バイトの領域（0x81E7–0x8327）。だから今の .lc は、印字の規則で SJIS の 2 バイトとしてそのまま書かれている。
- load は .lc も .l と同じ生のストリームで読む。版の検査は無い（Fsi_byte_code は長さ 3 未満だけを検査する）。

推奨:
- W（幅だけ）では .lc は変わらない（値が同じなので、同じバイトになる）。
- V で、.lc は UTF-8 で書き、1 行目に `;;; -*- Mode: Lisp; Encoding: utf-8; Lc-Format: 2 -*-` を書く。
- バイトコードの文字列はそのまま使う。命令の値（U+81E7 など）は漢字として UTF-8 でそのまま書かれる。PC などが孤立したサロゲート（D800–DFFF）になったときは、印字が `\X{…}` で書く（8.1）。
- load は .lc の 1 行目を見て、`Lc-Format: 2` が無ければ、同じ場所の .l を読む（警告を出す）。.l も無ければエラー（「古い .lc。作り直してください」）。V の前に作った .lc を Unicode の読みで読むと、命令の値が別の値になって壊れるため。
- lisp/ の .lc は install.bat（bytecompile.bat）が毎回作り直す。利用者の site-lisp の .lc は作り直しが要る（公開の変更として案内する）。

### 9.3 版の上げ方

| 時点 | ダンプ | DOC | .lc | 履歴など |
|---|---|---|---|---|
| S4-k | cdecl.h は変わらない。即値の形が変わるので DUMP_FORMAT を上げる（この定数は S4-k で足す） | 変えない | 変えない（キーは名前で書かれる） | 変えない |
| W | 自動（cdecl.h）＋DUMP_FORMAT | magic を変える | 変えない | 変えない |
| V | 自動＋DUMP_FORMAT | magic を変える | Lc-Format: 2 | Xyzzy-Data-Format: 2 |

### 9.4 旧い形のデータの読み替え（V）

- 履歴（.xyzzy.history）、resume、session（*.ssn）、キーボードマクロのファイルは、xyzzy が自分で書いたデータ。V の前に書いたものには、内部の符号の `\XNNNN`・`\xNN`・`#\xNNNN` が入っている。
- これらを読む関数（history.l の load-history-file、session.l の load-session、kbdmacro.l で読むもの）は、1 行目に `Xyzzy-Data-Format: 2` が無ければ、旧い形として読む（読みの変数 `si:*read-legacy-internal-codes*` を束縛して load する）。符号は cp932（4.5 の判定でそうなる）。`\X`・`\x`・`#\x` の値は「旧内部 → Unicode」の表で読み替える（旧いキーの値 0x8000–0x807F・0xFE00–0xFFFF はキーに、表に無い値は U+FFFD に）。
- 旧いキーの値を含む文字列（S4-k より前のキーボードマクロ）は、文字列ではなく文字のベクタとして読む。キーは文字列に入らないため。S4-k の execute_string の読み替え（3.6）は V で消す。
- 読み替えの表は、今の internal2wc_table（128KB）を名前を変えて移行の間だけ残す。何版か後で消す。
- 利用者が書いた .l の `\X`・`#\x` は読み替えない（V の後は Unicode の意味になる。公開の変更として案内する）。
- 旧い履歴を読むと、次に保存するとき新しい形で書かれる。

## 10. 移行の段取り

### 10.1 考え方

- 幅（16bit → 32bit）と値（内部の符号 → Unicode）を別々に入れる。W は機械的な変更（型、大きさ、印、ダンプ）で、試験は今の値のまま全部通る。V は意味の変更（変換器、表、フォント、文字の関数）。
- 値の解釈を持つ所は、V の前に「1 つの関数＋切り替え（`XYZZY_CHAR_UNICODE`）」の形に直しておく。切り替えが 0 の間は今の動き（試験が通る）、1 の側は新しい動き（新しい試験で確かめられる所は確かめる）。V は切り替えを 1 にして、出てきた不具合を直す。
- 新しいファイルだけで作れるもの（変換器、uprops の表）は、最初から並列に作る。
- `/utf-8` への切り替え（判断 9 の残り）と、manifest の activeCodePage=UTF-8 は V で入れる。

### 10.2 片の一覧

段 1（16bit のまま。並列）:

| 片 | 中身 | 触るファイル | 規模 | 依存 | 完了の基準と試験 |
|---|---|---|---|---|---|
| S4-k キー | 3 章 | 3.5 の表 | M | S4-d | 3.7 |
| S4-c ファイル | W 版のファイルの層（長いパス、8.3 の削除、GetFileSizeEx）。Char → UTF-16 の変換は 1 つの関数にまとめる。10.4 の F（ファイル名）の 165 箇所が対象。DnD.cc・com.cc・filer.cc の MultiByteToWideChar (CP_ACP)・WideCharToMultiByte (CP_OEMCP) のパスの変換（不具合 F8 を含む）もここで扱う | pathname.cc・pathname.h、fileio.cc、mman.cc・mman.h、stream.cc の create_file_stream（fopen）、filer.cc のファイル操作、DnD.cc、com.cc、glob.cc、ほか S4-c の役が決める | M〜L | S4-d | 合意の S4-c の基準。260 文字を超えるパス、4GB を超えるファイルの大きさ、既存の試験 |
| P1 変換器 | 4.2 の変換器（バイト ⇔ コードポイントの列）。生成器と表。試験用の si:%codec-decode・si:%codec-encode（コードポイントの整数の列を返す。内部の値に依らない） | 新規: codec.h、codec*.cc、gen-codec.cc、src/unicode/CP932.TXT。projects/gen-src1.vcxproj・xyzzy.vcxproj、gen-syms.cc（関数の登録を足すだけ） | L | なし | cp932 の全 1 バイト・2 バイトが OS（MB_ERR_INVALID_CHARS）と一致。5.3 の表の各場合の往復。重複符号の覚え（判断 A）。ほかの符号は今の表（i2w を通した結果）と一致するか、差を記録 |
| P2 属性の表 | 7.2 の uprops。試験用の si:%uprop-width など | 新規: uprops.h・uprops.cc、gen-uprops.cc、src/unicode/ucd/。projects、gen-syms.cc | M | なし | 代表の文字（ASCII、かな、漢字、ハングル、曖昧、絵文字、結合文字）の幅と群が UCD どおり |

- gen-syms.cc と projects の .vcxproj は P1・P2・S4-c が足すだけの変更をする。取り込みのときに合わせる。
- 8.2 の Lisp の直し（charname.l など）は、16bit のままでは今と同じ名前の集まりを保てない（charname.l は内部の値を全部回して、同じ Unicode に当たる別の文字集合の文字にも名前を付けている）。だから段 1 には置かず、R9 の切り替え 1 の側で行う。
- stream.cc は S4-k（st_keyboard）と S4-c（create_file_stream）が別の関数を触る。filer.cc も同じ。

段 2（W。1 人で順に）:

| 片 | 中身 | 触るファイル | 規模 | 依存 | 完了の基準と試験 |
|---|---|---|---|---|---|
| W 幅 | `typedef u_int32_t Char`。CHAR_LIMIT は 0x10000 のまま。`Char (-1)` → CHAR_NONE（69 箇所）。16bit の値を持つ表（ucs2tab.h など）は u_int16_t の型にして、読むところで広げる。`sizeof (Char)` の 60 箇所を見直す（ダンプ、DOC の magic、レジストリ、StrBuf、kbd）。Char を wchar_t や char の領域として使う所を直す（sockssl.cc の pwszServerName、kbd.cc の reconvert、lprint.cc の Fmessage_box・putmsg。10.4）。regex の NBITS（`sizeof (Char) * 8`）の計算が 32 でも正しいか確かめる。DUMP_FORMAT を上げる | cdecl.h、charset.h、ucs2.cc、ucs2tab.h、encoding.cc、insdel.cc、char.cc、data.cc、doc.cc、environ.cc、regex.cc、sockssl.cc、kbd.cc、lprint.cc、ほか機械的に | L | S4-k、S4-c | 全件（637 件＋S4-k・S4-c の増分）が通る。起動して cp932・UTF-8・EUC のファイルを開いて保存し、バイト列が同じ（S4-0 の試験）。メモリの使用量を記録する |

段 3（32bit、値は今のまま。並列。各片は「1 つの関数＋切り替え」の形にする）:

| 片 | 中身 | 触るファイル | 規模 | 依存 | 完了の基準と試験 |
|---|---|---|---|---|---|
| R1 正規表現と検索 | 7.4。charclass を範囲の並びに。fold256。RE_WORD_LIMIT | regex.cc・regex.h、search.cc | M | W | 既存の正規表現と検索の試験。範囲の大きい文字クラス（`[ぁ-ん]`、`[^a-z]`）と、BMP の外の値（切り替え 1 の側で）の試験を足す |
| R2 構文表 | 7.3。syntax_of と疎な上書き。ダンプの書き・読み | syntax.cc・syntax.h、data.cc（syntax_table の所だけ） | M | W | 既存の全件。非 ASCII に set-syntax-* を設定できる試験 |
| R3 画面と印刷 | 6.2。glyph_t を 64bit に。文字 → 枠・幅・UTF-16 を 1 つの関数に。ExtTextOutW に一本化。print の pixel を疎に | disp.cc、Window.h・Window.cc、font.cc・font.h、print.cc・print.h、statarea.cc、kbd.cc（kbd_encoding_font だけ） | L | W、P2 | 既存の全件。画面を撮って比べる（cp932 のファイル、ISO-8859 のファイル、GB・KSC・Big5 のファイル、罫線、半角カナ、制御文字の表示が今と同じ）。印刷のプレビューも |
| R4 ストリームと load | 4.4・4.5。ファイルとソケットのストリームに変換器を持たせる（切り替え 0 では「今の生の SJIS」を 1 つの変換器として包む）。load の判定 | stream.cc・stream.h、lread.cc（load_file）、lisp/misc.l・evalmacs.l の mc- の関数 | M | W、P1 | 既存の全件。UTF-8（BOM あり・なし・印あり）と cp932 の .l を load できる。:binary で 0〜255 の全バイトが往復する |
| R5 xyzzy のデータファイル | 4.3 の履歴・resume・session・キーボードマクロのファイル・abbrev を UTF-8 と版の印で書く。9.4 の旧い形の読み（切り替え 1 の側）。.lc の版の印と load の検査（9.2） | lisp/history.l、session.l、kbdmacro.l、abbrev.l、compile.l、lread.cc（.lc の 1 行目の検査だけ。R4 と同じファイルなので R4 の後） | M | R4 | 旧い履歴（今の形で書いたもの）を読んで、同じ値になる。新しい形で書いて読み戻す |
| R6 ini | 4.3。conf.cc に ini の読み書きを作る | conf.cc・conf.h | S | W、P1 | 今の cp932 の ini を読める。書いて読み戻す。フォントの名前（日本語）が往復する |
| R7 octet の関数 | 4.7 の「バイトの文字列」の規則（切り替え 1 の側）と、:encoding を足す | byte-stream.cc・byte-stream.h、encdec.cc、string.cc（Fsi_octet_length と make_string_simple だけ）、lisp/encdec.l（text-decode-region）、lisp/kanji.l（map-char-encoding-region） | S〜M | W、P1 | system-tests.l の HMAC・md5・octet-length が、直さずに通る。MIME のヘッダの復号（decode-mime-header）の試験を足す |
| R8 Win32 の境界 | 10.4。s2w・w2s を「ACP の変換器」1 つに通す（切り替え 0 は今の cp932、1 は UTF-8）。IME の再変換の MultiByteToWideChar (CP_ACP) を、Char から直接 UTF-16 を作る形に。gen-syms.cc の _ismbblead。xyzzycli との受け渡しを UTF-8 に決める（10.4） | string.cc（s2w・w2s の中身だけ）、kbd.cc（kbd_queue::reconvert）、gen-syms.cc（_ismbblead の所。生成器）、xyzzycli.cc、listen.cc（eval_xyzzysrv_param）、listen.h | M | W | 既存の全件。IME の再変換と、xyzzycli で日本語の名前のファイルを開くのを画面で確かめる |
| R9 文字の関数 | 8.1 の unicode-char・char-unicode・iso-*・kanji-char-p・kana-char-p・char-columns・graphic-char-p・map-to-half/full-width-*、kanjitab の作り直し。8.2 の Lisp の直し（charname.l は cnamedef.l の Unicode の値を回し、文字集合の前置きの名前〈`JISX0208_…`〉は変換器の can_encode で付ける。cmds.l の quote-char・what-cursor-position。encoding.l の insert-internal-char-table と map-* の関数） | char.cc、kanji.cc、move.cc（Fchar_columns・iso8859_word_char_p）、gen-ktab.cc、ces.cc（parse-char-encoding-*）、lisp/charname.l、lisp/cmds.l、lisp/encoding.l | M | W、P1、P2 | 切り替え 0 で既存の全件。切り替え 1 の側の試験（8.1 の表の各行、`#\HIRAGANA_LETTER_A` と `#\JISX0208_HIRAGANA_LETTER_A` が読める）を用意しておく |
| R10 キー入力 | WM_CHAR・WM_IME_CHAR・ImmGetCompositionString を UTF-16 で受ける経路に寄せる（切り替え 1 で） | kbd.cc（putc・puts の所）、toplev.cc（WM_CHAR・WM_PRIVATE_WCHAR・WM_IME_CHAR・IME の確定の所） | S〜M | W、S4-k | 画面で日本語の入力（IME）、BMP の外の文字（絵文字の入力）を確かめる |

- R4 と R5 は lread.cc を、R3 と R10 は kbd.cc を共有する。関数が違うが、それぞれ後の片が前の片の上で作る（R5 は R4 の後、R10 は R3 と並列でよいが取り込みで合わせる）。

段 4（V。1 人で）:

| 片 | 中身 | 触るファイル | 規模 | 依存 | 完了の基準と試験 |
|---|---|---|---|---|---|
| V 値 | `XYZZY_CHAR_UNICODE` を 1 に。encoding.cc の流れを変換器に置き換える。CHAR_LIMIT を 0x110000、char-code-limit。読み・印字の `\X{…}` と `#\x`。生のバイト。旧い形の読み替えの表。`/utf-8`。manifest に activeCodePage=UTF-8。charset.h の領域の定義を消す | cdecl.h、charset.h、encoding.cc・encoding.h、ucs2.cc、lread.cc・lprint.cc（エスケープの所）、init.cc、src/xyzzy.manifest、projects（/utf-8） | L | 段 3 の全部 | 全件が通る（V で変わる期待値は、試験を直して内訳を報告）。S4-0 の試験（cp932 のバイト列が同じ）。非 cp932 の文字（例: 中文、한글、絵文字）の名前のファイルを開いて保存できる（S4-c の層と合わせて）。画面を撮って確かめる |

段 5（V の後。並列）:

| 片 | 中身 | 触るファイル | 規模 | 依存 | 完了の基準と試験 |
|---|---|---|---|---|---|
| C1 古い枝の削除 | 切り替え 0 の側、gen-ucs2tab・gen-fontrange・gen-char-width・gen-jisx0212-*・gen-lucida-width・gen-ibmext・gen-utf2sjis・gen-iso2022state、ucs2tab.h（読み替えの表を除く）、fontrange.h など | 各片のファイル（片ごとに分けて並列にできる） | M | V | 全件。ビルドが OS の符号ページとフォントに依らない（生成器が src/unicode/ だけを読む） |
| C2 Lisp のソースを UTF-8 へ | lisp/・unittest/・samples/ を UTF-8 にし、`Encoding: utf-8` を書く | lisp/、unittest/、samples/（機械的） | S | V | 全件。bytecompile して起動できる |
| C3 フォントの枠 | FALLBACK の枠、既定の名前の見直し、ini の [Font] | font.cc・font.h、ChooseFont.cc、lisp/font.l | S | V | 画面を撮って確かめる（S7 と合わせてもよい） |
| C4 文書 | plan.md §4.10 の公開の変更、移行の案内（.lc の作り直し、`\X` の意味、キーボードマクロ、char-code） | docs/ | S | V | — |

### 10.3 順番と依存

```
S4-d ─┬─ S4-k ─┐
      ├─ S4-c ─┴─ W ─┬─ R1, R2, R8, R10 ──────────────┐
      │              ├─ R3 ·····(P2) ──────────────────┤
      ├─ P1 ·········├─ R4 ·····(P1) ─ R5 ─────────────┼─ V ─┬─ C1
      │              ├─ R6, R7 ·(P1) ──────────────────┤     ├─ C2
      └─ P2 ·········└─ R9 ·····(P1, P2) ──────────────┘     ├─ C3
                                                              └─ C4
```

- W は S4-k と S4-c の後。P1・P2 は W を待たない（新しいファイルだけ）。点線は「その片が P1・P2 を使う」の意味。
- 段 3 の片は W の後に並列。R5 は R4 の後。V は段 3 の全部の後。段 5 は V の後に並列。

- 各段の共通の完了の基準（plan.md §6 と合意）: install.bat → ダンプを書く起動 → run-tests.bat が通る。コミットして push。

### 10.4 Win32 に残る A 版（s2w・w2s）

- 今は CharacterSet NotSet で、Win32 の呼び出しは全部 A 版。s2w・w2s・make_string (const char *) は「cp932 のバイト ⇔ Char」。
- 呼び出しの分類（s2w・s2wl・a2w・w2s・w2sl・w2s_quote・make_string (const char *)・make_string_simple・pathname2cstr・make_path (const char *)・1 文字の i2w・MultiByteToWideChar・WideCharToMultiByte。432 箇所）:

| 分類 | 数 | 中身 | 扱う片 |
|---|---|---|---|
| W（Win32 の文字列） | 142 | レジストリ・環境（environ.cc 30）、IME・キー（kbd.cc 23）、ダイアログ、ファイラの表示、フォントの名前、メニュー、メッセージ | V で activeCodePage=UTF-8 により UTF-8 で通る。W 版へは S5・S7・S8 |
| F（ファイル名） | 165 | pathname2cstr 64、pathname.cc 45、com.cc 13、filer.cc 12、DnD.cc 11、archiver.cc 10、fileio.cc 9、process.cc 9、glob.cc 8 など | S4-c（W 版のファイルの層）。archiver.cc は S9 で消える |
| O（バイト） | 17 | chunk.cc 9（FFI の文字列）、encdec.cc 4（digest と hmac の鍵）、stream.cc 2（帯域外のデータ）、string.cc 1（octet-length）、kanji.cc 1（判定のためのバイト） | 4.7（R7）。FFI の文字列（chunk.cc）は W と同じく ACP の規則 |
| L（リテラルと表） | 38 | 記号の名前（data.cc の make_string_simple）、ces.cc の「自動判定」など、lread.cc の "LOADING"、msgdef.h の文（StrBuf::add 経由） | V の /utf-8 で UTF-8 のリテラルになる。activeCodePage=UTF-8 なら同じ変換でよい |
| X（その他） | 70 | 1 文字の i2w 55（画面・印刷・変換表・分類）、gen-ucs2tab.cc 7（生成器）、そのほか 8（GetProcAddress の名前〈狭い文字のまま〉、辞書のファイル〈edict.cc。cp932 のデータ〉、BinHex の中のファイル名など） | i2w は R3・R9・V で消える。辞書は V で cp932 の変換器で読む |

- 中身ではなく「Char が 16bit であること」に依っている所（W で直す）: kbd.cc の再変換（kbd_queue::reconvert）と lprint.cc の Fmessage_box・putmsg は、Char の領域を char * として w2s で書き込む。sockssl.cc は a2w で埋めた Char の配列を LPWSTR（pwszServerName）として渡している（Char が wchar_t と同じ 16bit だから動いている）。write-registry-literally と DOC は 9.1。
- `/utf-8` にすると、C++ の文字列リテラル（msgdef.h の 319 行など、非 ASCII を含むリテラルは 18 ファイル 419 行）は UTF-8 になる。一方、A 版の API の文字列は ACP（cp932）のまま。同じ s2w・w2s が両方に使われているので、どちらかが化ける。
- 推奨: V で manifest に `<activeCodePage>UTF-8</activeCodePage>` を入れる（Windows 10 1903 以降。Windows 11 が基準なので使える）。プロセスの ACP が UTF-8 になり、リテラルと A 版の文字列が同じ UTF-8 になる。s2w・w2s は UTF-8 の変換器 1 つでよい。A 版の残りも、cp932 に無い文字（ファイル名、ウィンドウの題など）を扱えるようになる。
- これは橋渡し。W 版への移行（決定 2）は S5（プロセス）、S7（画面）、S8（IME）で続ける。
- activeCodePage=UTF-8 で変わるもの（V で確かめる）:

| もの | 影響 | 対策 |
|---|---|---|
| xyzzy.ini（GetPrivateProfileStringA） | 古い cp932 の ini を UTF-8 として読んで化ける | R6 で自前の読み書きにする（V の前に） |
| CRT の _mbs*・_ismbblead（archiver.cc、gen-syms.cc） | UTF-8 の ACP では 2 バイト文字として扱わない | archiver.cc は S9 で消える。gen-syms.cc はビルドの道具なので /utf-8 のリテラルに合わせて直す（R8） |
| MultiByteToWideChar (CP_ACP)・WideCharToMultiByte (CP_OEMCP)（DnD.cc、com.cc、filer.cc、kbd.cc） | ACP が UTF-8 になる。OEMCP は 932 のまま | パスの所（DnD.cc、com.cc、filer.cc）は S4-c、IME の再変換（kbd.cc）は R8 で、Char から直接 UTF-16 を作る形にする |
| ANSI のウィンドウの WM_CHAR | UTF-8 のバイトが来るか、確かめていない | R10 で WM_PRIVATE_WCHAR（ToUnicode）と WM_IME_CHAR・ImmGetCompositionStringW の経路に寄せる。V で画面を撮って確かめる |
| 子プロセスのコマンド行・環境（CreateProcessA） | UTF-8 から UTF-16 に変換されて渡る（今より良くなる） | S5 で CreateProcessW にする |
| プロセスの出力の読み | `*default-process-encoding*`（sjis）で読む。ACP に依らない | 変えない |
| xyzzycli.exe → xyzzy.exe の受け渡し | xyzzycli は引数から作った Lisp の式を、共有メモリ（xyzzysrv_param の `char data[]`）に ACP（cp932）のバイトで書く。xyzzy は listen.cc の eval_xyzzysrv_param で `make_string (param->data)`（s2w）で読む。xyzzy だけ UTF-8 にすると、日本語の名前のファイルを xyzzycli から開けなくなる | R8 で受け渡しを UTF-8 に決める: xyzzycli は GetCommandLineW から UTF-8 を作って書き、xyzzy は UTF-8 として読む。listen.h の `xyzzysrv_name`（"xyzzy-server v2"）を v3 に上げ、版の違う組み合わせは繋がらないようにする |
| xyzzycli.exe・xyzzyenv.exe のそのほか（IsDBCSLeadByte で引数を区切る、xyzzycli.ini を A 版で読む） | 別の exe。manifest を入れなければ今のまま | S5 で扱う |

## 11. リスクと、この PC で確かめられないこと

| リスク | 内容 | 対策 |
|---|---|---|
| V が大きい | 値の解釈を変えると、全部が一度に変わる | 段 3 で「1 つの関数＋切り替え」にしておく。V は切り替えと、旧い流れの置き換えだけにする |
| 隠れた 16bit の前提 | `Char (-1)` の 69 箇所、`sizeof (Char)` の 60 箇所、`c >> 8` で 256 に畳む所（regex、search、disp）、16bit の表 | W を値の変更と分け、試験が今の値で通ることを確かめる。`c >> 8` は R1 と R3 で 1 つの関数にする |
| 重複符号の覚え | 同じ文字を 2 つの符号で書いたファイルは揃ってしまう | 判断 A。試験で揃い方を期待値にする |
| メモリ | 本文が 2 倍 | W で計測して記録する |
| 履歴などの旧い形 | 読み替えを忘れると、旧いマクロや文字が化ける | R5 で旧い形のファイルを試験に置く（今の版で書いた履歴を unittest/data に入れる） |
| 利用者の .lc と .l | 旧い .lc は読めない。`\X`・`#\x` の意味が変わる | load が .l に戻る。公開の変更として案内する（C4） |
| 曖昧な幅 | ギリシア文字などの幅が、読んだ符号によらず 1 つに決まる | 変数で選べる。既定は 2（cp932 の見え方を保つ） |
| フォント | GDI の ExtTextOutW はフォントの代替が弱い。絵文字は単色 | FALLBACK の枠。DirectWrite への移行は後の課題 |
| activeCodePage | 変わる所が広い | 10.4 の表を V で 1 つずつ確かめる。だめなら manifest から外し、s2w・w2s を「リテラル」と「A 版」に分ける案に戻る（R8 で分類を残しておく） |

この PC（日本語の Windows 11）で確かめられないこと:
- 英語版などの Windows（ACP が 932 でない）での動き。生成器を OS に依らなくし、activeCodePage=UTF-8 にすると、ACP の違いは効かなくなるはずだが、実機では確かめられない。
- IMM32 の経路の IME（この PC の日本語入力は TSF の Microsoft IME。BUG-0023 の注記）。activeCodePage=UTF-8 のときの IMM32 の A 版の動きも確かめられない。
- 中国語・韓国語の既定のフォント（MS Hei、GulimChe など）は、この PC に入っていない。GB・KSC・Big5 の表示は、入っている SimSun・Microsoft JhengHei・Malgun Gothic で確かめる。
- 中国語・韓国語の IME の入力。
- ARM64。

## 12. 調べて見つけた不具合（事実だけ）

| 番号 | 内容 | 場所 |
|---|---|---|
| F1 | `code-char` が 16bit に切り詰める。`(code-char #x10041)` が `#\A` になる（範囲のエラーにならない）。バイトコードの BCcode_char も同じ | char.cc Fcode_char、bytecode.cc |
| F2 | 構文表で、内部の値が 0x0100–0x7FFF の文字（ISO-8859、KSC、GB、JIS X 0212、UJP、ULATIN）は、正規表現の `\w` などと skip-syntax-spec で `table[cc >> 8]` を引くので、0x01〜0x7F の項目（制御文字や ASCII）の構文になる。ISO-8859-1 の é は junk になる | regex.h Regexp::char_syntax、search.cc skip_syntax_spec |
| F3 | フォントを作るときの「詰めが要るか」の判定 `ex[i][0].cx * 2 != ex[i][0].cx` は、幅が 0 でなければ常に真になる（`ex[i][1]` の書き違いと見られる） | font.cc FontSet::create |
| F4 | 先行バイト＋改行（例 0x81 0x0A）を 1 つの Char に組むので、その改行が行として数えられない（表示と行番号がずれる） | encoding.cc sjis_to_internal_stream、stream.cc readc_stream |
| F5 | si:environ が環境変数の名前を `make_string (*e, eq - *e)` で作る。この関数の 2 番目の引数は Char の数なのに、バイトの数を渡している。名前に 2 バイト文字があると、`=` の後ろまで名前に入る | environ.cc Fsi_environ |
| F6 | socket-stream-send-oob-data が `alloca (w2sl (string))` の領域に w2s で書く。w2s は終わりの NUL も書くので 1 バイトはみ出す | stream.cc Fsocket_stream_send_oob_data |
| F7 | StrBuf::make_string と make_substring で、長さが 0 のとき `make_string_simple ("", 0)` の結果を return していない（作った文字列を捨て、長さ 0 の領域を xmalloc して続ける） | StrBuf.cc |
| F8 | ドロップしたファイルのパスとショートカットの名前を WideCharToMultiByte (CP_OEMCP) で変えている（ほかは CP_ACP。結果は A 版のファイルの API に渡る）。日本語の Windows では OEM と ANSI が同じ 932 なので表に出ない | DnD.cc（571・648・689 行）、com.cc（170・172 行） |
| F9 | IME の再変換で w2s (lisp) が返した領域を解放していない | kbd.cc kbd_queue::documentfeed（content・before。BUG-0049。reconvert の隣の関数） |

## 13. 判断（師匠に渡すもの）

推奨のまま進めるなら「推奨で」と言ってください。どちらも、推奨で作り始めて、後で覆しても作り直しが小さい形にしてある。

| # | 項目 | 推奨 | 別案と影響 |
|---|---|---|---|
| A | cp932 の重複符号（398 個）の往復 | バッファごとに「どの符号で読んだか」を覚えて、同じ符号で書く。完了の基準「開いて保存し、バイト列が同じ」を、同じ文字を 2 つの符号で書いたファイル以外で満たす | 覚えを作らず、Windows と同じ正規化を受け入れる（NEC 選定 IBM 拡張は IBM 拡張に、NEC の 13 区の 9 個は JIS に、など）。基準を「398 個は正準形になる」と定義し直す。作りは小さくなるが、古いファイルを開いて保存すると、その文字のバイトが変わる |
| B | 東アジアの幅が曖昧な文字（ギリシア文字、キリル文字、罫線、○ など）の既定の幅 | 2（cp932 のファイルの見え方を今と同じに保つ。変数で 1 にできる） | 1（ISO-8859 のギリシア語・ロシア語のファイルが今と同じに見える。日本語の罫線や ○ が半分の幅になり、表が崩れる） |
