;;; -*- mode: lisp; package: user -*-
;;;
;;; Tests of the codec layer (src/codec*.cc): converters between bytes and
;;; Unicode code points.  They are called directly from Lisp:
;;;
;;;   (si:codec-decode encoding octets [memory]) => code-point vector, memory
;;;   (si:codec-encode encoding code-points [memory]) => octet vector
;;;   (si:codec-can-encode encoding code-point) => t or nil
;;;
;;; Code points are Unicode.  #x110080..#x1100ff are raw bytes: a byte that
;;; no character corresponds to (an invalid byte, a code that the character
;;; set does not define) is kept so that the encoder writes the same byte.
;;;
;;; memory is the memory of the duplicated codes of cp932 (design 5.4).
;;; Without it, a converter is the normalization of Windows.
;;;
;;; The cp932 tests check every code against the OS of the machine that runs
;;; the test (MultiByteToWideChar and WideCharToMultiByte of code page 932).
;;; The tests named ...-old-... compare with the converters in encoding.cc
;;; (convert-encoding-to-internal and char-unicode).  They are for the time
;;; when both exist.
;;;
;;; This file is ASCII: bytes and code points are written as numbers.

(require "foreign")

(in-package :user)

(c:define-dll-entry
  c:int
  (codec-test-mbtwc) (c:u_int c:u_long (c:void *) c:int (c:void *) c:int)
  "kernel32" "MultiByteToWideChar")

(c:define-dll-entry
  c:int
  (codec-test-wctmb) (c:u_int c:u_long (c:void *) c:int (c:void *) c:int
                      (c:void *) (c:void *))
  "kernel32" "WideCharToMultiByte")

(defvar *codec-test-in* (si:make-chunk nil 16))
(defvar *codec-test-out* (si:make-chunk nil 16))
(defvar *codec-test-flag* (si:make-chunk nil 4))

(defun codec-test-range (from to)
  (let ((l nil))
    (do ((i from (1+ i)))
        ((> i to))
      (push i l))
    (nreverse l)))

