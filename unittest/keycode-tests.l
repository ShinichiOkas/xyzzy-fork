;;; -*- mode: lisp; package: user -*-
;;;
;;; Keys are Lisp characters whose values are KEY_BASE (#x1000000) + the old
;;; 16bit layout (S4-k).  They never go into a string or a buffer.
;;;
;;; The tests write keys by name, not by value, so that they pass the same way
;;; after the characters become Unicode (V).  The only values in here are the
;;; ones that the design fixes (char-code of a key).

(require "isearch")

(defvar *keycode-test-function-key-names*
  '("PageUp" "PageDown" "End" "Home" "Left" "Up" "Right" "Down" "Pause"
    "MouseMove" "Scroll" "Apps" "Insert" "Delete" "Help"
    "F1" "F2" "F3" "F4" "F5" "F6" "F7" "F8" "F9" "F10" "F11" "F12"
    "F13" "F14" "F15" "F16" "F17" "F18" "F19" "F20" "F21" "F22" "F23" "F24"
    "LBtnDown" "LBtnUp" "LBtnMove" "RBtnDown" "RBtnUp" "RBtnMove"
    "MBtnDown" "MBtnUp" "MBtnMove"
    "XBtn1Down" "XBtn1Up" "XBtn1Move" "XBtn2Down" "XBtn2Up" "XBtn2Move"))

