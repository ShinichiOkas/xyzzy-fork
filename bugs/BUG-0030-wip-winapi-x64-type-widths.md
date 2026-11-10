# BUG-0030 lisp/wip/winapi.l の WPARAM・LPARAM・LRESULT と MSG が x64 で 4 バイトに写像されている（MSG は Win64 より小さく、GetMessage が超えて書く）

- 状態: 修正済み（2026-11-10、S1。lisp/wip を削除。決定 9）
- 対象: x64 版（feature/x64-only）
- 起票: 2026-11-10（BUG-0001 の型幅レビューで見つけた。独立レビューの確定指摘）

## 症状（コード上）
- lisp/wip/winapi.l:51-53: WPARAM は UINT（:uint32）、LPARAM と LRESULT は LONG（:int32）。Win64 では 8 バイト。
- lisp/wip/winapi.l:122-128: MSG の wParam と lParam が 4 バイトのため、Lisp 側の MSG は 32 バイト。Win64 の MSG は 48 バイト（wParam 16、lParam 24、time 32、pt 36）。
- lisp/wip/winapi.l:178-179: GetMessage は Win64 の 48 バイトを書くが、make-MSG が確保するのは 32 バイトなので、ヒープを 12〜16 バイト超えて書く。
- lisp/wip/winapi.l:181: DispatchMessage の戻り値が LONG（4 バイト）。LRESULT は Win64 では 8 バイト。
- 引数が切り詰められる例: SendMessage(hwnd, WM_SETTEXT, 0, 文字列チャンク) の LPARAM は、cast_to_long で下位 32 bit に切り詰められる（lisp/wip/winapi.l:251）。
- 戻り値が切り詰められる例: CB_GETITEMDATA のような、ポインタの値を返すメッセージ（BUG-0019 と関連）。
- 同じ型を使う wndproc の定義: lisp/wip/turtle.l:66-68、lisp/wip/fractal.l:30-32、lisp/wip/hellowin.l:37-38。ただし、init_c_callable が x64 で未対応のため、今は読み込み時に止まる。

## 確認
- 型の写像は lisp/foreign.l の定義（:uint32・:int32）と一致。MSG の配置は foreign.l の define-c-struct-expand で計算した（32 バイト）。
- Win64 の MSG の 48 バイトは Windows SDK の定義による（計測はしていない）。
- 独立レビュー（読み取り専用。実行していない）で、上の経路が確定と判定された。

## 関連
- BUG-0031（define-c-struct の整列）: PAINTSTRUCT と MSG の配置は、整列の誤りでも変わる。この票は BUG-0031 の後に直すのがよい。
- BUG-0028（size_t・ptrdiff_t・time_t の幅）。
- BUG-0001（FFI の x64 呼び出し側）。

## 参照箇所
- lisp/wip/winapi.l:43-54、122-128、174-181、251（SendMessage）
- lisp/wip/turtle.l:66-68、lisp/wip/fractal.l:30-32、lisp/wip/hellowin.l:37-38
- lisp/wip/hellowin.l:72-81（GetMessage の例。75-82 行はブロックコメント）