(defun codec-test-cp932-leads ()
  (append (codec-test-range #x81 #x9f) (codec-test-range #xe0 #xfc)))

(defun codec-test-raw (b)
  (if (>= b #x80) (+ #x110000 b) b))

(defun codec-test-decode (enc bytes &optional (memory nil memory-p))
  (coerce (if memory-p
              (si:codec-decode enc bytes memory)
            (si:codec-decode enc bytes))
          'list))

(defun codec-test-encode (enc points &optional (memory nil memory-p))
  (coerce (if memory-p
              (si:codec-encode enc points memory)
            (si:codec-encode enc points))
          'list))

;; The OS.  Strict decode (MB_ERR_INVALID_CHARS): a list of the code points,
;; or nil for an invalid or undefined code.
(defun codec-test-os-decode (&rest bytes)
  (let ((n 0))
    (dolist (b bytes)
      (si:pack-uint8 *codec-test-in* n b)
      (incf n))
    (let ((r (codec-test-mbtwc 932 8 *codec-test-in* n *codec-test-out* 8)))
      (if (zerop r)
          nil
        (let ((l nil))
          (dotimes (i r)
            (push (si:unpack-uint16 *codec-test-out* (* 2 i)) l))
          (nreverse l))))))

;; WC_NO_BEST_FIT_CHARS = #x400.  A list of bytes, or nil when the character
;; has no code (the default character would be written).
(defun codec-test-os-encode (u &optional (flags #x400))
  (si:pack-uint16 *codec-test-in* 0 u)
  (si:pack-uint32 *codec-test-flag* 0 0)
  (let ((r (codec-test-wctmb 932 flags *codec-test-in* 1 *codec-test-out* 8
                             0 *codec-test-flag*)))
    (if (or (zerop r) (/= 0 (si:unpack-uint32 *codec-test-flag* 0)))
        nil
      (let ((l nil))
        (dotimes (i r)
          (push (si:unpack-uint8 *codec-test-out* i) l))
        (nreverse l)))))


;;;; cp932 and the OS

(deftest codec-cp932-os-single-bytes ()
  "All 256 single bytes decode as the OS does."
  (let ((bad nil))
    (dotimes (b 256)
      (let ((os (codec-test-os-decode b))
            (me (codec-test-decode *encoding-sjis* (list b))))
        (unless (equal (or os (list (codec-test-raw b))) me)
          (push (list b os me) bad))))
    (or (nreverse bad) :ok))
  => :ok)

(deftest codec-cp932-os-double-bytes ()
  "All lead byte and trail byte pairs decode as the OS does: 9604 defined
codes, 1736 pairs that are not defined (1676 in the range of a trail byte and
60 with the trail byte 7f)."
  (let ((bad nil) (defined 0) (undefined 0))
    (dolist (h (codec-test-cp932-leads))
      (dolist (tb (codec-test-range #x40 #xfc))
        (let ((os (codec-test-os-decode h tb))
              (me (codec-test-decode *encoding-sjis* (list h tb))))
          (if os (incf defined) (incf undefined))
          (unless (equal (or os (list (codec-test-raw h) (codec-test-raw tb)))
                         me)
            (push (list h tb os me) bad)))))
    (list defined undefined (nreverse bad)))
  => (9604 1736 nil))

(deftest codec-cp932-os-encode-bmp ()
  "Every BMP code point encodes as the OS does (WC_NO_BEST_FIT_CHARS), except
the 8 characters in the JIS form that the encoder also takes."
  (let ((os-n 0) (diffs nil))
    (dolist (u (codec-test-range 0 #xffff))
      (unless (<= #xd800 u #xdfff)
        (let ((os (codec-test-os-encode u))
              (me (and (si:codec-can-encode *encoding-sjis* u)
                       (codec-test-encode *encoding-sjis* (list u)))))
          (when os (incf os-n))
          (unless (equal os me)
            (push (list u os me) diffs)))))
    (list os-n (nreverse diffs)))
  => (9402 ((#xa2 nil (#x81 #x91))
            (#xa3 nil (#x81 #x92))
            (#xa5 nil (#x5c))
            (#xac nil (#x81 #xca))
            (#x2016 nil (#x81 #x61))
            (#x203e nil (#x7e))
            (#x2212 nil (#x81 #x7c))
            (#x301c nil (#x81 #x60)))))

(deftest codec-cp932-os-roundtrip-changes ()
  "398 codes do not come back from a round trip: the converter and the OS
change the same codes to the same codes."
  (let ((os-changed nil) (me-changed nil))
    (dolist (h (codec-test-cp932-leads))
      (dolist (tb (codec-test-range #x40 #xfc))
        (let ((u (codec-test-os-decode h tb)))
          (when u
            (let ((os (codec-test-os-encode (car u)))
                  (me (codec-test-encode
                       *encoding-sjis*
                       (codec-test-decode *encoding-sjis* (list h tb)))))
              (unless (equal os (list h tb))
                (push (list h tb os) os-changed))
              (unless (equal me (list h tb))
                (push (list h tb me) me-changed)))))))
    (list (length os-changed) (length me-changed)
          (equal os-changed me-changed)))
  => (398 398 t))

(deftest codec-cp932-os-encode-non-bmp ()
  "Characters outside the BMP cannot be written."
  (list (codec-test-encode *encoding-sjis* '(#x1f600 #x20bb7 #x10ffff))
        (si:codec-can-encode *encoding-sjis* #x1f600))
  => ((#x3f #x3f #x3f) nil))

(deftest codec-cp932-encode-only-forms ()
  "The JIS form of 8 characters and the 4 PUA characters of the unused bytes
are written as the code of the Windows form."
  (codec-test-encode *encoding-sjis*
                     '(#x301c #x2016 #x2212 #xa2 #xa3 #xac #xa5 #x203e
                       #xf8f0 #xf8f1 #xf8f2 #xf8f3))
  => (#x81 #x60 #x81 #x61 #x81 #x7c #x81 #x91 #x81 #x92 #x81 #xca #x5c #x7e
      #xa0 #xfd #xfe #xff))

(deftest codec-cp932-user-defined-and-special-bytes ()
  "F040..F9FC is U+E000..U+E757. 0x80 is U+0080. The OS agrees (see the
tests above). The number of the user defined codes is 1880."
  (let ((n 0))
    (dolist (h (codec-test-range #xf0 #xf9))
      (dolist (tb (codec-test-range #x40 #xfc))
        (let ((u (codec-test-decode *encoding-sjis* (list h tb))))
          (when (and (null (cdr u)) (<= #xe000 (car u) #xe757))
            (incf n)))))
    (list n
          (codec-test-decode *encoding-sjis* '(#xf0 #x40 #xf9 #xfc #x80))))
  => (1880 (#xe000 #xe757 #x80)))


;;;; invalid and undefined bytes are kept (design 5.3)

(deftest codec-cp932-invalid-bytes-are-raw ()
  (list
   ;; 0xa0 and 0xfd..0xff: no character
   (codec-test-decode *encoding-sjis* '(#xa0 #xfd #xfe #xff))
   ;; a code that is not defined: both bytes
   (codec-test-decode *encoding-sjis* '(#x85 #x40 #x41))
   (codec-test-decode *encoding-sjis* '(#x85 #xa0))
   ;; a lead byte and a trail byte out of the range: the lead byte alone,
   ;; the next byte is read again (so a line end stays a line end)
   (codec-test-decode *encoding-sjis* '(#x81 #x0a #x41))
   (codec-test-decode *encoding-sjis* '(#x81 #x20))
   (codec-test-decode *encoding-sjis* '(#x81 #x7f))
   ;; a lead byte at the end
   (codec-test-decode *encoding-sjis* '(#x41 #x81)))
  => ((#x1100a0 #x1100fd #x1100fe #x1100ff)
      (#x110085 #x40 #x41)
      (#x110085 #x1100a0)
      (#x110081 #x0a #x41)
      (#x110081 #x20)
      (#x110081 #x7f)
      (#x41 #x110081)))

(deftest codec-cp932-raw-bytes-are-written-back ()
  (list
   (codec-test-encode *encoding-sjis*
                      '(#x1100a0 #x110085 #x40 #x1100fd #x110081))
   (codec-test-encode *encoding-sjis*
                      (codec-test-decode *encoding-sjis*
                                         '(#x85 #x40 #x81 #x0a #xa0 #xfd #x81))))
  => ((#xa0 #x85 #x40 #xfd #x81)
      (#x85 #x40 #x81 #x0a #xa0 #xfd #x81)))

(deftest codec-cp932-all-bytes-roundtrip-except-duplicates ()
  "Without the duplicated codes, any byte sequence comes back unchanged:
everything of 0..255, every lead and trail pair, in a mixed order."
  (let ((bad nil))
    ;; every single byte, and every pair of bytes in the range of the codes
    (dolist (b (codec-test-range 0 255))
      (let ((r (codec-test-encode *encoding-sjis*
                                  (codec-test-decode *encoding-sjis* (list b)))))
        (unless (equal r (list b)) (push (list b r) bad))))
    (dolist (h (append (codec-test-cp932-leads) '(#x80 #xa0 #xfd #xff)))
      (dolist (tb (codec-test-range 0 255))
        (let ((r (codec-test-encode *encoding-sjis*
                                    (codec-test-decode *encoding-sjis*
                                                       (list h tb)))))
          ;; The codes that are not canonical (398) are changed. Pairs that
          ;; are defined and canonical, or that are not defined, must come
          ;; back.
          (unless (or (equal r (list h tb))
                      (let ((os (codec-test-os-decode h tb)))
                        (and os (not (equal (codec-test-os-encode (car os))
                                            (list h tb))))))
            (push (list h tb r) bad)))))
    (or (nreverse bad) :ok))
  => :ok)


;;;; duplicated codes of cp932 (design 5.4, judgement A)

(defun codec-test-duplicated-codes ()
  "The 398 codes that the OS writes as another code."
  (let ((l nil))
    (dolist (h (codec-test-cp932-leads))
      (dolist (tb (codec-test-range #x40 #xfc))
        (let ((u (codec-test-os-decode h tb)))
          (when (and u (not (equal (codec-test-os-encode (car u)) (list h tb))))
            (push (list h tb) l)))))
    (nreverse l)))

(deftest codec-cp932-duplicates-with-memory ()
  "With the memory, each of the 398 codes comes back as it was read.
Without it, it is the code that Windows writes."
  (let ((with-memory 0) (without-memory 0) (bad nil))
    (dolist (code (codec-test-duplicated-codes))
      (multiple-value-bind (points memory)
          (si:codec-decode *encoding-sjis* code nil)
        (let ((w (codec-test-encode *encoding-sjis* points memory))
              (wo (codec-test-encode *encoding-sjis* points))
              (os (codec-test-os-encode (aref points 0))))
          (if (equal w code) (incf with-memory) (push (list code :memory w) bad))
          (if (equal wo os) (incf without-memory) (push (list code :os wo os) bad)))))
    (list with-memory without-memory (nreverse bad)))
  => (398 398 nil))

(deftest codec-cp932-duplicates-three-groups ()
  "NEC selected IBM extension (ED/EE), IBM extension (FA..FC) and NEC row 13
come back by the memory when they are in separate files."
  (list
   ;; ED40 and the character that FA5C also has
   (let ((r (multiple-value-list (si:codec-decode *encoding-sjis* '(#xed #x40) nil))))
     (list (coerce (car r) 'list) (cadr r)))
   (codec-test-encode *encoding-sjis* '(#x7e8a))
   ;; FA4A (Roman numeral X) is written as 8754 (NEC row 13) without memory
   (codec-test-encode *encoding-sjis* '(#x2160))
   (multiple-value-bind (p m) (si:codec-decode *encoding-sjis* '(#xfa #x4a) nil)
     (codec-test-encode *encoding-sjis* p m))
   ;; 8790 (NEC row 13, approximately equal) is written as 81e0 (JIS X 0208)
   (multiple-value-bind (p m) (si:codec-decode *encoding-sjis* '(#x87 #x90) nil)
     (list (codec-test-encode *encoding-sjis* p)
           (codec-test-encode *encoding-sjis* p m))))
  => ((( #x7e8a) ((#x7e8a . #xed40)))
      (#xfa #x5c)
      (#x87 #x54)
      (#xfa #x4a)
      ((#x81 #xe0) (#x87 #x90))))

(deftest codec-cp932-duplicates-first-one-stays ()
  "A character that is written with two codes in a file: the first one is
remembered and the character is written with it (the limit of the memory)."
  (multiple-value-bind (points memory)
      (si:codec-decode *encoding-sjis* '(#xed #x40 #xfa #x5c #xed #x40) nil)
    (list (coerce points 'list)
          memory
          (codec-test-encode *encoding-sjis* points memory)
          (codec-test-encode *encoding-sjis* points)))
  => ((#x7e8a #x7e8a #x7e8a)
      ((#x7e8a . #xed40))
      (#xed #x40 #xed #x40 #xed #x40)
      (#xfa #x5c #xfa #x5c #xfa #x5c)))

(deftest codec-cp932-duplicates-memory-carries-over ()
  "The memory is given to the next decode: the buffer keeps it."
  (multiple-value-bind (p1 m1) (si:codec-decode *encoding-sjis* '(#xed #x40) nil)
    (multiple-value-bind (p2 m2) (si:codec-decode *encoding-sjis* '(#x87 #x90 #xfa #x4a) m1)
      (list (coerce p1 'list) (coerce p2 'list)
            (sort (copy-list m2) #'< :key #'car)
            (codec-test-encode *encoding-sjis*
                               (list #x7e8a #x2252 #x2160) m2))))
  => ((#x7e8a)
      (#x2252 #x2160)
      ((#x2160 . #xfa4a) (#x2252 . #x8790) (#x7e8a . #xed40))
      (#xed #x40 #x87 #x90 #xfa #x4a)))

(deftest codec-cp932-memory-is-for-cp932-only ()
  "The memory has no effect on the other encodings."
  (multiple-value-bind (p m) (si:codec-decode *encoding-euc-jp* '(#xa4 #xa2) nil)
    (list (coerce p 'list) m))
  => ((#x3042) nil))


;;;; invalid bytes of the other encodings

(deftest codec-utf8-valid-and-invalid ()
  (list
   (codec-test-decode *encoding-utf8n* '(#x41 #xc3 #xa9 #xe3 #x81 #x82 #xf0 #x9f #x98 #x80))
   ;; a stray continuation byte, 0xc0 0xc1 (overlong), 0xf5.. (too large)
   (codec-test-decode *encoding-utf8n* '(#x80 #xbf #xc0 #x80 #xc1 #xbf #xf5 #xff #x41))
   ;; overlong 3 bytes, a surrogate, over U+10FFFF
   (codec-test-decode *encoding-utf8n* '(#xe0 #x80 #x80 #xed #xa0 #x80 #xf4 #x90 #x80 #x80))
   ;; cut at the end
   (codec-test-decode *encoding-utf8n* '(#x41 #xe3 #x81))
   (codec-test-decode *encoding-utf8n* '(#x41 #xe3 #x41))
   (codec-test-decode *encoding-utf8n* '(#x41 #xf0)))
  => ((#x41 #xe9 #x3042 #x1f600)
      (#x110080 #x1100bf #x1100c0 #x110080 #x1100c1 #x1100bf #x1100f5 #x1100ff #x41)
      (#x1100e0 #x110080 #x110080 #x1100ed #x1100a0 #x110080
       #x1100f4 #x110090 #x110080 #x110080)
      (#x41 #x1100e3 #x110081)
      (#x41 #x1100e3 #x41)
      (#x41 #x1100f0)))

(deftest codec-utf8-signature ()
  "A signature of UTF-8 is dropped (only the first character). The encoder
writes it."
  (list (codec-test-decode *encoding-utf8* '(#xef #xbb #xbf #x41 #xef #xbb #xbf))
        (codec-test-decode *encoding-utf8n* '(#xef #xbb #xbf #x41))
        (codec-test-encode *encoding-utf8* '(#x41))
        (codec-test-encode *encoding-utf8* nil)
        (codec-test-encode *encoding-utf8n* '(#x41)))
  => ((#x41 #xfeff)
      (#xfeff #x41)
      (#xef #xbb #xbf #x41)
      nil
      (#x41)))

(deftest codec-utf8-lone-surrogate-and-large-values-are-not-written ()
  (list (codec-test-encode *encoding-utf8n* '(#xd800 #x110000 #x41))
        (si:codec-can-encode *encoding-utf8n* #xd800)
        (si:codec-can-encode *encoding-utf8n* #x10ffff))
  => ((#x3f #x3f #x41) nil t))

(deftest codec-utf16-byte-order-and-surrogates ()
  (list
   ;; utf16: the byte order is detected by the signature
   (codec-test-decode *encoding-utf16* '(#xff #xfe #x41 #x00 #x3d #xd8 #x00 #xde))
   (codec-test-decode *encoding-utf16* '(#xfe #xff #x00 #x41 #xd8 #x3d #xde #x00))
   ;; the byte order of the encoding
   (codec-test-decode *encoding-utf16le* '(#x41 #x00 #x42 #x00))
   (codec-test-decode *encoding-utf16be* '(#x00 #x41 #x00 #x42))
   ;; a lone surrogate is the character; an odd byte at the end is raw
   (codec-test-decode *encoding-utf16le* '(#x3d #xd8 #x41 #x00 #x00 #xde #x5a))
   ;; encoder: the signature, then the surrogate pair
   (codec-test-encode *encoding-utf16le* '(#x41 #x1f600))
   (codec-test-encode *encoding-utf16be-bom* '(#x41))
   (codec-test-encode *encoding-utf16le-bom* nil))
  => ((#x41 #x1f600)
      (#x41 #x1f600)
      (#x41 #x42)
      (#x41 #x42)
      (#xd83d #x41 #xde00 #x5a)
      (#x41 #x00 #x3d #xd8 #x00 #xde)
      (#xfe #xff #x00 #x41)
      nil))



;;;; round trips and known bytes of every encoding

(defvar *codec-test-roundtrip-samples*
  ;; (encoding-variable code-points ...) The characters are those that the
  ;; encoding has. decode (encode x) = x.
  (let* ((jp '(#x41 #x20 #x3042 #x6f22 #x5b57 #xff76 #xff9e #x2252 #x2160
               #x7e8a #x3000 #x30fb #x2d8))
         (jp-no-0212 (remove #x2d8 jp))
         (kr '(#x41 #xac00 #xd55c #xae00 #x3042 #x4e2d #x20))
         (gb '(#x41 #x4e2d #x6587 #x3042 #x20))
         (big5 '(#x41 #x4e2d #x6587 #x3002 #x20))
         (cn '(#x41 #x4e2d #x6587 #x4e5c #x20))
         (utf '(#x41 #x20 #xe9 #x3042 #x6f22 #xff76 #xd7ff #xe000 #xfffd
                #x10000 #x1f600 #x10ffff #x2b #x2d #x26 #x7e))
         (l1 '(#x41 #xe9 #xfc #xa0 #xff))
         (all nil))
    (dolist (e `((*encoding-sjis* ,jp-no-0212)
                 (*encoding-euc-jp* ,jp) (*encoding-euc-jp-open* ,jp)
                 (*encoding-euc-jp-necext* ,jp) (*encoding-euc-jp-ibmext* ,jp)
                 (*encoding-jis* ,jp) (*encoding-jis-open* ,jp)
                 (*encoding-jis-necext* ,jp) (*encoding-jis-ibmext* ,jp)
                 (*encoding-iso-2022-jp-2* ,(append jp '(#xe9 #x3b1)))
                 (*encoding-iso-8bit-jp* ,(append jp '(#xe9 #x3b1)))
                 (*encoding-iso-8bit-jp-open* ,jp)
                 (*encoding-jis-sio* ,jp) (*encoding-jis8* ,jp)
                 (*encoding-euc-kr* ,kr) (*encoding-iso-2022-kr* ,kr)
                 (*encoding-iso-8bit-kr* ,kr)
                 (*encoding-euc-gb* ,gb) (*encoding-big5* ,big5)
                 (*encoding-iso-2022-cn* ,cn) (*encoding-iso-8bit-cn* ,cn)
                 (*encoding-iso-8859-1* ,l1)
                 (*encoding-iso-8859-2* (#x41 #x105 #x142 #xa0))
                 (*encoding-iso-8859-3* (#x41 #x126 #x127 #xa0))
                 (*encoding-iso-8859-4* (#x41 #x104 #x138 #xa0))
                 (*encoding-iso-8859-5* (#x41 #x410 #x44f #x2116 #xa0))
                 (*encoding-iso-8859-7* (#x41 #x391 #x3c9 #xa0))
                 (*encoding-iso-8859-9* (#x41 #x11e #x15f #xa0))
                 (*encoding-iso-8859-10* (#x41 #x104 #x138 #xa0))
                 (*encoding-iso-8859-13* (#x41 #x104 #x141 #xa0))
                 (*encoding-windows-latin1* (#x41 #x20ac #xe9 #x201c #x2122))
                 (*encoding-windows-latin2* (#x41 #x20ac #x141 #x105))
                 (*encoding-windows-cyrillic* (#x41 #x402 #x44f #x20ac))
                 (*encoding-windows-greek* (#x41 #x391 #x3c9 #x20ac))
                 (*encoding-windows-turkish* (#x41 #x11e #x15f #x20ac))
                 (*encoding-windows-baltic* (#x41 #x104 #x20ac #x201e))
                 (*encoding-koi8r* (#x41 #x410 #x44f #x2500))
                 (*encoding-koi8u* (#x41 #x404 #x454 #x491 #x2500))
                 (*encoding-binary* ,(codec-test-range 0 255))
                 (*encoding-utf8* ,utf) (*encoding-utf8n* ,utf)
                 (*encoding-utf16* ,utf) (*encoding-utf16le* ,utf)
                 (*encoding-utf16be* ,utf) (*encoding-utf16le-bom* ,utf)
                 (*encoding-utf16be-bom* ,utf)
                 (*encoding-utf7* ,utf) (*encoding-imap4* ,utf)
                 (*encoding-utf5* ,utf)
                 (*encoding-std-utf8n* ,(remove #xff3c utf))
                 (*encoding-std-utf16le* ,utf)
                 (*encoding-std-utf7* ,utf) (*encoding-std-utf5* ,utf)
                 (*encoding-utf8n-kr* ,utf) (*encoding-utf16-gb* ,utf)
                 (*encoding-utf7-big5* ,utf) (*encoding-utf5-cn* ,utf)))
      (push e all))
    (nreverse all)))

(deftest codec-roundtrip-every-encoding ()
  "decode (encode points) = points. The failed ones are listed."
  (let ((bad nil))
    (dolist (e *codec-test-roundtrip-samples*)
      (let* ((enc (symbol-value (car e)))
             (points (cadr e))
             (bytes (codec-test-encode enc points))
             (back (codec-test-decode enc bytes)))
        (unless (equal back points)
          (push (list (car e) points bytes back) bad))))
    (or (nreverse bad) :ok))
  => :ok)

(deftest codec-roundtrip-bytes-of-single-byte-sets ()
  "For the 1 byte sets, encode (decode bytes) = bytes for all 256 bytes: the
bytes that the set does not define are raw bytes."
  (let ((bad nil))
    (dolist (e '(*encoding-iso-8859-1* *encoding-iso-8859-2* *encoding-iso-8859-3*
                 *encoding-iso-8859-4* *encoding-iso-8859-5* *encoding-iso-8859-7*
                 *encoding-iso-8859-9* *encoding-iso-8859-10* *encoding-iso-8859-13*
                 *encoding-windows-latin1* *encoding-windows-latin2*
                 *encoding-windows-cyrillic* *encoding-windows-greek*
                 *encoding-windows-turkish* *encoding-windows-baltic*
                 *encoding-koi8r* *encoding-koi8u* *encoding-binary*
                 *encoding-big5* *encoding-euc-kr* *encoding-euc-gb*
                 *encoding-euc-jp* *encoding-utf8n* *encoding-utf16le*))
      (let* ((enc (symbol-value e))
             (bytes (codec-test-range 0 255))
             (back (codec-test-encode enc (codec-test-decode enc bytes))))
        (unless (equal back bytes)
          (push (list e back) bad))))
    (or (nreverse bad) :ok))
  => :ok)

(deftest codec-known-bytes ()
  "Bytes of known characters."
  (list
   ;; U+3042 (hiragana A), U+6F22 (kanji KAN), U+FF76 (half width KA)
   (codec-test-encode *encoding-sjis* '(#x3042 #x6f22 #xff76))
   (codec-test-encode *encoding-euc-jp* '(#x3042 #x6f22 #xff76))
   (codec-test-encode *encoding-jis* '(#x41 #x3042 #x6f22 #x42))
   (codec-test-encode *encoding-iso-2022-jp-2* '(#x3042 #xe9 #x41))
   (codec-test-encode *encoding-jis-sio* '(#xff76 #x41))
   (codec-test-encode *encoding-euc-kr* '(#xac00 #xd55c))
   (codec-test-encode *encoding-iso-2022-kr* '(#xac00 #x41))
   (codec-test-encode *encoding-euc-gb* '(#x4e2d #x6587))
   (codec-test-encode *encoding-big5* '(#x4e2d #x6587))
   (codec-test-encode *encoding-iso-2022-cn* '(#x4e2d #x41))
   (codec-test-encode *encoding-iso-8859-1* '(#xe9))
   (codec-test-encode *encoding-windows-latin1* '(#x20ac))
   (codec-test-encode *encoding-koi8r* '(#x430)))
  => ((#x82 #xa0 #x8a #xbf #xb6)
      (#xa4 #xa2 #xb4 #xc1 #x8e #xb6)
      (#x41 #x1b #x24 #x42 #x24 #x22 #x34 #x41 #x1b #x28 #x42 #x42)
      (#x1b #x24 #x42 #x24 #x22 #x1b #x2e #x41 #x1b #x4e #x69 #x1b #x28 #x42 #x41)
      (#x0e #x36 #x0f #x41)
      (#xb0 #xa1 #xc7 #xd1)
      (#x1b #x24 #x29 #x43 #x0e #x30 #x21 #x0f #x41)
      (#xd6 #xd0 #xce #xc4)
      (#xa4 #xa4 #xa4 #xe5)
      (#x1b #x24 #x29 #x41 #x0e #x56 #x50 #x0f #x41)
      (#xe9)
      (#x80)
      (#xc1)))


;;;; the character sets: every defined code decodes to a character and the
;;;; character is written and read back (judged by the counts of the standards)

(defun codec-test-pair-units (h1 h2 t1 t2 &optional prefix suffix)
  (let ((l nil))
    (dolist (h (codec-test-range h1 h2))
      (dolist (tb (codec-test-range t1 t2))
        (push (append prefix (list h tb) suffix) l)))
    (nreverse l)))

(defun codec-test-set-check (enc units)
  "For every unit (a list of bytes) that is one character: write the
character and read it again. Returns (number-of-characters
number-of-failed-round-trips number-of-units-written-as-other-bytes)."
  (let ((n 0) (bad 0) (changed 0))
    (dolist (u units)
      (let ((d (codec-test-decode enc u)))
        (when (and d (null (cdr d)) (< (car d) #x110000))
          (incf n)
          (let* ((b (codec-test-encode enc d))
                 (back (codec-test-decode enc b)))
            (unless (equal back d) (incf bad))
            (unless (equal b u) (incf changed))))))
    (list n bad changed)))

(deftest codec-table-euc-kr ()
  "KS C 5601: 2350 Hangul, 4888 Hanja, 986 symbols."
  (codec-test-set-check *encoding-euc-kr* (codec-test-pair-units #xa1 #xfe #xa1 #xfe))
  => (8224 0 0))

(deftest codec-table-euc-gb ()
  "GB 2312: 6763 Hanzi, 682 symbols."
  (codec-test-set-check *encoding-euc-gb* (codec-test-pair-units #xa1 #xfe #xa1 #xfe))
  => (7445 0 0))

(deftest codec-table-euc-jp ()
  "JIS X 0208 (6879) with NEC row 13 (83) and the NEC selected IBM extension
(374)."
  (codec-test-set-check *encoding-euc-jp* (codec-test-pair-units #xa1 #xfe #xa1 #xfe))
  => (7336 0 10))

(deftest codec-table-big5 ()
  "5 of the codes are the duplicates that are read but not written."
  (codec-test-set-check *encoding-big5* (codec-test-pair-units #xa1 #xf9 #x40 #xfe))
  => (13710 0 5))

(deftest codec-table-euc-jp-kana-and-jisx0212 ()
  "Half width kana by SS2 (63) and JIS X 0212 by SS3 (6067 and the 106 of
the IBM extension in rows 83 and 84)."
  (list (codec-test-set-check *encoding-euc-jp*
                              (mapcar #'(lambda (b) (list #x8e b))
                                      (codec-test-range #xa1 #xdf)))
        (codec-test-set-check *encoding-euc-jp*
                              (codec-test-pair-units #xa1 #xfe #xa1 #xfe '(#x8f))))
  => ((63 0 0) (6173 0 387)))

(deftest codec-table-euc-jp-vender ()
  "The vender variants of EUC-JP: OSF/JVC (rows 85 and over are the user
defined area), IBM extension, NEC extension."
  (list (codec-test-set-check *encoding-euc-jp-open*
                              (codec-test-pair-units #xa1 #xfe #xa1 #xfe))
        (codec-test-set-check *encoding-euc-jp-open*
                              (codec-test-pair-units #xa1 #xfe #xa1 #xfe '(#x8f)))
        (codec-test-set-check *encoding-euc-jp-ibmext*
                              (codec-test-pair-units #xa1 #xfe #xa1 #xfe))
        (codec-test-set-check *encoding-euc-jp-ibmext*
                              (codec-test-pair-units #xa1 #xfe #xa1 #xfe '(#x8f)))
        (codec-test-set-check *encoding-euc-jp-necext*
                              (codec-test-pair-units #xa1 #xfe #xa1 #xfe)))
  => ((7902 0 9) (7113 0 15) (7336 0 382) (6173 0 15) (7336 0 10)))

(deftest codec-table-iso-2022 ()
  "The sets of ISO-2022-JP (JIS X 0208, JIS X 0212, half width kana), the
Chinese and Korean sets with the escape sequences."
  (list (codec-test-set-check *encoding-jis*
                              (codec-test-pair-units #x21 #x7e #x21 #x7e
                                                     '(#x1b #x24 #x42) '(#x1b #x28 #x42)))
        (codec-test-set-check *encoding-jis*
                              (codec-test-pair-units #x21 #x7e #x21 #x7e
                                                     '(#x1b #x24 #x28 #x44) '(#x1b #x28 #x42)))
        (codec-test-set-check *encoding-jis*
                              (mapcar #'(lambda (b) (list #x1b #x28 #x49 b #x1b #x28 #x42))
                                      (codec-test-range #x21 #x5f)))
        (codec-test-set-check *encoding-iso-2022-kr*
                              (codec-test-pair-units #x21 #x7e #x21 #x7e
                                                     '(#x1b #x24 #x29 #x43 #x0e) '(#x0f)))
        (codec-test-set-check *encoding-iso-2022-cn*
                              (codec-test-pair-units #x21 #x7e #x21 #x7e
                                                     '(#x1b #x24 #x29 #x41 #x0e) '(#x0f)))
        (codec-test-set-check *encoding-iso-2022-cn*
                              (codec-test-pair-units #x21 #x7e #x21 #x7e
                                                     '(#x1b #x24 #x29 #x47 #x0e) '(#x0f)))
        (codec-test-set-check *encoding-iso-2022-cn*
                              (codec-test-pair-units #x21 #x7e #x21 #x7e
                                                     '(#x1b #x24 #x2a #x48 #x1b #x4e))))
  => ((7336 0 10) (6173 0 620) (63 0 0) (8224 0 0) (7445 0 0)
      (5864 0 3605) (7650 0 1055)))


;;;; the tables agree with the converters of encoding.cc

;;; encoding.cc reads the bytes into the internal code and char-unicode gives
;;; the Unicode of it. For every code that both of them read as one
;;; character, the character is the same. (The new converters read more
;;; codes as characters, and keep the codes that the old ones turn into some
;;; other character as raw bytes: see BUG-0053.)

(defun codec-test-old-decode (enc bytes)
  (map 'list #'char-unicode
       (convert-encoding-to-internal enc (map 'string #'code-char bytes))))

(defun codec-test-old-disagreements (enc units)
  "The units that the old and the new converters both read as one character
and that are different characters."
  (let ((r nil))
    (dolist (u units)
      (let ((m (codec-test-decode enc u))
            (o (codec-test-old-decode enc u)))
        (when (and m (null (cdr m)) (< (car m) #x110000)
                   o (null (cdr o)) (car o)
                   (not (equal m o)))
          (push (list u o m) r))))
    (nreverse r)))

(deftest codec-old-agree-cp932 ()
  (codec-test-old-disagreements
   *encoding-sjis*
   (append (mapcar #'list (codec-test-range 0 255))
           (codec-test-pair-units #x81 #x9f #x40 #xfc)
           (codec-test-pair-units #xe0 #xfc #x40 #xfc)))
  ;; (The old one reads 0xa0, 0xfd..0xff as U+F8F0..U+F8F3; they are raw
  ;; bytes in the new one and not compared.)
  => nil)

(deftest codec-old-agree-euc-kr-gb-big5 ()
  (list
   (codec-test-old-disagreements *encoding-euc-kr*
                                 (codec-test-pair-units #xa1 #xfe #xa1 #xfe))
   (codec-test-old-disagreements *encoding-euc-gb*
                                 (codec-test-pair-units #xa1 #xfe #xa1 #xfe))
   (codec-test-old-disagreements *encoding-big5*
                                 (codec-test-pair-units #xa1 #xf9 #x40 #xfe)))
  => (nil nil nil))

(deftest codec-old-agree-euc-jp ()
  "All the vender variants, the kana and JIS X 0212."
  (let ((r nil))
    (dolist (e '(*encoding-euc-jp* *encoding-euc-jp-open*
                 *encoding-euc-jp-necext* *encoding-euc-jp-ibmext*))
      (let ((enc (symbol-value e)))
        (push (list e
                    (codec-test-old-disagreements
                     enc (codec-test-pair-units #xa1 #xfe #xa1 #xfe))
                    (codec-test-old-disagreements
                     enc (codec-test-pair-units #xa1 #xfe #xa1 #xfe '(#x8f)))
                    (codec-test-old-disagreements
                     enc (mapcar #'(lambda (b) (list #x8e b))
                                 (codec-test-range #xa1 #xdf))))
              r)))
    (nreverse r))
  => ((*encoding-euc-jp* nil nil nil)
      (*encoding-euc-jp-open* nil nil nil)
      (*encoding-euc-jp-necext* nil nil nil)
      (*encoding-euc-jp-ibmext* nil nil nil)))

(deftest codec-old-agree-iso-2022 ()
  (let ((jis (codec-test-pair-units #x21 #x7e #x21 #x7e
                                    '(#x1b #x24 #x42) '(#x1b #x28 #x42))))
    (list
     (codec-test-old-disagreements *encoding-jis* jis)
     (codec-test-old-disagreements *encoding-jis-open* jis)
     (codec-test-old-disagreements *encoding-jis-necext* jis)
     (codec-test-old-disagreements *encoding-jis-ibmext* jis)
     (codec-test-old-disagreements
      *encoding-iso-8bit-jp*
      (codec-test-pair-units #x21 #x7e #x21 #x7e '(#x1b #x24 #x28 #x44) '(#x1b #x28 #x42)))
     (codec-test-old-disagreements
      *encoding-iso-8bit-jp*
      (codec-test-pair-units #x21 #x7e #x21 #x7e '(#x1b #x24 #x41) '(#x1b #x28 #x42)))
     (codec-test-old-disagreements
      *encoding-iso-8bit-jp*
      (codec-test-pair-units #x21 #x7e #x21 #x7e '(#x1b #x24 #x28 #x43) '(#x1b #x28 #x42)))
     (codec-test-old-disagreements
      *encoding-iso-2022-kr*
      (codec-test-pair-units #x21 #x7e #x21 #x7e '(#x1b #x24 #x29 #x43 #x0e) '(#x0f)))
     (codec-test-old-disagreements
      *encoding-iso-2022-cn*
      (codec-test-pair-units #x21 #x7e #x21 #x7e '(#x1b #x24 #x29 #x41 #x0e) '(#x0f)))))
  => (nil nil nil nil nil nil nil nil nil))

(deftest codec-old-agree-single-byte-sets ()
  "ISO 8859 from 0xa0 (0x80-0x9f are the C1 controls here and a character
that has no Unicode in encoding.cc), the Windows code pages and KOI8."
  (let ((r nil))
    (dolist (e '(*encoding-iso-8859-1* *encoding-iso-8859-2* *encoding-iso-8859-3*
                 *encoding-iso-8859-4* *encoding-iso-8859-5* *encoding-iso-8859-7*
                 *encoding-iso-8859-9* *encoding-iso-8859-10* *encoding-iso-8859-13*))
      (push (list e (codec-test-old-disagreements
                     (symbol-value e)
                     (mapcar #'list (codec-test-range #xa0 #xff))))
            r))
    (dolist (e '(*encoding-windows-latin1* *encoding-windows-latin2*
                 *encoding-windows-cyrillic* *encoding-windows-greek*
                 *encoding-windows-turkish* *encoding-windows-baltic*
                 *encoding-koi8r* *encoding-koi8u*))
      (push (list e (codec-test-old-disagreements
                     (symbol-value e)
                     (mapcar #'list (codec-test-range 0 255))))
            r))
    (remove-if #'(lambda (x) (null (cadr x))) (nreverse r)))
  => nil)


;;;; UTF-7, IMAP4 mailbox names, UTF-5 (the examples of RFC 2152 and RFC 3501)

(defun codec-test-codes (string)
  (map 'list #'char-code string))

(deftest codec-utf7-rfc2152 ()
  (list
   (codec-test-decode *encoding-utf7* (codec-test-codes "Hi Mom -+Jjo--!"))
   (codec-test-encode *encoding-utf7*
                      '(#x48 #x69 #x20 #x4d #x6f #x6d #x20 #x2d #x263a #x2d #x21))
   (codec-test-decode *encoding-utf7* (codec-test-codes "+ZeVnLIqe-"))
   (codec-test-encode *encoding-utf7* '(#x65e5 #x672c #x8a9e))
   (codec-test-decode *encoding-utf7* (codec-test-codes "A+ImIDkQ."))
   ;; a surrogate pair in a shift, and "+-" is "+"
   (codec-test-decode *encoding-utf7* (codec-test-codes "A+-B+2D3eAA-"))
   (codec-test-encode *encoding-utf7* '(#x41 #x2b #x42 #x1f600)))
  => ((#x48 #x69 #x20 #x4d #x6f #x6d #x20 #x2d #x263a #x2d #x21)
      (#x48 #x69 #x20 #x4d #x6f #x6d #x20 #x2d #x2b #x4a #x6a #x6f #x2d #x2d #x21)
      (#x65e5 #x672c #x8a9e)
      (#x2b #x5a #x65 #x56 #x6e #x4c #x49 #x71 #x65 #x2d)
      (#x41 #x2262 #x391 #x2e)
      (#x41 #x2b #x42 #x1f600)
      (#x41 #x2b #x2d #x42 #x2b #x32 #x44 #x33 #x65 #x41 #x41 #x2d)))

(deftest codec-imap4-rfc3501 ()
  "\"~peter/mail/&U,BTFw-/&ZeVnLIqe-\" is \"~peter/mail/<Taipei>/<Nihongo>\"."
  (list
   (codec-test-decode *encoding-imap4*
                      (codec-test-codes "~peter/mail/&U,BTFw-/&ZeVnLIqe-&-"))
   (codec-test-encode *encoding-imap4* '(#x7e #x70 #x2f #x53f0 #x5317 #x2f #x26)))
  => ((#x7e #x70 #x65 #x74 #x65 #x72 #x2f #x6d #x61 #x69 #x6c #x2f
       #x53f0 #x5317 #x2f #x65e5 #x672c #x8a9e #x26)
      (#x7e #x70 #x2f #x26 #x55 #x2c #x42 #x54 #x46 #x77 #x2d #x2f #x26 #x2d)))

(deftest codec-utf5 ()
  "The first digit of a character is G..V, the others are 0..F."
  (list
   (codec-test-encode *encoding-utf5* '(#x3042 #x41 #x1f600 0))
   (codec-test-decode *encoding-utf5* (codec-test-codes "J042K1HF600G")))
  => ((#x4a #x30 #x34 #x32 #x4b #x31 #x48 #x46 #x36 #x30 #x30 #x47)
      (#x3042 #x41 #x1f600 0)))

(deftest codec-std-utf-writes-the-jis-form ()
  "The std- encodings write the 6 characters that Windows and JIS see
differently (U+FF5E and so on) in the JIS form (U+301C and so on). The others
write what they are given. Reading converts nothing. (The old converter also
wrote the ASCII backslash and tilde as the yen sign and the overline: see
BUG-0051. They are not changed here.)"
  (list
   (codec-test-encode *encoding-std-utf8n*
                      '(#xff5e #x2225 #xff0d #xffe0 #xffe1 #xffe2 #x41 #x5c #x7e))
   (codec-test-encode *encoding-utf8n* '(#xff5e #x2225 #x5c))
   (codec-test-decode *encoding-std-utf8n* '(#xe3 #x80 #x9c))
   (codec-test-encode *encoding-std-utf16le* '(#xff5e))
   (codec-test-decode *encoding-std-utf16le* '(#x1c #x30)))
  => ((#xe3 #x80 #x9c #xe2 #x80 #x96 #xe2 #x88 #x92 #xc2 #xa2 #xc2 #xa3 #xc2 #xac
       #x41 #x5c #x7e)
      (#xef #xbd #x9e #xe2 #x88 #xa5 #x5c)
      (#x301c)
      (#x1c #x30)
      (#x301c)))


;;;; the escape sequences and the shifts of ISO 2022 and EUC

(deftest codec-iso-2022-decode-details ()
  (list
   (codec-test-decode *encoding-jis*
                      '(#x1b #x24 #x42 #x24 #x22 #x1b #x28 #x42 #x41))
   ;; ESC that is not a sequence is a character, also at the end
   (codec-test-decode *encoding-jis* '(#x41 #x1b #x78 #x42))
   (codec-test-decode *encoding-jis* '(#x41 #x1b))
   (codec-test-decode *encoding-jis* '(#x1b #x24))
   ;; the first byte of a pair at the end
   (codec-test-decode *encoding-jis* '(#x1b #x24 #x42 #x24))
   ;; SO and SI (ISO-2022-KR)
   (codec-test-decode *encoding-iso-2022-kr*
                      '(#x1b #x24 #x29 #x43 #x0e #x30 #x21 #x0f #x41))
   ;; ESC . A designates ISO 8859-1 to G2, ESC N is the single shift
   (codec-test-decode *encoding-iso-2022-jp-2* '(#x1b #x2e #x41 #x1b #x4e #x69))
   ;; EUC: SS2 (kana), SS3 (JIS X 0212), and the bytes that are not
   (codec-test-decode *encoding-euc-jp* '(#x8e #xb6 #x8f #xa2 #xc3))
   (codec-test-decode *encoding-euc-jp* '(#x8e))
   (codec-test-decode *encoding-euc-jp* '(#xa4 #x41))
   (codec-test-decode *encoding-euc-jp* '(#xa4))
   (codec-test-decode *encoding-euc-jp* '(#x8e #x8f))
   ;; 8 bit forms
   (codec-test-decode *encoding-jis8* '(#xb6 #x41))
   (codec-test-decode *encoding-iso-8bit-jp* '(#xa4 #xa2 #x41)))
  => ((#x3042 #x41)
      (#x41 #x1b #x78 #x42)
      (#x41 #x1b)
      (#x1b #x24)
      (#x24)
      (#xac00 #x41)
      (#xe9)
      (#xff76 #xa6)
      (#x11008e)
      (#x1100a4 #x41)
      (#x1100a4)
      (#x11008e #x11008f)
      (#xff76 #x41)
      (#x3042 #x41)))

(deftest codec-iso-2022-encode-details ()
  (list
   ;; ASCII is restored at the end of a line and at the end of the text
   (codec-test-encode *encoding-jis* '(#x3042 #x0a #x3042))
   (codec-test-encode *encoding-jis* '(#x41 #x3042))
   ;; characters that the encoding does not have are '?'
   (codec-test-encode *encoding-euc-jp* '(#xac00 #x41))
   (codec-test-encode *encoding-iso-8859-1* '(#x3042))
   (codec-test-encode *encoding-sjis* '(#x20ac #x3042))
   (codec-test-encode *encoding-euc-kr* '(#x80 #x20ac))
   ;; JIS X 0212 by SS3 in EUC-JP, the character of ISO 8859-1 in ISO-2022-JP-2
   (codec-test-encode *encoding-euc-jp* '(#xa6))
   (si:codec-can-encode *encoding-euc-jp* #xe9)
   (si:codec-can-encode *encoding-euc-jp* #xac00)
   (si:codec-can-encode *encoding-sjis* #x3042)
   (si:codec-can-encode *encoding-sjis* #x20ac)
   (si:codec-can-encode *encoding-utf8* #x10ffff)
   (si:codec-can-encode *encoding-iso-8859-1* #xff))
  => ((#x1b #x24 #x42 #x24 #x22 #x1b #x28 #x42 #x0a
       #x1b #x24 #x42 #x24 #x22 #x1b #x28 #x42)
      (#x41 #x1b #x24 #x42 #x24 #x22 #x1b #x28 #x42)
      (#x3f #x41)
      (#x3f)
      (#x3f #x82 #xa0)
      (#x3f #x3f)
      (#x8f #xa2 #xc3)
      t nil t nil t t))

(deftest codec-raw-bytes-are-written-back-by-every-encoding ()
  (let ((bad nil))
    (dolist (e *codec-test-roundtrip-samples*)
      (let ((r (codec-test-encode (symbol-value (car e))
                                  '(#x41 #x110080 #x1100ff #x42))))
        ;; The encoder writes the byte itself. The encodings that write the
        ;; characters with 2 bytes (UTF-16) or with a shift (UTF-7) write the
        ;; other characters in their own way: only the byte is checked.
        (unless (and (member #x80 r) (member #xff r))
          (push (list (car e) r) bad))))
    (or (nreverse bad) :ok))
  => :ok)


;;;; the converters keep their state across the pieces of the input

(defun codec-test-pseudo-random-bytes (n seed)
  (let ((l nil) (x seed))
    (dotimes (i n)
      (setq x (logand (+ (* x 1103515245) 12345) #x7fffffff))
      (push (logand (ash x -16) 255) l))
    (nreverse l)))

(deftest codec-decode-in-pieces-is-the-same ()
  "The input in pieces of 1, 2, 3 and 7 bytes gives the same characters as
the whole input, also for bytes that are not text (a sequence that is cut at
the end of a piece must wait for the rest)."
  (let ((bad nil))
    (dolist (e *codec-test-roundtrip-samples*)
      (let ((enc (symbol-value (car e))))
        (dolist (bytes (list (codec-test-encode enc (cadr e))
                             (codec-test-range 0 255)
                             (codec-test-pseudo-random-bytes 700 (+ 7 (length bad)))))
          (let ((whole (coerce (si:codec-decode enc bytes t) 'list)))
            (dolist (n '(1 2 3 7))
              (let ((r (coerce (si:codec-decode enc bytes t n) 'list)))
                (unless (equal r whole)
                  (push (list (car e) n) bad))))))))
    (or (nreverse bad) :ok))
  => :ok)

(deftest codec-encode-in-pieces-is-the-same ()
  (let ((bad nil))
    (dolist (e *codec-test-roundtrip-samples*)
      (let* ((enc (symbol-value (car e)))
             (points (append (cadr e) (cadr e) '(#x3f #x41)))
             (whole (coerce (si:codec-encode enc points t) 'list)))
        (dolist (n '(1 2 3 7))
          (let ((r (coerce (si:codec-encode enc points t n) 'list)))
            (unless (equal r whole)
              (push (list (car e) n) bad))))))
    (or (nreverse bad) :ok))
  => :ok)
