;;; -*- mode: lisp; package: user -*-
;;;
;;; Syntax of non-ASCII characters in regexps and skip-syntax-spec (BUG-0042).
;;;
;;; The regexp classes \w \W \s? and skip-syntax-spec-forward/backward used
;;; to index the syntax table with (code >> 8) for a character of 256 or more.
;;; A character whose internal value is 0x0100-0x7FFF (ISO-8859, JIS X 0212,
;;; ...) was therefore taken for a control character or for an ASCII letter.
;;; Every character of 256 or more is SCkanji, as in the syntax scanner.
;;;
;;; Characters are made from numbers so that this file stays ASCII.
;;; Each deftest returns the list of the failed cases; nil is a pass.

(in-package :user)

(defvar *regexp-syntax-spec-chars* "wkjx. ")

(defun regexp-syntax-chars-1 (kind)
  (case kind
    (:iso8859-1-e-acute (iso-code-char #xe9 :iso8859-1))
    (:iso8859-1-a-grave (iso-code-char #xc0 :iso8859-1))
    (:iso8859-1-nbsp (iso-code-char #xa0 :iso8859-1))
    (:iso8859-2-a-ogonek (iso-code-char #xb1 :iso8859-2))
    (:jisx0212-kanji (iso-code-char #x3021 :jisx0212))
    (:jisx0212-kanji-2 (iso-code-char #x4f7e :jisx0212))
    (:jisx0208-kanji (iso-code-char #x306c :jisx0208))
    (:jisx0208-hiragana (iso-code-char #x2422 :jisx0208))
    (:jisx0208-katakana (iso-code-char #x2522 :jisx0208))
    (:jisx0208-fullwidth-a (iso-code-char #x2341 :jisx0208))
    (:halfwidth-kana (unicode-char #xff71))
    (:ascii-upper #\A)
    (:ascii-lower #\z)
    (:ascii-digit #\5)
    (:ascii-hyphen #\-)
    (:ascii-paren #\()
    (:ascii-space #\SPC)
    (:ascii-underscore #\_)))

;; Expected syntax class of each character, as one of the spec characters of
;; skip-syntax-spec: w word, k kana, j kanji, . punctuation, space white.
(defparameter *regexp-syntax-expected*
  '((:iso8859-1-e-acute . #\j)
    (:iso8859-1-a-grave . #\j)
    (:iso8859-1-nbsp . #\j)
    (:iso8859-2-a-ogonek . #\j)
    (:jisx0212-kanji . #\j)
    (:jisx0212-kanji-2 . #\j)
    (:jisx0208-kanji . #\j)
    (:jisx0208-hiragana . #\j)
    (:jisx0208-katakana . #\j)
    (:jisx0208-fullwidth-a . #\j)
    (:halfwidth-kana . #\k)
    (:ascii-upper . #\w)
    (:ascii-lower . #\w)
    (:ascii-digit . #\w)
    (:ascii-hyphen . #\.)
    (:ascii-paren . #\.)
    (:ascii-space . #\SPC)
    (:ascii-underscore . #\.)))

;; With-buffer helper: number of characters skipped by SPEC from the start of
;; a buffer that holds only C (0 or 1), forward.
(defun regexp-syntax-skip-forward (spec c)
  (save-excursion
    (let ((b (create-new-buffer "*regexp-syntax*")))
      (unwind-protect
          (progn
            (set-buffer b)
            (insert (string c))
            (goto-char (point-min))
            (skip-syntax-spec-forward spec)
            (- (point) (point-min)))
        (delete-buffer b)))))

;; Same, backward from the end of the buffer.
(defun regexp-syntax-skip-backward (spec c)
  (save-excursion
    (let ((b (create-new-buffer "*regexp-syntax*")))
      (unwind-protect
          (progn
            (set-buffer b)
            (insert (string c))
            (goto-char (point-max))
            (skip-syntax-spec-backward spec)
            (- (point-max) (point)))
        (delete-buffer b)))))

(defun regexp-syntax-spec-matches (c)
  "The list of the spec characters that match C, by regexp (\\sX)."
  (let ((s (string c)))
    (remove nil
            (map 'list
                 #'(lambda (spec)
                     (and (string-match (format nil "\\s~C" spec) s) spec))
                 *regexp-syntax-spec-chars*))))

(defun regexp-syntax-skip-matches (c skipper)
  "The list of the spec characters that skip C with SKIPPER."
  (remove nil
          (map 'list
               #'(lambda (spec)
                   (and (= (funcall skipper (string spec) c) 1) spec))
               *regexp-syntax-spec-chars*)))

(defun regexp-syntax-failures (check)
  (let ((r nil))
    (dolist (e *regexp-syntax-expected*)
      (let* ((c (regexp-syntax-chars-1 (car e)))
             (want (list (cdr e)))
             (got (funcall check c (cdr e))))
        (unless (equal got want)
          (push (list (car e) (char-code c) :want want :got got) r))))
    (nreverse r)))

(deftest regexp-syntax-class-s ()
  "\\sX matches exactly the expected class."
  (regexp-syntax-failures #'(lambda (c w) (regexp-syntax-spec-matches c)))
  => nil)

(deftest regexp-syntax-skip-forward ()
  "skip-syntax-spec-forward skips exactly the expected class."
  (regexp-syntax-failures
   #'(lambda (c w)
       (regexp-syntax-skip-matches c #'regexp-syntax-skip-forward)))
  => nil)

(deftest regexp-syntax-skip-backward ()
  "skip-syntax-spec-backward skips exactly the expected class."
  (regexp-syntax-failures
   #'(lambda (c w)
       (regexp-syntax-skip-matches c #'regexp-syntax-skip-backward)))
  => nil)

(deftest regexp-syntax-skip-negated ()
  "skip-syntax-spec with ^ skips everything but the expected class."
  (let ((r nil))
    (dolist (e *regexp-syntax-expected*)
      (let* ((c (regexp-syntax-chars-1 (car e)))
             (own (string (cdr e)))
             (not-own (concatenate 'string "^" own)))
        (unless (and (= (regexp-syntax-skip-forward not-own c) 0)
                     (= (regexp-syntax-skip-forward own c) 1))
          (push (car e) r))))
    (nreverse r))
  => nil)

(deftest regexp-syntax-word-class ()
  "\\w matches only the word characters, \\W matches all the others."
  (let ((r nil))
    (dolist (e *regexp-syntax-expected*)
      (let* ((c (regexp-syntax-chars-1 (car e)))
             (s (string c))
             (word (eql (cdr e) #\w)))
        (unless (and (eq (and (string-match "\\w" s) t) word)
                     (eq (and (string-match "\\W" s) t) (not word)))
          (push (list (car e) (char-code c)) r))))
    (nreverse r))
  => nil)

(deftest regexp-syntax-word-in-text ()
  "\\w+ in a mixed string stops at a character of 256 or more."
  (let* ((e-acute (regexp-syntax-chars-1 :iso8859-1-e-acute))
         (kanji (regexp-syntax-chars-1 :jisx0212-kanji))
         (s (concatenate 'string "ab" (string e-acute) "cd" (string kanji) "ef")))
    (string-match "\\w+" s 2)
    (list (match-beginning 0) (match-end 0)))
  => (3 5))

(deftest regexp-syntax-not-word-run ()
  "\\W+ takes the whole run of non-word characters."
  (let* ((e-acute (regexp-syntax-chars-1 :iso8859-1-e-acute))
         (kanji (regexp-syntax-chars-1 :jisx0212-kanji))
         (s (concatenate 'string "ab" (string e-acute) (string kanji) "-cd")))
    (string-match "\\W+" s)
    (list (match-beginning 0) (match-end 0)))
  => (2 5))

(deftest regexp-syntax-search-in-buffer ()
  "re-search-forward and re-search-backward find a character of 256 or more by class."
  (let ((b (create-new-buffer "*regexp-syntax*"))
        (e-acute (regexp-syntax-chars-1 :iso8859-1-e-acute))
        (kanji (regexp-syntax-chars-1 :jisx0212-kanji)))
    (unwind-protect
        (save-excursion
          (set-buffer b)
          (insert "ab" (string e-acute) "-" (string kanji) "cd")
          (goto-char (point-min))
          (let* ((j1 (and (re-search-forward "\\sj" t) (point)))
                 (j2 (and (goto-char (match-end 0))
                          (re-search-forward "\\sj" t)
                          (point)))
                 (j3 (and (goto-char (match-end 0))
                          (re-search-forward "\\sj" t)))
                 (w1 (progn (goto-char (point-max))
                            (and (re-search-backward "\\W" t) (point))))
                 (w2 (progn (goto-char (point-max))
                            (and (re-search-backward "\\w" t) (point)))))
            (list j1 j2 j3 w1 w2)))
      (delete-buffer b)))
  => (2 4 nil 4 6))