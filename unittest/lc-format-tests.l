;;; -*- mode: lisp; package: user -*-
;;;
;;; Tests for the version mark of an .lc (S4 stage 3 R5-A, design 9.2).
;;;
;;;   - compile-file writes ";;; -*- Mode: Lisp; ...; Lc-Format: N -*-" on
;;;     line 1.  N is 1 while a char is the internal code (the raw Shift_JIS
;;;     form), 2 when a char is Unicode (UTF-8).
;;;   - load reads an .lc of its own version.  An .lc without the mark counts
;;;     as version 1.
;;;   - an .lc of another version is not read.  The .l next to it is loaded
;;;     instead; without a .l it is an error.
;;;
;;; The expected version comes from (si:char-unicode-p), so that the file
;;; passes the same way after the chars become Unicode (V).  Everything here
;;; is ASCII.

(in-package :user)

(defvar *lc-format-result* nil)

(defun lc-format-expected ()
  (if (si:char-unicode-p) 2 1))

(defun lc-format-other ()
  (if (si:char-unicode-p) 1 2))

(defun lc-format-write (path text)
  (with-open-file (s path
                     :direction :output
                     :encoding :binary
                     :if-exists :supersede)
    (dotimes (i (length text))
      (write-char (char text i) s))))

(defun lc-format-first-line (path)
  (with-open-file (s path
                     :direction :input
                     :encoding :binary)
    (let ((r nil) c)
      (while (and (setq c (read-char s nil nil))
                  (/= (char-code c) 10))
        (unless (= (char-code c) 13)
          (push c r)))
      (coerce (nreverse r) 'string))))

;; Makes a unique .l file (it exists, empty) and returns (l-path . lc-path).
;; Neither the .lc nor anything else exists beyond the empty .l.
(defun lc-format-paths ()
  (let ((l (make-temp-file-name "lcf" "l")))
    (cons l (concat l "c"))))

(defun lc-format-cleanup (paths)
  (dolist (p (list (car paths) (cdr paths)))
    (when (file-exist-p p)
      (delete-file p))))

(defun lc-format-load (path)
  (setq *lc-format-result* :none)
  (handler-case
      (progn
        (load path :verbose nil)
        *lc-format-result*)
    (error () :error)))

(deftest lc-format-compile-file-writes-the-mark ()
  "compile-file writes the mark with the version on line 1."
  (let ((paths (lc-format-paths)))
    (unwind-protect
        (progn
          (lc-format-write (car paths)
                           "(setq user::*lc-format-result* :compiled)\r\n")
          (compile-file (car paths))
          (lc-format-first-line (cdr paths)))
      (lc-format-cleanup paths)))
  => #.(if (si:char-unicode-p)
           ";;; -*- Mode: Lisp; Encoding: utf-8; Lc-Format: 2 -*-"
         ";;; -*- Mode: Lisp; Encoding: Shift_JIS; Lc-Format: 1 -*-"))

(deftest lc-format-compile-file-output-loads ()
  "The .lc that compile-file wrote is read by load."
  (let ((paths (lc-format-paths)))
    (unwind-protect
        (progn
          (lc-format-write (car paths)
                           "(setq user::*lc-format-result* :compiled)\r\n")
          (compile-file (car paths))
          (lc-format-load (cdr paths)))
      (lc-format-cleanup paths)))
  => :compiled)

(deftest lc-format-mark-of-the-version-loads ()
  "An .lc with the mark of this version is read."
  (let ((paths (lc-format-paths)))
    (unwind-protect
        (progn
          (lc-format-write (cdr paths)
                           (format nil ";;; -*- Mode: Lisp; Lc-Format: ~D -*-\r\n(setq user::*lc-format-result* :lc)\r\n"
                                   (lc-format-expected)))
          (lc-format-load (cdr paths)))
      (lc-format-cleanup paths)))
  => :lc)

(deftest lc-format-missing-mark-counts-as-version-1 ()
  "An .lc without the mark is version 1: read before V, refused after."
  (let ((paths (lc-format-paths)))
    (unwind-protect
        (progn
          (lc-format-write (cdr paths)
                           "(setq user::*lc-format-result* :lc)\r\n")
          (lc-format-load (cdr paths)))
      (lc-format-cleanup paths)))
  => #.(if (si:char-unicode-p) :error :lc))

(deftest lc-format-mark-with-the-same-line-as-encoding-mark ()
  "The mark may follow other keys of the -*- line, in any case."
  (let ((paths (lc-format-paths)))
    (unwind-protect
        (progn
          (lc-format-write (cdr paths)
                           (format nil ";;; -*- mode: lisp; lc-format:~D; -*-\r\n(setq user::*lc-format-result* :lc)\r\n"
                                   (lc-format-expected)))
          (lc-format-load (cdr paths)))
      (lc-format-cleanup paths)))
  => :lc)

(deftest lc-format-other-version-loads-the-source ()
  "An .lc of another version is skipped when the .l is next to it."
  (let ((paths (lc-format-paths)))
    (unwind-protect
        (progn
          (lc-format-write (car paths)
                           "(setq user::*lc-format-result* :l)\r\n")
          (lc-format-write (cdr paths)
                           (format nil ";;; -*- Mode: Lisp; Lc-Format: ~D -*-\r\n(setq user::*lc-format-result* :lc)\r\n"
                                   (lc-format-other)))
          (lc-format-load (cdr paths)))
      (lc-format-cleanup paths)))
  => :l)

(deftest lc-format-other-version-without-source-is-an-error ()
  "An .lc of another version with no .l is an error, and is not evaluated."
  (let ((paths (lc-format-paths)))
    (unwind-protect
        (progn
          (delete-file (car paths))
          (lc-format-write (cdr paths)
                           (format nil ";;; -*- Mode: Lisp; Lc-Format: ~D -*-\r\n(setq user::*lc-format-result* :lc)\r\n"
                                   (lc-format-other)))
          (lc-format-load (cdr paths)))
      (lc-format-cleanup paths)))
  => :error)

(deftest lc-format-garbage-version-is-refused ()
  "A mark without a number is not a version this build reads."
  (let ((paths (lc-format-paths)))
    (unwind-protect
        (progn
          (delete-file (car paths))
          (lc-format-write (cdr paths)
                           ";;; -*- Mode: Lisp; Lc-Format: x -*-\r\n(setq user::*lc-format-result* :lc)\r\n")
          (lc-format-load (cdr paths)))
      (lc-format-cleanup paths)))
  => :error)

(deftest lc-format-source-is-not-checked ()
  "Only an .lc has a version: a .l with a mark of another version loads."
  (let ((paths (lc-format-paths)))
    (unwind-protect
        (progn
          (lc-format-write (car paths)
                           (format nil ";;; -*- Mode: Lisp; Lc-Format: ~D -*-\r\n(setq user::*lc-format-result* :l)\r\n"
                                   (lc-format-other)))
          (lc-format-load (car paths)))
      (lc-format-cleanup paths)))
  => :l)
