# BUG-0031 lisp/foreign.l の define-c-struct の整列が先頭のメンバーだけで決まり、入れ子の構造体の整列も誤る（x64 の配置がずれる）

- 状態: 未修正（重大度: 中）
- 対象: x64 版（feature/x64-only）。x86 にも同じ規則の誤りがある
- 起票: 2026-11-10（BUG-0001 の型幅レビューで見つけた。独立レビューの確定指摘）

## 症状（コード上）
- lisp/foreign.l:281-282: 構造体の整列を、先頭のメンバーの整列だけで決める（unless struct-align）。
- lisp/foreign.l:299-300: 構造体型のメンバーは、min(その型のサイズ, pack) で整列する。入れ子の構造体（POINT・RECT など）は、自分の最大の整列（4）ではなく、サイズ（8・16）で整列される。
- lisp/foreign.l:307: 総サイズを、この整列で丸める。
- 具体例（x64、lisp/wip/winapi.l の PAINTSTRUCT）: rcPaint（RECT）は Win64 では 12 に置かれるが、Lisp では 16。以降の fRestore・fIncUpdate・rgbReserved が 4 バイトずれる（Lisp では 32・36・40、Win64 では 28・32・36）。
- 具体例（一般）: (char c)(char *p)(char d) は、Lisp の総サイズが 17。x64 の C の sizeof は 24。
- 試験: unittest/foreign-tests.l の c-size-of-struct（(int a)(double a)(char a)）は 20 を期待するが、x64 の C の配置は 24。試験は誤った値で通っている（コメントで注記済み）。

## 確認
- 独立レビュー（読み取り専用）で、上の経路が確定と判定された。PAINTSTRUCT の配置は、Lisp の計算を C の自然な整列の配置と比べて再計算した（スクラッチのスクリプト。xyzzy では実行していない）。
- BUG-0030 の経路（MSG.pt）も、この整列の誤りの影響を受ける。

## 直す方向（参考）
- 構造体の整列は、メンバーの整列の最大値にする。今の c-type-definition には整列が無いので、定義の段階で持たせる必要がある。

## 参照箇所
- lisp/foreign.l:281-282、299-300、307
- lisp/wip/winapi.l:122-136（MSG・PAINTSTRUCT）
- unittest/foreign-tests.l（c-size-of-struct）
