;;; -*- mode: lisp; package: user -*-
;;;
;;; Tests for char_face_of (src/font.cc), S4 stage 3 R3.  It is the one
;;; function that decides the font slot, the display width and the UTF-16
;;; units of a character, for the screen, the printer and the status area.
;;; The two sides are tested by si:char-face-internal (the old internal code,
;;; XYZZY_CHAR_UNICODE 0) and si:char-face-unicode (a Unicode code point,
;;; XYZZY_CHAR_UNICODE 1), whichever side the build uses.
;;;
;;; Both return (font width nwc wc0 wc1 subst):
;;;   font   :ascii :jp :latin :cyrillic :greek :cn-simplified
;;;          :cn-traditional :hangul :georgian :lucida
;;;   width  columns (1 or 2)
;;;   nwc    the number of UTF-16 units (0: nothing to draw)
;;;   wc0 wc1  the UTF-16 units
;;;   subst  what the printer draws when nwc is 0 (0: nothing)
;;; The Han slots as numbers (the HAN argument): 1 jp, 5 gb, 6 big5, 7 hangul.
;;; This file is ASCII only.

(in-package :user)

(defun char-face-range-bad (from to font width)
  "The codes in FROM..TO whose face is not as expected: the font, the
width (when WIDTH), and the UTF-16 unit equal to char-unicode."
  (let ((bad nil))
    (do ((c from (1+ c)))
        ((> c to))
      (let ((f (si:char-face-internal c)))
        (when (or (not (eq (car f) font))
                  (and width (/= (second f) width))
                  (and (/= (third f) 0)
                       (not (eql (fourth f) (char-unicode (code-char c))))))
          (push (list c f) bad))))
    (nreverse bad)))

;;; The old internal code

