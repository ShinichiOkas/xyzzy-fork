;;; -*- mode: lisp; package: user -*-
;;;
;;; Tests for the Unicode property tables (uprops, src/uprops.cc).
;;; The tables are built from the UCD 18.0.0 files in src/unicode/ucd/.
;;; Characters are given as integer code points, not as Lisp characters,
;;; because the Lisp characters do not hold Unicode code points yet.

(defun uprops-info (c)
  "(category east-asian-width script width width-with-1 font-group font-group-with-1 word graphic)"
  (list (si:uprops-category c)
        (si:uprops-east-asian-width c)
        (si:uprops-script c)
        (si:uprops-width c)
        (si:uprops-width c 1)
        (si:uprops-font-group c)
        (si:uprops-font-group c 1)
        (si:uprops-word-char-p c)
        (si:uprops-graphic-p c)))

(defun uprops-case (c)
  "(upcase downcase titlecase casefold)"
  (list (si:uprops-upcase c)
        (si:uprops-downcase c)
        (si:uprops-titlecase c)
        (si:uprops-casefold c)))

(deftest uprops-version ()
  (si:uprops-version)
  => "18.0.0"
  (integerp (si:uprops-table-size))
  => t
  (< 10000 (si:uprops-table-size) 1000000)
  => t)

;;; ASCII

