;;; -*- mode: lisp; package: user -*-
;;;
;;; Tests of the keyboard input of UTF-16 (S4 R10).  The keyboard and the IME
;;; give characters as UTF-16 code units (ToUnicode, WM_CHAR of a Unicode
;;; window, WM_IME_CHAR, ImmGetCompositionStringW).  They all go through one
;;; function that makes Chars from the units:
;;;
;;;   (editor::*utf16-to-chars units [unicode [half-width]]) => list of values
;;;   (editor::*kbd-queue-put-utf16 unit)  puts one unit into the key queue
;;;
;;; unicode is 0 (the internal codes of now), 1 (Unicode code points) or nil
;;; (the side that this build uses).  Both sides can be tested in every build.
;;;
;;; This file is ASCII: the characters are written as numbers.

(in-package :user)

(defun kbd-utf16-units (units unicode)
  (editor::*utf16-to-chars units unicode))

;;;; the Unicode side: a Char is a code point

(deftest kbd-utf16-unicode-bmp ()
  ;; ASCII, kana, kanji, half width katakana, the last code of the BMP that is
  ;; not a surrogate
  (kbd-utf16-units '(#x41 #x3042 #x6f22 #xff71 #xffff #xe000) 1)
  => (#x41 #x3042 #x6f22 #xff71 #xffff #xe000))

(deftest kbd-utf16-unicode-pair ()
  (list (kbd-utf16-units '(#xd83d #xde00) 1)
        (kbd-utf16-units '(#xd800 #xdc00) 1)
        (kbd-utf16-units '(#xdbff #xdfff) 1))
  => ((#x1f600) (#x10000) (#x10ffff)))

(deftest kbd-utf16-unicode-pair-among-others ()
  (kbd-utf16-units '(#x41 #xd83d #xde00 #x3042 #xd840 #xdc0b #x62) 1)
  => (#x41 #x1f600 #x3042 #x2000b #x62))

(deftest kbd-utf16-unicode-lone-surrogates ()
  ;; a surrogate with no partner is the character of its own value
  (list (kbd-utf16-units '(#xde00) 1)
        (kbd-utf16-units '(#xde00 #xd83d) 1)
        (kbd-utf16-units '(#xd83d #x41) 1)
        (kbd-utf16-units '(#xd83d #xd83d #xde00) 1)
        (kbd-utf16-units '(#x41 #xd83d) 1))
  => ((#xde00) (#xde00 #xd83d) (#xd83d #x41) (#xd83d #x1f600) (#x41 #xd83d)))

;;;; the side of now: each unit is a character by itself

(deftest kbd-utf16-internal-bmp ()
  ;; the characters of the internal codes (the Unicode of the character is
  ;; the unit)
  (let ((units '(#x41 #x3042 #x6f22 #xff71)))
    (equal (kbd-utf16-units units 0)
           (mapcar #'(lambda (u) (char-code (unicode-char u))) units)))
  => t)

(deftest kbd-utf16-internal-surrogates-are-independent ()
  ;; a pair is not combined: the units are what each one gives, in order.
  ;; (No internal code has a value above #xffff.)
  (let* ((hi (kbd-utf16-units '(#xd83d) 0))
         (lo (kbd-utf16-units '(#xde00) 0))
         (both (kbd-utf16-units '(#xd83d #xde00) 0)))
    (list (equal both (append hi lo))
          (null (member-if #'(lambda (c) (> c #xffff)) both))
          (not (equal both '(#x1f600)))))
  => (t t t))

(deftest kbd-utf16-internal-half-width ()
  ;; the keyboard takes the half width form of a character that has one
  (let ((full (kbd-utf16-units '(#xff21) 0))
        (half (editor::*utf16-to-chars '(#xff21) 0 t)))
    (list (= (length full) 1) (= (length half) 1)))
  => (t t))

;;;; the queue

(defun kbd-utf16-queue (units)
  ;; what the key queue gives for the units, read as characters
  (let ((n (length (editor::*utf16-to-chars units nil t))))
    (dolist (u units)
      (editor::*kbd-queue-put-utf16 u))
    (let ((r nil))
      (dotimes (i n)
        (push (char-code (read-char *keyboard*)) r))
      (nreverse r))))

(deftest kbd-utf16-queue-same-as-the-function ()
  (let ((units '(#x41 #x3042 #x6f22 #xd83d #xde00 #x62)))
    (equal (kbd-utf16-queue units)
           (editor::*utf16-to-chars units nil t)))
  => t)

(deftest kbd-utf16-queue-ascii ()
  (kbd-utf16-queue '(#x61 #x20 #x7a))
  => (#x61 #x20 #x7a))

(deftest kbd-utf16-put-type-error ()
  (editor::*kbd-queue-put-utf16 "a")
  !! type-error)

(deftest kbd-utf16-put-range-error ()
  (editor::*kbd-queue-put-utf16 #x10000)
  !! range-error)
