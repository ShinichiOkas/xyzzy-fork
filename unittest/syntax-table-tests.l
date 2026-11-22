;;; -*- mode: lisp; package: user -*-
;;;
;;; The syntax table of a character that is not ASCII (S4 R2).
;;;
;;; The table is dense for 0..255 and sparse for a character of 256 or more:
;;; set-syntax-* stores such a character in the overrides, and syntax-of (\sX,
;;; skip-syntax-spec, forward-word, the syntax-*-p queries) looks at the
;;; overrides first and falls back to the default rule.
;;;
;;; Characters are made from numbers so that this file stays ASCII.  A test
;;; makes its own syntax table and never touches *default-syntax-table*.

(in-package :user)

(defun st-hira () (unicode-char #x3042))       ; HIRAGANA LETTER A
(defun st-kanji () (unicode-char #x4e00))      ; CJK ideograph one
(defun st-eacute () (unicode-char #x00e9))     ; Latin small e with acute
(defun st-alpha () (unicode-char #x03b1))      ; Greek small alpha
(defun st-ideo-space () (unicode-char #x3000)) ; IDEOGRAPHIC SPACE
(defun st-lquote () (unicode-char #x300c))     ; LEFT CORNER BRACKET
(defun st-rquote () (unicode-char #x300d))     ; RIGHT CORNER BRACKET

(defmacro with-syntax-test-buffer ((table) &body body)
  "Run BODY in a fresh buffer that uses TABLE."
  `(save-excursion
     (let ((#1=#:b (create-new-buffer "*syntax-table-test*")))
       (unwind-protect
           (progn (set-buffer #1#)
                  (use-syntax-table ,table)
                  ,@body)
         (delete-buffer #1#)))))

(defun st-match-p (re c)
  (and (string-match re (string c)) t))

(defun st-skip-forward (table spec &rest chars)
  "How many characters of CHARS skip-syntax-spec-forward skips with SPEC."
  (with-syntax-test-buffer (table)
    (apply #'insert (mapcar #'string chars))
    (goto-char (point-min))
    (skip-syntax-spec-forward spec)
    (- (point) (point-min))))

(defun st-forward-word (table &rest chars)
  "The point after (forward-word 1) from the start of a buffer of CHARS."
  (with-syntax-test-buffer (table)
    (apply #'insert (mapcar #'(lambda (c) (if (stringp c) c (string c))) chars))
    (goto-char (point-min))
    (forward-word 1)
    (- (point) (point-min))))

(defun st-backward-word (table &rest chars)
  "The point after (forward-word -1) from the end of a buffer of CHARS."
  (with-syntax-test-buffer (table)
    (apply #'insert (mapcar #'(lambda (c) (if (stringp c) c (string c))) chars))
    (goto-char (point-max))
    (forward-word -1)
    (- (point) (point-min))))

(deftest syntax-nonascii-set-returns-t ()
  "set-syntax-* accepts a character that is not ASCII (it returned NIL)."
  (let ((tab (make-syntax-table)))
    (list (set-syntax-word tab (st-hira))
          (set-syntax-punctuation tab (st-kanji))
          (set-syntax-whitespace tab (st-ideo-space))
          (set-syntax-match tab (st-lquote) (st-rquote))
          (set-syntax-string tab (st-alpha))
          (set-syntax-quote tab (st-eacute))
          (set-syntax-escape tab (st-eacute))
          (set-syntax-symbol tab (st-hira))
          (set-syntax-junk tab (st-hira))
          (set-syntax-math tab (st-hira))
          (set-syntax-start-comment tab (st-hira) t)
          (set-syntax-end-comment tab (st-hira) t nil)
          (set-syntax-end-c++-comment tab (st-hira) nil)
          (set-syntax-start-c++-comment tab (st-hira) nil)
          (set-syntax-start-column-comment tab (st-hira) nil)))
  => (t t t t t t t t t t t t t t t))

(deftest syntax-nonascii-query ()
  "The syntax-*-p functions answer for a character that is not ASCII."
  (let ((tab (make-syntax-table))
        (a (st-hira))
        (e (st-eacute)))
    (list (syntax-word-p a tab)         ; default: kanji
          (syntax-word-p e tab)         ; default: a Latin letter is a word
          (progn (set-syntax-word tab a) (syntax-word-p a tab))
          (syntax-junk-p a tab)
          (progn (set-syntax-punctuation tab e)
                 (list (syntax-punctuation-p e tab) (syntax-word-p e tab)))
          (syntax-word-p (st-kanji) tab)))
  => (nil t t nil (t nil) nil))

(deftest syntax-nonascii-query-kinds ()
  "Each kind of set-syntax-* is seen by its syntax-*-p."
  (let ((tab (make-syntax-table))
        (a (st-hira))
        (k (st-kanji)))
    (list (progn (set-syntax-whitespace tab a) (syntax-whitespace-p a tab))
          (progn (set-syntax-math tab a) (syntax-math-p a tab))
          (progn (set-syntax-string tab a) (syntax-string-p a tab))
          (progn (set-syntax-escape tab a) (syntax-escape-p a tab))
          (progn (set-syntax-quote tab a) (syntax-quote-p a tab))
          (progn (set-syntax-symbol tab a) (syntax-symbol-p a tab))
          (progn (set-syntax-symbol-prefix tab a) (syntax-symbol-prefix-p a tab))
          (progn (set-syntax-start-comment tab a) (syntax-start-comment-p a tab))
          (progn (set-syntax-end-comment tab a) (syntax-end-comment-p a tab))
          (progn (set-syntax-end-c++-comment tab a) (syntax-end-cplusplus-comment-p a tab))
          (progn (set-syntax-junk tab a) (syntax-junk-p a tab))
          (progn (set-syntax-match tab a k)
                 (list (syntax-open-p a tab) (syntax-close-p k tab)
                       (syntax-close-p a tab) (syntax-open-p k tab)))
          (progn (set-syntax-tag tab a k)
                 (list (syntax-open-tag-p a tab) (syntax-close-tag-p k tab)))))
  => (t t t t t t t t t t t (t t nil nil) (t t)))

(deftest syntax-nonascii-comment-bits ()
  "The comment flags of a character that is not ASCII are kept, and a new
multi-character comment clears the old one."
  (let ((tab (make-syntax-table))
        (a (st-hira))
        (k (st-kanji))
        (e (st-eacute)))
    (set-syntax-start-multi-comment tab (concatenate 'string (string a) (string k)))
    (set-syntax-end-multi-comment tab (concatenate 'string (string k) (string a)))
    (set-syntax-start-c++-comment tab e nil)
    (set-syntax-start-column-comment tab (st-alpha) nil)
    (list (syntax-start-multi-comment-1-p a tab)
          (syntax-start-multi-comment-2-p a tab)
          (syntax-start-multi-comment-2-p k tab)
          (syntax-end-multi-comment-1-p k tab)
          (syntax-end-multi-comment-2-p a tab)
          (syntax-c++-comment-p e tab)
          (syntax-start-column-comment-p (st-alpha) tab)
          (progn
            (set-syntax-start-multi-comment tab (concatenate 'string (string e) (string a)))
            (list (syntax-start-multi-comment-1-p a tab)
                  (syntax-start-multi-comment-2-p k tab)
                  (syntax-start-multi-comment-1-p e tab)
                  (syntax-start-multi-comment-2-p a tab)))))
  => (t nil t t t t t (nil nil t t)))

(deftest syntax-nonascii-key-is-not-a-character ()
  "A key is not a character: set-syntax-* gives NIL and the query NIL."
  (let ((tab (make-syntax-table)))
    (list (set-syntax-word tab #\F1)
          (syntax-word-p #\F1 tab)
          (set-syntax-match tab #\F1 #\F2)))
  => (nil nil nil))

(deftest syntax-nonascii-regexp-class ()
  "\\sX and \\w follow set-syntax-word on a character that is not ASCII."
  (let ((tab (make-syntax-table))
        (a (st-hira)))
    (with-syntax-test-buffer (tab)
      (let ((before (list (st-match-p "\\sj" a) (st-match-p "\\sw" a)
                          (st-match-p "\\w" a) (st-match-p "\\W" a))))
        (set-syntax-word tab a)
        (list before
              (list (st-match-p "\\sj" a) (st-match-p "\\sw" a)
                    (st-match-p "\\w" a) (st-match-p "\\W" a))))))
  => ((t nil nil t) (nil t t nil)))

(deftest syntax-nonascii-regexp-other-classes ()
  "\\s. \\s  \\s( \\s) match a character set to punctuation, white, open, close."
  (let ((tab (make-syntax-table))
        (a (st-hira))
        (k (st-kanji))
        (e (st-eacute))
        (lq (st-lquote))
        (rq (st-rquote)))
    (set-syntax-punctuation tab a)
    (set-syntax-whitespace tab (st-ideo-space))
    (set-syntax-match tab lq rq)
    (set-syntax-punctuation tab e)
    (with-syntax-test-buffer (tab)
      (list (st-match-p "\\s." a)
            (st-match-p "\\sj" a)
            (st-match-p "\\s " (st-ideo-space))
            (st-match-p "\\s(" lq)
            (st-match-p "\\s)" rq)
            (st-match-p "\\s)" lq)
            (st-match-p "\\s." e)
            (st-match-p "\\sw" e)
            (st-match-p "\\sj" k))))
  => (t nil t t t nil t nil t))

(deftest syntax-nonascii-regexp-search-finds-overridden ()
  "string-match and re-search-forward find a character by a class that the
default syntax of the character does not have (the fastmap)."
  (let ((tab (make-syntax-table))
        (a (st-hira)))
    (set-syntax-punctuation tab a)
    (with-syntax-test-buffer (tab)
      (let ((s (concatenate 'string "ab" (string a) "cd")))
        (insert s)
        (goto-char (point-min))
        (list (string-match "\\s." s)
              (progn (string-match "\\s." (concatenate 'string "ab" (string (st-kanji)) "-"))
                     (match-beginning 0))
              (and (re-search-forward "\\s." t) (point))
              (progn (goto-char (point-max))
                     (and (re-search-backward "\\s." t) (point)))
              (progn (goto-char (point-min))
                     (and (re-search-forward "\\s.\\w" t) (point)))))))
  => (2 3 2 2 2))

(deftest syntax-nonascii-skip-syntax-spec ()
  "skip-syntax-spec-forward and -backward follow the overrides."
  (let ((tab (make-syntax-table))
        (a (st-hira))
        (k (st-kanji)))
    (list (st-skip-forward tab "w" "a" a k)
          (progn (set-syntax-word tab a) (st-skip-forward tab "w" "a" a k))
          (progn (set-syntax-word tab k) (st-skip-forward tab "w" "a" a k))
          (st-skip-forward tab "^w" a "x")
          (with-syntax-test-buffer (tab)
            (insert "-" (string a) (string k))
            (skip-syntax-spec-backward "w")
            (- (point) (point-min)))))
  => (1 2 3 0 1))

(deftest syntax-nonascii-forward-word ()
  "forward-word treats a character set to word like a letter, and a character
set to punctuation as a break."
  (let ((tab (make-syntax-table))
        (a (st-hira))
        (e (st-eacute)))
    (list (st-forward-word tab "ab" a)  ; hiragana ends the word
          (progn (set-syntax-word tab a) (st-forward-word tab "ab" a))
          (st-backward-word tab a "ab")
          (st-forward-word tab "a" e "b") ; a Latin letter is a word
          (progn (set-syntax-punctuation tab e) (st-forward-word tab "a" e "b"))
          (progn (set-syntax-whitespace tab e) (st-forward-word tab "a" e "b"))
          (st-backward-word tab "a" e "b")))
  => (2 3 0 3 1 1 2))

(deftest syntax-nonascii-word-char-p-agrees ()
  "word-char-p, \\w and forward-word agree for a character that is set."
  (let ((tab (make-syntax-table))
        (a (st-hira))
        (e (st-eacute)))
    (set-syntax-word tab a)
    (set-syntax-punctuation tab e)
    (with-syntax-test-buffer (tab)
      (list (list (and (word-char-p a) t) (st-match-p "\\w" a)
                  (= (st-forward-word tab "a" a) 2))
            (list (and (word-char-p e) t) (st-match-p "\\w" e)
                  (= (st-forward-word tab "a" e) 2)))))
  => ((t t t) (nil nil nil)))

(deftest syntax-nonascii-copy-syntax-table ()
  "copy-syntax-table copies the overrides, and the two tables are independent."
  (let ((src (make-syntax-table))
        (dst (make-syntax-table))
        (a (st-hira))
        (k (st-kanji))
        (e (st-eacute)))
    (set-syntax-word src a)
    (set-syntax-punctuation dst e)
    (set-syntax-whitespace dst k)
    (copy-syntax-table src dst)
    (list (syntax-word-p a dst)
          (syntax-word-p e dst)         ; dst's own override is gone
          (syntax-whitespace-p k dst)
          (progn (set-syntax-punctuation src a) (syntax-word-p a dst))
          (progn (set-syntax-whitespace dst k) (syntax-whitespace-p k src))
          (progn (copy-syntax-table dst dst) (syntax-whitespace-p k dst))))
  => (t t nil t nil t))

(deftest syntax-nonascii-copy-to-fresh-table ()
  "A table copied from a table that has overrides keeps them after the source is gone."
  (let ((dst (make-syntax-table))
        (a (st-hira)))
    (let ((src (make-syntax-table)))
      (set-syntax-word src a)
      (set-syntax-punctuation src (st-eacute))
      (copy-syntax-table src dst))
    (gc)
    (list (syntax-word-p a dst) (syntax-punctuation-p (st-eacute) dst)))
  => (t t))

(deftest syntax-nonascii-many-overrides ()
  "Many characters (the sparse table grows): each one keeps its own syntax,
whatever the order they were set in."
  (let ((chars (st-range-chars #x3041 #x3093))
        (r nil))
    (dolist (order (list chars (reverse chars)))
      (let ((tab (make-syntax-table)) (i 0))
        ;; every other one goes to word, the next one to punctuation
        (dolist (c order)
          (if (evenp i) (set-syntax-word tab c) (set-syntax-punctuation tab c))
          (setq i (1+ i)))
        (setq i 0)
        (dolist (c order)
          (unless (if (evenp i) (and (syntax-word-p c tab) (not (syntax-punctuation-p c tab)))
                    (and (syntax-punctuation-p c tab) (not (syntax-word-p c tab))))
            (push (char-code c) r))
          (setq i (1+ i)))
        ;; the katakana were not set: they keep the default (kanji)
        (dolist (ch (st-range-chars #x30a1 #x30f6))
              do (let ((ch (unicode-char c)))
                   (when (or (syntax-word-p ch tab) (syntax-punctuation-p ch tab))
                     (push (char-code ch) r))))))
    (nreverse r))
  => nil)

(deftest syntax-nonascii-default-unaffected ()
  "A table that nobody set keeps the default rule for a character of 256 or more."
  (let ((tab (make-syntax-table)))
    (list (syntax-word-p (st-eacute) tab)
          (syntax-word-p (st-alpha) tab)
          (syntax-word-p (st-hira) tab)
          (syntax-word-p (st-kanji) tab)
          (st-match-p "\\sw" (st-eacute))
          (st-match-p "\\sj" (st-hira))
          (st-match-p "\\sk" (unicode-char #xff71))))
  => (t t nil nil t t t))

(deftest syntax-unicode-default-rule ()
  "The default rule from the Unicode properties (the XYZZY_CHAR_UNICODE side):
Latin, Greek, Cyrillic, Thai letters and marks are word, half width kana is
kana, C1 controls are junk, CJK, Hangul, full width forms and symbols are kanji."
  (mapcar #'si:syntax-unicode-default
          '(#x00e9 #x0391 #x0431 #x0e01 #x0301 #x0660
            #xff71
            #x0080 #x009f
            #x3042 #x30a2 #x4e00 #xac00 #x3000 #xff21 #x20ac #x2500 #x1f600 #x3001
            #xa0 #xb7))
  => ("w" "w" "w" "w" "w" "w"
      "k"
      "x" "x"
      "j" "j" "j" "j" "j" "j" "j" "j" "j" "j"
      "j" "j"))

;; The overrides go into the dump (xyzzy.wxp) and come back.  The dump holds
;; the table as it is at startup, and lisp/ sets no character of 256 or more
;; in a table, so the test writes an image of this xyzzy, with a table that has
;; overrides in a variable, and reads it in another xyzzy.
(defvar *syntax-dump-test-table* nil)

(deftest syntax-nonascii-survives-dump ()
  "The sparse overrides are written to the dump and read back."
  (let ((tab (make-syntax-table))
        (src (make-syntax-table))
        (image (make-temp-file-name "syn")))
    (set-syntax-word tab (unicode-char #x3042))
    (set-syntax-punctuation tab (unicode-char #x00e9))
    (set-syntax-match tab (unicode-char #x300c) (unicode-char #x300d))
    (set-syntax-start-multi-comment tab (coerce (list (unicode-char #x3044) (unicode-char #x3046)) 'string))
    (dolist (ch (st-range-chars #x30a1 #x30f6))
          do (set-syntax-word src (unicode-char c)))
    (copy-syntax-table src (make-syntax-table))
    (setq *syntax-dump-test-table* tab)
    (unwind-protect
        (progn
          (dump-xyzzy image)
          (eval-in-another-xyzzy
           '(let ((tab *syntax-dump-test-table*))
              (list (syntax-word-p (unicode-char #x3042) tab)
                    (syntax-punctuation-p (unicode-char #x00e9) tab)
                    (syntax-open-p (unicode-char #x300c) tab)
                    (syntax-close-p (unicode-char #x300d) tab)
                    (syntax-start-multi-comment-1-p (unicode-char #x3044) tab)
                    (syntax-start-multi-comment-2-p (unicode-char #x3046) tab)
                    (syntax-word-p (unicode-char #x3044) tab)
                    (syntax-word-p (unicode-char #x4e00) tab)
                    (progn (set-syntax-word tab (unicode-char #x4e00))
                           (syntax-word-p (unicode-char #x4e00) tab))))
           :options (format nil "-image ~A -q" image)))
      (setq *syntax-dump-test-table* nil)
      (ignore-errors (delete-file image))))
  => (t t t t t t nil nil t))
