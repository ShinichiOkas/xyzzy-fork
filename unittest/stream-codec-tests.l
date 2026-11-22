;;; -*- mode: lisp; package: user -*-
;;;
;;; Tests of the converter of the file streams (S4 R4: stream.cc) and of the
;;; encoding of a file that is loaded (design 4.4, 4.5).
;;;
;;; The tests do not depend on the value of a Char (the internal code of today
;;; or Unicode): a character is looked at through char-unicode, and bytes
;;; are written as numbers.  This file is ASCII.

(in-package :user)

(defvar *r4-v* nil)

(defun r4-write-bytes (path bytes)
  (with-open-file (s path
                     :direction :output
                     :encoding :binary
                     :if-exists :supersede)
    (dolist (b bytes)
      (write-char (code-char b) s))))

(defun r4-read-bytes (path)
  (with-open-file (s path :direction :input :encoding :binary)
    (let ((r nil) c)
      (while (setq c (read-char s nil nil))
        (push (char-code c) r))
      (nreverse r))))

(defun r4-ascii (string)
  (map 'list #'char-code string))

(defun r4-unicodes (string)
  (map 'list #'char-unicode string))

(defun r4-with-temp (fn)
  (let ((tmp (make-temp-file-name)))
    (unwind-protect
        (funcall fn tmp)
      (when (file-exist-p tmp)
        (delete-file tmp)))))

(defun r4-load-value (bytes)
  "Writes BYTES to a file, loads it and returns the characters of *r4-v* as Unicode."
  (r4-with-temp
   #'(lambda (tmp)
       (setq *r4-v* nil)
       (r4-write-bytes tmp bytes)
       (load tmp :verbose nil)
       (r4-unicodes *r4-v*))))

(defun r4-setq (bytes)
  "The bytes of (setq user::*r4-v* \"...\") with BYTES inside the string."
  (append (r4-ascii "(setq user::*r4-v* \"") bytes (r4-ascii "\")")))

;; U+3042 (a hiragana) = E3 81 82 in UTF-8, 82 A0 in cp932, A4 A2 in EUC-JP.
;; U+6F22 (a kanji) = E6 BC A2, 8A BF, B4 C1.

;;; ---------------------------------------------------------------------
;;; load: the encoding of the file

