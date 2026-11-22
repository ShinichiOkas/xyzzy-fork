;;; -*- mode: lisp; package: user -*-
;;;
;;; Tests for the functions on the value of a character (S4 piece R9):
;;; unicode-char, char-unicode, iso-char-code, iso-code-char, iso-char-charset,
;;; kanji-char-p, kana-char-p, graphic-char-p, char-columns,
;;; map-to-half-width-*, map-to-full-width-*, parse-char-encoding-*.
;;;
;;; Every function has the value switch XYZZY_CHAR_UNICODE (0: a Char is an
;;; internal code, 1: a Char is a Unicode code point).  The tests are in
;;; three groups.
;;;   1. They work on code points (si:unicode-*, si:iso-code-unicode) and
;;;      reach the 1 side of the switch whatever the switch is.
;;;   2. They ask for characters by their Unicode (unicode-char) and read
;;;      them by char-unicode, so that they pass in both sides.
;;;   3. #+xyzzy-char-unicode: only the 1 side of the switch can pass them
;;;      (the feature :xyzzy-char-unicode is set when the switch is 1).
;;;
;;; The file is ASCII: characters are written with numbers.

(in-package :user)

(defun char-fn-codes (string)
  (map 'list #'char-unicode string))

(defun char-fn-string (codes)
  (let ((s (make-string (length codes))))
    (dotimes (i (length codes))
      (setf (char s i) (unicode-char (nth i codes))))
    s))

;;; the switch

(deftest char-fn-switch-and-feature ()
  (eq (null (si:char-unicode-p))
      (null (member :xyzzy-char-unicode *features*)))
  => t)

;;; 1. code points -> charsets

