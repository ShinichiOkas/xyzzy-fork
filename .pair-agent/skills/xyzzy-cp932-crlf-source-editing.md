---
name: xyzzy の src/ は UTF-8、lisp/unittest/samples の .l は cp932、作業ツリーは CRLF。cp932 と UTF-8 の変換に msys の iconv を使わない
description: src/*.cc・*.h・*.rc は UTF-8（BOM なし）、lisp/unittest/samples の .l は cp932。作業ツリーは CRLF（core.autocrlf=true）。Git Bash の sed -i は CR を落とす。msys の iconv の CP932 は 0x817C を U+2212 に写すので Windows の cp932 とずれる。変換は Windows の表で行い、exe と生成物で検める
type: domain
maturity: forming
proposed_by: ai
created_at: 2026-11-22
updated_at: 2026-11-22
provenance: xyzzy-fork S4（OLE Automation 削除）で msys の sed が CR を落としたのを自分で見つけて直した。S4-d（コミット 67da895）で src を UTF-8 にし、msys iconv の 0x817C のずれをビルドの C4566 と gen-ktab の 1 バイトのずれで見つけた
---

## 事実

- `src/` の C++ のソース（`.cc`・`.h`・`.rc`）は UTF-8（BOM なし）。S4-d（67da895）で cp932 から変えた
- ビルドは全 vcxproj で `/source-charset:utf-8 /execution-charset:.932`。実行時の narrow の文字列は cp932 のまま。`/utf-8` への切り替えは、内部の文字を Unicode にする段階 4b で行う予定
- `xyzzy.rc`・`xyzzycli.rc` は `#pragma code_page(65001)`
- `lisp/`・`unittest/`・`samples/` の `.l` は cp932・CRLF のまま（xyzzy 自身が読むため）。段階 4a で load の符号の判定を決めるまで変えない
- 生成器（gen-syms の `putq`、gen-msg の `print_quote`）は非 ASCII を 8 進のエスケープで出す。生成物（`src/gen/`）は ASCII
- 作業ツリーは CRLF（`core.autocrlf=true`）。リポジトリ内は LF なので、CR が落ちても `git diff` には出ない（作業ツリーだけが LF 混じりになる）
- Git Bash（msys）の `sed -i` は CR を落とす。実例: S4 で `src/lprint.cc` などを `sed -i` で編集し、作業ツリーの CRLF が崩れていたのを後から見つけた
- **msys の iconv の CP932 は Windows の cp932 と表が違う。** 0x817C（全角のマイナス −）を U+2212 に写す。Windows の cp932（コンパイラの `/execution-charset:.932`、`WideCharToMultiByte`、.NET の `Encoding` 932）は 0x817C ↔ U+FF0D。実例: iconv で変換したソースをビルドすると C4566（現在のコードページで表示できない）が出て文字が `?` になり、`src/gen-ktab.cc` の半角・全角の表が 1 バイトずれた。iconv 同士の往復では元に戻るので、往復の検査では見つからない

## 規則

- ペア固有の confirmed `no-destructive-shell-edits-use-edit-tool` が先。Edit ツールで足りるなら Edit を使う
- `src/` は UTF-8 なので、Edit ツールで普通に編集できる（日本語のコメントもそのまま書ける）。編集の後は BOM が付いていないことと CRLF のままであることを見る
- `lisp/`・`unittest/`・`samples/` の `.l`（cp932）を Edit ツールで扱ったときにバイトが保たれるかは、確かめていない（確かめたら、この節を書き換える）
- **cp932 と UTF-8 の変換に msys（Git Bash）の iconv を使わない。** 変換は Windows の表で行う
  - Windows PowerShell 5.1 の `[System.Text.Encoding]::GetEncoding(932)` を使う
  - pwsh 7 では `CodePagesEncodingProvider` の登録が要る。5.1 では登録が要らず、登録の行は型が無くて失敗する
  - 変換のスクリプトは「読む符号・書く符号・BOM なし・CRLF を保つ」を明示する
- 符号を変える変更では、変換の前の `xyzzy.exe` と生成物（`src/gen/`）を取っておき、変換の後に次の 4 つを確かめる（S4-d で msys iconv の問題を見つけた形）
  1. 生成物をバイトで比べる
  2. exe の `.rsrc` の節をバイトで比べる
  3. exe の中の cp932 の日本語の文字列の集合を比べる
  4. ビルドのログで C4566・C4828・C4819 が 0 件
- シェルで `.l` などを一括して編集するときは、次を守る:
  - `export LC_ALL=C` を付ける
  - `sed -b -i`（バイナリ扱い。CR を落とさない）か `perl -i -pe` を使う。行の末尾の一致は `\r$` まで書く（例: `s/)\r$/))\r/`）
  - cp932 のファイルに日本語を含む行を書き込むときは UTF-8 のまま流さない。Windows の表でバイトにしてから渡す（msys iconv は上の理由で使わない。ASCII の行だけなら問題ない）
  - perl の正規表現にバックスラッシュを含むパス（例 `..\src\x`）を入れるときは、`\x5c` で書く。Windows の bash を経由すると `\\` が崩れる（[[bash-tool-heredoc-may-collapse-backslashes]] と同じ系統）
- 編集の後に必ず検める:
  - 単独の LF（CRLF の一部でない）が増えていないか。`git ls-files --eol <file>` の `w/` が `crlf` のままか
  - 文字化けしていないか（日本語を含むコメントの行を数行見る）
  - 作業ツリーの改行の形ではなく、コミットされるバイトを確かめる場面は [[check-line-endings-in-the-committed-bytes-not-the-working-tree]]

## 成熟度

forming のまま。S4-d で msys iconv の落とし穴と検査の仕方が実測で確かめられたが、師匠の原文の宣言ではなく AI の観測なので confirmed にしない。師匠が宣言・叱責したら confirmed に上げる。

## 取り消し

AI の観測（S4・S4-d）。師匠の原文の裏付けは無い。取り消すときはこのファイルを消す。
