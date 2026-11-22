;;; -*- mode: lisp; package: user -*-
;;;
;;; Tests for the width of Char (S4 stage W).  A Char is 32bit, and the values
;;; of the characters are still the internal codes of xyzzy.
;;;
;;; What these tests watch:
;;;   - "no character" is not the value #xffff any more.  A character whose
;;;     value is #xffff is an ordinary character.
;;;   - the tables of 16bit values (windows code pages, ...) still give the
;;;     same characters, and the bytes that no table covers still come back.
;;;   - a file opened and saved gives the same bytes: cp932, UTF-8 (also a
;;;     character beyond the BMP), EUC-JP, the 8bit encodings.
;;;   - the bitmaps of regexp character classes, in which every word has the
;;;     width of Char, still work at the first and the last bit of a word.
;;;   - a key read as text never gives a value that no table can take.
;;;
;;; The tests write values with numbers and ask for characters by their
;;; Unicode (unicode-char), so that this file stays ASCII and passes the same
;;; way after the characters become Unicode (V).

(in-package :user)

(defun char-width-write-bytes (path bytes)
  (with-open-file (s path
                     :direction :output
                     :encoding :binary
                     :if-exists :supersede)
    (dolist (b bytes)
      (write-char (code-char b) s))))

(defun char-width-read-bytes (path)
  (with-open-file (s path
                     :direction :input
                     :encoding :binary)
    (let ((r nil) c)
      (while (setq c (read-char s nil nil))
        (push (char-code c) r))
      (nreverse r))))

(defun char-width-first-diff (a b)
  (let ((i 0))
    (loop
      (cond ((and (null a) (null b)) (return nil))
            ((or (null a) (null b) (/= (car a) (car b)))
             (return (list i (car a) (car b)))))
      (setq a (cdr a) b (cdr b))
      (incf i))))

(defun char-width-roundtrip (bytes encoding)
  "Write BYTES to a file, open it as ENCODING, save it and return
(the-text-of-the-buffer the-bytes-read-back)."
  (let ((tmp (make-temp-file-name))
        (buf nil))
    (unwind-protect
        (progn
          (char-width-write-bytes tmp bytes)
          (setq buf (ed::find-file-internal tmp t encoding nil t))
          (save-excursion
            (set-buffer buf)
            (let ((text (buffer-substring (point-min) (point-max))))
              (set-buffer-modified-p t)
              (save-buffer)
              (list text (char-width-read-bytes tmp)))))
      (when buf
        (delete-buffer buf))
      (when (file-exist-p tmp)
        (delete-file tmp)))))

(defun char-width-check-roundtrip (bytes encoding)
  "nil if the bytes come back the same, otherwise (offset expected actual)."
  (char-width-first-diff bytes (cadr (char-width-roundtrip bytes encoding))))

(defun char-width-range (from to)
  (let ((r nil))
    (while (<= from to)
      (push from r)
      (incf from))
    (nreverse r)))

;;;; "no character" is not a character value

