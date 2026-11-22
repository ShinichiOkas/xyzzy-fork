# BUG-0063 試験の時間切れで `kill-process` を呼んでも、子の xyzzy の OS プロセスが残る

- 状態: 未修正（2026-11-22。見つけたのは段 4 の V の入り口で、BUG-0060 の直しの確かめの後）
- 関連: BUG-0060（この直しは「xyzzy の内側の記録を片付ける」ところまで。OS プロセスの実際の終了はこの票）

## 見たこと

- `unittest/child-xyzzy-tests.l` の `child-xyzzy-timeout-kills-the-child` は、`(sleep-for 1000)` の子を 3 秒の時間切れで止める試験。deskrun で 2 件・4.2 秒合格。
- 試験が合格した直後、タスクマネージャに子の xyzzy（PID 3480）が残っていた。コマンド行は `S:/.../xyzzy.exe -q -e "(progn (with-open-file ... (sleep-for 1000) ... kill-xyzzy))"`。
- 試験は `(= before (length (test-live-subprocesses)))` を t と判定した。`test-live-subprocesses` は xyzzy の `*test-children*` と `buffer-list` の `buffer-process` を数える関数（unittest/simple-test.l 1109）。unwind-protect が `*test-children*` から削り、`eval-in-another-xyzzy--delete-tmpbuf` が *Temp* バッファを消すので、xyzzy の内側からは「子は無い」ように見える。
- 実体は、`kill-test-subprocess` の `(kill-process proc)` は xyzzy の process-status を :run でなくするだけで、OS プロセスを本当に終わらせていない。TerminateProcess が子に効かないか、効くまでの間に xyzzy の記録だけが先に消える。

## 次の一手（調べの役の提案）

- `kill-process` の実装（process.cc）を見て、TerminateProcess の戻り値と GetExitCodeProcess のタイミングを確かめる。
- 試験の成功の条件に「OS プロセスが本当に居ない」を足す（たとえば `(not (os-process-alive-p pid))`。無ければ新しく作る。GetExitCodeProcess で STILL_ACTIVE を見るか、OpenProcess + WaitForSingleObject で 0 を確かめる）。
- 直したら BUG-0060 の直しの続きとして、kill-process の後の確かめを待ち時間に組み込む。
