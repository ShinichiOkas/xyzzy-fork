# BUG-0060 試験が起こした子の xyzzy が終わらないと、run-tests が 60 分止まる（1 本の共有ランナーを 1 時間ふさいだ）

- 状態: 修正（2026-11-22。unittest/simple-test.l に時間切れで kill-process、kill-xyzzy の前の掃除、試験ごとの見張り。unittest/child-xyzzy-tests.l に試験 2 件。deskrun で 2 件・4.2 秒合格を確かめた）
- 対象: unittest/simple-test.l（eval-in-another-xyzzy、write-batch-report-and-exit）、unittest/run-tests-helper.l、run-tests.bat

## 症状（見たこと）
- CI の run 45003342041（ブランチ worktree-agent-ad6156d271b435471、コミット d977e87）のジョブ test が 2026-11-22 09:03:33〜10:04:49 UTC（約 61 分）。手順 3（Build、ダンプの書き出し、全試験）が 10:04:39 に `Process completed with exit code 1`。ci-test.ps1 の「run-tests.bat timed out (60 min)」の上限で止まった（当時の上限は 60 分。今は 20 分）。
- アップロードされた ci-out/ は install.log と tests.log の 2 つ。tests.log は 0 バイト（報告は試験の最後にだけ書かれる）。
- このコミットの unittest/syntax-table-tests.l の `syntax-nonascii-survives-dump` は、`dump-xyzzy` で画像を書き、`(eval-in-another-xyzzy form :options "-image <画像> -q")` で 2 つ目の xyzzy を起こして画像を読ませる試験だった。R2 はこの試験を後で表の形（lisp/misc.l）に置き換え、今の木には無い。
- 子が終わらなかった理由は、この run のログからは分からない（R2 は「run-tests-helper.l のフックが子の引数を取った」と推測したが、確かめていない）。

## 再現（HEAD = feature/x64-only の d7501a1。画像は使わない）
- 試験: `(eval-in-another-xyzzy '(progn (sleep-for 1000) 1) :options "-q" :timeout 10)` と、その後ろにもう 1 件の試験。
- `ci/ci-test.ps1 -Tests unittest\zz-….l` を deskrun（別デスクトップ）で 300 秒。deskrun の時間切れ（exit 99）まで終わらず、tests.log は 0 バイトのまま、試験の親の xyzzy と子の xyzzy の両方が生き残った（PID を限って止めた）。子の時間切れは 10 秒のはずで、試験自体は 10 秒で失敗するのに、実行は終わらない。
- 同じ HEAD で、`dump-xyzzy` した画像を `-image <画像> -q` で子に読ませて `(list *var* (+ 1 2))` を評価させる試験は 0.7 秒で通る（`-image` の子そのものは動く）。

## コードで見えること
- eval-in-another-xyzzy の時間切れは `signal-process` を呼ぶ。Fsignal_process は NormalProcess の p_event を SetEvent するだけで、プロセスを終わらせない（終わらせるのは `kill-process` = TerminateProcess）。時間切れの後も子は生き、ed::process-list に残る。
- 試験が終わると write-batch-report-and-exit が `kill-xyzzy` を呼ぶ。Buffer::query_kill_xyzzy は query_kill_subprocesses を呼び、process-list が空でなければ `yes_or_no_p`（Msubprocesses_are_running）で確認の対話を開く。バッチには答える者がおらず、報告（tests.log）も書かれないまま止まる。