(deftest char-width-ffff-is-an-ordinary-character ()
  ;; #xffff used to be the value of "no character" in the tables.
  (let* ((c (code-char #xffff))
         (s (string c)))
    (list (char-code c)
          (length s)
          (char-code (char s 0))
          (eql c (char s 0))
          (char-code (char (concat "a" s "b") 1))))
  => (#xffff 1 #xffff t #xffff))

(deftest char-width-fffe-and-ffff-in-a-buffer ()
  (with-temp-buffer
    (insert (code-char #xfffe) (code-char #xffff) #\a)
    (list (char-code (char-after (point-min)))
          (char-code (char-after (+ (point-min) 1)))
          (char-code (char-after (+ (point-min) 2)))))
  => (#xfffe #xffff 97))

(deftest char-width-code-char-limits ()
  (list (characterp (code-char 0))
        (characterp (code-char (- char-code-limit 1)))
        (char-code (code-char (- char-code-limit 1))))
  => (t t #.(- char-code-limit 1)))

;;;; 8bit encodings: the tables of 16bit values

(defun char-width-8bit-roundtrip (encoding bytes)
  (char-width-check-roundtrip (append bytes (list 10)) encoding))

(deftest char-width-iso-8859-roundtrip ()
  ;; every byte of the upper half is defined in these code pages
  (let ((bad nil))
    (dolist (e (list *encoding-iso-8859-1* *encoding-iso-8859-2*
                     *encoding-iso-8859-3* *encoding-iso-8859-4*
                     *encoding-iso-8859-5* *encoding-iso-8859-7*
                     *encoding-iso-8859-9* *encoding-iso-8859-10*
                     *encoding-iso-8859-13*))
      (let ((d (char-width-8bit-roundtrip e (char-width-range #xa0 #xff))))
        (when d
          (push (list (char-encoding-name e) d) bad))))
    (nreverse bad))
  => nil)

(deftest char-width-koi8-roundtrip ()
  (list (char-width-8bit-roundtrip *encoding-koi8r* (char-width-range #x80 #xff))
        (char-width-8bit-roundtrip *encoding-koi8u* (char-width-range #x80 #xff)))
  => (nil nil))

(deftest char-width-windows-codepage-defined-bytes-roundtrip ()
  ;; The bytes that a Windows code page does not define are left out.
  (let ((bad nil))
    (dolist (x (list (cons *encoding-windows-latin1* '(#x81 #x8d #x8f #x90 #x9d))
                     (cons *encoding-windows-cyrillic* '(#x98))))
      (let* ((e (car x))
             (bytes (remove-if (lambda (b) (member b (cdr x)))
                               (char-width-range #x80 #xff)))
             (d (char-width-8bit-roundtrip e bytes)))
        (when d
          (push (list (char-encoding-name e) d) bad))))
    (nreverse bad))
  => nil)

(deftest char-width-windows-codepage-undefined-bytes-decode ()
  ;; An undefined byte is decoded (to a character), and the text is the same
  ;; length as the file.
  (let ((r (char-width-roundtrip (list #x81 #x8d #x8f #x90 #x9d 10)
                                 *encoding-windows-latin1*)))
    (length (car r)))
  => 6)

;;;; cp932, EUC-JP, UTF-8

(defvar *char-width-sjis-bytes*
  ;; "A" + hiragana, kanji, katakana (full width), half width katakana, "z"
  '(#x41 #x82 #xa0 #x82 #xa2 #x93 #xfa #x96 #x7b #x83 #x41 #xb1 #xb2 #xb3 #x7a #x0a))

(defvar *char-width-euc-jp-bytes*
  '(#xc6 #xfc #xcb #xdc #xb8 #xec #xa5 #xc6 #xa5 #xad #xa5 #xb9 #xa5 #xc8
    #xa1 #xa7 #xb4 #xc1 #xbb #xfa #xa4 #xc8 #xa5 #xab #xa5 #xca #x20 #xa5 #xa2
    #xa5 #xa4 #xa5 #xa6 #x20 #x41 #x42 #x43 #x20 #x31 #x32 #x33 #x0a))

(defvar *char-width-utf8-bytes*
  '(#xe6 #x97 #xa5 #xe6 #x9c #xac #xe8 #xaa #x9e #xe3 #x83 #x86 #xe3 #x82 #xad
    #xe3 #x82 #xb9 #xe3 #x83 #x88 #xef #xbc #x9a #xe6 #xbc #xa2 #xe5 #xad #x97
    #xe3 #x81 #xa8 #xe3 #x82 #xab #xe3 #x83 #x8a #x20 #xef #xbd #xb1 #xef #xbd
    #xb2 #xef #xbd #xb3 #x20 #x41 #x42 #x43 #x20 #x31 #x32 #x33 #x0a))

;; "a" U+1F600 "b" U+20BB7: beyond the BMP
(defvar *char-width-utf8-astral-bytes*
  '(#x61 #xf0 #x9f #x98 #x80 #x62 #xf0 #xa0 #xae #xb7 #x0a))

(deftest char-width-sjis-roundtrip-explicit ()
  (char-width-check-roundtrip *char-width-sjis-bytes* *encoding-sjis*)
  => nil)

(deftest char-width-sjis-roundtrip-auto ()
  (char-width-check-roundtrip *char-width-sjis-bytes* nil)
  => nil)

(deftest char-width-euc-jp-roundtrip-explicit ()
  (char-width-check-roundtrip *char-width-euc-jp-bytes* *encoding-euc-jp*)
  => nil)

(deftest char-width-euc-jp-roundtrip-auto ()
  (char-width-check-roundtrip *char-width-euc-jp-bytes* nil)
  => nil)

(deftest char-width-utf8-roundtrip-explicit ()
  (char-width-check-roundtrip *char-width-utf8-bytes* *encoding-utf8n*)
  => nil)

(deftest char-width-utf8-roundtrip-auto ()
  (char-width-check-roundtrip *char-width-utf8-bytes* nil)
  => nil)

(deftest char-width-utf8-astral-roundtrip ()
  (char-width-check-roundtrip *char-width-utf8-astral-bytes* *encoding-utf8n*)
  => nil)

(deftest char-width-utf8-bom-roundtrip ()
  (char-width-check-roundtrip (append '(#xef #xbb #xbf) *char-width-utf8-bytes*)
                              *encoding-utf8*)
  => nil)

(deftest char-width-utf16-roundtrip ()
  ;; "a", U+3042, U+1F600 (a pair of surrogates), LF
  (let ((le '(#xff #xfe #x61 #x00 #x42 #x30 #x3d #xd8 #x00 #xde #x0a #x00)))
    (char-width-check-roundtrip le *encoding-utf16le-bom*))
  => nil)

(deftest char-width-utf8-text-values ()
  ;; the text of the Japanese file is the same as the one of the sjis file for
  ;; the characters they share, and ASCII is ASCII
  (let ((u (car (char-width-roundtrip *char-width-utf8-bytes* *encoding-utf8n*))))
    (list (length u)
          (char-code (char u (- (length u) 2)))
          (eql (char u (- (length u) 3)) #\2)))
  => (26 #x33 t))

;;;; the bytes of unknown sequences are kept

(deftest char-width-sjis-invalid-bytes-roundtrip ()
  ;; a lead byte followed by a byte that is not a trail byte, a lone trail byte
  (char-width-check-roundtrip '(#x41 #x81 #x20 #x42 #x0a) *encoding-sjis*)
  => nil)

;;;; writers

(defun char-width-encode-chars (chars encoding)
  "The bytes that the characters (a list of codes) become in ENCODING."
  (let ((tmp (make-temp-file-name))
        (bytes nil))
    (unwind-protect
        (progn
          (with-temp-buffer
            (dolist (c chars)
              (insert (code-char c)))
            (write-region (point-min) (point-max) tmp nil encoding))
          (setq bytes (char-width-read-bytes tmp)))
      (when (file-exist-p tmp)
        (delete-file tmp)))
    bytes))

(deftest char-width-iso-2022-cn-big5-char-without-cns ()
  ;; BUG-0057: a Big5 character that CNS 11643 and GB 2312 do not have was
  ;; written as garbage (8bit bytes in a 7bit encoding).  U+FF64 is one of them.
  (let ((c (char-code (unicode-char #xff64))))
    (list (every #'(lambda (b) (< b #x80))
                 (char-width-encode-chars (list c) *encoding-iso-2022-cn*))
          (every #'(lambda (b) (< b #x80))
                 (char-width-encode-chars (list (char-code #\a) c (char-code #\b))
                                          *encoding-iso-2022-cn*))))
  => (t t))
;;;; regexp character classes (the bitmap words have the width of Char)

(defun char-width-class-test-codes (clo chi)
  "For every character that can be in a string, does [clo-chi] agree with the
range?  Returns the list of the characters that do not.  (The negated class
[^clo-chi] is checked for the characters below 256 only: BUG-0056.)"
  (let* ((re (concat "[" (string (code-char clo)) "-" (string (code-char chi)) "]"))
         (nre (concat "[^" (string (code-char clo)) "-" (string (code-char chi)) "]"))
         (bad nil))
    (dolist (i (char-width-class-sample clo chi))
      (let* ((c (code-char i))
             (s (and c (string c))))
        (when s
          (let ((in (and (<= clo i) (<= i chi))))
            (unless (eq (and (string-match re s) t) in)
              (push (list :in i) bad))
            (when (< i 256)
              (unless (eq (and (string-match nre s) t) (not in))
                (push (list :not i) bad)))))))
    (nreverse bad)))

(defun char-width-class-sample (clo chi)
  "The codes to try: the first two rows, the edge of every word of every row,
and the neighbourhood of both ends of the range."
  (let ((r nil))
    (dotimes (i 512)
      (push i r))
    (let ((row 2))
      (while (< (* row 256) char-code-limit)
        (dolist (off '(0 1 15 16 31 32 47 48 63 64 95 96 127 128 191 192 255))
          (push (+ (* row 256) off) r))
        (incf row)))
    (dolist (e (list clo chi))
      (let ((i (- e 40)))
        (while (<= i (+ e 40))
          (when (and (>= i 0) (< i char-code-limit))
            (push i r))
          (incf i))))
    (sort (delete-duplicates r) #'<)))

(defun char-width-class-test (lo hi)
  "The same with the characters of the Unicode code points LO and HI."
  (char-width-class-test-codes (char-code (unicode-char lo))
                               (char-code (unicode-char hi))))

(deftest char-width-class-bit-31-of-a-word ()
  ;; #x1f, #x3f, #x5f and #x7f are the last bit of a 32bit word
  (list (char-width-class-test #x3f #x5f)
        (char-width-class-test #x20 #x7e))
  => (nil nil))

(deftest char-width-class-word-edges ()
  ;; bits 15/16 and 31/32 of a word of the bitmap
  (list (char-width-class-test #x4f #x50)
        (char-width-class-test #x5f #x60)
        (char-width-class-test #x30 #x30)
        (char-width-class-test #x7e #x7e))
  => (nil nil nil nil))

(deftest char-width-class-wide-range ()
  ;; hiragana, Greek and Cyrillic: the bitmap rows other than the first
  (list (char-width-class-test #x3041 #x3093)
        (char-width-class-test #x0391 #x03c9)
        (char-width-class-test #x0410 #x044f))
  => (nil nil nil))

(deftest char-width-class-whole-row-all-bits ()
  ;; A class that covers a whole row of 256 characters: every word is all ones.
  (let ((c1 (string (code-char #x2100)))
        (c2 (string (code-char #x21ff)))
        (c3 (string (code-char #x20ff)))
        (c4 (string (code-char #x2200))))
    (let ((re (concat "[" c1 "-" c2 "]")))
      (list (and (string-match re c1) t)
            (and (string-match re c2) t)
            (and (string-match re (string (code-char #x2180))) t)
            (and (string-match re c3) t)
            (and (string-match re c4) t))))
  => (t t t nil nil))

(deftest char-width-class-several-rows ()
  ;; a class over three rows (the first and the last are partial)
  (char-width-class-test-codes #x20f0 #x2210)
  => nil)

;;;; keys read as text

(defun char-width-key-text (keys)
  (dolist (k keys)
    (editor::*kbd-queue-put k))
  (editor::*kbd-queue-put #\LFD)
  (read-line *keyboard*))

(deftest char-width-key-read-line-is-text ()
  ;; A key is not text: read as text it must never give a value that is not
  ;; a character of a string.
  (let ((s (char-width-key-text (list #\a #\F1 #\M-x #\C-S-F12 #\M-F1 #\LBtnDown))))
    (list (length s)
          (every #'(lambda (c) (< (char-code c) char-code-limit)) s)
          (char-code (char s 0))))
  => (6 t 97))

(deftest char-width-key-echo-stream-is-text ()
  (editor::*kbd-queue-put #\a)
  (editor::*kbd-queue-put #\F1)
  (editor::*kbd-queue-put #\M-x)
  (editor::*kbd-queue-put #\LFD)
  (let* ((out (make-string-output-stream))
         (e (make-echo-stream *keyboard* out))
         (s (read-line e))
         (echoed (get-output-stream-string out)))
    (list (length s)
          (every #'(lambda (c) (< (char-code c) char-code-limit)) s)
          (every #'(lambda (c) (< (char-code c) char-code-limit)) echoed)
          (string= s (subseq echoed 0 (length s)))))
  => (3 t t t))

(deftest char-width-key-read-symbol-is-text ()
  (editor::*kbd-queue-put #\a)
  (editor::*kbd-queue-put #\F1)
  (editor::*kbd-queue-put #\M-x)
  (editor::*kbd-queue-put #\SPC)
  (let ((s (read *keyboard*)))
    (list (symbolp s)
          (every #'(lambda (c) (< (char-code c) char-code-limit))
                 (symbol-name s))))
  => (t t))

(deftest char-width-read-char-still-gives-the-key ()
  (editor::*kbd-queue-put #\a)
  (editor::*kbd-queue-put #\F1)
  (editor::*kbd-queue-put #\M-x)
  (list (read-char *keyboard*) (read-char *keyboard*) (read-char *keyboard*))
  => (#\a #\F1 #\M-x))

;;;; documentation (etc/DOC keeps the Char array of the documentation)

(deftest char-width-documentation-string-from-doc-file ()
  ;; The documentation of the built-in functions is in etc/DOC.
  (let ((d (documentation 'apropos 'function)))
    (or (null d) (stringp d)))
  => t)

;;;; strings and buffers keep every value below char-code-limit

(deftest char-width-string-of-all-characters ()
  (let* ((s (with-output-to-string (o)
              (dotimes (i char-code-limit)
                (write-char (code-char i) o))))
         (bad nil))
    (dotimes (i char-code-limit)
      (unless (= (char-code (char s i)) i)
        (push i bad)))
    (list (length s) (length bad)))
  => (#.char-code-limit 0))

(deftest char-width-buffer-of-all-characters ()
  (with-temp-buffer
    (let ((bad 0))
      (dotimes (i char-code-limit)
        (insert (code-char i)))
      (goto-char (point-min))
      (dotimes (i char-code-limit)
        (unless (= (char-code (char-after (point))) i)
          (incf bad))
        (forward-char 1))
      (list (buffer-size) bad)))
  => (#.char-code-limit 0))
