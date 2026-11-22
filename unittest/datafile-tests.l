;;; -*- mode: lisp; package: user -*-
;;;
;;; Tests of the data files of xyzzy (S4 R5: lisp/datafile.l, src/legacy.cc;
;;; docs/refactoring/s4-char-design.md 9.4): the mark of the format, the
;;; old internal code -> Unicode table, the rewrite of the old format, and
;;; the samples of the old format (data/legacy-*.txt, written by the xyzzy
;;; before R5 with data/make-legacy-samples.l).
;;;
;;; The expected values are made with unicode-char from Unicode code points,
;;; so that they hold on both sides of XYZZY_CHAR_UNICODE where they can.
;;; This file is ASCII.

(in-package :user)

(defvar *r5d-dir* (directory-namestring *load-pathname*))

;; The variables that the samples set.
(defvar *r5-legacy-strings* nil)
(defvar *r5-legacy-chars* nil)
(defvar *r5-legacy-macros* nil)
(defvar *r5-legacy-raw* nil)
(defvar *r5-legacy-abbrev-table* nil)

(defun r5d-sample (name)
  (merge-pathnames (concat "data/" name) *r5d-dir*))

(defun r5d-u (&rest codes)
  "A string of the Unicode CODES (unicode-char may give 2 characters)."
  (apply #'concat (mapcar #'(lambda (u)
                              (let ((x (unicode-char u)))
                                (if (characterp x) (string x) x)))
                          codes)))

(defun r5d-c (&rest codes)
  (map 'string #'code-char codes))