(deftest char-face-internal-ascii-kana ()
  (si:char-face-internal #x41)
  => (:ascii 1 1 65 0 0)
  (si:char-face-internal #x20)
  => (:ascii 1 1 32 0 0)
  ;; JIS X 0201 kana: half width, the Japanese slot
  (si:char-face-internal #xb1)
  => (:jp 1 1 #xff71 0 0)
  (si:char-face-internal #xdf)
  => (:jp 1 1 #xff9f 0 0))

(deftest char-face-internal-cp932 ()
  (si:char-face-internal #x82a0)
  => (:jp 2 1 #x3042 0 0)
  (si:char-face-internal #x889f)
  => (:jp 2 1 #x4e9c 0 0)
  (si:char-face-internal #x8140)
  => (:jp 2 1 #x3000 0 0)
  ;; a lead byte without a trail byte: nothing to draw, the printer draws the
  ;; middle dot (0x8145)
  (si:char-face-internal #x8100)
  => (:jp 2 0 0 0 0 #x30fb))

(deftest char-face-internal-iso-and-latin ()
  (char-face-range-bad #x100 #x2ff :latin 1)
  => nil
  (char-face-range-bad #x300 #x37f :cyrillic 1)
  => nil
  (char-face-range-bad #x380 #x3ff :greek 1)
  => nil
  (char-face-range-bad #x400 #x57f :latin 1)
  => nil
  (char-face-range-bad #x1300 #x14ff :latin 1)
  => nil
  (char-face-range-bad #x1500 #x157f :georgian 1)
  => nil)

(deftest char-face-internal-ipa-and-lucida ()
  ;; IPA is drawn with the Japanese slot, half width
  (char-face-range-bad #x1580 #x15df :jp 1)
  => nil
  ;; spacing modifier letters and combining marks: the Lucida face
  (char-face-range-bad #x1600 #x16bf :lucida 1)
  => nil
  (car (si:char-face-internal #x1600))
  => :lucida)

(deftest char-face-internal-cjk-slots ()
  ;; KS C 5601, GB 2312, Big5 and JIS X 0212 keep their own slots
  (char-face-range-bad #x1700 #x1790 :hangul 2)
  => nil
  (char-face-range-bad #x3a00 #x3a90 :cn-simplified 2)
  => nil
  (char-face-range-bad #xa140 #xa17f :cn-traditional 2)
  => nil
  (char-face-range-bad #x5d00 #x5d7f :jp nil)
  => nil
  ;; JIS X 0212 half width and the Unicode Japanese area
  (char-face-range-bad #xd00 #xfff :jp 1)
  => nil
  (char-face-range-bad #x1000 #x12ff :jp 2)
  => nil)

(deftest char-face-internal-by-unicode ()
  ;; a character that only KS C 5601 or GB 2312 has
  (car (si:char-face-internal (char-code (unicode-char #xd55c))))
  => :hangul
  (fourth (si:char-face-internal (char-code (unicode-char #xd55c))))
  => #xd55c
  (car (si:char-face-internal (char-code (unicode-char #x6c49))))
  => :cn-simplified
  (fourth (si:char-face-internal (char-code (unicode-char #x6c49))))
  => #x6c49)

;;; A Unicode code point

(deftest char-face-unicode-basic ()
  (si:char-face-unicode #x41)
  => (:ascii 1 1 65 0 0)
  (si:char-face-unicode #x3042)
  => (:jp 2 1 #x3042 0 0)
  (si:char-face-unicode #xff71)
  => (:jp 1 1 #xff71 0 0)
  (si:char-face-unicode #xd55c)
  => (:hangul 2 1 #xd55c 0 0)
  (si:char-face-unicode #x10d0)
  => (:georgian 1 1 #x10d0 0 0))

(deftest char-face-unicode-han-language ()
  ;; a Han character follows the Han slot of the buffer
  (si:char-face-unicode #x6f22)
  => (:jp 2 1 #x6f22 0 0)
  (si:char-face-unicode #x6f22 1)
  => (:jp 2 1 #x6f22 0 0)
  (si:char-face-unicode #x6f22 5)
  => (:cn-simplified 2 1 #x6f22 0 0)
  (si:char-face-unicode #x6f22 6)
  => (:cn-traditional 2 1 #x6f22 0 0)
  (si:char-face-unicode #x6f22 7)
  => (:hangul 2 1 #x6f22 0 0)
  ;; kana are always the Japanese slot
  (si:char-face-unicode #x3042 7)
  => (:jp 2 1 #x3042 0 0))

(deftest char-face-unicode-ambiguous-width ()
  ;; East Asian Width Ambiguous: width 2 and the Han slot by default
  (let ((ed:*east-asian-ambiguous-width* 2))
    (list (si:char-face-unicode #x3b1)
          (si:char-face-unicode #x25cb 7)
          (si:char-face-unicode #x410)))
  => ((:jp 2 1 #x3b1 0 0)
      (:hangul 2 1 #x25cb 0 0)
      (:jp 2 1 #x410 0 0))
  ;; with 1: the script of the character, one column
  (let ((ed:*east-asian-ambiguous-width* 1))
    (list (si:char-face-unicode #x3b1)
          (si:char-face-unicode #x410)
          (si:char-face-unicode #xe9)
          (second (si:char-face-unicode #x25cb))
          (second (si:char-face-unicode #x2500))))
  => ((:greek 1 1 #x3b1 0 0)
      (:cyrillic 1 1 #x410 0 0)
      (:latin 1 1 #xe9 0 0)
      1
      1))

(deftest char-face-unicode-beyond-bmp ()
  ;; surrogate pairs; the slot of the other characters is LATIN until C3
  (si:char-face-unicode #x1f600)
  => (:latin 2 2 #xd83d #xde00 0)
  (si:char-face-unicode #x20bb7)
  => (:jp 2 2 #xd842 #xdfb7 0)
  ;; a surrogate has nothing to draw
  (third (si:char-face-unicode #xd800))
  => 0
  (third (si:char-face-unicode #xdfff))
  => 0)

(deftest char-face-unicode-lucida ()
  (si:char-face-unicode #x2b0)
  => (:lucida 1 1 #x2b0 0 0)
  ;; a combining mark still takes one column on the screen
  (si:char-face-unicode #x301)
  => (:lucida 1 1 #x301 0 0)
  (si:char-face-unicode #x36f)
  => (:lucida 1 1 #x36f 0 0))

;;; The slot of the Han characters of a buffer, from its encoding

(deftest char-face-encoding-han-font ()
  (list (si:encoding-han-font ed::*encoding-sjis*)
        (si:encoding-han-font ed::*encoding-euc-kr*)
        (si:encoding-han-font ed::*encoding-euc-gb*)
        (si:encoding-han-font ed::*encoding-big5*)
        (si:encoding-han-font ed::*encoding-iso-8859-1*)
        (si:encoding-han-font ed::*encoding-utf8n*)
        (si:encoding-han-font ed::*encoding-utf8n-kr*))
  => (1 7 5 6 1 1 7))
