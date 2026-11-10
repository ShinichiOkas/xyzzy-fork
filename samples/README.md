# samples

遊びとデモの Lisp モジュール。標準では読み込まれない（起動時に自動で読み込まない）。使うときは次の手順で読み込む。

## 読み込み方

1. ファイルを読み込む。式で書くときは、Lisp の文字列の中の `\` を `\\` と書く。

   ```lisp
   (load-file "C:\\xyzzy\\samples\\hanoi.l")
   ```

   `M-x load-file` でファイルを選んで読み込むこともできる（`load-file` は対話的に呼べるコマンド）。
2. 読み込んだあと、コマンド名を実行する（例: `M-x hanoi`）。

`C:\xyzzy` は xyzzy を置いたフォルダに読み替える。

## 含まれるもの

| ファイル | コマンド |
|---|---|
| hanoi.l | `hanoi`（ハノイの塔） |
| life.l | `life`（ライフゲーム）、`life-mode` |
| ccurve.l | `c-curve`（C 曲線）、`dragon-curve`（ドラゴン曲線）。1 つのファイルを読めば両方使える |
| gomoku.l | `gomoku`（五目並べ） |
