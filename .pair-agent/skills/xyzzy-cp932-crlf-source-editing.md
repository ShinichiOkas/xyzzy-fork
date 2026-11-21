---
name: xyzzy の src/ は cp932・作業ツリーは CRLF。シェルで編集するなら sed -b か perl を使い、編集後に行末とバイトを確かめる
description: src/*.cc・*.h・lisp/*.l は cp932、作業ツリーは CRLF（core.autocrlf=true）。Git Bash の sed -i は CR を落とす。perl の正規表現のバックスラッシュは \x5c で書く
type: domain
maturity: forming
proposed_by: ai
created_at: 2026-11-22
provenance: xyzzy-fork S4（OLE Automation 削除）。編集中に msys の sed が CR を落としたのを自分で見つけて直した
---

## 事実

- `src/` の 51 ファイルは cp932（UTF-8 は 0。`docs/refactoring/inventory/inventory-build.md`）。`lisp/` の `.l` も cp932
- 作業ツリーは CRLF（`core.autocrlf=true`）。リポジトリ内は LF なので、CR が落ちても `git diff` には出ない（作業ツリーだけが LF 混じりになる）
- Git Bash（msys）の `sed -i` は CR を落とす。実例: S4 で `src/lprint.cc` などを `sed -i` で編集し、作業ツリーの CRLF が崩れていたのを後から見つけた

## 規則

- ペア固有の confirmed `no-destructive-shell-edits-use-edit-tool` が先。Edit ツールで足りるなら Edit を使う。cp932 のファイルを Edit ツールで扱ったときにバイトが保たれるかは、S4 では確かめていない（確かめたら、この節を書き換える）
- シェルで一括して編集するときは、次を守る:
  - `export LC_ALL=C` を付ける
  - `sed -b -i`（バイナリ扱い。CR を落とさない）か `perl -i -pe` を使う。行の末尾の一致は `\r$` まで書く（例: `s/)\r$/))\r/`）
  - cp932 の日本語を含む行を書き込むときは UTF-8 のまま流さない。`printf '...' | iconv -f utf-8 -t cp932` でバイトにしてから渡す（S4 の `Tvacant2` のコメントはこの形で入れた）
  - perl の正規表現にバックスラッシュを含むパス（例 `..\src\x`）を入れるときは、`\x5c` で書く。Windows の bash を経由すると `\\` が崩れる（[[bash-tool-heredoc-may-collapse-backslashes]] と同じ系統）
- 編集の後に必ず検める:
  - 単独の LF（CRLF の一部でない）が増えていないか。`git ls-files --eol <file>` の `w/` が `crlf` のままか
  - 文字化けしていないか（日本語を含むコメントの行を数行見る）
  - 作業ツリーの改行の形ではなく、コミットされるバイトを確かめる場面は [[check-line-endings-in-the-committed-bytes-not-the-working-tree]]

## 取り消し

AI の観測（1 スプリント）。師匠の原文の裏付けは無い。取り消すときはこのファイルを消す。
