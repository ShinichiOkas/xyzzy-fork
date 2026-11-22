;;; -*- Mode: Lisp; Package: EDITOR -*-
;;;
;;; This file is part of xyzzy.
;;;
;;; The data files that xyzzy writes for itself: the history, the resume
;;; file, the session, the keyboard macro file and the abbrev file
;;; (docs/refactoring/s4-char-design.md 4.3, 9.3, 9.4; S4 piece R5).
;;;
;;; They are written in UTF-8, and the first line is the mark
;;;   ;;; -*- Encoding: utf-8; Xyzzy-Data-Format: N -*-
;;; N says what the values of \x, \X and #\x in the file are:
;;;   1  the old internal code (an xyzzy whose Char is not Unicode:
;;;      si:char-unicode-p is nil).  A file without the mark is also 1; it
;;;      is the raw cp932 form that xyzzy wrote before R5.
;;;   2  Unicode (si:char-unicode-p is t).
;;; load finds the encoding by the mark (stream.cc stream_detect_load_encoding).
;;; load-data-file reads a file of the format 1 on an xyzzy of Unicode by
;;; rewriting the values of the old internal code (load-legacy-data-file).

(provide "datafile")

(in-package "editor")

(defun data-file-format ()
  "The format of the data files that this xyzzy writes."
  (if (si:char-unicode-p) 2 1))

(defun write-data-file-mark (stream)
  "Writes the first line of a data file (the encoding is UTF-8)."
  (format stream ";;; -*- Encoding: utf-8; Xyzzy-Data-Format: ~D -*-~%"
          (data-file-format)))

(defun data-file-params (file)
  "The parameters of the -*- -*- on the first 2 lines of FILE, as an alist of
strings (key . value) with the keys in lower case.  nil if there is no file."
  (with-open-file (s file :direction :input :encoding :binary
                     :if-does-not-exist nil)
    (when s
      (let ((params nil))
        (dotimes (i 2)
          (let ((line (read-line s nil)))
            (unless line
              (return))
            (when (string-match "-\\*-\\(.*\\)-\\*-" line)
              (dolist (item (split-string (match-string 1) ";" t " \t\r"))
                (let ((colon (position #\: item)))
                  (when colon
                    (push (cons (string-downcase
                                 (string-trim " \t" (subseq item 0 colon)))
                                (string-trim " \t\r" (subseq item (1+ colon))))
                          params)))))))
        (nreverse params)))))

(defun data-file-format-of (file)
  "The value of Xyzzy-Data-Format in FILE (an integer), or nil."
  (let ((v (cdr (assoc "xyzzy-data-format" (data-file-params file)
                       :test #'string=))))
    (and v (parse-integer v :junk-allowed t))))

(defun load-data-file (file &key (if-does-not-exist :error) verbose print)
  "Loads a data file of xyzzy.  On an xyzzy of Unicode, a file of the old
format is read by load-legacy-data-file."
  (if (or (not (si:char-unicode-p))
          (eql (data-file-format-of file) 2))
      (load file :if-does-not-exist if-does-not-exist
            :verbose verbose :print print)
    (load-legacy-data-file file :if-does-not-exist if-does-not-exist
                           :verbose verbose :print print)))

(defun load-legacy-data-file (file &key (if-does-not-exist :error) verbose print)
  ;; Not implemented yet: the rewrite of the old internal code comes next.
  (load file :if-does-not-exist if-does-not-exist
        :verbose verbose :print print))