(defun r5d-a ()
  "Hiragana A as a character."
  (char (r5d-u #x3042) 0))

(defun r5d-with-temp (fn)
  (let ((tmp (make-temp-file-name)))
    (unwind-protect
        (funcall fn tmp)
      (when (file-exist-p tmp)
        (delete-file tmp)))))

(defun r5d-write-text (path text)
  (with-open-file (s path :direction :output :if-exists :supersede)
    (princ text s)))

(defun r5d-write-bytes (path bytes)
  (with-open-file (s path :direction :output :encoding :binary
                     :if-exists :supersede)
    (dolist (b bytes)
      (write-char (code-char b) s))))

(defun r5d-ascii (string)
  (map 'list #'char-code string))

(defun r5d-load (fn file)
  "Loads FILE with FN (ed::load-data-file or ed::load-legacy-data-file) in
the package user, after clearing the variables of the samples."
  (setq *r5-legacy-strings* nil
        *r5-legacy-chars* nil
        *r5-legacy-macros* nil
        *r5-legacy-raw* nil)
  (let ((*package* (find-package "user")))
    (funcall fn file)))

(defun r5d-macro (name)
  (cdr (assoc name *r5-legacy-macros* :test #'string=)))

;; The values that make-legacy-samples.l wrote.
(defun r5d-expected-strings ()
  (list (concat "a" (string #\") "b" (string #\\) "c")
        (r5d-u #x3042 #x6f22)
        (r5d-u #xff71)
        (r5d-u #xe9 #xac00 #x4eec)
        (r5d-u #x416 #x3b1)
        (r5d-u #x1f600)
        (r5d-u #xa000)
        (r5d-c #x61 #x0a #x09 #x62)))

(defun r5d-expected-chars ()
  (list #\a
        (r5d-a)
        (char (r5d-u #xe9) 0)
        (char (r5d-u #xac00) 0)
        (char (r5d-u #xff71) 0)
        ;; the lone lead byte #x81: the old table has U+30FB for it
        (if (si:char-unicode-p) (code-char #x30fb) (code-char #x81))
        #\C-x #\M-x #\F1 #\C-M-F1))

;;; ---------------------------------------------------------------------
;;; si:legacy-internal-to-unicode

(deftest r5d-table-ascii ()
  (si:legacy-internal-to-unicode '(#x41 #x7e #x0a))
  => (#x41 #x7e #x0a))

(deftest r5d-table-cp932 ()
  (si:legacy-internal-to-unicode '(#x82a0 #x8abf #xb1))
  => (#x3042 #x6f22 #xff71))

(deftest r5d-table-other-charsets ()
  ;; ISO-8859-1 e acute, KS C 5601 GA, GB 2312, ISO-8859-5 ZHE, ISO-8859-7
  ;; alpha: the values in the sample
  (si:legacy-internal-to-unicode '(#x0169 #x1c82 #x46a2 #x0336 #x03e1))
  => (#xe9 #xac00 #x4eec #x416 #x3b1))

(deftest r5d-table-pairs ()
  (si:legacy-internal-to-unicode '(#xd83d #xde00 #x41 #xd6a0 #xd700))
  => (#x1f600 #x41 #xa000))

(deftest r5d-table-old-keys ()
  (si:legacy-internal-to-unicode '(#x8078 #xff0f #xfe0f))
  => (#x1008078 #x100ff0f #x100fe0f))

(deftest r5d-table-old-keys-are-the-keys-of-today ()
  (mapcar #'code-char (si:legacy-internal-to-unicode '(#x8078 #xff0f)))
  => (#\M-x #\F1))

(deftest r5d-table-no-value ()
  (si:legacy-internal-to-unicode '(#x10000 -1))
  => (#xfffd #xfffd))

(deftest r5d-table-vector-and-empty ()
  (list (si:legacy-internal-to-unicode (vector #x82a0))
        (si:legacy-internal-to-unicode nil))
  => ((#x3042) nil))

;;; ---------------------------------------------------------------------
;;; The mark and the format

(deftest r5d-mark ()
  (with-output-to-string (s)
    (ed::write-data-file-mark s))
  => #.(format nil ";;; -*- Encoding: utf-8; Xyzzy-Data-Format: ~D -*-~%"
               (if (si:char-unicode-p) 2 1)))

(deftest r5d-params ()
  (r5d-with-temp
   #'(lambda (tmp)
       (r5d-write-text tmp ";;; -*- Encoding: utf-8; Xyzzy-Data-Format: 2 -*-\n(setq a 1)\n")
       (ed::data-file-params tmp)))
  => (("encoding" . "utf-8") ("xyzzy-data-format" . "2")))

(deftest r5d-format-of-files ()
  (r5d-with-temp
   #'(lambda (tmp)
       (list
        (progn
          (r5d-write-text tmp ";;; -*- Encoding: utf-8; Xyzzy-Data-Format: 2 -*-\n(setq a 1)\n")
          (ed::data-file-format-of tmp))
        (progn
          ;; the mark on the second line (the session file)
          (r5d-write-text tmp ";;; xyzzy session file.\n;;; -*- Encoding: utf-8; Xyzzy-Data-Format: 1 -*-\n")
          (ed::data-file-format-of tmp))
        (progn
          (r5d-write-text tmp "(setq a 1)\n")
          (ed::data-file-format-of tmp))
        (progn
          (delete-file tmp)
          (ed::data-file-format-of tmp)))))
  => (2 1 nil nil))

(deftest r5d-encoding-of-the-old-format ()
  (r5d-with-temp
   #'(lambda (tmp)
       (list
        (progn
          (r5d-write-text tmp ";;; -*- Encoding: utf-8; Xyzzy-Data-Format: 1 -*-\n")
          (char-encoding-type (ed::legacy-data-file-encoding tmp)))
        (eq (ed::legacy-data-file-encoding (r5d-sample "legacy-history.txt"))
            *encoding-sjis*))))
  => (:utf8 t))

;;; ---------------------------------------------------------------------
;;; The rewrite of the old format (the text)

(deftest r5d-rewrite-string-escapes ()
  (ed::legacy-data-text-to-unicode "(setq x \"a\\X0169\\x41\\101b\")")
  => "(setq x \"a\\X00e9\\X0041\\X0041b\")")

(deftest r5d-rewrite-keeps-the-other-escapes ()
  (ed::legacy-data-text-to-unicode "\"q\\\"b\\\\s\\n\"")
  => "\"q\\\"b\\\\s\\n\"")

(deftest r5d-rewrite-pairs ()
  (ed::legacy-data-text-to-unicode "\"\\Xd83d\\Xde00\\Xd6a0\\Xd700\"")
  => #.(concat "\""
               (let ((c (code-char #x1f600))) (if c (string c) "\\Xfffd"))
               "\\Xa000\""))

(deftest r5d-rewrite-chars ()
  (ed::legacy-data-text-to-unicode "(#\\x0169 #\\x82a0 #\\a #\\M-x #\\F1 #\\x #\\( #\\101)")
  => "(#\\x00e9 #\\x3042 #\\a #\\M-x #\\F1 #\\x #\\( #\\x0041)")

(deftest r5d-rewrite-old-macro-string ()
  (ed::legacy-data-text-to-unicode "(\"old\" . \"a\\X8078\\Xff0f\")")
  => "(\"old\" . #(#\\x0061 #\\M-x #\\F1))")

(deftest r5d-rewrite-skips-comments-bars-and-escaped-symbols ()
  (ed::legacy-data-text-to-unicode
   (concat ";; \"\\X0169\" #\\x0169" (string (code-char 10))
           "(|a\"b\\|c| a\\\"b #'car \"\\X0169\")"))
  => #.(concat ";; \"\\X0169\" #\\x0169" (string (code-char 10))
               "(|a\"b\\|c| a\\\"b #'car \"\\X00e9\")"))

(deftest r5d-rewrite-reads-as-unicode ()
  (map 'list #'char-code
       (read-from-string
        (ed::legacy-data-text-to-unicode "\"\\X0169\\Xd6a0\\Xd700\"")))
  => (#xe9 #xa000))

;;; ---------------------------------------------------------------------
;;; The samples written by the xyzzy before R5, read by load-data-file:
;;; the same values (0: load as it is; 1: the rewrite).

(deftest r5d-sample-history-strings ()
  (progn
    (r5d-load #'ed::load-data-file (r5d-sample "legacy-history.txt"))
    (equal *r5-legacy-strings* (r5d-expected-strings)))
  => t)

(deftest r5d-sample-history-chars ()
  (progn
    (r5d-load #'ed::load-data-file (r5d-sample "legacy-history.txt"))
    (equal *r5-legacy-chars* (r5d-expected-chars)))
  => t)

(deftest r5d-sample-history-macros ()
  (progn
    (r5d-load #'ed::load-data-file (r5d-sample "legacy-history.txt"))
    (list (coerce (r5d-macro "vec") 'list)
          (stringp (r5d-macro "old"))
          (map 'list #'char-code (r5d-macro "old"))))
  => #.(list (list #\a #\M-x #\F1 (char (r5d-u #x3042) 0))
             (not (si:char-unicode-p))
             (if (si:char-unicode-p)
                 (mapcar #'char-code (list #\a #\M-x #\F1 (char (r5d-u #x3042) 0)))
               (list #x61 #x8078 #xff0f (char-code (char (r5d-u #x3042) 0))))))

(deftest r5d-sample-history-raw-bytes ()
  ;; Codes that cp932 does not define: the same on the 0 side.  (On the 1
  ;; side they are what the cp932 converter makes of the bytes.)
  (progn
    (r5d-load #'ed::load-data-file (r5d-sample "legacy-history.txt"))
    (or (si:char-unicode-p)
        (equal *r5-legacy-raw* (list (r5d-c #x8540 #x81)))))
  => t)

(deftest r5d-sample-kbdmacro ()
  (let ((ed::*saved-kbd-macro-alist* nil))
    (let ((*package* (find-package "user")))
      (ed::load-data-file (r5d-sample "legacy-kbdmacro.txt")))
    (list (coerce (cdr (assoc "vec" ed::*saved-kbd-macro-alist* :test #'string=)) 'list)
          (map 'list #'char-code
               (cdr (assoc "old" ed::*saved-kbd-macro-alist* :test #'string=)))))
  => #.(list (list #\a #\M-x #\F1 (char (r5d-u #x3042) 0))
             (if (si:char-unicode-p)
                 (mapcar #'char-code (list #\a #\M-x #\F1 (char (r5d-u #x3042) 0)))
               (list #x61 #x8078 #xff0f (char-code (char (r5d-u #x3042) 0))))))

(deftest r5d-sample-abbrev ()
  (let ((ed::*abbrev-table-name-list* nil)
        (ed::*abbrevs-changed* ed::*abbrevs-changed*))
    (setq *r5-legacy-abbrev-table* nil)
    (let ((*package* (find-package "user")))
      (ed::load-data-file (r5d-sample "legacy-abbrev.txt")))
    (list (equal (car (gethash "ja" *r5-legacy-abbrev-table*)) (r5d-u #x3042 #x6f22))
          (equal (car (gethash "ko" *r5-legacy-abbrev-table*)) (r5d-u #xac00 #xe9))))
  => (t t))

;;; ---------------------------------------------------------------------
;;; The rewrite of the old format, run on the samples (the 1 side reads
;;; them this way; on the 0 side the escapes become Unicode values while
;;; the cp932 text stays the old code, so only those values are compared).

(deftest r5d-legacy-load-history ()
  (progn
    (r5d-load #'ed::load-legacy-data-file (r5d-sample "legacy-history.txt"))
    (list (nth 0 *r5-legacy-strings*)
          (equal (nth 1 *r5-legacy-strings*) (r5d-u #x3042 #x6f22))
          (map 'list #'char-code (nth 3 *r5-legacy-strings*))
          (map 'list #'char-code (nth 4 *r5-legacy-strings*))
          (map 'list #'char-code (nth 6 *r5-legacy-strings*))
          (mapcar #'char-code (subseq *r5-legacy-chars* 2 4))
          (char-code (nth 5 *r5-legacy-chars*))
          (nthcdr 6 *r5-legacy-chars*)))
  => #.(list (concat "a" (string #\") "b" (string #\\) "c")
             t
             '(#xe9 #xac00 #x4eec)
             '(#x416 #x3b1)
             '(#xa000)
             '(#xe9 #xac00)
             #x30fb
             (list #\C-x #\M-x #\F1 #\C-M-F1)))

(deftest r5d-legacy-load-old-macro ()
  (progn
    (r5d-load #'ed::load-legacy-data-file (r5d-sample "legacy-history.txt"))
    (list (vectorp (r5d-macro "old"))
          (stringp (r5d-macro "old"))
          (coerce (r5d-macro "old") 'list)
          (coerce (r5d-macro "vec") 'list)))
  => #.(list t nil
             (list #\a #\M-x #\F1 (char (r5d-u #x3042) 0))
             (list #\a #\M-x #\F1 (char (r5d-u #x3042) 0))))

(deftest r5d-legacy-load-utf8-format-1 ()
  ;; A file of the format 1 in UTF-8 (what the 0 side writes from R5 on):
  ;; U+3042 in UTF-8, and e acute as the old internal code.
  (r5d-with-temp
   #'(lambda (tmp)
       (r5d-write-bytes
        tmp
        (append (r5d-ascii ";;; -*- Encoding: utf-8; Xyzzy-Data-Format: 1 -*-")
                '(#x0d #x0a)
                (r5d-ascii "(setq user::*r5-legacy-raw* \"")
                '(#xe3 #x81 #x82)
                (r5d-ascii "\\X0169\")")
                '(#x0d #x0a)))
       (setq *r5-legacy-raw* nil)
       (ed::load-legacy-data-file tmp)
       (list (char *r5-legacy-raw* 0)
             (char-code (char *r5-legacy-raw* 1)))))
  => #.(list (char (r5d-u #x3042) 0) #xe9))

(deftest r5d-legacy-load-missing-file ()
  (r5d-with-temp
   #'(lambda (tmp)
       (delete-file tmp)
       (list (ed::load-data-file tmp :if-does-not-exist nil)
             (ed::load-legacy-data-file tmp :if-does-not-exist nil))))
  => (nil nil))
