# BUG-0023 kbd.cc: HKL を fixnum に変換して戻すとき、上位 32bit の扱いが未確認

- 状態: 要確認（この PC には問題になる形の HKL が登録されていない）
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-09（ポインタ幅の監査による）

## 症状（コード上）
- src/kbd.cc:1447 `make_fixnum (int (LONG_PTR (h[i])))` は、HKL の下位 32bit を符号付き `int` として fixnum にする。
- src/kbd.cc:1497 `HKL (LONG_PTR (fixnum_value (layout)))` は、符号拡張して HKL に戻す。
- src/kbd.cc:1511-1512 の `int (LONG_PTR (hkl))` も下位 32bit の切り取り。
- 上位 32bit が 0 で、下位 32bit の最上位ビットが 1 の HKL では、戻した値が元の値と一致しない（コード上の推論。未実測）。

## 実測
- 2026-11-09、この PC（64bit の PowerShell から GetKeyboardLayoutList を呼んだ）: 件数 1。値は 0x0000000004110411（上位 32bit は 0x00000000、下位 32bit の最上位ビットは 0）。下位の最上位ビットが 1 の HKL は、この PC では確認できていない。
- 他の配列（下位の最上位ビットが 1 になる IME など）では未確認。
- 確認の方法: 64bit のプロセスで GetKeyboardLayoutList を呼び、各 HKL の上位 32bit と下位 32bit を出力する（IME の配列を含む環境で）。

## 参照箇所
- src/kbd.cc:1417、1447、1497、1511-1512

## 追記（2026-11-10）
- この PC の入力方式は、日本語（0411）の TSF の 2 件（Get-WinUserLanguageList）。HKL は 0x04110411 で、上位 32bit は 0。HKLM の Keyboard Layouts の登録は 218 件あり、日本語の配列は 00000411 の 1 件だけ。最上位ビットが立つ KLID（8〜F で始まるもの）は 0 件。つまり、この PC には、問題になる形（下位 32bit の最上位ビットが 1）の HKL は登録されていない。確かめるには、IMM32 形式の IME（KLID が E0xx… で登録されるもの）を入れる必要がある。入れる場合はインストールになるので、承認が要る。