(deftest uprops-ascii ()
  (uprops-info #x41)
  => ("Lu" "Na" "Latin" 1 1 :ascii :ascii t t)
  (uprops-info #x7a)
  => ("Ll" "Na" "Latin" 1 1 :ascii :ascii t t)
  (uprops-info #x30)
  => ("Nd" "Na" "Common" 1 1 :ascii :ascii t t)
  (uprops-info #x20)
  => ("Zs" "Na" "Common" 1 1 :ascii :ascii nil t)
  (uprops-info #x5f)
  => ("Pc" "Na" "Common" 1 1 :ascii :ascii nil t))

(deftest uprops-control ()
  (uprops-info #x00)
  => ("Cc" "N" "Common" 0 0 :ascii :ascii nil nil)
  (uprops-info #x0a)
  => ("Cc" "N" "Common" 0 0 :ascii :ascii nil nil)
  (uprops-info #x7f)
  => ("Cc" "N" "Common" 0 0 :ascii :ascii nil nil)
  (uprops-info #x85)
  => ("Cc" "N" "Common" 0 0 :latin :latin nil nil))

;;; East Asian characters

(deftest uprops-kana ()
  (uprops-info #x3042)                  ; HIRAGANA LETTER A
  => ("Lo" "W" "Hiragana" 2 2 :jp :jp t t)
  (uprops-info #x30a2)                  ; KATAKANA LETTER A
  => ("Lo" "W" "Katakana" 2 2 :jp :jp t t)
  (uprops-info #xff76)                  ; HALFWIDTH KATAKANA LETTER KA
  => ("Lo" "H" "Katakana" 1 1 :jp :jp t t)
  (uprops-info #x3000)                  ; IDEOGRAPHIC SPACE
  => ("Zs" "F" "Common" 2 2 :jp :jp nil t)
  (uprops-info #x3001)                  ; IDEOGRAPHIC COMMA
  => ("Po" "W" "Common" 2 2 :jp :jp nil t)
  (uprops-info #x30fc)                  ; KATAKANA-HIRAGANA PROLONGED SOUND MARK (script Common)
  => ("Lm" "W" "Common" 2 2 :jp :jp t t)
  (uprops-info #x30fb)                  ; KATAKANA MIDDLE DOT
  => ("Po" "W" "Common" 2 2 :jp :jp nil t)
  (uprops-info #xff21)                  ; FULLWIDTH LATIN CAPITAL LETTER A
  => ("Lu" "F" "Latin" 2 2 :jp :jp t t))

(deftest uprops-han ()
  (uprops-info #x4e9c)                  ; CJK UNIFIED IDEOGRAPH-4E9C
  => ("Lo" "W" "Han" 2 2 :han :han t t)
  (uprops-info #x3400)                  ; extension A
  => ("Lo" "W" "Han" 2 2 :han :han t t)
  (uprops-info #x20000)                 ; extension B
  => ("Lo" "W" "Han" 2 2 :han :han t t)
  (uprops-info #x2f00)                  ; KANGXI RADICAL ONE
  => ("So" "W" "Han" 2 2 :han :han nil t))

(deftest uprops-hangul ()
  (uprops-info #xac00)                  ; HANGUL SYLLABLE GA
  => ("Lo" "W" "Hangul" 2 2 :hangul :hangul t t)
  (uprops-info #x1100)                  ; HANGUL CHOSEONG KIYEOK
  => ("Lo" "W" "Hangul" 2 2 :hangul :hangul t t)
  (uprops-info #xffa1)                  ; HALFWIDTH HANGUL LETTER KIYEOK
  => ("Lo" "H" "Hangul" 1 1 :hangul :hangul t t))

;;; Emoji, combining marks, format characters

(deftest uprops-emoji ()
  (uprops-info #x1f600)                 ; GRINNING FACE
  => ("So" "W" "Common" 2 2 :fallback :fallback nil t)
  (uprops-info #x1f1ef)                 ; REGIONAL INDICATOR SYMBOL LETTER J
  => ("So" "N" "Common" 1 1 :fallback :fallback nil t))

(deftest uprops-combining ()
  (uprops-info #x301)                   ; COMBINING ACUTE ACCENT
  => ("Mn" "A" "Inherited" 0 0 :latin :latin t t)
  (uprops-info #x3099)                  ; COMBINING KATAKANA-HIRAGANA VOICED SOUND MARK
  => ("Mn" "W" "Inherited" 0 0 :jp :jp t t)
  (uprops-info #x200b)                  ; ZERO WIDTH SPACE
  => ("Cf" "N" "Common" 0 0 :fallback :fallback nil nil)
  (uprops-info #x200d)                  ; ZERO WIDTH JOINER
  => ("Cf" "N" "Inherited" 0 0 :fallback :fallback nil nil)
  (uprops-info #xad)                    ; SOFT HYPHEN keeps one cell
  => ("Cf" "A" "Common" 1 1 :latin :latin nil nil))

;;; Characters of East Asian Width Ambiguous.  The width is 2 by default and
;;; they use the Han language font slot then; with 1 they use their own script.

(deftest uprops-ambiguous ()
  (uprops-info #x25cb)                  ; WHITE CIRCLE
  => ("So" "A" "Common" 2 1 :han :fallback nil t)
  (uprops-info #x3b1)                   ; GREEK SMALL LETTER ALPHA
  => ("Ll" "A" "Greek" 2 1 :han :greek t t)
  (uprops-info #x410)                   ; CYRILLIC CAPITAL LETTER A
  => ("Lu" "A" "Cyrillic" 2 1 :han :cyrillic t t)
  (uprops-info #x2500)                  ; BOX DRAWINGS LIGHT HORIZONTAL
  => ("So" "A" "Common" 2 1 :han :fallback nil t)
  (uprops-info #xe9)                    ; LATIN SMALL LETTER E WITH ACUTE
  => ("Ll" "A" "Latin" 2 1 :han :latin t t)
  (uprops-info #xa7)                    ; SECTION SIGN
  => ("Po" "A" "Common" 2 1 :han :latin nil t))

(deftest uprops-ambiguous-variable ()
  (si:uprops-width #x25cb)
  => 2
  (let ((ed:*east-asian-ambiguous-width* 1))
    (list (si:uprops-width #x25cb)
          (si:uprops-width #x3b1)
          (si:uprops-font-group #x3b1)
          (si:uprops-width #x3042)      ; not ambiguous
          (si:uprops-width #x41)))
  => (1 1 :greek 2 1)
  (let ((ed:*east-asian-ambiguous-width* 2))
    (list (si:uprops-width #x25cb)
          (si:uprops-font-group #x3b1)))
  => (2 :han)
  ;; an explicit argument wins over the variable
  (let ((ed:*east-asian-ambiguous-width* 1))
    (si:uprops-width #x25cb 2))
  => 2
  ed:*east-asian-ambiguous-width*
  => 2)

;;; Other scripts

(deftest uprops-other-scripts ()
  (uprops-info #x5d0)                   ; HEBREW LETTER ALEF
  => ("Lo" "N" "Hebrew" 1 1 :fallback :fallback t t)
  (uprops-info #xe01)                   ; THAI CHARACTER KO KAI
  => ("Lo" "N" "Thai" 1 1 :fallback :fallback t t)
  (uprops-info #x10d0)                  ; GEORGIAN LETTER AN
  => ("Ll" "N" "Georgian" 1 1 :georgian :georgian t t)
  (uprops-info #x254)                   ; LATIN SMALL LETTER OPEN O (IPA)
  => ("Ll" "N" "Latin" 1 1 :latin :latin t t)
  (uprops-info #x2b0)                   ; MODIFIER LETTER SMALL H
  => ("Lm" "N" "Latin" 1 1 :latin :latin t t))

;;; Private use, unassigned, surrogates, the last code point

(deftest uprops-special-ranges ()
  (uprops-info #xe000)                  ; first private use (East Asian Width A)
  => ("Co" "A" "Unknown" 2 1 :han :fallback nil t)
  (uprops-info #xf8ff)
  => ("Co" "A" "Unknown" 2 1 :han :fallback nil t)
  (uprops-info #x378)                   ; unassigned in the Greek block
  => ("Cn" "N" "Unknown" 1 1 :fallback :fallback nil nil)
  (uprops-info #xd800)                  ; surrogate
  => ("Cs" "N" "Unknown" 1 1 :fallback :fallback nil nil)
  (uprops-info #x10fffd)                ; last private use
  => ("Co" "A" "Unknown" 2 1 :han :fallback nil t)
  (uprops-info #x10ffff)                ; last code point: a noncharacter
  => ("Cn" "N" "Unknown" 1 1 :fallback :fallback nil nil)
  (uprops-info #xfffe)                  ; noncharacter
  => ("Cn" "N" "Unknown" 1 1 :fallback :fallback nil nil))

;;; Case

(deftest uprops-case-ascii ()
  (uprops-case #x41)
  => (#x41 #x61 #x41 #x61)
  (uprops-case #x61)
  => (#x41 #x61 #x41 #x61)
  (uprops-case #x30)
  => (#x30 #x30 #x30 #x30))

(deftest uprops-case-letters ()
  (uprops-case #xe9)                    ; e with acute
  => (#xc9 #xe9 #xc9 #xe9)
  (uprops-case #x3b1)                   ; alpha
  => (#x391 #x3b1 #x391 #x3b1)
  (uprops-case #x410)                   ; Cyrillic A
  => (#x410 #x430 #x410 #x430)
  (uprops-case #xff21)                  ; fullwidth A
  => (#xff21 #xff41 #xff21 #xff41)
  (uprops-case #x10400)                 ; DESERET CAPITAL LETTER LONG I: outside the BMP
  => (#x10400 #x10428 #x10400 #x10428)
  (uprops-case #x3042)                  ; no case
  => (#x3042 #x3042 #x3042 #x3042))

(deftest uprops-case-special ()
  ;; DZ with caron, titlecase form: the three forms map to each other
  (uprops-case #x1c5)
  => (#x1c4 #x1c6 #x1c5 #x1c6)
  ;; SHARP S has no simple uppercase; CAPITAL SHARP S folds to it (status S)
  (uprops-case #xdf)
  => (#xdf #xdf #xdf #xdf)
  (uprops-case #x1e9e)
  => (#x1e9e #xdf #x1e9e #xdf)
  ;; I WITH DOT ABOVE: simple lowercase is i, but it has no simple folding
  (uprops-case #x130)
  => (#x130 #x69 #x130 #x130))

;;; Argument checks

(deftest uprops-argument-errors ()
  (handler-case (si:uprops-category #x110000)
    (error (c) :error))
  => :error
  (handler-case (si:uprops-category -1)
    (error (c) :error))
  => :error
  (handler-case (si:uprops-width 65 3)
    (error (c) :error))
  => :error
  (handler-case (si:uprops-category "a")
    (error (c) :error))
  => :error)

;;; Whole table sweeps

(deftest uprops-sweep-bmp ()
  ;; the properties agree with each other for every BMP code point
  (let ((bad nil))
    (dotimes (c #x10000)
      (let ((cat (si:uprops-category c))
            (ea (si:uprops-east-asian-width c))
            (w (si:uprops-width c 1))
            (w2 (si:uprops-width c 2))
            (g (si:uprops-graphic-p c)))
        (unless (and (member w '(0 1 2))
                     (member w2 '(0 1 2))
                     ;; Wide and Fullwidth are 2 cells, unless they take no cell
                     (or (not (member ea '("W" "F") :test #'string=))
                         (= w 0) (= w 2))
                     ;; a non-graphic character is a control, format,
                     ;; surrogate, unassigned or separator
                     (eq (not g)
                         (and (member cat '("Cc" "Cf" "Cs" "Cn" "Zl" "Zp")
                                      :test #'string=)
                              t))
                     ;; only Ambiguous characters change with the argument
                     (or (= w w2) (string= ea "A")))
          (push c bad))))
    (subseq (nreverse bad) 0 (min 5 (length bad))))
  => nil)

(deftest uprops-sweep-case ()
  ;; lower and upper case forms round trip for the common letters
  (let ((bad nil))
    (dotimes (c #x3000)
      (let ((cat (si:uprops-category c)))
        (when (string= cat "Lu")
          (let ((d (si:uprops-downcase c)))
            (when (and (/= c d)
                       (/= (si:uprops-upcase d) c)
                       ;; a few capital letters share one lowercase letter
                       (not (member c '(#x130 #x1e9e #x3f4 #x2126 #x212a #x212b))))
              (push c bad))))))
    (subseq (nreverse bad) 0 (min 5 (length bad))))
  => nil)
