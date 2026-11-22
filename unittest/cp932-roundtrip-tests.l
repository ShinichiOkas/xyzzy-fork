;;; -*- mode: lisp; package: user -*-
;;;
;;; cp932 round trip tests (S4-0 safety net).
;;;
;;; Open a file of known bytes as sjis (cp932), save it, and check that the
;;; bytes did not change.  Each deftest covers one kind of bytes so that a
;;; broken kind can be told from the others.
;;;
;;; Two ways of opening are tried for every kind of bytes:
;;;   :explicit  ... the encoding is given (*encoding-sjis*)
;;;   :auto      ... the encoding is detected by xyzzy
;;; and two kinds of line ends: CRLF and LF.
;;;
;;; Bytes are written as numbers (#x82 ...) so that this file stays ASCII.
;;; The result of a check is :ok, or the list of the failed cases
;;; ((:diff CASE-NUMBER OFFSET EXPECTED-BYTE ACTUAL-BYTE) ...).
;;;

(in-package :user)

(defvar *cp932-rt-detected* nil
  "Last detected (encoding-name eol-code) of every :auto check, for probing.")

(defun cp932-rt-write-bytes (path bytes)
  (with-open-file (s path
                     :direction :output
                     :encoding :binary
                     :if-exists :supersede)
    (dolist (b bytes)
      (write-char (code-char b) s))))

(defun cp932-rt-read-bytes (path)
  (with-open-file (s path
                     :direction :input
                     :encoding :binary)
    (let ((r nil) c)
      (while (setq c (read-char s nil nil))
        (push (char-code c) r))
      (nreverse r))))

(defun cp932-rt-roundtrip (bytes mode)
  "Write BYTES to a file, open it, save it and return the bytes read back."
  (let ((tmp (make-temp-file-name))
        (buf nil))
    (unwind-protect
        (progn
          (cp932-rt-write-bytes tmp bytes)
          (setq buf (ed::find-file-internal tmp t
                                          (if (eq mode :explicit)
                                              *encoding-sjis*
                                            nil)
                                          nil t))
          (save-excursion
            (set-buffer buf)
            (when (eq mode :auto)
              (push (list (char-encoding-name (buffer-fileio-encoding))
                          (buffer-eol-code))
                    *cp932-rt-detected*))
            ;; Save even if the buffer is not modified.
            (set-buffer-modified-p t)
            (save-buffer))
          (cp932-rt-read-bytes tmp))
      (when buf
        (delete-buffer buf))
      (when (file-exist-p tmp)
        (delete-file tmp)))))

(defun cp932-rt-first-diff (a b)
  (let ((i 0))
    (loop
      (cond ((and (null a) (null b)) (return nil))
            ((or (null a) (null b) (/= (car a) (car b)))
             (return (list i (car a) (car b)))))
      (setq a (cdr a) b (cdr b))
      (incf i))))

(defun cp932-rt-check-bytes (cases mode)
  "CASES is a list of byte lists.  Every case is written, opened, saved and
compared.  Returns :ok or the list of the failed cases:
 (:diff CASE-NUMBER OFFSET EXPECTED-BYTE ACTUAL-BYTE)."
  (let ((n 0) (failures nil))
    (dolist (bytes cases)
      (let* ((actual (cp932-rt-roundtrip bytes mode))
             (diff (cp932-rt-first-diff bytes actual)))
        (when diff
          (push (list* :diff n diff) failures)))
      (incf n))
    (if failures
        (let ((f (nreverse failures)))
          (if (> (length f) 8) (subseq f 0 8) f))
      :ok)))

(defun cp932-rt-check (cases mode eol)
  "CASES is a list of cases.  A case is a list of rows; a row is a list of
bytes.  Rows are joined by EOL (:crlf or :lf)."
  (let ((eol-bytes (if (eq eol :crlf) '(#x0d #x0a) '(#x0a))))
    (cp932-rt-check-bytes
     (mapcar (lambda (rows)
               (mapcan (lambda (row)
                         (append row (copy-list eol-bytes)))
                       rows))
             cases)
     mode)))

(defun cp932-rt-range (from to)
  (let (r)
    (do ((b from (1+ b)))
        ((> b to))
      (push b r))
    (nreverse r)))

(defun cp932-rt-codes (lead from to)
  "Two byte codes LEAD FROM .. LEAD TO as a list of bytes."
  (let (r)
    (do ((b from (1+ b)))
        ((> b to))
      (push lead r)
      (push b r))
    (nreverse r)))

(defun cp932-rt-rows (lead-from lead-to ranges)
  "One row per lead byte; a row holds every trail byte in RANGES
(a list of (from . to))."
  (let (rows)
    (do ((lead lead-from (1+ lead)))
        ((> lead lead-to))
      (push (mapcan (lambda (range)
                      (cp932-rt-codes lead (car range) (cdr range)))
                    ranges)
            rows))
    (nreverse rows)))

(defun cp932-rt-ascii (string)
  (map 'list #'char-code string))

;;; Both ways of opening, both kinds of line ends:
;;; (explicit-crlf auto-crlf explicit-lf auto-lf).
(defun cp932-rt-all-ways (rows)
  (list (cp932-rt-check (list rows) :explicit :crlf)
        (cp932-rt-check (list rows) :auto :crlf)
        (cp932-rt-check (list rows) :explicit :lf)
        (cp932-rt-check (list rows) :auto :lf)))

;;;; 1. ASCII and line ends

(defun cp932-rt-ascii-rows ()
  (list (cp932-rt-range #x20 #x7e)
        (cp932-rt-ascii "The quick brown fox jumps over the lazy dog.")
        (cons 9 (cp932-rt-ascii "tab at the head"))
        nil
        (cp932-rt-ascii "last line")))

(deftest cp932-roundtrip-ascii ()
  (cp932-rt-all-ways (cp932-rt-ascii-rows))
  => (:ok :ok :ok :ok))

(deftest cp932-roundtrip-ascii-eol-variants ()
  (let ((text (cp932-rt-ascii "line1")))
    (flet ((both (bytes)
             (list (cp932-rt-roundtrip bytes :explicit)
                   (cp932-rt-roundtrip bytes :auto))))
      (let ((cases (list
                    ;; CRLF, no line end at the end of the file
                    (append text '(#x0d #x0a) (cp932-rt-ascii "line2"))
                    ;; LF, no line end at the end of the file
                    (append text '(#x0a) (cp932-rt-ascii "line2"))
                    ;; empty file
                    nil
                    ;; only line ends
                    '(#x0d #x0a #x0d #x0a #x0d #x0a)
                    '(#x0a #x0a #x0a)
                    ;; one character, no line end
                    '(#x61))))
        (mapcar (lambda (bytes)
                  (let ((r (both bytes)))
                    (if (and (equal (first r) bytes) (equal (second r) bytes))
                        :ok
                      (list :diff bytes r))))
                cases))))
  => (:ok :ok :ok :ok :ok :ok))

(deftest cp932-roundtrip-ascii-control ()
  ;; Control characters other than line ends and ESC, and DEL.
  (let ((row (append (cp932-rt-range #x01 #x08)
                     '(#x0b #x0c)
                     (cp932-rt-range #x0e #x1a)
                     (cp932-rt-range #x1c #x1f)
                     '(#x7f))))
    (cp932-rt-all-ways (list (cp932-rt-ascii "a") row (cp932-rt-ascii "b"))))
  => (:ok :ok :ok :ok))

;; NUL, ESC, Ctrl-Z, a lone CR and CR CR LF.
(deftest cp932-roundtrip-ascii-special ()
  (let ((cases '((#x61 #x00 #x62)
                 (#x61 #x1a #x62)
                 (#x61 #x1b #x62 #x0d #x0a)
                 (#x61 #x0d #x62)
                 (#x61 #x0d #x0d #x0a #x62))))
    (list (cp932-rt-check-bytes cases :explicit)
          (cp932-rt-check-bytes cases :auto)))
  => (:ok :ok))

;; CRLF and LF in one file.
(defun cp932-rt-mixed-line-end-cases ()
  '((#x61 #x0d #x0a #x62 #x0a #x63 #x0d #x0a)      ; CRLF first
    (#x61 #x0d #x0a #x62 #x0d #x63 #x0a)           ; CRLF, CR and LF
    (#x61 #x0a #x62 #x0d #x0a #x63 #x0a)))         ; LF first

;; The encoding is detected.
(deftest cp932-roundtrip-mixed-line-ends-auto ()
  (cp932-rt-check-bytes (cp932-rt-mixed-line-end-cases) :auto)
  => :ok)

;; The encoding is given: the line ends are kept as when it is detected
;; (BUG-0040).
(deftest cp932-roundtrip-mixed-line-ends-explicit ()
  (cp932-rt-check-bytes (cp932-rt-mixed-line-end-cases) :explicit)
  => :ok)

;;;; 2. JIS X 0208

(deftest cp932-roundtrip-jis-symbols-kana ()
  ;; row 1 symbols, full width digits and letters, hiragana, katakana,
  ;; Greek, Cyrillic and box drawing
  (cp932-rt-all-ways
   (list (cp932-rt-codes #x81 #x40 #x7e)
         (cp932-rt-codes #x81 #x80 #x9e)
         (append (cp932-rt-codes #x82 #x4f #x58)
                 (cp932-rt-codes #x82 #x60 #x79)
                 (cp932-rt-codes #x82 #x81 #x9a))
         (cp932-rt-codes #x82 #x9f #xf1)
         (append (cp932-rt-codes #x83 #x40 #x7e)
                 (cp932-rt-codes #x83 #x80 #x96))
         (append (cp932-rt-codes #x83 #x9f #xb6)
                 (cp932-rt-codes #x83 #xbf #xd6))
         (append (cp932-rt-codes #x84 #x40 #x60)
                 (cp932-rt-codes #x84 #x70 #x7e)
                 (cp932-rt-codes #x84 #x80 #x91))
         (cp932-rt-codes #x84 #x9f #xbe)))
  => (:ok :ok :ok :ok))

(deftest cp932-roundtrip-jis-kanji-level1 ()
  ;; 88 9F - 98 72
  (cp932-rt-all-ways
   (append (cp932-rt-rows #x88 #x88 '((#x9f . #xfc)))
           (cp932-rt-rows #x89 #x97 '((#x40 . #x7e) (#x80 . #xfc)))
           (cp932-rt-rows #x98 #x98 '((#x40 . #x72)))))
  => (:ok :ok :ok :ok))

(deftest cp932-roundtrip-jis-kanji-level2 ()
  ;; 98 9F - EA A4
  (cp932-rt-all-ways
   (append (cp932-rt-rows #x98 #x98 '((#x9f . #xfc)))
           (cp932-rt-rows #x99 #x9f '((#x40 . #x7e) (#x80 . #xfc)))
           (cp932-rt-rows #xe0 #xe9 '((#x40 . #x7e) (#x80 . #xfc)))
           (cp932-rt-rows #xea #xea '((#x40 . #x7e) (#x80 . #xa4)))))
  => (:ok :ok :ok :ok))

(deftest cp932-roundtrip-jis-second-byte-5c ()
  ;; Two byte characters whose second byte is 5C (backslash in ASCII).
  ;; hyoo 955C, so 835C, 8A5C ... in the middle of a line, at the end of a
  ;; line and at the end of the file.
  (let ((chars '((#x81 #x5c) (#x83 #x5c) (#x89 #x5c) (#x8a #x5c) (#x8b #x5c)
                 (#x8c #x5c) (#x8d #x5c) (#x8e #x5c) (#x8f #x5c) (#x90 #x5c)
                 (#x91 #x5c) (#x92 #x5c) (#x93 #x5c) (#x94 #x5c) (#x95 #x5c)
                 (#x96 #x5c) (#x97 #x5c) (#x98 #x5c) (#x99 #x5c) (#x9a #x5c)
                 (#x9b #x5c) (#x9c #x5c) (#x9d #x5c) (#x9e #x5c) (#x9f #x5c)
                 (#xe0 #x5c) (#xe1 #x5c) (#xe2 #x5c) (#xe3 #x5c) (#xe4 #x5c)
                 (#xe5 #x5c) (#xe6 #x5c) (#xe7 #x5c) (#xe8 #x5c) (#xe9 #x5c)
                 (#xea #x5c))))
    (list
     ;; the characters followed by ASCII, an escape like backslash and quote
     (cp932-rt-check (list (list (mapcan (lambda (c)
                                           (append c (list #x61 #x5c #x22 #x6e)))
                                         (mapcar #'copy-list chars))))
                     :explicit :crlf)
     (cp932-rt-check (list (list (mapcan (lambda (c)
                                           (append c (list #x61 #x5c #x22 #x6e)))
                                         (mapcar #'copy-list chars))))
                     :auto :crlf)
     ;; at the end of a line
     (cp932-rt-check (list (mapcar #'copy-list chars)) :explicit :lf)
     (cp932-rt-check (list (mapcar #'copy-list chars)) :auto :lf)
     ;; at the end of the file
     (cp932-rt-check-bytes (mapcar (lambda (c)
                                     (append (cp932-rt-ascii "x") c))
                                   chars)
                           :explicit)
     (cp932-rt-check-bytes (mapcar (lambda (c)
                                     (append (cp932-rt-ascii "x") c))
                                   chars)
                           :auto)))
  => (:ok :ok :ok :ok :ok :ok))

;;;; 3. half width katakana

(deftest cp932-roundtrip-halfwidth-katakana ()
  (cp932-rt-all-ways
   (list (cp932-rt-range #xa1 #xdf)
         (append (cp932-rt-ascii "a") (cp932-rt-range #xb1 #xb5)
                 (cp932-rt-ascii "b") (cp932-rt-range #xd0 #xdf)
                 (cp932-rt-ascii "c"))
         (list #xb1)
         (list #xdf)
         (list #xa1 #xa2 #xa3 #xa4 #xa5)))
  => (:ok :ok :ok :ok))

;;;; 4. NEC row 13 (87 40 - 87 9C)

(defun cp932-rt-nec13-rows ()
  (list (cp932-rt-codes #x87 #x40 #x53)    ; circled 1 - 20
        (cp932-rt-codes #x87 #x54 #x5d)    ; roman numerals I - X
        (cp932-rt-codes #x87 #x5f #x75)    ; unit symbols
        (cp932-rt-codes #x87 #x7e #x7e)
        (cp932-rt-codes #x87 #x80 #x9c)))  ; includes the math symbols 87 90 - 87 9C

(deftest cp932-roundtrip-nec-row13 ()
  (cp932-rt-all-ways (cp932-rt-nec13-rows))
  => (:ok :ok :ok :ok))

(defun cp932-rt-pairs (&rest codes)
  "CODES are two byte codes like #x8790; returns the bytes."
  (mapcan (lambda (code) (list (ash code -8) (logand code #xff))) codes))

(deftest cp932-roundtrip-nec-row13-duplicates ()
  ;; The NEC row 13 characters which have the same Unicode character as
  ;; another cp932 code.  Every code must stay the code it was.
  (cp932-rt-all-ways
   (list
    ;; NEC row 13 and then the JIS code of the same character
    (cp932-rt-pairs #x8790 #x81e0  #x8791 #x81df  #x8792 #x81e7
                    #x8795 #x81e3  #x8796 #x81db  #x8797 #x81da
                    #x879a #x81e6  #x879b #x81bf  #x879c #x81be)
    ;; the other order
    (cp932-rt-pairs #x81e0 #x8790  #x81df #x8791  #x81e7 #x8792
                    #x81e3 #x8795  #x81db #x8796  #x81da #x8797
                    #x81e6 #x879a  #x81bf #x879b  #x81be #x879c)
    ;; the NEC only ones in between
    (cp932-rt-pairs #x8793 #x8794 #x8798 #x8799 #x81e0 #x8790)
    ;; NEC row 13 and the IBM extension (roman numerals, kabu, numero, TEL)
    (append (cp932-rt-codes #x87 #x54 #x5d)
            (cp932-rt-codes #xfa #x4a #x53)
            (cp932-rt-pairs #x878a #xfa58  #x8782 #xfa59  #x8784 #xfa5a
                            #x879a #xfa5b  #xfa5b #x879a #xfa58 #x878a))
    ;; the three codes of "not": 81CA, EEF9 and FA54
    (cp932-rt-pairs #x81ca #xeef9 #xfa54 #xfa54 #xeef9 #x81ca)))
  => (:ok :ok :ok :ok))

;;;; 5. NEC selected IBM extension (ED 40 - EE FC)

(deftest cp932-roundtrip-nec-selected-ibm ()
  (cp932-rt-all-ways
   (append (cp932-rt-rows #xed #xed '((#x40 . #x7e) (#x80 . #xfc)))
           (cp932-rt-rows #xee #xee '((#x40 . #x7e) (#x80 . #xec) (#xef . #xfc)))))
  => (:ok :ok :ok :ok))

(deftest cp932-roundtrip-nec-selected-ibm-duplicates ()
  ;; ED/EE characters and the same characters in FA - FC.  Every code must
  ;; stay the code it was.  ED40 = FA5C, ED41 = FA5D, EEEF = FA40, EEF9 = FA54.
  (cp932-rt-all-ways
   (list
    (append (cp932-rt-codes #xed #x40 #x42)
            (cp932-rt-codes #xfa #x5c #x5e)
            (cp932-rt-codes #xee #xef #xf8)
            (cp932-rt-codes #xfa #x40 #x49)
            (cp932-rt-codes #xee #xf9 #xfc)
            (cp932-rt-codes #xfa #x54 #x57))
    (append (cp932-rt-codes #xfa #x5c #x5e)
            (cp932-rt-codes #xed #x40 #x42)
            (cp932-rt-codes #xfa #x54 #x57)
            (cp932-rt-codes #xee #xf9 #xfc))))
  => (:ok :ok :ok :ok))

;;;; 6. IBM extension (FA 40 - FC 4B)

(deftest cp932-roundtrip-ibm-extension ()
  (cp932-rt-all-ways
   (append (cp932-rt-rows #xfa #xfb '((#x40 . #x7e) (#x80 . #xfc)))
           (cp932-rt-rows #xfc #xfc '((#x40 . #x4b)))))
  => (:ok :ok :ok :ok))

;;;; 7. user defined area (F0 40 - F9 FC)

(deftest cp932-roundtrip-user-defined ()
  (cp932-rt-all-ways
   (cp932-rt-rows #xf0 #xf9 '((#x40 . #x7e) (#x80 . #xfc))))
  => (:ok :ok :ok :ok))

;;;; 8. invalid and stray bytes

(defun cp932-rt-invalid-cases ()
  (let ((a (cp932-rt-ascii "abc"))
        (crlf '(#x0d #x0a))
        (lf '(#x0a)))
    (list
     ;; a lead byte at the end of the file
     (append a '(#x81))
     (append a '(#x9f))
     (append a '(#xe0))
     (append a '(#xea))
     (append a '(#xf0))
     (append a '(#xf9))
     (append a '(#xfc))
     ;; a lead byte at the end of the file after a line end
     (append a crlf '(#x81))
     (append a lf '(#xe0))
     ;; a lead byte before a line end
     (append a '(#x81) crlf a)
     (append a '(#x81) lf a)
     (append a '(#xe0) crlf)
     ;; a lead byte followed by ASCII that is not a trail byte
     (append a '(#x81 #x20) a)
     (append a '(#x81 #x3f) a)
     (append a '(#xe0 #x39) a)
     (append a '(#x81 #x7f) a)
     ;; a lead byte followed by a byte that is not a trail byte
     (append a '(#x81 #xfd) a)
     (append a '(#x81 #xfe) a)
     (append a '(#x81 #xff) a)
     ;; single bytes between ASCII
     (append a '(#x80) a)
     (append a '(#xa0) a)
     (append a '(#xfd) a)
     (append a '(#xfe) a)
     (append a '(#xff) a)
     ;; single bytes at the start of the file
     '(#x80 #x61)
     '(#xa0 #x61)
     '(#xfd #x61)
     '(#xfe #x61)
     '(#xff #x61)
     ;; single bytes at the end of the file
     (append a '(#x80))
     (append a '(#xa0))
     (append a '(#xfd))
     (append a '(#xfe))
     (append a '(#xff))
     ;; single bytes followed by a trail byte range character
     (append a '(#x80 #x40) a)
     (append a '(#xa0 #x40) a)
     (append a '(#xfd #x40) a)
     (append a '(#xfe #x40) a)
     (append a '(#xff #x40) a)
     ;; lead bytes that are not used (EB - EF, FD - FF) with a trail byte
     (append a '(#xeb #x40) a)
     (append a '(#xef #xfc) a)
     ;; two byte codes that nobody defines
     (append a '(#x81 #xad) a)
     (append a '(#x85 #x40) a)
     (append a '(#x87 #x76) a)
     (append a '(#x84 #xbf) a)
     (append a '(#x98 #x73) a)
     (append a '(#xea #xa5) a)
     ;; a stray byte pair of 0x80 range
     (append a '(#x80 #x80 #x80) a)
     (append a '(#xff #xff #xff) a))))

;; Cases whose bytes changed when the encoding is detected (not given):
;; the detection chose big5 or euc-jp and the bytes were converted to "?"
;; or to another byte sequence (BUG-0039).
(defun cp932-rt-invalid-cases-auto-lossy ()
  (let ((a (cp932-rt-ascii "abc")))
    (list
     (append a '(#xe0 #x0d #x0a))        ; big5, E0 -> 3F
     (append a '(#xe0 #x39) a)           ; big5, E0 -> 3F
     (append a '(#xa0) a)                ; big5, A0 -> 3F
     '(#xa0 #x61)                        ; big5, A0 -> 3F
     (append a '(#xa0))                  ; big5, A0 -> 3F
     (append a '(#xfe))                  ; euc-jp, FE -> 8E FE
     (append a '(#xa0 #x40) a))))        ; big5, A0 -> 3F

;; The encoding is given: every case keeps its bytes.
(deftest cp932-roundtrip-invalid-bytes ()
  (cp932-rt-check-bytes (cp932-rt-invalid-cases) :explicit)
  => :ok)

;; The encoding is detected: the cases which keep their bytes today.
(deftest cp932-roundtrip-invalid-bytes-auto ()
  (let ((lossy (cp932-rt-invalid-cases-auto-lossy)))
    (cp932-rt-check-bytes
     (remove-if (lambda (c) (member c lossy :test #'equal))
                (cp932-rt-invalid-cases))
     :auto))
  => :ok)

;; The encoding is detected: the cases which lost bytes (BUG-0039).
(deftest cp932-roundtrip-invalid-bytes-auto-lossy ()
  (cp932-rt-check-bytes (cp932-rt-invalid-cases-auto-lossy) :auto)
  => :ok)

;; A lead byte followed by a line end: the line end is a line end, and the
;; bytes are kept (BUG-0044).
(defun cp932-rt-lead-line-end-cases ()
  '((#x61 #x81 #x0a #x62 #x0a)                     ; lead LF
    (#x61 #xe0 #x0a #x62 #x0a)
    (#x61 #xfc #x0a #x62 #x0a)
    (#x61 #x81 #x0d #x0a #x62 #x0d #x0a)           ; lead CRLF
    (#x61 #x81 #x0a #x82 #xa0 #x0a)))              ; lead LF, a character

(defun cp932-rt-line-count (bytes mode)
  "Open BYTES as a file and return the line number at the end of the buffer."
  (let ((tmp (make-temp-file-name))
        (buf nil))
    (unwind-protect
        (progn
          (cp932-rt-write-bytes tmp bytes)
          (setq buf (ed::find-file-internal tmp t
                                          (if (eq mode :explicit)
                                              *encoding-sjis*
                                            nil)
                                          nil t))
          (save-excursion
            (set-buffer buf)
            (goto-char (point-max))
            (current-line-number)))
      (when buf
        (delete-buffer buf))
      (when (file-exist-p tmp)
        (delete-file tmp)))))

(deftest cp932-roundtrip-lead-line-end ()
  (list (cp932-rt-check-bytes (cp932-rt-lead-line-end-cases) :explicit)
        (cp932-rt-check-bytes (cp932-rt-lead-line-end-cases) :auto))
  => (:ok :ok))

;; Every case has two line ends.
(deftest cp932-lead-line-end-is-line-end ()
  (list (mapcar (lambda (c) (cp932-rt-line-count c :explicit))
                (cp932-rt-lead-line-end-cases))
        (mapcar (lambda (c) (cp932-rt-line-count c :auto))
                (cp932-rt-lead-line-end-cases)))
  => ((3 3 3 3 3) (3 3 3 3 3)))

;; A file stream reads the same way.
(deftest cp932-lead-line-end-file-stream ()
  (let ((tmp (make-temp-file-name)))
    (unwind-protect
        (progn
          (cp932-rt-write-bytes tmp '(#x61 #x81 #x0a #x62 #x0a))
          (with-open-file (s tmp :direction :input)
            (let* ((l1 (read-line s nil nil))
                   (l2 (read-line s nil nil)))
              (list (map 'list #'char-code l1)
                    (map 'list #'char-code l2)))))
      (when (file-exist-p tmp)
        (delete-file tmp))))
  => ((#x61 #x81) (#x62)))