(deftest r4-load-utf8-without-bom ()
  (r4-load-value (r4-setq '(#xe3 #x81 #x82 #xe6 #xbc #xa2)))
  => (#x3042 #x6f22))

(deftest r4-load-utf8-with-bom ()
  (r4-load-value (append '(#xef #xbb #xbf)
                         (r4-setq '(#xe3 #x81 #x82 #xe6 #xbc #xa2))))
  => (#x3042 #x6f22))

(deftest r4-load-utf8-with-mark ()
  (r4-load-value (append (r4-ascii ";;; -*- Mode: Lisp; Encoding: utf-8 -*-")
                         '(#x0a)
                         (r4-setq '(#xe3 #x81 #x82))))
  => (#x3042))

(deftest r4-load-utf8-mark-on-the-second-line ()
  (r4-load-value (append (r4-ascii ";; first line")
                         '(#x0a)
                         (r4-ascii ";; -*- coding: utf-8 -*-")
                         '(#x0d #x0a)
                         (r4-setq '(#xe3 #x81 #x82))))
  => (#x3042))

(deftest r4-load-cp932 ()
  (r4-load-value (r4-setq '(#x82 #xa0 #x8a #xbf)))
  => (#x3042 #x6f22))

(deftest r4-load-cp932-with-mark ()
  (r4-load-value (append (r4-ascii ";;; -*- Encoding: sjis -*-")
                         '(#x0a)
                         (r4-setq '(#x82 #xa0 #x8a #xbf))))
  => (#x3042 #x6f22))

(deftest r4-load-ascii-only ()
  (r4-load-value (r4-setq (r4-ascii "abc")))
  => (#x61 #x62 #x63))

;; C3 A9 is valid UTF-8 (U+00E9) and is two half width kana in cp932.  The
;; mark has priority over the whole-file check.
(deftest r4-load-valid-utf8-is-utf8 ()
  (r4-load-value (r4-setq '(#xc3 #xa9)))
  => (#xe9))

(deftest r4-load-mark-has-priority-over-utf8-check ()
  (r4-load-value (append (r4-ascii ";;; -*- Encoding: sjis -*-")
                         '(#x0a)
                         (r4-setq '(#xc3 #xa9))))
  => (#xff83 #xff69))

;; 82 A0 and a lone E3: not valid UTF-8, so cp932.
(deftest r4-load-invalid-utf8-is-cp932 ()
  (r4-load-value (r4-setq '(#x82 #xa0 #x41)))
  => (#x3042 #x41))

(deftest r4-load-utf16le-bom ()
  (r4-load-value (append '(#xff #xfe)
                        (mapcan #'(lambda (b) (list b 0))
                                (r4-setq (r4-ascii "AB")))))
  => (#x41 #x42))

(deftest r4-load-crlf-lines ()
  (r4-load-value (append (r4-ascii "(setq user::*r4-v* ")
                         '(#x0d #x0a)
                         (r4-ascii "\"x\")")))
  => (#x78))

;;; ---------------------------------------------------------------------
;;; :binary

(deftest r4-binary-all-bytes-roundtrip ()
  (r4-with-temp
   #'(lambda (tmp)
       (let ((bytes (let ((r nil)) (dotimes (i 256) (push i r)) (nreverse r))))
         (r4-write-bytes tmp bytes)
         (list (equal (r4-read-bytes tmp) bytes)
               (length (r4-read-bytes tmp))))))
  => (t 256))

(deftest r4-binary-ignores-char-encoding ()
  (r4-with-temp
   #'(lambda (tmp)
       (with-open-file (s tmp :direction :output :encoding :binary
                          :char-encoding *encoding-utf8n*
                          :if-exists :supersede)
         (write-char (code-char #xe9) s)
         (write-char (code-char #x41) s))
       (r4-read-bytes tmp)))
  => (#xe9 #x41))

;;; ---------------------------------------------------------------------
;;; open :char-encoding

(defun r4-hiragana-a ()
  (convert-encoding-to-internal *encoding-utf8n*
                                (map 'string #'code-char '(#xe3 #x81 #x82))))

(defun r4-write-with (encoding string &optional (eol :canonical))
  (r4-with-temp
   #'(lambda (tmp)
       (with-open-file (s tmp :direction :output :encoding eol
                          :char-encoding encoding
                          :if-exists :supersede)
         (write-string string s))
       (r4-read-bytes tmp))))

(defun r4-read-with (encoding bytes &optional (eol :canonical))
  (r4-with-temp
   #'(lambda (tmp)
       (r4-write-bytes tmp bytes)
       (with-open-file (s tmp :direction :input :encoding eol
                          :char-encoding encoding)
         (let ((r nil) c)
           (while (setq c (read-char s nil nil))
             (push (char-unicode c) r))
           (nreverse r))))))

(deftest r4-open-default-is-cp932 ()
  (r4-write-with nil (r4-hiragana-a))
  => (#x82 #xa0))

(deftest r4-open-sjis ()
  (r4-write-with *encoding-sjis* (r4-hiragana-a))
  => (#x82 #xa0))

(deftest r4-open-utf8n-write ()
  (r4-write-with *encoding-utf8n* (r4-hiragana-a))
  => (#xe3 #x81 #x82))

(deftest r4-open-utf8-bom-write ()
  (r4-write-with *encoding-utf8* (r4-hiragana-a))
  => (#xef #xbb #xbf #xe3 #x81 #x82))

(deftest r4-open-euc-jp-write ()
  (r4-write-with *encoding-euc-jp* (r4-hiragana-a))
  => (#xa4 #xa2))

(deftest r4-open-utf16le-write ()
  (r4-write-with *encoding-utf16le* (r4-hiragana-a))
  => (#x42 #x30))

(deftest r4-open-utf16le-bom-write ()
  (r4-write-with *encoding-utf16le-bom* (r4-hiragana-a))
  => (#xff #xfe #x42 #x30))

(deftest r4-open-utf8n-read ()
  (r4-read-with *encoding-utf8n* '(#xe3 #x81 #x82 #x41))
  => (#x3042 #x41))

(deftest r4-open-euc-jp-read ()
  (r4-read-with *encoding-euc-jp* '(#xa4 #xa2 #x41))
  => (#x3042 #x41))

(deftest r4-open-utf16le-read ()
  (r4-read-with *encoding-utf16le* '(#x42 #x30 #x41 #x00))
  => (#x3042 #x41))

(deftest r4-open-default-read-cp932 ()
  (r4-read-with nil '(#x82 #xa0 #xb1 #x41))
  => (#x3042 #xff71 #x41))

(deftest r4-open-canonical-eol-utf8 ()
  (list (r4-write-with *encoding-utf8n* (format nil "a~%b") :canonical)
        (r4-write-with *encoding-utf8n* (format nil "a~%b") :raw))
  => ((#x61 #x0d #x0a #x62) (#x61 #x0a #x62)))

(deftest r4-open-canonical-eol-utf16le ()
  (r4-write-with *encoding-utf16le* (format nil "a~%") :canonical)
  => (#x61 #x00 #x0d #x00 #x0a #x00))

(deftest r4-open-read-crlf ()
  (list (r4-read-with *encoding-utf8n* '(#x61 #x0d #x0a #x62) :canonical)
        (r4-read-with *encoding-utf8n* '(#x61 #x0d #x0a #x62) :raw)
        (r4-read-with *encoding-utf8n* '(#x61 #x0d #x62) :canonical))
  => ((#x61 #x0a #x62) (#x61 #x0d #x0a #x62) (#x61 #x0d #x62)))

(deftest r4-open-invalid-char-encoding ()
  (r4-with-temp
   #'(lambda (tmp)
       (open tmp :direction :output :char-encoding 1)))
  !! type-error)

(deftest r4-open-append-has-no-second-signature ()
  (r4-with-temp
   #'(lambda (tmp)
       (with-open-file (s tmp :direction :output :char-encoding *encoding-utf8*
                          :if-exists :supersede)
         (write-string "a" s))
       (with-open-file (s tmp :direction :output :char-encoding *encoding-utf8*
                          :if-exists :append)
         (write-string "b" s))
       (r4-read-bytes tmp)))
  => (#xef #xbb #xbf #x61 #x62))

(deftest r4-open-file-position-in-bytes ()
  (r4-with-temp
   #'(lambda (tmp)
       (r4-write-bytes tmp '(#xe3 #x81 #x82 #x41))
       (with-open-file (s tmp :direction :input :char-encoding *encoding-utf8n*)
         (let ((a (file-position s)))
           (read-char s)
           (let ((b (file-position s)))
             (read-char s)
             (list a b (file-position s)))))))
  => (0 3 4))

(deftest r4-open-file-position-reset ()
  (r4-with-temp
   #'(lambda (tmp)
       (r4-write-bytes tmp '(#xe3 #x81 #x82 #x41))
       (with-open-file (s tmp :direction :input :char-encoding *encoding-utf8n*)
         (read-char s)
         (file-position s 0)
         (list (char-unicode (read-char s)) (char-unicode (read-char s))))))
  => (#x3042 #x41))

(deftest r4-open-io-stream ()
  (r4-with-temp
   #'(lambda (tmp)
       (with-open-file (s tmp :direction :io :char-encoding *encoding-utf8n*
                          :if-exists :supersede)
         (write-string (r4-hiragana-a) s)
         (file-position s 0)
         (list (char-unicode (read-char s))
               (progn (file-position s :end) (file-position s))))))
  => (#x3042 3))

;; The bytes that no encoding takes are kept (BUG-0052, BUG-0053).  This is
;; true of the unicode side; with the internal code of today a byte that
;; UTF-8 does not accept is not a character of its own.
(deftest r4-open-invalid-utf8-roundtrip ()
  (if (> char-code-limit #x10000)
      (r4-with-temp
       #'(lambda (tmp)
           (let ((bytes '(#x41 #x85 #xff #xe3 #x81 #x42)))
             (r4-write-bytes tmp bytes)
             (let ((s (with-open-file (s tmp :char-encoding *encoding-utf8n*)
                        (let ((r (make-string 6)) (n 0) c)
                          (while (setq c (read-char s nil nil))
                            (setf (char r n) c)
                            (incf n))
                          (subseq r 0 n)))))
               (with-open-file (o tmp :direction :output
                                  :char-encoding *encoding-utf8n*
                                  :if-exists :supersede)
                 (write-string s o))
               (equal (r4-read-bytes tmp) bytes)))))
    t)
  => t)
