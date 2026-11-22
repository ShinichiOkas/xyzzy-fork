;;; -*- mode: lisp; package: user -*-
;;;
;;; Tests for the data files of the keyboard macros (write-kbd-macro-file,
;;; load-kbd-macro-file) and of the abbrevs (write-abbrev-file,
;;; read-abbrev-file) (S4 piece R5, docs/refactoring/s4-char-design.md
;;; 4.3 and 9.4).
;;;
;;; The files are written in UTF-8, and the first line is the mark
;;;   ;;; -*- Encoding: utf-8; Xyzzy-Data-Format: N -*-
;;; They are read back to the same values.  The old forms are in
;;; unittest/datafile-tests.l.
;;;
;;; The file is ASCII: the non-ASCII characters are made from numbers.

(in-package :user)

(defvar *r5c-hiragana-a* #x3042)
(defvar *r5c-kanji-kan* #x6f22)

(defun r5c-mark-line ()
  (format nil ";;; -*- Encoding: utf-8; Xyzzy-Data-Format: ~D -*-"
          (ed::data-file-format)))

(defun r5c-read-lines (file n)
  "The first N lines of FILE (read as bytes, so that the encoding is not
guessed), without the CR."
  (with-open-file (s file :direction :input :encoding :binary)
    (let ((lines nil))
      (dotimes (i n)
        (let ((line (read-line s nil)))
          (unless line
            (return))
          (push (string-right-trim '(#\Return) line) lines)))
      (nreverse lines))))

(defun r5c-file-bytes (file)
  (with-open-file (s file :direction :input :encoding :binary)
    (let ((bytes nil))
      (loop
        (let ((c (read-char s nil)))
          (unless c
            (return))
          (push (char-code c) bytes)))
      (nreverse bytes))))

(defun r5c-subseq-p (sub list)
  "True if the list SUB is in the list LIST."
  (let ((n (length sub)))
    (loop
      (when (null list)
        (return nil))
      (when (and (>= (length list) n)
                 (equal sub (subseq list 0 n)))
        (return t))
      (setq list (cdr list)))))

(defun r5c-hiragana-a-utf8-p (file)
  "True if FILE has the UTF-8 bytes of HIRAGANA LETTER A."
  (r5c-subseq-p '(#xe3 #x81 #x82) (r5c-file-bytes file)))

(defun r5c-delete-file (file)
  (when (file-exist-p file)
    (delete-file file)))

;;; keyboard macro

(defun r5c-kbd-macros ()
  (list (cons "vec" (vector #\a #\M-x #\F1 (unicode-char *r5c-hiragana-a*)))
        (cons "str" (concat "a" (string (unicode-char *r5c-kanji-kan*)) "b"))))

(defun r5c-kbd-macros-equal (a b)
  (and (= (length a) (length b))
       (every #'(lambda (x y)
                  (and (equal (car x) (car y))
                       (equal (coerce (cdr x) 'list)
                              (coerce (cdr y) 'list))))
              a b)))

(deftest r5c-kbdmacro-write-mark ()
  (let ((tmp (make-temp-file-name))
        (ed::*saved-kbd-macro-alist* (r5c-kbd-macros)))
    (unwind-protect
        (progn
          (ed::write-kbd-macro-file tmp)
          (let ((lines (r5c-read-lines tmp 2)))
            (equal (list (car lines)
                         (cadr lines)
                         (ed::data-file-format-of tmp))
                   (list (r5c-mark-line)
                         ";;; This file contains keyboard macro definition."
                         (ed::data-file-format)))))
      (r5c-delete-file tmp)))
  => t)

(deftest r5c-kbdmacro-write-utf8 ()
  (let ((tmp (make-temp-file-name))
        (ed::*saved-kbd-macro-alist* (r5c-kbd-macros)))
    (unwind-protect
        (progn
          (ed::write-kbd-macro-file tmp)
          (r5c-hiragana-a-utf8-p tmp))
      (r5c-delete-file tmp)))
  => t)

(deftest r5c-kbdmacro-roundtrip ()
  (let ((tmp (make-temp-file-name))
        (macros (r5c-kbd-macros))
        (ed::*saved-kbd-macro-alist* nil))
    (unwind-protect
        (progn
          (setq ed::*saved-kbd-macro-alist* macros)
          (ed::write-kbd-macro-file tmp)
          (setq ed::*saved-kbd-macro-alist* nil)
          (ed::load-kbd-macro-file tmp)
          (r5c-kbd-macros-equal macros ed::*saved-kbd-macro-alist*))
      (r5c-delete-file tmp)))
  => t)

(deftest r5c-kbdmacro-keys-kept ()
  ;; the keys come back as the same characters (#\M-x, #\F1)
  (let ((tmp (make-temp-file-name))
        (ed::*saved-kbd-macro-alist* nil))
    (unwind-protect
        (progn
          (setq ed::*saved-kbd-macro-alist* (r5c-kbd-macros))
          (ed::write-kbd-macro-file tmp)
          (setq ed::*saved-kbd-macro-alist* nil)
          (ed::load-kbd-macro-file tmp)
          (let ((v (cdr (assoc "vec" ed::*saved-kbd-macro-alist*
                               :test #'string=))))
            (equal (list (aref v 0) (aref v 1) (aref v 2)
                         (char-unicode (aref v 3)))
                   (list #\a #\M-x #\F1 *r5c-hiragana-a*))))
      (r5c-delete-file tmp)))
  => t)

(deftest r5c-kbdmacro-load-no-file ()
  (let ((tmp (make-temp-file-name)))
    (r5c-delete-file tmp)
    (handler-case (progn (ed::load-kbd-macro-file tmp) :loaded)
      (error (c) :error)))
  => :error)

;;; abbrev

(defvar *r5c-abbrev-table*)

(defun r5c-abbrev-defs ()
  (list (list "ko" "kotoba" nil 3)
        (list "ja" (concat "x" (string (unicode-char *r5c-hiragana-a*))
                           (string (unicode-char *r5c-kanji-kan*)))
              nil 0)))

(defun r5c-abbrev-expansions (table)
  (list (ed::abbrev-expansion "ko" table)
        (ed::abbrev-expansion "ja" table)))

(deftest r5c-abbrev-write-mark ()
  (let ((tmp (make-temp-file-name))
        (ed::*abbrev-table-name-list* nil)
        (ed::*abbrevs-changed* nil))
    (makunbound '*r5c-abbrev-table*)
    (unwind-protect
        (progn
          (ed::define-abbrev-table '*r5c-abbrev-table* (r5c-abbrev-defs))
          (ed::write-abbrev-file tmp)
          (equal (list (car (r5c-read-lines tmp 1))
                       (ed::data-file-format-of tmp)
                       (r5c-hiragana-a-utf8-p tmp))
                 (list (r5c-mark-line) (ed::data-file-format) t)))
      (makunbound '*r5c-abbrev-table*)
      (r5c-delete-file tmp)))
  => t)

(deftest r5c-abbrev-roundtrip ()
  (let ((tmp (make-temp-file-name))
        (ed::*abbrev-table-name-list* nil)
        (ed::*abbrevs-changed* nil)
        (ed::*save-abbrevs* nil)
        (expected nil))
    (makunbound '*r5c-abbrev-table*)
    (unwind-protect
        (progn
          (ed::define-abbrev-table '*r5c-abbrev-table* (r5c-abbrev-defs))
          (setq expected (r5c-abbrev-expansions *r5c-abbrev-table*))
          (ed::write-abbrev-file tmp)
          ;; forget the table, then read it back
          (makunbound '*r5c-abbrev-table*)
          (setq ed::*abbrev-table-name-list* nil)
          (ed::read-abbrev-file tmp t)
          (list (equal expected
                       (r5c-abbrev-expansions *r5c-abbrev-table*))
                (length (ed::abbrev-expansion "ja" *r5c-abbrev-table*))
                (length ed::*abbrev-table-name-list*)))
      (makunbound '*r5c-abbrev-table*)
      (r5c-delete-file tmp)))
  => (t 3 1))

(deftest r5c-abbrev-roundtrip-count ()
  ;; the count (the fourth value) is kept
  (let ((tmp (make-temp-file-name))
        (ed::*abbrev-table-name-list* nil)
        (ed::*abbrevs-changed* nil)
        (ed::*save-abbrevs* nil))
    (makunbound '*r5c-abbrev-table*)
    (unwind-protect
        (progn
          (ed::define-abbrev-table '*r5c-abbrev-table* (r5c-abbrev-defs))
          (ed::write-abbrev-file tmp)
          (makunbound '*r5c-abbrev-table*)
          (setq ed::*abbrev-table-name-list* nil)
          (ed::read-abbrev-file tmp t)
          (list (caddr (gethash "ko" *r5c-abbrev-table*))
                (caddr (gethash "ja" *r5c-abbrev-table*))))
      (makunbound '*r5c-abbrev-table*)
      (r5c-delete-file tmp)))
  => (3 0))

(deftest r5c-abbrev-write-keeps-changed ()
  ;; writing to a file other than *abbrev-file-name* leaves *abbrevs-changed*
  (let ((tmp (make-temp-file-name))
        (ed::*abbrev-table-name-list* nil)
        (ed::*abbrevs-changed* t))
    (unwind-protect
        (progn
          (ed::write-abbrev-file tmp)
          ed::*abbrevs-changed*)
      (r5c-delete-file tmp)))
  => t)
