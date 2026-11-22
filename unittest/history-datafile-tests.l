;;; -*- mode: lisp; package: user -*-
;;;
;;; Tests of the history file, the resume file and the session file as
;;; data files (S4 R5-B: history.l, session.l; design 4.3, 9.4): they are
;;; written in UTF-8 with the mark on the first line, and read through
;;; load-data-file.
;;;
;;; The tests do not depend on the value of a Char: characters are made by
;;; unicode-char, looked at through char-unicode, and bytes are numbers.
;;; This file is ASCII.

(in-package :user)

(defvar *r5b-hist* nil)

(defun r5b-with-temp (fn)
  (let ((tmp (make-temp-file-name)))
    (unwind-protect
        (funcall fn tmp)
      (when (file-exist-p tmp)
        (delete-file tmp)))))

(defun r5b-write-bytes (path bytes)
  (with-open-file (s path
                     :direction :output
                     :encoding :binary
                     :if-exists :supersede)
    (dolist (b bytes)
      (write-char (code-char b) s))))

(defun r5b-read-bytes (path)
  (with-open-file (s path :direction :input :encoding :binary)
    (let ((r nil) c)
      (while (setq c (read-char s nil nil))
        (push (char-code c) r))
      (nreverse r))))

(defun r5b-ascii (string)
  (map 'list #'char-code string))

(defun r5b-unicodes (string)
  (map 'list #'char-unicode string))

