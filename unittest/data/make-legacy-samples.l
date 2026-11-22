;;; -*- Mode: Lisp; Package: user -*-
;;;
;;; Makes the samples of the old form of the data files of xyzzy (S4 R5,
;;; docs/refactoring/s4-char-design.md 9.4 and the risk table of 11):
;;;
;;;   legacy-history.txt  save-history-file
;;;   legacy-kbdmacro.txt the body of save-kbd-macro-to-file
;;;   legacy-abbrev.txt   write-abbrev-file
;;;
;;; They were written by the xyzzy of d7501a1 (feature/x64-only, before R5:
;;; the raw cp932 bytes, the old internal code in \X, \x and #\x) with
;;;   ci\ci-test.ps1 -Tests unittest\data\make-legacy-samples.l
;;; Do not run this file again after R5: it would write the new form.  The
;;; values are in unittest/datafile-tests.l (r5-legacy-*), which reads the
;;; samples.  This file is ASCII; the characters are made from numbers.

(in-package :user)

(defvar *r5-sample-dir* (directory-namestring *load-pathname*))

(defvar *r5-legacy-strings* nil)
(defvar *r5-legacy-chars* nil)
(defvar *r5-legacy-macros* nil)
(defvar *r5-legacy-raw* nil)

(defun r5-u (&rest codes)
  "A string of the Unicode CODES (unicode-char may give 2 characters)."
  (apply #'concat (mapcar #'(lambda (u)
                              (let ((x (unicode-char u)))
                                (if (characterp x) (string x) x)))
                          codes)))

(defun r5-c (&rest codes)
  "A string of the internal CODES."
  (map 'string #'code-char codes))

(defun r5-legacy-values ()
  (setq *r5-legacy-strings*
        (list (concat "a" (string #\") "b" (string #\\) "c")
              ;; hiragana A, kanji KAN (cp932: raw bytes)
              (r5-u #x3042 #x6f22)
              ;; half width katakana A (cp932: one byte)
              (r5-u #xff71)
              ;; Latin e with acute, Hangul GA, a Han that cp932 does not have
              ;; (the old internal code: \X)
              (r5-u #xe9 #xac00 #x4eec)
              ;; Cyrillic ZHE, Greek alpha (cp932 has them: raw bytes)
              (r5-u #x416 #x3b1)
              ;; outside the BMP (a surrogate pair of the old internal code)
              (r5-u #x1f600)
              ;; Yi syllable IT: no table has it (the pair of the undefined
              ;; UTF-16 character of the old internal code)
              (r5-u #xa000)
              ;; a newline and a tab
              (r5-c #x61 #x0a #x09 #x62))
        *r5-legacy-raw*
        (list
         ;; a code that cp932 does not define, and a lone lead byte
         (r5-c #x8540 #x81))
        *r5-legacy-chars*
        (list #\a
              (char (r5-u #x3042) 0)
              (char (r5-u #xe9) 0)
              (char (r5-u #xac00) 0)
              (char (r5-u #xff71) 0)
              (code-char #x81)
              #\C-x #\M-x #\F1 #\C-M-F1)
        *r5-legacy-macros*
        (list
         ;; a keyboard macro of today: a vector of characters (S4-k)
         (cons "vec" (vector #\a #\M-x #\F1 (char (r5-u #x3042) 0)))
         ;; a keyboard macro before S4-k: the old values of the keys
         ;; (M-x = #x8078, F1 = #xff0f) in a string
         (cons "old" (concat "a" (r5-c #x8078 #xff0f) (r5-u #x3042))))))

(deftest r5-make-legacy-history ()
  (progn
    (r5-legacy-values)
    (let ((ed::*history-file-name* (merge-pathnames "legacy-history.txt" *r5-sample-dir*))
          (ed::*history-file-loaded* t)
          (ed::*save-history* 200)
          (ed::*minibuffer-history-variables* '(*r5-legacy-strings*))
          (ed::*history-variable-list* '(*r5-legacy-chars* *r5-legacy-macros*
                                         *r5-legacy-raw*))
          (ed::*save-history-hook* nil)
          (ed::*save-resume-info* nil))
      (ed::save-history-file))
    t)
  => t)

(deftest r5-make-legacy-kbdmacro ()
  (progn
    (r5-legacy-values)
    ;; The body of save-kbd-macro-to-file (the file name comes from a dialog
    ;; there), without the date.
    (let ((ed::*saved-kbd-macro-alist* *r5-legacy-macros*))
      (with-open-file (s (merge-pathnames "legacy-kbdmacro.txt" *r5-sample-dir*)
                         :direction :output
                         :if-exists :supersede
                         :if-does-not-exist :create)
        (format s ";;; This file contains keyboard macro definition.~%")
        (format s "(setq editor::*saved-kbd-macro-alist*~%")
        (format s "  '(~{~S~^~%    ~}))~%" ed::*saved-kbd-macro-alist*)))
    t)
  => t)

(defvar *r5-legacy-abbrev-table* nil)

(deftest r5-make-legacy-abbrev ()
  (progn
    (r5-legacy-values)
    (setq *r5-legacy-abbrev-table* nil)
    (define-abbrev-table '*r5-legacy-abbrev-table*
      (list (list "ja" (r5-u #x3042 #x6f22) nil 0)
            (list "ko" (r5-u #xac00 #xe9) nil 0)))
    (let ((ed::*abbrev-table-name-list* '(*r5-legacy-abbrev-table*))
          (ed::*abbrev-file-name* "~/.r5-not-this-file"))
      (write-abbrev-file (merge-pathnames "legacy-abbrev.txt" *r5-sample-dir*)))
    t)
  => t)
