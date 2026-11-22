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

;;; ---------------------------------------------------------------------
;;; The old format (design 9.4)
;;;
;;; A file of the format 1 has the values of the old internal code in the
;;; escapes of the strings (\xNN, \XNNNN, \NNN) and of the characters
;;; (#\xNNNN, #\NNN).  The text itself is in the encoding of the mark (UTF-8
;;; from R5 on), or cp932 without the mark.  legacy-data-text-to-unicode
;;; rewrites the escapes to the values of today with
;;; si:legacy-internal-to-unicode (the old table: pairs are joined, the old
;;; keys are the keys of today, no value is U+FFFD).  A string with an old
;;; key (a keyboard macro before S4-k) becomes a vector of characters,
;;; because a key never lives in a string.  The reader of today then reads
;;; the text.  The rewrite only looks at the text that xyzzy writes with
;;; write :readably: strings, characters, comments, |symbols| and the
;;; escape of a symbol; the rest is copied as it is.

(defun legacy-data-hex (n width)
  (let ((s (string-downcase (format nil "~x" n))))
    (while (< (length s) width)
      (setq s (concat "0" s)))
    s))

(defun legacy-data-char-syntax (u)
  "The syntax of a character of the value U of today."
  (cond ((>= u #x1000000)
         (let ((c (code-char u)))
           (if c (prin1-to-string c) "#\\xfffd")))
        ((< u #x10000)
         (concat "#\\x" (legacy-data-hex u 4)))
        (t
         (let ((c (code-char u)))
           (if c (concat "#\\" (string c)) "#\\xfffd")))))

(defun legacy-data-string-char (u)
  "U (a value of today, not a key) in a string."
  (if (< u #x10000)
      (concat "\\X" (legacy-data-hex u 4))
    (let ((c (code-char u)))
      (if c (string c) "\\Xfffd"))))

(defun legacy-data-delimiter-p (c)
  (or (<= (char-code c) 32)
      (find c "()\"';`,")))

(defun legacy-data-digits (text start end radix max)
  "The value of up to MAX digits of RADIX at START, and the end."
  (let ((k start) (v 0))
    (while (and (< k end) (< (- k start) max)
                (digit-char-p (char text k) radix))
      (setq v (+ (* v radix) (digit-char-p (char text k) radix)))
      (setq k (1+ k)))
    (values (and (> k start) v) k)))

(defun legacy-data-string-items (text start)
  "The items of the string at START (after the opening quote): a list of
(:char character raw-text) and (:code old-value), and the end."
  (let ((n (length text)) (k start) (items nil))
    (loop
      (when (>= k n)
        (return))
      (let ((code (char-code (char text k))))
        (cond ((= code 34)              ; "
               (setq k (1+ k))
               (return))
              ((and (= code 92) (< (1+ k) n)) ; \
               (let* ((d (char text (1+ k)))
                      (dc (char-code d)))
                 (cond ((or (= dc 120) (= dc 88)) ; x X
                        (multiple-value-bind (v e)
                            (legacy-data-digits text (+ k 2) n 16 (if (= dc 120) 2 4))
                          (cond (v
                                 (push (list :code v) items)
                                 (setq k e))
                                (t
                                 (push (list :char d (subseq text k (+ k 2))) items)
                                 (setq k (+ k 2))))))
                       ((<= 48 dc 55)   ; 0-7
                        (multiple-value-bind (v e)
                            (legacy-data-digits text (+ k 2) n 8 2)
                          (let ((x (- dc 48)))
                            (when v
                              (setq x (+ (* x (expt 8 (- e k 2))) v)))
                            (push (list :code x) items)
                            (setq k e))))
                       (t
                        (push (list :char
                                    (case dc
                                      (97 (code-char 7)) (98 (code-char 8))
                                      (102 (code-char 12)) (110 (code-char 10))
                                      (114 (code-char 13)) (116 (code-char 9))
                                      (118 (code-char 11)) (63 (code-char 127))
                                      (10 nil)
                                      (t d))
                                    (subseq text k (+ k 2)))
                              items)
                        (setq k (+ k 2))))))
              (t
               (push (list :char (char text k) (string (char text k))) items)
               (setq k (1+ k))))))
    (values (nreverse items) k)))

(defun legacy-data-translate-items (items)
  "Replaces the runs of (:code old-value) with (:value value-of-today)."
  (let ((r nil) (run nil))
    (flet ((flush ()
             (when run
               (dolist (u (si:legacy-internal-to-unicode (nreverse run)))
                 (push (list :value u) r))
               (setq run nil))))
      (dolist (item items)
        (if (eq (car item) :code)
            (push (cadr item) run)
          (progn
            (flush)
            (push item r))))
      (flush))
    (nreverse r)))

(defun legacy-data-string (text j o)
  "Rewrites the string at J (the opening quote) to O and returns the end."
  (multiple-value-bind (items end)
      (legacy-data-string-items text (1+ j))
    (setq items (legacy-data-translate-items items))
    (cond ((find-if #'(lambda (x) (and (eq (car x) :value)
                                       (>= (cadr x) #x1000000)))
                    items)
           ;; An old keyboard macro: a vector of characters.
           (princ "#(" o)
           (let ((first t))
             (dolist (x items)
               (let ((u (if (eq (car x) :value)
                            (cadr x)
                          (and (cadr x) (char-code (cadr x))))))
                 (when u
                   (unless first
                     (princ " " o))
                   (setq first nil)
                   (princ (legacy-data-char-syntax u) o)))))
           (princ ")" o))
          (t
           (princ "\"" o)
           (dolist (x items)
             (princ (if (eq (car x) :value)
                        (legacy-data-string-char (cadr x))
                      (caddr x))
                    o))
           (princ "\"" o)))
    end))

(defun legacy-data-sharp (text j o)
  "Rewrites #\\xNNNN and #\\NNN at J; copies any other # as it is."
  (let ((n (length text)))
    (cond ((and (< (1+ j) n) (= (char-code (char text (1+ j))) 92))
           (let* ((s (+ j 2)) (k s))
             (when (< k n)
               (setq k (if (and (= (char-code (char text k)) 92) (< (1+ k) n))
                           (+ k 2)
                         (1+ k)))
               (while (and (< k n) (not (legacy-data-delimiter-p (char text k))))
                 (setq k (if (and (= (char-code (char text k)) 92) (< (1+ k) n))
                             (+ k 2)
                           (1+ k)))))
             (let* ((token (subseq text s k))
                    (v (cond ((and (> (length token) 1)
                                   (string-match "\\`[xX][0-9a-fA-F]+\\'" token))
                              (parse-integer token :start 1 :radix 16))
                             ((and (> (length token) 1)
                                   (string-match "\\`[0-7]+\\'" token))
                              (parse-integer token :radix 8)))))
               (if v
                   (princ (legacy-data-char-syntax
                           (car (si:legacy-internal-to-unicode (list v))))
                          o)
                 (princ (subseq text j k) o)))
             k))
          (t
           (princ "#" o)
           (1+ j)))))

(defun legacy-data-bars (text j o)
  "Copies |...| at J (a symbol name) and returns the end."
  (let ((n (length text)) (k (1+ j)))
    (loop
      (when (>= k n)
        (return))
      (let ((code (char-code (char text k))))
        (cond ((= code 92) (setq k (+ k 2)))
              ((= code 124) (setq k (1+ k)) (return))
              (t (setq k (1+ k))))))
    (setq k (min k n))
    (princ (subseq text j k) o)
    k))

(defun legacy-data-text-to-unicode (text)
  "TEXT is the text of a data file of the format 1.  Returns the text with
the values of today in the escapes (see above)."
  (let ((n (length text)) (i 0))
    (with-output-to-string (o)
      (loop
        (let ((j (string-match "\\([\"#;|]\\|\\\\\\)" text i)))
          (unless j
            (princ (subseq text i) o)
            (return))
          (princ (subseq text i j) o)
          (setq i
                (case (char-code (char text j))
                  (34 (legacy-data-string text j o))
                  (35 (legacy-data-sharp text j o))
                  (59 (let ((e (position (code-char 10) text :start j)))
                        (setq e (if e (1+ e) n))
                        (princ (subseq text j e) o)
                        e))
                  (124 (legacy-data-bars text j o))
                  (t (let ((e (min n (+ j 2))))
                       (princ (subseq text j e) o)
                       e)))))))))

(defun legacy-data-file-encoding (file)
  "The encoding of the text of FILE of the format 1: the Encoding of the mark
\(UTF-8 from R5 on), or cp932."
  (let* ((params (data-file-params file))
         (name (cdr (or (assoc "encoding" params :test #'string=)
                        (assoc "coding" params :test #'string=))))
         (encoding (and name (gethash name *mime-charset-name-hash-table*))))
    (if (char-encoding-p encoding)
        encoding
      *encoding-sjis*)))

(defun read-data-file-text (file encoding if-does-not-exist)
  (with-open-file (s file :direction :input :char-encoding encoding
                     :if-does-not-exist if-does-not-exist)
    (when s
      (with-output-to-string (o)
        (loop
          (multiple-value-bind (line missing-newline-p)
              (read-line s nil)
            (unless line
              (return))
            (princ line o)
            (unless missing-newline-p
              (terpri o))))))))

(defun load-legacy-data-file (file &key (if-does-not-exist :error) verbose print)
  "Loads FILE of the format 1 (the old internal code) on an xyzzy of Unicode.
VERBOSE is not used (the file is read from a string)."
  (let ((text (read-data-file-text file (legacy-data-file-encoding file)
                                   if-does-not-exist)))
    (when text
      (with-input-from-string (s (legacy-data-text-to-unicode text))
        (load s :verbose nil :print print))
      t)))