(defun r5b-lines (bytes)
  "Splits BYTES at LF; a CR before the LF is dropped."
  (let ((lines nil) (cur nil))
    (dolist (b bytes)
      (cond ((= b #x0a)
             (when (and cur (= (car cur) #x0d))
               (pop cur))
             (push (nreverse cur) lines)
             (setq cur nil))
            (t (push b cur))))
    (when cur
      (push (nreverse cur) lines))
    (nreverse lines)))

(defun r5b-mark-bytes ()
  (r5b-ascii (format nil ";;; -*- Encoding: utf-8; Xyzzy-Data-Format: ~D -*-"
                     (if (si:char-unicode-p) 2 1))))

(defun r5b-contains (bytes sub)
  (let ((n (length sub)))
    (do ((l bytes (cdr l)))
        ((< (length l) n) nil)
      (when (equal (subseq l 0 n) sub)
        (return t)))))

;; A string of a, hiragana A (U+3042), a kanji (U+6F22), b.
(defun r5b-value ()
  (list (concatenate 'string "a"
                     (string (unicode-char #x3042))
                     (string (unicode-char #x6f22))
                     "b")
        "plain"))

(defun r5b-save-history (file value)
  "Saves the history file FILE with *r5b-hist* = VALUE; returns nothing useful."
  (let ((old (symbol-value '*r5b-hist*)))
    (unwind-protect
        (let ((editor::*history-file-name* file)
              (editor::*history-file-loaded* t)
              (editor::*save-history* 100)
              (editor::*save-history-hook* nil)
              (editor::*save-resume-info* nil)
              (editor::*history-variable-list* (list '*r5b-hist*))
              (editor::*minibuffer-history-variables* nil))
          (setq *r5b-hist* value)
          (editor::save-history-file))
      (setq *r5b-hist* old))))

(defun r5b-load-history (file)
  (let ((old (symbol-value '*r5b-hist*)))
    (unwind-protect
        (let ((editor::*history-file-name* file)
              (editor::*history-file-loaded* nil)
              (editor::*load-history-hook* nil)
              (editor::*save-resume-info* nil)
              (editor::*history-variable-list* (list '*r5b-hist*)))
          (setq *r5b-hist* nil)
          (editor::load-history-file)
          *r5b-hist*)
      (setq *r5b-hist* old))))

;;; ---------------------------------------------------------------------
;;; the history file

(deftest r5b-history-first-line-is-the-mark ()
  (r5b-with-temp
   #'(lambda (tmp)
       (r5b-save-history tmp (r5b-value))
       (equal (car (r5b-lines (r5b-read-bytes tmp)))
              (r5b-mark-bytes))))
  => t)

(deftest r5b-history-format-of-the-file ()
  (r5b-with-temp
   #'(lambda (tmp)
       (r5b-save-history tmp (r5b-value))
       (eql (editor::data-file-format-of tmp)
            (if (si:char-unicode-p) 2 1))))
  => t)

(deftest r5b-history-is-utf-8 ()
  (r5b-with-temp
   #'(lambda (tmp)
       (r5b-save-history tmp (r5b-value))
       (let ((bytes (r5b-read-bytes tmp)))
         ;; hiragana A, kanji: E3 81 82, E6 BC A2.  Not the cp932 form.
         (values (r5b-contains bytes '(#xe3 #x81 #x82 #xe6 #xbc #xa2))
                 (r5b-contains bytes '(#x82 #xa0))
                 (r5b-contains bytes '(#xef #xbb #xbf))))))
  => t
  => nil
  => nil)

(deftest r5b-history-read-back ()
  (r5b-with-temp
   #'(lambda (tmp)
       (r5b-save-history tmp (r5b-value))
       (let ((v (r5b-load-history tmp)))
         (values (length v)
                 (r5b-unicodes (car v))
                 (cadr v)))))
  => 2
  => (#x61 #x3042 #x6f22 #x62)
  => "plain")

(deftest r5b-history-read-back-of-a-missing-file ()
  (r5b-with-temp
   #'(lambda (tmp)
       (when (file-exist-p tmp)
         (delete-file tmp))
       (r5b-load-history tmp)))
  => nil)

;;; ---------------------------------------------------------------------
;;; the resume file (save-history-file with *save-resume-info*)

(deftest r5b-resume-file-has-the-mark-and-the-magic ()
  (let* ((prefix (format nil ".r5b-resume-~D-" (random 1000000)))
         (dir (user-config-path))
         (hist (make-temp-file-name))
         files)
    (unwind-protect
        (progn
          (let ((editor::*history-file-name* hist)
                (editor::*history-file-loaded* t)
                (editor::*save-history* 100)
                (editor::*save-history-hook* nil)
                (editor::*save-resume-info* t)
                (editor::*resume-file-name-long* prefix)
                (editor::*history-variable-list* nil)
                (editor::*minibuffer-history-variables* nil))
            (editor::save-history-file))
          (setq files (directory dir :wild (concat prefix "*")
                                 :absolute t :file-only t))
          (let ((lines (r5b-lines (r5b-read-bytes (car files)))))
            (values (length files)
                    (equal (car lines) (r5b-mark-bytes))
                    (equal (cadr lines)
                           (r5b-ascii ";;; xyzzy session file.")))))
      (dolist (f files)
        (ignore-errors (delete-file f)))
      (ignore-errors (delete-file hist))))
  => 1
  => t
  => t)

;;; ---------------------------------------------------------------------
;;; the session file

(deftest r5b-session-file-mark-and-magic ()
  (r5b-with-temp
   #'(lambda (tmp)
       (editor::write-session-file tmp)
       (let ((lines (r5b-lines (r5b-read-bytes tmp))))
         (values (equal (car lines) (r5b-mark-bytes))
                 (equal (cadr lines)
                        (r5b-ascii ";;; xyzzy session file."))
                 (editor::session-file-p tmp)
                 (eql (editor::data-file-format-of tmp)
                      (if (si:char-unicode-p) 2 1))))))
  => t
  => t
  => t
  => t)

(deftest r5b-session-file-p-old-form ()
  (r5b-with-temp
   #'(lambda (tmp)
       (r5b-write-bytes tmp (append (r5b-ascii ";;; xyzzy session file.")
                                    '(#x0d #x0a)
                                    (r5b-ascii ";;; This file is generated.")
                                    '(#x0d #x0a)))
       (editor::session-file-p tmp)))
  => t)

(deftest r5b-session-file-p-old-form-lf ()
  (r5b-with-temp
   #'(lambda (tmp)
       (r5b-write-bytes tmp (append (r5b-ascii ";;; xyzzy session file.")
                                    '(#x0a)))
       (editor::session-file-p tmp)))
  => t)

(deftest r5b-session-file-p-new-form ()
  (r5b-with-temp
   #'(lambda (tmp)
       (r5b-write-bytes tmp (append (r5b-mark-bytes)
                                    '(#x0d #x0a)
                                    (r5b-ascii ";;; xyzzy session file.")
                                    '(#x0d #x0a)))
       (editor::session-file-p tmp)))
  => t)

(deftest r5b-session-file-p-mark-without-magic ()
  (r5b-with-temp
   #'(lambda (tmp)
       (r5b-write-bytes tmp (append (r5b-mark-bytes)
                                    '(#x0d #x0a)
                                    (r5b-ascii "(setq a 1)")
                                    '(#x0d #x0a)))
       (editor::session-file-p tmp)))
  => nil)

(deftest r5b-session-file-p-not-a-session-file ()
  (r5b-with-temp
   #'(lambda (tmp)
       (r5b-write-bytes tmp (append (r5b-ascii "(setq a 1)") '(#x0a)
                                    (r5b-ascii ";;; xyzzy session file.")
                                    '(#x0a)))
       (editor::session-file-p tmp)))
  => nil)

(deftest r5b-session-file-p-empty-file ()
  (r5b-with-temp
   #'(lambda (tmp)
       (r5b-write-bytes tmp nil)
       (editor::session-file-p tmp)))
  => nil)

(deftest r5b-load-session-refuses-a-non-session-file ()
  (r5b-with-temp
   #'(lambda (tmp)
       (r5b-write-bytes tmp (r5b-ascii "(setq a 1)"))
       (handler-case (progn (editor::load-session tmp t) :loaded)
         (error () :refused))))
  => :refused)
