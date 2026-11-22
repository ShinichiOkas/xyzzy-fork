;;; -*- Mode: Lisp; Package: SYSTEM -*-
;;;
;;; This file is part of xyzzy.
;;;

(in-package "system")

(provide "charname")
(require "cnamedef")

;; The charsets that give a prefix to a name: #\JISX0208_HIRAGANA_LETTER_A
(defvar *character-name-charsets*
  '(:us-ascii :jisx0201-kana
    :iso8859-1 :iso8859-2 :iso8859-3 :iso8859-4 :iso8859-5
    :iso8859-7 :iso8859-9 :iso8859-10 :iso8859-13
    :jisx0208 :jisx0212 :gb2312 :ksc5601 :big5))

;; A char is a Unicode code point (si:char-unicode-p): a name is the name of
;; the code point of cnamedef.l, and a name with the prefix of a charset is
;; there for every charset that has the code point.
(defun make-character-name-hash-table-unicode ()
  (let ((h (make-hash-table :size 1 :test 'equal)))
    (dolist (x *character-name-alist*)
      (let ((c (ed:unicode-char (cdr x))))
	(setf (gethash (car x) h) c)
	(dolist (cs *character-name-charsets*)
	  (when (si:unicode-iso-code (cdr x) cs)
	    (setf (gethash (concatenate 'string (string-upcase (symbol-name cs))
					"_" (car x))
			   h)
		  c)))))
    h))

(defun make-character-name-hash-table ()
  (if (si:char-unicode-p)
      (make-character-name-hash-table-unicode)
    (let ((v (make-vector 65536))
	  (h (make-hash-table :size 1 :test 'equal)))
      (dolist (x *character-name-alist*)
	(setf (svref v (cdr x)) (car x))
	(setf (gethash (car x) h) (ed:unicode-char (cdr x))))
      (dotimes (i char-code-limit)
	(let* ((c (code-char i))
	       (u (ed:char-unicode c)))
	  (when (and u (svref v u))
	    (let ((cs (ed:iso-char-charset c)))
	      (when cs
		(setf (gethash (concatenate 'string (symbol-name cs)
					    "_" (svref v u)) h)
		      (code-char i)))))))
      h)))

(setq *character-name-hash-table*
      (make-character-name-hash-table))
