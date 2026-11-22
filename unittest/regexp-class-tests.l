;;; -*- mode: lisp; package: user -*-
;;;
;;; Character classes in regexps, the search tables and skip-chars (S4 R1).
;;;
;;; BUG-0056: a negated class [^...] did not match a character whose high byte
;;; (code >> 8) was a character of the class.  The search tables (fastmap, the
;;; Boyer-Moore table) were indexed by the high byte, which collided with the
;;; ASCII slots.  The character class is now a list of ranges, and the 256-slot
;;; tables are indexed by fold256.
;;;
;;; Characters are made from numbers so that this file stays ASCII.  A test
;;; that needs a character outside the Lisp characters uses si:regexp-class-test
;;; and si:regexp-fold256, which take integers.
;;; Each deftest returns the list of the failed cases; nil is a pass.

(in-package :user)

(defmacro with-regexp-class-buffer (&body body)
  "Run BODY in a fresh buffer (the search functions use the current buffer's
syntax table and the buffer is the subject of the search)."
  `(save-excursion
     (let ((#1=#:b (create-new-buffer "*regexp-class*")))
       (unwind-protect
           (progn (set-buffer #1#) ,@body)
         (delete-buffer #1#)))))

(defun regexp-class-char (c)
  "C is an integer code or a character; the character."
  (if (integerp c) (code-char c) c))

(defun regexp-class-string-failures (pattern in-class-p)
  "The first codes (0 .. 65535) for which string-match of the one-character
string and (funcall IN-CLASS-P code) disagree."
  (let ((re (compile-regexp pattern))
        (r nil)
        (n 0))
    (with-regexp-class-buffer
      (block scan
        (dotimes (i 65536)
          (let ((c (code-char i)))
            (when c
              (let ((m (and (string-match re (string c)) t))
                    (w (and (funcall in-class-p i) t)))
                (unless (eq m w)
                  (push i r)
                  (when (> (incf n) 8)
                    (return-from scan)))))))))
    (nreverse r)))

(defun regexp-class-buffer-failures (pattern in-class-p)
  "Same as regexp-class-string-failures, but search a buffer that holds one
character per line with ^PATTERN$ (the search of the line start and the
fastmap of the buffer search)."
  (let ((re (compile-regexp (concatenate 'string "^" pattern "$")))
        (codes nil)
        (r nil)
        (n 0))
    (with-regexp-class-buffer
      (dotimes (i 65536)
        (let ((c (code-char i)))
          (when (and c (/= i 10) (/= i 13))
            (push i codes)
            (insert (string c) "\n"))))
      (setq codes (nreverse codes))
      ;; Scan forward once and compare the matched lines with the expectation.
      (goto-char (point-min))
      (let ((matched nil))
        (while (scan-buffer re :regexp t :no-dup t)
          (push (char-code (char-after (match-beginning 0))) matched)
          (goto-char (match-end 0)))
        (setq matched (nreverse matched))
        (let ((want (remove-if-not in-class-p codes)))
          (unless (equal matched want)
            (dolist (i (set-exclusive-or matched want))
              (when (< (incf n) 9)
                (push i r)))))))
    (sort r #'<)))

(defun regexp-class-range-pred (lo hi &optional negate)
  #'(lambda (i) (if negate (not (<= lo i hi)) (<= lo i hi))))

;;; BUG-0056: the four patterns of the ticket, every code 0 .. 65535.

(deftest regexp-class-negated-bug-0056 ()
  "The negated classes of BUG-0056 match every character outside the range."
  (list (regexp-class-string-failures "[^A]" (regexp-class-range-pred 65 65 t))
        (regexp-class-string-failures "[^a-z]" (regexp-class-range-pred 97 122 t))
        (regexp-class-string-failures "[^?-_]" (regexp-class-range-pred 63 95 t))
        (regexp-class-string-failures "[^ -~]" (regexp-class-range-pred 32 126 t)))
  => (nil nil nil nil))

(deftest regexp-class-positive-ascii ()
  "The positive classes with an ASCII range are not wrong for any code."
  (list (regexp-class-string-failures "[A]" (regexp-class-range-pred 65 65))
        (regexp-class-string-failures "[a-z]" (regexp-class-range-pred 97 122))
        (regexp-class-string-failures "[?-_]" (regexp-class-range-pred 63 95))
        (regexp-class-string-failures "[ -~]" (regexp-class-range-pred 32 126)))
  => (nil nil nil nil))

(deftest regexp-class-negated-bug-0056-simple-examples ()
  (let ((c (code-char #x4142)))
    (list (string-match "[^A]" (string c))
          (string-match "[^A]" (string (code-char #x4200)))
          (string-match "[^A]*" (concatenate 'string (string c) (string c) "A"))
          (match-end 0)
          (string-match "[^A]+$" (concatenate 'string "A" (string c) (string c)))
          (string-match "^[^A]" (string c))
          (string-match "[^A]" "A")
          (string-match "[^a-z]" "abc")))
  => (0 0 0 2 1 0 nil nil))

(deftest regexp-class-negated-buffer-search ()
  "The same through the buffer search (^X$ per line)."
  (list (regexp-class-buffer-failures "[^A]" (regexp-class-range-pred 65 65 t))
        (regexp-class-buffer-failures "[^a-z]" (regexp-class-range-pred 97 122 t))
        (regexp-class-buffer-failures "[^ -~]" (regexp-class-range-pred 32 126 t))
        (regexp-class-buffer-failures "[a-z]" (regexp-class-range-pred 97 122)))
  => (nil nil nil nil))

;;; Classes with a large range of the double byte characters.

(defun regexp-class-wide-char (kind)
  (ecase kind
    (:hira-a (unicode-char #x3041))
    (:hira-n (unicode-char #x3093))
    (:kanji-first (unicode-char #x4e00))
    (:kanji-last (unicode-char #x9fa0))
    (:fullwidth-a (unicode-char #xff21))
    (:kata-a (unicode-char #x30a1))))

(defun regexp-class-range-test (lo-kind hi-kind)
  "Failures of [LO-HI], [^LO-HI] and [LO-HI]+ against every code."
  (let* ((lo (regexp-class-wide-char lo-kind))
         (hi (regexp-class-wide-char hi-kind))
         (a (char-code lo))
         (b (char-code hi))
         (cls (concatenate 'string "[" (string lo) "-" (string hi) "]"))
         (ncls (concatenate 'string "[^" (string lo) "-" (string hi) "]")))
    (list (regexp-class-string-failures cls (regexp-class-range-pred a b))
          (regexp-class-string-failures ncls (regexp-class-range-pred a b t))
          (regexp-class-buffer-failures cls (regexp-class-range-pred a b)))))

(deftest regexp-class-wide-hiragana ()
  (regexp-class-range-test :hira-a :hira-n)
  => (nil nil nil))

(deftest regexp-class-wide-kanji ()
  (regexp-class-range-test :kanji-first :kanji-last)
  => (nil nil nil))

(deftest regexp-class-wide-mixed ()
  "A class with several ranges and single characters, positive and negated."
  (let* ((c1 (regexp-class-wide-char :hira-a))
         (c2 (regexp-class-wide-char :hira-n))
         (c3 (regexp-class-wide-char :kata-a))
         (c4 (regexp-class-wide-char :fullwidth-a))
         (cls (concatenate 'string "[a-c" (string c3) (string c1) "-" (string c2)
                           "x" (string c4) "0-9]"))
         (ncls (concatenate 'string "[^" (subseq cls 1)))
         (in #'(lambda (i)
                 (or (<= 97 i 99) (= i 120) (<= 48 i 57)
                     (= i (char-code c3)) (= i (char-code c4))
                     (<= (char-code c1) i (char-code c2))))))
    (list (regexp-class-string-failures cls in)
          (regexp-class-string-failures ncls #'(lambda (i) (not (funcall in i))))
          (regexp-class-buffer-failures cls in)))
  => (nil nil nil))

(deftest regexp-class-closure ()
  "A class under * + ? and {n,m}, the simple closure."
  (let* ((lo (regexp-class-wide-char :hira-a))
         (hi (regexp-class-wide-char :hira-n))
         (s (concatenate 'string "a" (string lo) (string hi) (string lo) "b"))
         (cls (concatenate 'string "[" (string lo) "-" (string hi) "]"))
         (ncls (concatenate 'string "[^" (string lo) "-" (string hi) "]")))
    (list (progn (string-match (concatenate 'string cls "+") s) (match-end 0))
          (progn (string-match (concatenate 'string ncls "*") s) (match-end 0))
          (progn (string-match (concatenate 'string "a" cls "\\{2\\}") s)
                 (match-end 0))
          (string-match (concatenate 'string "a" cls "\\{4\\}") s)
          (progn (string-match (concatenate 'string "\\(" ncls "\\)" cls "+") s)
                 (match-end 0))))
  => (4 1 3 nil 4))

(deftest regexp-class-case-fold ()
  "A class and a literal under case folding (ASCII only)."
  (let ((c (code-char #x4142)))
    (list (let ((re (compile-regexp "[a-c]" t))) (list (string-match re "xB")
                                                       (string-match re "xD")))
          (let ((re (compile-regexp "[^a-c]" t))) (list (string-match re "AB")
                                                        (string-match re (string c))
                                                        (string-match re "xA")))
          (let ((re (compile-regexp "[^A]" t))) (list (string-match re "a")
                                                      (string-match re (string c))))))
  => ((1 nil) (nil 0 1) (nil 0)))

(deftest regexp-class-backward-search ()
  "Backward regexp search, with a negated class and a high-byte character."
  (let ((c (code-char #x4142)))
    (with-regexp-class-buffer
      (insert "A" (string c) "AA")
      (list (progn (goto-char (point-max))
                   (and (scan-buffer "[^A]" :regexp t :reverse t) (point)))
            (progn (goto-char (point-max))
                   (and (scan-buffer "^[^A]" :regexp t :reverse t) (point)))
            (progn (goto-char (point-max))
                   (and (scan-buffer "[^a]" :regexp t :reverse t :case-fold t)
                        (point))))))
  => (1 nil 1))

;;; The Boyer-Moore search (scan-buffer without :regexp), forward and backward.

(defun regexp-class-scan (text pattern &rest keys)
  "The position of the match of PATTERN in TEXT (forward from 0), or nil."
  (with-regexp-class-buffer
    (insert text)
    (goto-char (point-min))
    (and (apply #'scan-buffer pattern keys) (point))))

(defun regexp-class-scan-back (text pattern &rest keys)
  (with-regexp-class-buffer
    (insert text)
    (goto-char (point-max))
    (and (apply #'scan-buffer pattern :reverse t keys) (point))))

(deftest regexp-class-bm-high-byte-collision ()
  "A character whose high byte is the ASCII letter 'A' is not 'A' for the
skip table of the search (the table is indexed by fold256)."
  (let* ((c (code-char #x4142))
         (c2 (code-char #x4101))
         (s1 (concatenate 'string "xxxx" (string c) "yy"))
         (s2 (concatenate 'string "AAAA" (string c2) "AA" (string c) "A")))
    (list (regexp-class-scan s1 (concatenate 'string "x" (string c)))
          (regexp-class-scan s1 (concatenate 'string (string c) "y"))
          (regexp-class-scan s2 (concatenate 'string "A" (string c)))
          (regexp-class-scan s2 (concatenate 'string (string c2) "AA"))
          (regexp-class-scan-back s2 (concatenate 'string "A" (string c)))
          (regexp-class-scan-back s2 (concatenate 'string (string c2) "A"))
          (regexp-class-scan s2 (concatenate 'string (string c) "B"))
          (regexp-class-scan "AAAAAAAA" (concatenate 'string "A" (string c)))
          (regexp-class-scan (string c) "A")
          (regexp-class-scan (string c) "B")))
  => (5 6 6 5 7 6 nil nil nil nil))

(deftest regexp-class-bm-ascii ()
  (list (regexp-class-scan "hello world" "world")
        (regexp-class-scan "hello world" "o w")
        (regexp-class-scan "hello world" "xyz")
        (regexp-class-scan "hello hello" "hello" :no-dup t)
        (regexp-class-scan-back "hello world hello" "hello")
        (regexp-class-scan "Hello WORLD" "world" :case-fold t)
        (regexp-class-scan "Hello WORLD" "world")
        (regexp-class-scan-back "ABC abc" "ABC" :case-fold t)
        (regexp-class-scan "abc" "abc" :tail t))
  => (11 7 nil 5 13 11 nil 5 3))

(deftest regexp-class-bm-wide ()
  "Search of double byte and single byte (half width kana) characters."
  (let* ((a (unicode-char #x3042))
         (i (unicode-char #x3044))
         (k (unicode-char #xff71))
         (text (concatenate 'string "xx" (string a) (string i) "yy" (string k) "zz")))
    (list (regexp-class-scan text (concatenate 'string (string a) (string i)))
          (regexp-class-scan text (string i))
          (regexp-class-scan text (string k))
          (regexp-class-scan text (concatenate 'string "y" (string k) "z"))
          (regexp-class-scan-back text (string a))
          (regexp-class-scan text (concatenate 'string (string i) (string a)))))
  => (5 4 9 11 3 nil))

(deftest regexp-class-bm-iso-8859 ()
  "Case folding applies to ASCII letters only; an ISO-8859 letter has to
match itself and nothing else."
  (let* ((e (iso-code-char #xe9 :iso8859-1))
         (e-upper (iso-code-char #xc9 :iso8859-1))
         (text (concatenate 'string "CAF" (string e) " caf" (string e))))
    (list (regexp-class-scan text (concatenate 'string "caf" (string e)))
          (regexp-class-scan text (concatenate 'string "caf" (string e)) :case-fold t)
          (regexp-class-scan text (concatenate 'string "CAF" (string e)) :case-fold t)
          (regexp-class-scan text (concatenate 'string "f" (string e-upper))
                             :case-fold t)
          (regexp-class-scan-back text (concatenate 'string "af" (string e))
                                  :case-fold t)))
  => (9 4 4 nil 8))

(deftest regexp-class-regexp-iso-8859 ()
  "The regexp search of ISO-8859 letters, with and without case folding."
  (let* ((e (iso-code-char #xe9 :iso8859-1))
         (a (iso-code-char #xe0 :iso8859-1))
         (text (concatenate 'string "CAF" (string e) " " (string a))))
    (with-regexp-class-buffer
      (insert text)
      (list (progn (goto-char (point-min))
                   (and (scan-buffer (string e) :regexp t :case-fold t) (point)))
            (progn (goto-char (point-min))
                   (and (scan-buffer (concatenate 'string "[" (string a) (string e) "]+")
                                     :regexp t :case-fold t :tail t)
                        (point)))
            (progn (goto-char (point-min))
                   (and (scan-buffer (concatenate 'string "[^" (string e) "]")
                                     :regexp t :tail t)
                        (point)))
            (progn (goto-char (point-min))
                   (and (scan-buffer "caf" :regexp t :case-fold t :tail t)
                        (point))))))
  => (4 5 1 3))

;;; skip-chars-forward / skip-chars-backward

(defun regexp-class-skip (text spec dir &optional (start nil))
  "Move over TEXT from the start (DIR 1) or the end (-1) with the skip-chars of
SPEC; (value . number of the characters moved)."
  (with-regexp-class-buffer
    (insert text)
    (let ((from (if (eq dir 1) (or start (point-min)) (or start (point-max)))))
      (goto-char from)
      (let ((v (if (eq dir 1)
                   (skip-chars-forward spec)
                 (skip-chars-backward spec))))
        (cons v (abs (- (point) from)))))))

(deftest regexp-class-skip-chars-ascii ()
  (list (regexp-class-skip "abcXYZ" "a-c" 1)
        (regexp-class-skip "abcXYZ" "^a-c" 1)
        (regexp-class-skip "abcXYZ" "X-Z" -1)
        (regexp-class-skip "abcXYZ" "^X-Z" -1)
        (regexp-class-skip "abcXYZ" "ab" 1)
        (regexp-class-skip "abcXYZ" "xyz" 1)
        (regexp-class-skip (format nil "ab~%cd") "a-d" 1)
        (regexp-class-skip (format nil "ab~%cd") "^\n" 1)
        (regexp-class-skip (format nil "ab~%cd") "a-d" -1))
  => ((t . 3) (nil . 0) (t . 3) (nil . 0) (t . 2) (nil . 0) (t . 2) (t . 2) (t . 2)))

(deftest regexp-class-skip-chars-high-byte ()
  "skip-chars with a character whose high byte is an ASCII letter (BUG-0056
for the skip table)."
  (let* ((c (code-char #x4142))
         (text (concatenate 'string "AA" (string c) "AA")))
    (list (regexp-class-skip text "A" 1)
          (regexp-class-skip text "^A" 1)
          (regexp-class-skip text (string c) 1 2)
          (regexp-class-skip text "A" -1)
          (regexp-class-skip text "^A" -1 2)
          (regexp-class-skip text "^B" 1)
          (regexp-class-skip text (concatenate 'string "A" (string c)) 1)))
  => ((t . 2) (nil . 0) (t . 1) (t . 2) (nil . 0) (t . 5) (t . 5)))

(deftest regexp-class-skip-chars-wide ()
  (let* ((lo (regexp-class-wide-char :hira-a))
         (hi (regexp-class-wide-char :hira-n))
         (k (regexp-class-wide-char :kanji-first))
         (spec (concatenate 'string (string lo) "-" (string hi)))
         (text (concatenate 'string (string lo) (string hi) (string k) "a")))
    (list (regexp-class-skip text spec 1)
          (regexp-class-skip text (concatenate 'string "^" spec) 1)
          (regexp-class-skip text (concatenate 'string "^" spec) -1)
          (regexp-class-skip text (concatenate 'string "^" spec "a") 1 2)
          (regexp-class-skip text (concatenate 'string (string k) "a") -1)))
  => ((t . 2) (nil . 0) (t . 2) (t . 1) (t . 2)))

;;; The range set used by the compiled class, with integers beyond the Lisp
;;; characters of the current value (si:regexp-class-test, si:regexp-fold256).

(deftest regexp-class-test-ranges ()
  "Membership by the compiled form: ASCII bits and the sorted, merged ranges of
the other characters, including code points outside the BMP."
  (let ((ranges '((#x41 . #x5a) #x3042 (#x3044 . #x3060) (#x1f600 . #x1f64f)
                  (#x20000 . #x2a6df) #x10ffff #x7f #x80 (#xff . #x100))))
    (flet ((in (c) (si:regexp-class-test ranges c)))
      (list (mapcar #'in '(#x40 #x41 #x5a #x5b #x61))
            (mapcar #'in '(#x7e #x7f #x80 #x81 #xfe #xff #x100 #x101))
            (mapcar #'in '(#x3041 #x3042 #x3043 #x3044 #x3060 #x3061))
            (mapcar #'in '(#x1f5ff #x1f600 #x1f64f #x1f650 #x1f7ff))
            (mapcar #'in '(#x1ffff #x20000 #x2a6df #x2a6e0 #x10fffe #x10ffff)))))
  => ((nil t t nil nil)
      (nil t t nil nil t t nil)
      (nil t nil t t nil)
      (nil t t nil nil)
      (nil t t nil nil t)))

(deftest regexp-class-test-negated ()
  (let ((ranges '(#x1f600 (#x41 . #x5a) (#x3042 . #x3044))))
    (flet ((in (c) (si:regexp-class-test ranges c t)))
      (mapcar #'in '(#x40 #x41 #x5a #x5b #x3041 #x3043 #x3045 #x1f600 #x1f601
                     #x10ffff #xffffff)))
  => (t nil nil t t nil t nil t t t))

(deftest regexp-class-test-merge-and-order ()
  "Overlapping, adjacent, reversed and repeated ranges, in any order."
  (let ((ranges '((#x200 . #x300) (#x100 . #x1ff) (#x250 . #x260) (#x400 . #x3ff)
                  #x301 #x301 (#x500 . #x500))))
    (flet ((in (c) (si:regexp-class-test ranges c)))
      (list (mapcar #'in '(#xff #x100 #x1ff #x200 #x300 #x301 #x302))
            (mapcar #'in '(#x3ff #x400 #x4ff #x500 #x501)))))
  => ((nil t t t t t nil) (nil nil nil t nil)))

(deftest regexp-class-test-empty ()
  (list (si:regexp-class-test nil #x41)
        (si:regexp-class-test nil #x41 t)
        (si:regexp-class-test nil #x1f600 t))
  => (nil t t))

(deftest regexp-class-test-many-ranges ()
  "A class with many ranges (every 4th block of 16 codes)."
  (let ((ranges (loop for i from 0 below 4000
                      collect (cons (+ #x10000 (* i 64)) (+ #x10000 (* i 64) 15)))))
    (let ((bad nil))
      (dotimes (n 4000)
        (let ((base (+ #x10000 (* n 64))))
          (dolist (c (list base (+ base 15) (+ base 16) (+ base 63)))
            (let ((in (si:regexp-class-test ranges c))
                  (want (<= (- c base) 15)))
              (unless (eq (and in t) want)
                (push c bad))))))
      (nreverse bad)))
  => nil)

(deftest regexp-fold256-slots ()
  "fold256: ASCII is itself, everything else lands in 0x80 .. 0xFF."
  (list (let ((bad nil))
          (dotimes (c 128) (unless (= (si:regexp-fold256 c) c) (push c bad)))
          bad)
        (let ((lo 255) (hi 0))
          (dolist (c '(#x80 #xff #x100 #x4142 #x3042 #xffff #x10000 #x1f600
                       #x10ffff #xffffffff))
            (let ((f (si:regexp-fold256 c)))
              (setq lo (min lo f) hi (max hi f))))
          (list lo hi)))
  => (nil (128 255)))