(defvar *keycode-test-modifiers*
  '("" "S-" "C-" "M-" "S-C-" "C-M-" "S-C-M-"))

(defun keycode-test-read-char (name)
  (let ((c (read-from-string (concat "#\\" name))))
    c))

(defun keycode-test-round-trip-p (c)
  (eql c (read-from-string (prin1-to-string c))))

;;;; names

(deftest keycode-function-key-names-count ()
  (length *keycode-test-function-key-names*)
  => 54)

(deftest keycode-function-key-name-round-trip ()
  ;; every name, every combination of modifiers: read, print, and the spelling
  ;; comes back
  (let ((bad nil))
    (dolist (m *keycode-test-modifiers*)
      (dolist (n *keycode-test-function-key-names*)
        (let* ((spelling (concat m n))
               (c (keycode-test-read-char spelling)))
          (unless (and (characterp c)
                       (string= (prin1-to-string c) (concat "#\\" spelling)))
            (push spelling bad)))))
    bad)
  => nil)

(deftest keycode-function-keys-are-keys ()
  (let ((bad nil))
    (dolist (n *keycode-test-function-key-names*)
      (let ((c (keycode-test-read-char n)))
        (unless (and (characterp c)
                     (>= (char-code c) char-code-limit))
          (push n bad))))
    bad)
  => nil)

(deftest keycode-function-keys-differ ()
  ;; 54 names x 7 modifier sets are 378 different characters
  (let ((h (make-hash-table :test #'eql)))
    (dolist (m *keycode-test-modifiers*)
      (dolist (n *keycode-test-function-key-names*)
        (setf (gethash (keycode-test-read-char (concat m n)) h) t)))
    (hash-table-count h))
  => 378)

(deftest keycode-pseudo-control-chars ()
  ;; "C-!" and the like (the Shift and Ctrl bits are a part of their number).
  ;; Every one that the reader takes round-trips, with and without M-.
  (let ((n 0) (bad nil))
    (dotimes (code 95)
      (let* ((ch (code-char (+ 33 code)))
             (c (or (ignore-errors
                     (read-from-string (concat "#\\C-" (string ch))))
                    ;; the characters that end a token need a backslash
                    (ignore-errors
                     (read-from-string (concat "#\\C-\\" (string ch)))))))
        (when (and (characterp c) (>= (char-code c) char-code-limit))
          (incf n)
          (let ((m (set-meta-bit c t)))
            (unless (and (keycode-test-round-trip-p c)
                         (keycode-test-round-trip-p m)
                         (not (eql c m))
                         (eql (set-meta-bit m nil) c))
              (push ch bad))))))
    (values n bad))
  => 35
  => nil)

(deftest keycode-pseudo-control-char-spelling ()
  (values (prin1-to-string #\C-!)
          (prin1-to-string #\C-M-!)
          (princ-to-string #\C-!))
  => "#\\C-!"
  => "#\\C-M-!"
  => "C-!")

(deftest keycode-meta-ascii-round-trip ()
  (let ((bad nil))
    (dotimes (code 127)
      (let ((c (set-meta-bit (code-char code) t)))
        (unless (and (>= (char-code c) char-code-limit)
                     (keycode-test-round-trip-p c)
                     (eql (set-meta-bit c nil) (code-char code)))
          (push code bad))))
    bad)
  => nil)

(deftest keycode-meta-ascii-spelling ()
  (values (eql #\M-a (set-meta-bit #\a t))
          (prin1-to-string #\M-a)
          (prin1-to-string #\M-SPC)
          (prin1-to-string #\C-M-a)
          (eql #\C-M-a (set-meta-bit #\C-a t)))
  => t
  => "#\\M-a"
  => "#\\M-SPC"
  => "#\\C-M-a"
  => t)

;;;; values

(deftest keycode-char-code-of-keys ()
  (values (characterp #\F1)
          (>= (char-code #\F1) char-code-limit)
          (< (char-code #\a) char-code-limit)
          (char-code #\F1)
          (char-code #\M-x)
          (char-code #\C-x)
          (char-code #\x))
  => t
  => t
  => t
  => #x100ff0f
  => #x1008078
  => #x18
  => #x78)

(deftest keycode-code-char-of-keys ()
  (values (eql (code-char #x100ff0f) #\F1)
          (eql (code-char #x1008078) #\M-x)
          (eql (code-char (char-code #\C-M-F1)) #\C-M-F1)
          (eql (code-char (char-code #\C-!)) #\C-!))
  => t
  => t
  => t
  => t)

;; BUG-0041: code-char cut the code to 16bit, so (code-char #x10041) was #\A.
(deftest keycode-code-char-out-of-range ()
  (values (code-char #x10041)
          (code-char #x1ffff)
          (code-char -1)
          (code-char #x110041)
          ;; KEY_BASE + a plain character is not a key
          (code-char #x1000041)
          ;; the ones that the layout has no key for
          (code-char #x100ff3f)
          (code-char #x100fff8)
          (code-char #x1008080)
          (code-char #x1000000)
          (code-char #x1010000))
  => nil
  => nil
  => nil
  => nil
  => nil
  => nil
  => nil
  => nil
  => nil
  => nil)

(deftest keycode-code-char-text ()
  (values (code-char 65)
          (code-char 0)
          (char-code (code-char #xffff))
          (eql (code-char 65) #\A))
  => #\A
  => #\NUL
  => #xffff
  => t)

(deftest keycode-code-char-out-of-range-compiled ()
  ;; the byte code has its own code-char
  (let ((f (compile nil '(lambda (n) (code-char n)))))
    (values (funcall f #x10041)
            (funcall f 65)
            (funcall f #x100ff0f)))
  => nil
  => #\A
  => #\F1)

(deftest keycode-code-char-type-error ()
  (code-char "a")
  !! type-error)

(deftest keycode-char-code-compiled ()
  (let ((f (compile nil '(lambda (c) (char-code c)))))
    (values (funcall f #\F1) (funcall f #\a)))
  => #x100ff0f
  => #x61)

;;;; keys do not go into text

(deftest keycode-string-of-key ()
  (string #\M-x)
  !! type-error)

(deftest keycode-make-string-of-key ()
  (make-array 2 :element-type 'character :initial-element #\F1)
  !! type-error)

(deftest keycode-setf-char-of-key ()
  (let ((s (make-array 2 :element-type 'character :initial-element #\a)))
    (setf (char s 0) #\C-F1))
  !! type-error)

(deftest keycode-write-char-of-key ()
  (with-output-to-string (s)
    (write-char #\F1 s))
  !! type-error)

(deftest keycode-insert-key ()
  (with-temp-buffer
    (insert #\F1))
  !! type-error)

(deftest keycode-insert-text-still-works ()
  (with-temp-buffer
    (insert #\a #\b)
    (buffer-substring (point-min) (point-max)))
  => "ab")

;;;; printing

(deftest keycode-princ-names ()
  (values (princ-to-string #\C-M-F1)
          (princ-to-string #\M-x)
          (princ-to-string #\S-C-M-F12)
          (princ-to-string #\LBtnDown)
          (format nil "~C" #\M-x)
          (format nil "~:C" #\F1)
          (format nil "~@C" #\M-x)
          (format nil "~A" #\F1)
          (princ-to-string #\a))
  => "C-M-F1"
  => "M-x"
  => "S-C-M-F12"
  => "LBtnDown"
  => "M-x"
  => "F1"
  => "#\\M-x"
  => "F1"
  => "a")

(deftest keycode-prin1-names ()
  (values (prin1-to-string #\F1)
          (prin1-to-string #\C-F1)
          (prin1-to-string #\S-F1)
          (prin1-to-string '(#\M-x #\F1)))
  => "#\\F1"
  => "#\\C-F1"
  => "#\\S-F1"
  => "(#\\M-x #\\F1)")

;;;; the character functions

(deftest keycode-char-predicates ()
  (values (alpha-char-p #\F1) (alpha-char-p #\M-a)
          (digit-char-p #\F1) (digit-char-p #\M-1)
          (upper-case-p #\M-A) (lower-case-p #\M-a)
          (both-case-p #\M-a) (alphanumericp #\M-a)
          (graphic-char-p #\F1) (graphic-char-p #\M-a)
          (standard-char-p #\F1)
          (kanji-char-p #\F1) (kana-char-p #\F1)
          (word-char-p #\F1))
  => nil => nil
  => nil => nil
  => nil => nil
  => nil => nil
  => nil => nil
  => nil
  => nil => nil
  => nil)

(deftest keycode-char-case ()
  (values (eql (char-upcase #\M-a) #\M-a)
          (eql (char-downcase #\M-A) #\M-A)
          (eql (char-upcase #\F1) #\F1)
          (char-upcase #\a)
          (char-downcase #\A))
  => t
  => t
  => t
  => #\A
  => #\a)

(deftest keycode-char-comparison ()
  (values (char= #\F1 #\F1)
          (char= #\F1 #\F2)
          (char/= #\F1 #\F2)
          (char< #\a #\F1)
          (char> #\F2 #\F1)
          (char<= #\M-a #\M-b)
          (char>= #\M-a #\M-b)
          (char-equal #\M-a #\M-a)
          (char-equal #\M-a #\M-A)
          (char-lessp #\a #\M-a)
          (char-not-equal #\F1 #\F2)
          (eql #\F1 #\F1)
          (equal #\F1 #\F1)
          (equalp #\M-a #\M-a))
  => t => nil => t => t => t => t => nil => t => nil => t => t => t => t => t)

(deftest keycode-char-comparison-compiled ()
  (let ((f (compile nil '(lambda (a b)
                           (list (char= a b) (char< a b) (char-equal a b))))))
    (values (funcall f #\F1 #\F1)
            (funcall f #\a #\F1)
            (funcall f #\M-a #\M-A)))
  => (t nil t)
  => (nil t nil)
  => (nil nil nil))

(deftest keycode-as-hash-key-and-case ()
  (let ((h (make-hash-table :test #'eql))
        (e (make-hash-table :test #'equalp)))
    (setf (gethash #\F1 h) 1 (gethash #\M-x h) 2 (gethash #\a h) 3)
    (setf (gethash #\M-a e) 4)
    (values (gethash #\F1 h) (gethash #\M-x h) (gethash #\a h) (gethash #\F2 h)
            (gethash #\M-a e)
            (case #\F1 (#\F2 :no) (#\F1 :yes))
            (case #\M-x (#\x :no) (#\M-x :yes))
            (member #\C-F1 (list #\F1 #\C-F1))
            (position #\F1 (vector #\a #\F1))))
  => 1 => 2 => 3 => nil => 4 => :yes => :yes
  => (#\C-F1)
  => 1)

(deftest keycode-set-meta-bit ()
  (values (eql (set-meta-bit #\x t) #\M-x)
          (eql (set-meta-bit #\M-x nil) #\x)
          (eql (set-meta-bit #\F1 t) #\M-F1)
          (eql (set-meta-bit #\M-F1 nil) #\F1)
          (eql (set-meta-bit #\S-C-F1 t) #\S-C-M-F1)
          (eql (set-meta-bit #\S-C-M-F1 nil) #\S-C-F1)
          (eql (set-meta-bit #\x nil) #\x)
          (eql (set-meta-bit #\M-x t) #\M-x)
          (eql (set-meta-bit #\F1 nil) #\F1))
  => t => t => t => t => t => t => t => t => t)

(deftest keycode-set-meta-bit-non-ascii-text ()
  ;; a non-ASCII text character has no meta bit
  (let ((c (unicode-char #x3042)))
    (values (eql (set-meta-bit c t) c)
            (eql (set-meta-bit c nil) c)))
  => t
  => t)

;;;; keymaps

(deftest keycode-keymap-index-round-trip ()
  ;; 128 ASCII + 4 x 63 function keys.  The one index that does not come back
  ;; is 373 (Shift + Ctrl + number 56): it is the empty character, which is not a
  ;; key (it was not before S4-k either).
  (let ((bad nil))
    (dotimes (i 380)
      (let ((c (editor::*keymap-index-char i)))
        (unless (and (characterp c)
                     (eql (editor::*keymap-char-index c) i))
          (push i bad))))
    (values bad
            (editor::*keymap-index-char 380)
            (editor::*keymap-index-char -1)
            (editor::*keymap-char-index #\M-x)
            (editor::*keymap-char-index (unicode-char #x3042))))
  => (373)
  => nil
  => nil
  => nil
  => nil)

(deftest keycode-keymap-index-values ()
  (values (editor::*keymap-char-index #\a)
          (eql (editor::*keymap-index-char 97) #\a)
          (editor::*keymap-char-index #\PageUp)
          (editor::*keymap-char-index #\S-PageUp)
          (editor::*keymap-char-index #\C-PageUp)
          (editor::*keymap-char-index #\S-C-PageUp)
          (eql (editor::*keymap-index-char 128) #\PageUp))
  => 97
  => t
  => 128
  => 191
  => 254
  => 317
  => t)

(defun keycode-test-keymap-checks (map)
  (define-key map #\F1 'keycode-test-f1)
  (define-key map #\C-F1 'keycode-test-c-f1)
  (define-key map #\S-C-F1 'keycode-test-sc-f1)
  (define-key map #\M-x 'keycode-test-m-x)
  (define-key map #\M-F2 'keycode-test-m-f2)
  (define-key map #\C-! 'keycode-test-c-bang)
  (define-key map #\a 'keycode-test-a)
  (define-key map '(#\C-x #\F3) 'keycode-test-cx-f3)
  (values (lookup-keymap map #\F1)
          (lookup-keymap map #\C-F1)
          (lookup-keymap map #\S-C-F1)
          (lookup-keymap map #\S-F1)
          (lookup-keymap map #\M-F1)
          (lookup-keymap map #\M-x)
          (lookup-keymap map '(#\ESC #\x))
          (lookup-keymap map #\M-F2)
          (lookup-keymap map '(#\ESC #\F2))
          (lookup-keymap map #\C-!)
          (lookup-keymap map #\a)
          (lookup-keymap map #\A t)
          (lookup-keymap map #\A)
          (lookup-keymap map '(#\C-x #\F3))
          (lookup-keymap map '(#\C-x #\F4))))

(deftest keycode-define-key-dense-keymap ()
  (keycode-test-keymap-checks (make-keymap))
  => keycode-test-f1
  => keycode-test-c-f1
  => keycode-test-sc-f1
  => nil
  => nil
  => keycode-test-m-x
  => keycode-test-m-x
  => keycode-test-m-f2
  => keycode-test-m-f2
  => keycode-test-c-bang
  => keycode-test-a
  => keycode-test-a
  => nil
  => keycode-test-cx-f3
  => nil)

(deftest keycode-define-key-sparse-keymap ()
  (keycode-test-keymap-checks (make-sparse-keymap))
  => keycode-test-f1
  => keycode-test-c-f1
  => keycode-test-sc-f1
  => nil
  => nil
  => keycode-test-m-x
  => keycode-test-m-x
  => keycode-test-m-f2
  => keycode-test-m-f2
  => keycode-test-c-bang
  => keycode-test-a
  => keycode-test-a
  => nil
  => keycode-test-cx-f3
  => nil)

(deftest keycode-sparse-keymap-holds-keys ()
  ;; the key in a sparse keymap is the character itself (eql)
  (let ((map (make-sparse-keymap)))
    (define-key map #\F5 'keycode-test-f5)
    (define-key map #\M-x 'keycode-test-m-x)
    (values (assoc #\F5 (cdr map))
            (not (null (assoc #\ESC (cdr map))))))
  => (#\F5 . keycode-test-f5)
  => t)

(deftest keycode-keymapp-and-macro ()
  ;; a vector of 380 characters is a kbd macro, not a keymap
  (values (not (null (keymapp (make-keymap))))
          (keymapp (make-array 380 :initial-element #\a))
          (editor::*kbd-macro-p (make-array 380 :initial-element #\a))
          (editor::*kbd-macro-p (make-keymap)))
  => t
  => nil
  => t
  => nil)

;;;; the key queue

(defun keycode-test-drain-queue ()
  ;; run what is in the key queue as commands
  (editor::*kbd-queue-dispatch))

(deftest keycode-queue-read-char ()
  (progn
    (editor::*kbd-queue-put #\a)
    (editor::*kbd-queue-put #\F1)
    (editor::*kbd-queue-put #\M-x)
    (editor::*kbd-queue-put #\C-S-F12)
    (list (read-char *keyboard*) (read-char *keyboard*)
          (read-char *keyboard*) (read-char *keyboard*)))
  => (#\a #\F1 #\M-x #\S-C-F12))

(deftest keycode-queue-marks-are-not-a-part-of-the-key ()
  ;; mouse and menu events carry a mark above the key.  It never reaches Lisp,
  ;; and it does not make a key of a text character
  (progn
    (editor::*kbd-queue-put #\LBtnDown 1)
    (editor::*kbd-queue-put #\C-RBtnUp 1)
    (editor::*kbd-queue-put #\a 1)
    (editor::*kbd-queue-put #\F1 2)
    (list (read-char *keyboard*) (read-char *keyboard*)
          (read-char *keyboard*) (read-char *keyboard*)))
  => (#\LBtnDown #\C-RBtnUp #\a #\F1))

(deftest keycode-queue-text-is-not-mouse ()
  ;; the largest text character is not mistaken for a mark
  (let ((c (code-char #xffff)))
    (editor::*kbd-queue-put c)
    (editor::*kbd-queue-put (code-char #x8000))
    (list (eql (read-char *keyboard*) c)
          (char-code (read-char *keyboard*))))
  => (t #x8000))

(deftest keycode-queue-put-type-error ()
  (editor::*kbd-queue-put "a")
  !! type-error)

(deftest keycode-unread-and-peek-key ()
  (progn
    (editor::*kbd-queue-put #\F2)
    (let* ((p (peek-char nil *keyboard*))
           (c (read-char *keyboard*)))
      (unread-char #\M-F3 *keyboard*)
      (list p c (peek-char nil *keyboard*) (read-char *keyboard*))))
  => (#\F2 #\F2 #\M-F3 #\M-F3))

(deftest keycode-get-recent-keys ()
  (progn
    (editor::*kbd-queue-put #\a)
    (editor::*kbd-queue-put #\F1)
    (editor::*kbd-queue-put #\M-x)
    (read-char *keyboard*)
    (read-char *keyboard*)
    (read-char *keyboard*)
    (let ((keys (get-recent-keys)))
      (list (vectorp keys)
            (stringp keys)
            (coerce (subseq keys (- (length keys) 3)) 'list))))
  => (t nil (#\a #\F1 #\M-x)))

;;;; dispatch

(defvar *keycode-test-seen* nil)

(defun keycode-test-command ()
  (interactive)
  (push *last-command-char* *keycode-test-seen*))

(deftest keycode-dispatch-keys ()
  ;; each key reaches its command and *last-command-char* is the key itself
  (let ((*keycode-test-seen* nil)
        (map (make-sparse-keymap))
        (keys (list #\F5 #\C-F5 #\S-C-M-F5 #\M-F5 #\M-z #\C-! #\M-C-! #\LBtnDown
                    #\S-MBtnUp #\C-M-a)))
    (dolist (k keys)
      (define-key map k 'keycode-test-command))
    (with-temp-buffer
      (use-keymap map)
      (dolist (k keys)
        (editor::*kbd-queue-put k))
      (editor::*kbd-queue-dispatch))
    (equal (reverse *keycode-test-seen*) keys))
  => t)

(deftest keycode-dispatch-keys-through-meta-prefix ()
  (let ((*keycode-test-seen* nil)
        (map (make-sparse-keymap)))
    (define-key map '(#\ESC #\z) 'keycode-test-command)
    (with-temp-buffer
      (use-keymap map)
      (editor::*kbd-queue-put #\M-z)
      (editor::*kbd-queue-put #\ESC)
      (editor::*kbd-queue-put #\z)
      (editor::*kbd-queue-dispatch))
    ;; M-z and ESC z are the same key sequence
    (length *keycode-test-seen*))
  => 2)

(deftest keycode-dispatch-ascii-and-text ()
  (with-temp-buffer
    (editor::*kbd-queue-put #\a)
    (editor::*kbd-queue-put #\b)
    (editor::*kbd-queue-put (unicode-char #x3042))
    (editor::*kbd-queue-dispatch)
    (let ((s (buffer-substring (point-min) (point-max))))
      (list (length s)
            (char= (char s 0) #\a)
            (char= (char s 1) #\b)
            (eql (char s 2) (unicode-char #x3042)))))
  => (3 t t t))

(deftest keycode-dispatch-menu-mark-is-not-a-key ()
  ;; a menu event carries the menu id: its low bits are not a key, so the key
  ;; that has the same low bits is not run
  (let ((*keycode-test-seen* nil)
        (map (make-sparse-keymap)))
    (define-key map #\F5 'keycode-test-command)
    (with-temp-buffer
      (use-keymap map)
      (editor::*kbd-queue-put #\F5 2)
      (editor::*kbd-queue-dispatch))
    *keycode-test-seen*)
  => nil)

(deftest keycode-dispatch-marked-text-is-not-mouse ()
  ;; a mouse mark on a text character is dropped: the character is typed
  (with-temp-buffer
    (editor::*kbd-queue-put #\a 1)
    (editor::*kbd-queue-dispatch)
    (buffer-substring (point-min) (point-max)))
  => "a")

;;;; keyboard macros

(defun keycode-test-record-macro ()
  ;; type: a, Left, M-f (insert, move, move by a word) and record it
  (editor::*kbd-queue-put #\a)
  (editor::*kbd-queue-put #\Left)
  (editor::*kbd-queue-put #\M-f)
  (start-save-kbd-macro)
  (editor::*kbd-queue-dispatch)
  (stop-save-kbd-macro))

(deftest keycode-kbd-macro-is-a-vector-of-characters ()
  (with-temp-buffer
    (insert "bc d")
    (goto-char (point-min))
    (let ((macro (keycode-test-record-macro)))
      (values (vectorp macro)
              (stringp macro)
              (length macro)
              (equalp macro (vector #\a #\Left #\M-f))
              (every #'characterp macro)
              (buffer-substring (point-min) (point-max))
              (editor::*kbd-macro-p macro))))
  => t
  => nil
  => 3
  => t
  => t
  => "abc d"
  => t)

(deftest keycode-kbd-macro-runs ()
  (let ((macro (with-temp-buffer
                 (insert "bc d")
                 (goto-char (point-min))
                 (keycode-test-record-macro))))
    (with-temp-buffer
      (insert "xy z")
      (goto-char (point-min))
      (command-execute macro)
      (list (buffer-substring (point-min) (point-max))
            (- (point) (point-min)))))
  => ("axy z" 3))

(deftest keycode-kbd-macro-as-function-definition ()
  (let ((macro (with-temp-buffer
                 (insert "bc d")
                 (goto-char (point-min))
                 (keycode-test-record-macro))))
    (setf (symbol-function 'keycode-test-macro) macro)
    (unwind-protect
        (with-temp-buffer
          (insert "xy z")
          (goto-char (point-min))
          (command-execute 'keycode-test-macro)
          (list (commandp 'keycode-test-macro)
                (buffer-substring (point-min) (point-max))))
      (fmakunbound 'keycode-test-macro)))
  => (t "axy z"))

(deftest keycode-kbd-macro-old-string-form ()
  ;; a macro written before S4-k is a string of the 16bit values (#x8066 is M-f,
  ;; #xff04 is Left): it still runs
  (let ((macro (concat "a"
                       (string (code-char #xff04))
                       (string (code-char #x8066)))))
    (with-temp-buffer
      (insert "xy z")
      (goto-char (point-min))
      (command-execute macro)
      (list (buffer-substring (point-min) (point-max))
            (- (point) (point-min)))))
  => ("axy z" 3))

(deftest keycode-kbd-macro-repeat ()
  (let ((macro (vector #\a #\a)))
    (with-temp-buffer
      (let ((*prefix-value* 3))
        (command-execute macro))
      (buffer-substring (point-min) (point-max))))
  => "aaaaaa")

(deftest keycode-kbd-macro-empty ()
  (values (commandp (vector))
          (editor::*kbd-macro-p (vector))
          (editor::*kbd-macro-p (vector #\a 1))
          (editor::*kbd-macro-p "abc")
          (editor::*kbd-macro-p 'a))
  => nil
  => nil
  => nil
  => t
  => nil)

(deftest keycode-kbd-macro-not-a-command ()
  (command-execute (vector #\a 1))
  !! error)

;;;; the function bar and quit char take keys

(deftest keycode-set-quit-char-takes-keys ()
  (let ((old (quit-char)))
    (unwind-protect
        (values (eql (set-quit-char #\C-g) #\C-g)
                (eql (quit-char) #\C-g)
                (eql (set-quit-char #\M-F10) #\M-F10)
                (eql (quit-char) #\M-F10))
      (set-quit-char old)))
  => t
  => t
  => t
  => t)

(deftest keycode-function-bar-label-takes-keys ()
  (values (set-function-bar-label #\F1 "KeycodeTest")
          (set-function-bar-label #\M-F1 nil)
          (set-function-bar-label #\F1 nil))
  => t
  => t
  => t)

(deftest keycode-function-bar-label-rejects-others ()
  (set-function-bar-label #\a "x")
  !! error)

;;;; Lisp code that reads keys from the keyboard

(deftest keycode-digit-argument-reads-a-key ()
  ;; process-prefix-args calls digit-char-p, set-meta-bit and unread-char with
  ;; the next key.  M-2 then Right moves two characters.
  (with-temp-buffer
    (insert "abcdefgh")
    (goto-char (point-min))
    (editor::*kbd-queue-put #\Right)
    (let ((*last-command-char* #\M-2))
      (digit-argument nil))
    (editor::*kbd-queue-dispatch)
    (point))
  => 2)

(deftest keycode-universal-argument-reads-keys ()
  ;; C-u 3 Right
  (with-temp-buffer
    (insert "abcdefgh")
    (goto-char (point-min))
    (editor::*kbd-queue-put #\3)
    (editor::*kbd-queue-put #\Right)
    (let ((*last-command-char* #\C-u))
      (universal-argument nil))
    (editor::*kbd-queue-dispatch)
    (point))
  => 3)

(deftest keycode-isearch-exits-on-a-function-key ()
  ;; the key ends the search and is run as a command: a, Right
  (with-temp-buffer
    (insert "xaxbxa")
    (goto-char (point-min))
    (editor::*kbd-queue-put #\a)
    (editor::*kbd-queue-put #\Right)
    (editor::*kbd-queue-put #\RET)
    (editor::isearch-forward)
    (editor::*kbd-queue-dispatch)
    (point))
  => 3)

(deftest keycode-quote-char-of-a-key-is-an-error ()
  ;; C-q F1: a key cannot be inserted (it used to insert a character of the
  ;; internal code #xff0f)
  (with-temp-buffer
    (editor::*kbd-queue-put #\F1)
    (quote-char))
  !! type-error)

(deftest keycode-quote-char-of-text ()
  (with-temp-buffer
    (editor::*kbd-queue-put #\z)
    (quote-char)
    (buffer-substring (point-min) (point-max)))
  => "z")

(deftest keycode-kbd-macro-prints-and-reads-back ()
  ;; the history file and insert-kbd-macro write the macro with ~S / write
  ;; :readably
  (let* ((macro (vector #\a #\F9 #\M-x #\S-C-M-F1 #\C-!))
         (text (with-output-to-string (s)
                 (write (list "name" macro) :stream s :escape t :circle t
                        :pretty t :readably t)))
         (back (read-from-string text)))
    (values (equalp back (list "name" macro))
            (every #'eql (cadr back) macro)
            (not (null (string-match "#.M-x" text)))))
  => t
  => t
  => t)