(deftest char-fn-unicode-iso-code-1byte ()
  (si:unicode-iso-code #x41 :us-ascii)
  => #x41
  (si:unicode-iso-code #x3042 :us-ascii)
  => nil
  (si:unicode-iso-code #xff71 :jisx0201-kana)
  => #xb1
  (si:unicode-iso-code #xff9f :jisx0201-kana)
  => #xdf
  (si:unicode-iso-code #x3042 :jisx0201-kana)
  => nil
  (si:unicode-iso-code #xe9 :iso8859-1)
  => #xe9
  (si:unicode-iso-code #x41 :iso8859-1)
  => nil
  (si:unicode-iso-code #x3b1 :iso8859-7)
  => #xe1
  (si:unicode-iso-code #x431 :iso8859-5)
  => #xd1)

(deftest char-fn-unicode-iso-code-2byte ()
  (si:unicode-iso-code #x3042 :jisx0208)
  => #x2422
  (si:unicode-iso-code #x30a2 :jisx0208)
  => #x2522
  (si:unicode-iso-code #x3042 :jisx0212)
  => nil
  (si:unicode-iso-code #x4e2d :gb2312)
  => #x5650
  (si:unicode-iso-code #xd55c :ksc5601)
  => #x4751
  (si:unicode-iso-code #x4e2d :big5)
  => #xa4a4
  (si:unicode-iso-code #xd55c :jisx0208)
  => nil
  (si:unicode-iso-code #x1f600 :jisx0208)
  => nil
  (si:unicode-iso-code #x110085 :iso8859-1)
  => nil)

(deftest char-fn-iso-code-unicode ()
  (si:iso-code-unicode #x2422 :jisx0208)
  => #x3042
  (si:iso-code-unicode #x5650 :gb2312)
  => #x4e2d
  (si:iso-code-unicode #x4751 :ksc5601)
  => #xd55c
  (si:iso-code-unicode #xa4a4 :big5)
  => #x4e2d
  (si:iso-code-unicode #xb1 :jisx0201-kana)
  => #xff71
  (si:iso-code-unicode #x31 :jisx0201-kana)
  => #xff71
  (si:iso-code-unicode #xe9 :iso8859-1)
  => #xe9
  (si:iso-code-unicode #x41 :us-ascii)
  => #x41
  ;; out of the 94 x 94 form
  (si:iso-code-unicode #x2022 :jisx0208)
  => nil
  (si:iso-code-unicode #x7f21 :jisx0208)
  => nil)

(deftest char-fn-iso-code-roundtrip ()
  ;; a code point that a charset has comes back from the code of the charset
  (let ((r nil))
    (dolist (u '(#x41 #x3042 #x4e2d #x6f22 #xff71 #xe9 #x3b1 #x416 #xd55c
                 #x4e00 #x9fa0 #xff01 #x2252))
      (dolist (cs '(:us-ascii :jisx0201-kana :jisx0208 :jisx0212 :gb2312
                    :ksc5601 :big5 :iso8859-1 :iso8859-2 :iso8859-5
                    :iso8859-7))
        (let ((code (si:unicode-iso-code u cs)))
          (when (and code (not (eql (si:iso-code-unicode code cs) u)))
            (push (list u cs code) r)))))
    r)
  => nil)

(deftest char-fn-iso-code-order-of-charsets ()
  ;; JIS X 0208 has the hiragana and the common kanji; JIS X 0212 the
  ;; accented letters (the first one that has the character is the charset
  ;; of iso-char-code; this is in char-fn-unicode-iso-charset-1)
  (not (null (si:unicode-iso-code #xe9 :jisx0212)))
  => t
  (not (null (si:unicode-iso-code #xe9 :iso8859-1)))
  => t
  (not (null (si:unicode-iso-code #x4e2d :jisx0208)))
  => t)

(deftest char-fn-iso-code-vender ()
  ;; The IBM extension character U+7E8A (cp932 FA5C, NEC selected ED40)
  (not (null (si:unicode-iso-code #x7e8a :jisx0208 :necext)))
  => t
  (si:iso-code-unicode (si:unicode-iso-code #x7e8a :jisx0208 :necext)
                       :jisx0208 :necext)
  => #x7e8a
  (let ((c (si:unicode-iso-code #x7e8a :jisx0208 :ibmext)))
    (or (null c) (eql (si:iso-code-unicode c :jisx0208 :ibmext) #x7e8a)))
  => t
  (let ((c (si:unicode-iso-code #x7e8a :jisx0212 :ibmext)))
    (or (null c) (eql (si:iso-code-unicode c :jisx0212 :ibmext) #x7e8a)))
  => t
  (let ((c (si:unicode-iso-code #x7e8a :jisx0208 :osfjvc)))
    (or (null c) (eql (si:iso-code-unicode c :jisx0208 :osfjvc) #x7e8a)))
  => t
  (let ((c (si:unicode-iso-code #x7e8a :jisx0212 :osfjvc)))
    (or (null c) (eql (si:iso-code-unicode c :jisx0212 :osfjvc) #x7e8a)))
  => t)

(deftest char-fn-unicode-iso-code-bad-charset ()
  (handler-case (si:unicode-iso-code #x41 :no-such-charset)
    (error (c) :error))
  => :error)

;;; 1. code points -> properties

(deftest char-fn-unicode-char-columns ()
  (mapcar #'si:unicode-char-columns
          '(#x41 #x20 #x3042 #x30a2 #x4e2d #xff21 #xff71 #xac00 #x1f600
            #x301 #x1 #x1f #x7f #x85 #x9f #x110085 #x110ff))
  => (1 1 2 2 2 2 1 2 2 1 2 2 2 2 2 4 1)
  ;; East Asian Ambiguous follows *east-asian-ambiguous-width*
  (let ((ed:*east-asian-ambiguous-width* 2))
    (mapcar #'si:unicode-char-columns '(#x3b1 #x416 #x2500 #xa1 #xe9)))
  => (2 2 2 2 2)
  (let ((ed:*east-asian-ambiguous-width* 1))
    (mapcar #'si:unicode-char-columns '(#x3b1 #x416 #x2500 #xa1 #xe9)))
  => (1 1 1 1 1))

(deftest char-fn-unicode-kanji-char-p ()
  (mapcar #'si:unicode-kanji-char-p
          '(#x41 #x3042 #x4e2d #xff21 #xff71 #xac00 #x1 #x110085))
  => (nil t t t nil t nil nil)
  (let ((ed:*east-asian-ambiguous-width* 2))
    (mapcar #'si:unicode-kanji-char-p '(#x3b1 #x2500 #xe9)))
  => (t t t)
  (let ((ed:*east-asian-ambiguous-width* 1))
    (mapcar #'si:unicode-kanji-char-p '(#x3b1 #x2500 #xe9)))
  => (nil nil nil))

(deftest char-fn-unicode-kana-char-p ()
  (mapcar #'si:unicode-kana-char-p
          '(#x41 #x30a2 #xff60 #xff61 #xff71 #xff9f #xffa0))
  => (nil nil nil t t t nil))

(deftest char-fn-unicode-graphic-char-p ()
  (mapcar #'si:unicode-graphic-char-p
          '(#x41 #x20 #x3042 #xff71 #x1f600 #xe9 #xfeff #x2028 #xe000
            #x0 #x1f #x7f #x85 #x9f #xd800 #x378 #x110085 #x10ffff))
  => (t t t t t t t t t nil nil nil nil nil nil nil nil nil))

;;; 1. map-to-half-width, map-to-full-width (code points)

(deftest char-fn-unicode-map-to-half-width ()
  ;; ASCII
  (si:unicode-map-to-half-width '(#xff21 #xff11 #x3000 #xff01 #x3042) :ascii t)
  => (#x41 #x31 #x20 #x21 #x3042)
  (si:unicode-map-to-half-width '(#xff21 #x30a2) :katakana t)
  => (#xff21 #xff71)
  ;; kana
  (si:unicode-map-to-half-width '(#x30a2 #x3042 #x30ac #x30f4 #x30d1 #x30fc)
                                :katakana t)
  => (#xff71 #x3042 #xff76 #xff9e #xff73 #xff9e #xff8a #xff9f #xff70)
  (si:unicode-map-to-half-width '(#x30a2 #x3042 #x304c #x3071 #x30fc)
                                :hiragana t)
  => (#x30a2 #xff71 #xff76 #xff9e #xff8a #xff9f #xff70)
  (si:unicode-map-to-half-width '(#x30a2 #x3042 #x300c #x3002 #x309b)
                                :hiragana t :katakana t)
  => (#xff71 #xff71 #xff62 #xff61 #xff9e)
  ;; the punctuation of kana is not converted by :ascii alone
  (si:unicode-map-to-half-width '(#x300c #x3002 #x30fc) :ascii t)
  => (#x300c #x3002 #x30fc)
  ;; :ascii with kana
  (si:unicode-map-to-half-width '(#xff21 #x30a2 #x3042) :ascii t :katakana t)
  => (#x41 #xff71 #x3042)
  ;; nothing to do
  (si:unicode-map-to-half-width '(#xff21 #x30a2) :greek t :cyrillic t)
  => (#xff21 #x30a2)
  (si:unicode-map-to-half-width nil :ascii t)
  => nil)

(deftest char-fn-unicode-map-to-full-width ()
  (si:unicode-map-to-full-width '(#x41 #x31 #x20 #x21 #xff71) :ascii t)
  => (#xff21 #xff11 #x3000 #xff01 #xff71)
  (si:unicode-map-to-full-width '(#x41 #xff71 #xff76) :katakana t)
  => (#x41 #x30a2 #x30ab)
  (si:unicode-map-to-full-width '(#x41 #xff71 #xff76) :hiragana t)
  => (#x41 #x3042 #x304b)
  ;; voiced and semi voiced sound marks
  (si:unicode-map-to-full-width '(#xff76 #xff9e #xff8a #xff9f #xff73 #xff9e)
                                :katakana t)
  => (#x30ac #x30d1 #x30f4)
  (si:unicode-map-to-full-width '(#xff76 #xff9e #xff8a #xff9f)
                                :hiragana t)
  => (#x304c #x3071)
  ;; a hiragana has no voiced form of u; a mark that does not join stays
  (si:unicode-map-to-full-width '(#xff73 #xff9e #xff71 #xff9e) :hiragana t)
  => (#x3046 #x309b #x3042 #x309b)
  ;; the mark joins the character before it, one mark of a run
  (si:unicode-map-to-full-width '(#xff76 #xff9e #xff9e) :katakana t)
  => (#x30ac #x309b)
  ;; already full width characters are joined too
  (si:unicode-map-to-full-width '(#x304b #x309b #x30cf #x309c) :hiragana t)
  => (#x304c #x30cf #x309c)
  (si:unicode-map-to-full-width '(#x30cf #x309c) :katakana t)
  => (#x30d1)
  ;; both kana flags: error
  (handler-case (si:unicode-map-to-full-width '(#xff71) :hiragana t :katakana t)
    (error (c) :error))
  => :error
  ;; Greek and Cyrillic: one code point
  (si:unicode-map-to-full-width '(#x3b1 #x416) :greek t :cyrillic t)
  => (#x3b1 #x416))

(deftest char-fn-unicode-map-width-roundtrip ()
  (let ((l '(#x30a2 #x30ac #x30d1 #x30f4 #x30fc #x300c #x3002 #x309b #x309c
             #xff21 #x3000)))
    (si:unicode-map-to-full-width
     (si:unicode-map-to-half-width l :ascii t :katakana t)
     :ascii t :katakana t))
  => (#x30a2 #x30ac #x30d1 #x30f4 #x30fc #x300c #x3002 #x309b #x309c
      #xff21 #x3000))

;;; 2. characters by their Unicode: the same in both sides

(deftest char-fn-char-unicode ()
  (mapcar #'(lambda (u) (char-unicode (unicode-char u)))
          '(#x41 #x3042 #x30a2 #x4e2d #xff71 #xe9 #x3b1 #x416 #xd55c))
  => (#x41 #x3042 #x30a2 #x4e2d #xff71 #xe9 #x3b1 #x416 #xd55c)
  (characterp (unicode-char #x3042))
  => t
  (unicode-char #x110000)
  => nil
  (unicode-char -1)
  => nil)

(deftest char-fn-iso-char-code-charset ()
  (multiple-value-list (iso-char-code (unicode-char #x41)))
  => (#x41 :us-ascii)
  (multiple-value-list (iso-char-code (unicode-char #x3042)))
  => (#x2422 :jisx0208)
  (multiple-value-list (iso-char-code (unicode-char #xff71)))
  => (#xb1 :jisx0201-kana)
  (iso-char-charset (unicode-char #x4e2d))
  => :jisx0208
  (multiple-value-list (iso-char-code (unicode-char #xd55c)))
  => (#x4751 :ksc5601))

(deftest char-fn-iso-code-char ()
  (mapcar #'(lambda (x) (char-unicode (iso-code-char (car x) (cdr x))))
          '((#x2422 . :jisx0208) (#xb1 . :jisx0201-kana)
            (#x4751 . :ksc5601) (#x5650 . :gb2312) (#xe9 . :iso8859-1)
            (#x41 . :us-ascii) (#xa4a4 . :big5)))
  => (#x3042 #xff71 #xd55c #x4e2d #xe9 #x41 #x4e2d))

(deftest char-fn-kanji-kana-graphic-columns ()
  (list (kanji-char-p (unicode-char #x3042))
        (kanji-char-p (unicode-char #x4e2d))
        (kanji-char-p (unicode-char #x41))
        (kanji-char-p (unicode-char #xff71)))
  => (t t nil nil)
  (list (kana-char-p (unicode-char #xff71))
        (kana-char-p (unicode-char #x30a2))
        (kana-char-p (unicode-char #x41)))
  => (t nil nil)
  (list (graphic-char-p (unicode-char #x41))
        (graphic-char-p (unicode-char #x3042))
        (graphic-char-p (unicode-char #xff71))
        (graphic-char-p (code-char 1))
        (graphic-char-p (code-char 127)))
  => (t t t nil nil)
  (list (char-columns (unicode-char #x41))
        (char-columns (unicode-char #x3042))
        (char-columns (unicode-char #xff71))
        (char-columns (code-char 1))
        (char-columns (code-char 127)))
  => (1 2 1 2 2))

(deftest char-fn-map-to-width-string ()
  (char-fn-codes
   (map-to-half-width-string (char-fn-string '(#xff21 #xff11 #x3000 #x3042))
                             :ascii t))
  => (#x41 #x31 #x20 #x3042)
  (char-fn-codes
   (map-to-half-width-string (char-fn-string '(#x30a2 #x3042 #x30ac #x30d1 #x30fc))
                             :katakana t))
  => (#xff71 #x3042 #xff76 #xff9e #xff8a #xff9f #xff70)
  (char-fn-codes
   (map-to-half-width-string (char-fn-string '(#x30a2 #x3042 #x304c))
                             :hiragana t))
  => (#x30a2 #xff71 #xff76 #xff9e)
  (char-fn-codes
   (map-to-full-width-string (char-fn-string '(#x41 #x31 #x20 #x3042))
                             :ascii t))
  => (#xff21 #xff11 #x3000 #x3042)
  (char-fn-codes
   (map-to-full-width-string (char-fn-string '(#xff76 #xff9e #xff8a #xff9f #xff71))
                             :katakana t))
  => (#x30ac #x30d1 #x30a2)
  (char-fn-codes
   (map-to-full-width-string (char-fn-string '(#xff76 #xff9e #xff71))
                             :hiragana t))
  => (#x304c #x3042)
  (map-to-half-width-string "abc" :ascii t)
  => "abc"
  (map-to-half-width-string "abc")
  => "abc")

(deftest char-fn-map-to-width-region ()
  (with-temp-buffer
    (insert (char-fn-string '(#x41 #x30a2 #x30ac #x30d1 #x42)))
    (map-to-half-width-region (point-min) (point-max) :katakana t)
    (list (char-fn-codes (buffer-substring (point-min) (point-max)))
          (point)))
  => ((#x41 #xff71 #xff76 #xff9e #xff8a #xff9f #x42) 7)
  (with-temp-buffer
    (insert (char-fn-string '(#x41 #xff71 #xff76 #xff9e #xff8a #xff9f #x42)))
    (map-to-full-width-region (point-min) (point-max) :katakana t)
    (list (char-fn-codes (buffer-substring (point-min) (point-max)))
          (point)))
  => ((#x41 #x30a2 #x30ac #x30d1 #x42) 5)
  (with-temp-buffer
    (insert (char-fn-string '(#x41 #x31 #xff71)))
    (map-to-full-width-region (point-min) (point-max) :ascii t)
    (char-fn-codes (buffer-substring (point-min) (point-max))))
  => (#xff21 #xff11 #xff71))

;;; parse-char-encoding

(deftest char-fn-parse-char-encoding ()
  (parse-char-encoding-string t (char-fn-string '(#x41)))
  => (:us-ascii)
  (parse-char-encoding-string '(:us-ascii :jisx0208) (char-fn-string '(#x41 #x3042)))
  => (:us-ascii :jisx0208)
  (parse-char-encoding-string '(:us-ascii :jisx0208) (char-fn-string '(#x41 #x3042 #xd55c)))
  => 2
  (parse-char-encoding-string '(:us-ascii :jisx0201-kana :jisx0208)
                              (char-fn-string '(#x41 #xff71 #x3042)))
  => (:us-ascii :jisx0201-kana :jisx0208)
  (parse-char-encoding-string '(:us-ascii :ksc5601) (char-fn-string '(#xd55c #x41)))
  => (:us-ascii :ksc5601))

;;; 3. the 1 side only

#+xyzzy-char-unicode
(deftest char-fn-unicode-side-values ()
  ;; a char is the code point
  (char-code (unicode-char #x3042))
  => #x3042
  (char-code (unicode-char #xe9))
  => #xe9
  (char-code (unicode-char #xff71))
  => #xff71
  ;; beyond the BMP: one character
  (characterp (unicode-char #x1f600))
  => t
  (char-unicode (unicode-char #x1f600))
  => #x1f600
  (unicode-char #x10ffff)
  => #.(unicode-char #x10ffff)
  (unicode-char #x110000)
  => nil
  ;; the same charset in iso-char-code: the first one in the order
  (iso-char-charset (unicode-char #xe9))
  => :jisx0212
  (iso-char-charset (unicode-char #x3b1))
  => :jisx0208
  (iso-char-charset (unicode-char #x416))
  => :jisx0208
  (iso-char-charset (unicode-char #x1f600))
  => nil)

#+xyzzy-char-unicode
(deftest char-fn-unicode-side-kanji-columns ()
  (let ((ed:*east-asian-ambiguous-width* 1))
    (list (kanji-char-p (unicode-char #xe9))
          (char-columns (unicode-char #xe9))))
  => (nil 1)
  (let ((ed:*east-asian-ambiguous-width* 2))
    (list (kanji-char-p (unicode-char #xe9))
          (char-columns (unicode-char #xe9))))
  => (t 2)
  (kanji-char-p (unicode-char #x3042))
  => t
  (char-columns (unicode-char #x1f600))
  => 2
  (graphic-char-p (unicode-char #xe9))
  => t
  (graphic-char-p (unicode-char #xfeff))
  => t
  (extended-alphabet-char-p (unicode-char #xe9))
  => t
  (extended-alphabet-char-p (unicode-char #x3042))
  => nil)

#+xyzzy-char-unicode
(deftest char-fn-unicode-side-names ()
  ;; a name, and a name with the prefix of a charset
  (char-code (gethash "hiragana_letter_a" si:*character-name-hash-table*))
  => #x3042
  (char-code (gethash "JISX0208_hiragana_letter_a" si:*character-name-hash-table*))
  => #x3042
  (char-code (gethash "JISX0212_latin_small_letter_e_with_acute"
                      si:*character-name-hash-table*))
  => #xe9
  (char-code (gethash "ISO8859-1_latin_small_letter_e_with_acute"
                      si:*character-name-hash-table*))
  => #xe9)

;;; 0 side only: the values do not change

#-xyzzy-char-unicode
(deftest char-fn-internal-side-values ()
  (char-code (unicode-char #x41))
  => #x41
  ;; hiragana A is the cp932 code
  (char-code (unicode-char #x3042))
  => #x82a0
  (multiple-value-list (iso-char-code (unicode-char #xe9)))
  => (#xe9 :iso8859-1))
