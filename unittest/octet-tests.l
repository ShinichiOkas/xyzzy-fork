;;; -*- mode: lisp; package: user -*-
;;;
;;; Tests of the octet functions and of the converter of the ACP (S4 stage 3,
;;; R7 and R8).
;;;
;;; - R7: the functions that handle octets (si:base64-encode, si:md5,
;;;   si:hmac-*, si:octet-length, ...) take :encoding, which tells how the
;;;   characters of the input become octets.  The rule of "a string of bytes"
;;;   (design 4.7) is the side of XYZZY_CHAR_UNICODE 1; its converter is
;;;   called directly by si:octet-rule-encode, so that it is tested before the
;;;   switch is turned on.
;;; - R8: s2w and w2s go through the converter of the ACP.  The UTF-8 side is
;;;   called directly by si:acp-utf8-decode, si:acp-utf8-encode and
;;;   si:chars-to-utf16 (the UTF-16 of the IME reconversion).
;;;
;;; The characters are asked for by their Unicode (unicode-char) and the
;;; bytes are written with numbers, so that this file stays ASCII and passes
;;; the same way whatever the value of XYZZY_CHAR_UNICODE is.

(in-package :user)

(defun octet-test-string (&rest codes)
  (coerce (mapcar #'unicode-char codes) 'string))

(defun octet-test-bytes-string (&rest bytes)
  (coerce (mapcar #'code-char bytes) 'string))

(defun octet-test-trim (s)
  (string-trim '(#\LFD #\RET #\SPC) s))

(defun octet-test-list (v)
  (coerce v 'list))

;;; The converter of the ACP: the UTF-8 side against the codec layer

(defparameter *octet-test-utf8-sequences*
  '(() (#x41) (#x7f) (#xc3 #xa9) (#xe3 #x81 #x82) (#xf0 #x9f #x98 #x80)
    (#x80) (#xbf) (#xff) (#xfe) (#xc0 #x80) (#xc1 #xbf) (#xc2) (#xe3 #x81)
    (#xe3 #x41) (#xf0 #x9f #x98) (#xed #xa0 #x80) (#xf4 #x90 #x80 #x80)
    (#xf0 #x80 #x80 #x80) (#xe0 #x80 #x80) (#x61 #xe3 #x81 #x82 #x62)
    (#xe3 #x81 #x82 #xe3 #x81) (#xf8 #x88 #x80 #x80 #x80)
    (#x41 #x80 #x42 #xc3 #xa9 #xff)))

(deftest acp-utf8-decode-same-as-codec ()
  (let ((r nil))
    (dolist (seq *octet-test-utf8-sequences*)
      (unless (equalp (si:acp-utf8-decode seq)
                      (si:codec-decode *encoding-utf8n* seq))
        (push seq r)))
    r)
  => nil)

(deftest acp-utf8-decode-values ()
  (values
   (octet-test-list (si:acp-utf8-decode '(#x41 #xc3 #xa9 #xe3 #x81 #x82)))
   (octet-test-list (si:acp-utf8-decode '(#xf0 #x9f #x98 #x80)))
   (octet-test-list (si:acp-utf8-decode '(#x80 #xff)))
   (octet-test-list (si:acp-utf8-decode #(#xe3 #x81))))
  => (#x41 #xe9 #x3042)
  => (#x1f600)
  => (#x110080 #x1100ff)
  => (#x1100e3 #x110081))

(deftest acp-utf8-encode-same-as-codec ()
  (let ((points '(#x41 #xe9 #x3042 #x10000 #x1f600 #x10ffff #x110080 #x1100ff
                  #xd800 #xdfff #x110000 #x1fffff #x7f #x80 #x7ff #x800 #xffff
                  #x0 #x61))
        (r nil))
    (dolist (p points)
      (unless (equalp (si:acp-utf8-encode (list p))
                      (si:codec-encode *encoding-utf8n* (list p)))
        (push p r)))
    (unless (equalp (si:acp-utf8-encode points)
                    (si:codec-encode *encoding-utf8n* points))
      (push :all r))
    r)
  => nil)

(deftest acp-utf8-round-trip ()
  (let ((r nil))
    (dolist (seq *octet-test-utf8-sequences*)
      (let ((back (octet-test-list
                   (si:acp-utf8-encode (si:acp-utf8-decode seq)))))
        ;; what is a valid UTF-8, and every invalid byte, come back
        (unless (equal back seq)
          (push seq r))))
    r)
  => nil)

(deftest acp-utf8-encode-unwritable ()
  (values
   (octet-test-list (si:acp-utf8-encode '(#xd800)))
   (octet-test-list (si:acp-utf8-encode '(#x110000)))
   (octet-test-list (si:acp-utf8-encode '(#x110080 #x110081))))
  => (#x3f)
  => (#x3f)
  => (#x80 #x81))

(deftest acp-utf8-error ()
  (si:acp-utf8-decode '(256))
  !! range-error)

(deftest chars-to-utf16-values ()
  (values
   (octet-test-list (si:chars-to-utf16 '(#x41 #x3042)))
   (octet-test-list (si:chars-to-utf16 '(#x10000 #x1f600 #x10ffff)))
   (octet-test-list (si:chars-to-utf16 '(#xd800 #xdfff #x110000 #x110080)))
   (octet-test-list (si:chars-to-utf16 nil)))
  => (#x41 #x3042)
  => (#xd800 #xdc00 #xd83d #xde00 #xdbff #xdfff)
  => (#xfffd #xfffd #xfffd #xfffd)
  => nil)

;;; The rule of "a string of bytes" (the side of XYZZY_CHAR_UNICODE 1)

(deftest octet-rule-encode-bytes ()
  (values
   (octet-test-list (si:octet-rule-encode '(#x61 #xe9 #xff #x0)))
   (octet-test-list (si:octet-rule-encode '(#x1100e9 #x61 #x1100ff)))
   (octet-test-list (si:octet-rule-encode nil)))
  => (#x61 #xe9 #xff #x0)
  => (#xe9 #x61 #xff)
  => nil)

(deftest octet-rule-encode-default-encoding ()
  ;; a string with a character beyond U+00FF is converted by
  ;; *default-fileio-encoding* (sjis)
  (let ((*default-fileio-encoding* *encoding-sjis*))
    (values
     (octet-test-list (si:octet-rule-encode '(#x3042)))
     (octet-test-list (si:octet-rule-encode '(#x61 #x3042 #x62)))
     (octet-test-list (si:octet-rule-encode '(#xff71 #x6f22 #x5b57)))))
  => (#x82 #xa0)
  => (#x61 #x82 #xa0 #x62)
  => (#xb1 #x8a #xbf #x8e #x9a))

(deftest octet-rule-encode-one-by-one ()
  ;; characters from a stream are decided one by one: a byte is a byte
  (let ((*default-fileio-encoding* *encoding-sjis*))
    (octet-test-list (si:octet-rule-encode '(#x61 #xe9 #x3042 #x62 #xe9)
                                           :one-by-one t)))
  => (#x61 #xe9 #x82 #xa0 #x62 #xe9))

(deftest octet-rule-encode-encoding ()
  (values
   (octet-test-list (si:octet-rule-encode '(#x3042 #xe9) :encoding *encoding-utf8n*))
   (octet-test-list (si:octet-rule-encode '(#x61 #x3042) :encoding *encoding-sjis*))
   (octet-test-list (si:octet-rule-encode '(#x61 #xe9) :encoding *encoding-utf8n*))
   (octet-test-list (si:octet-rule-encode '(#x3042) :encoding *encoding-euc-jp*))
   ;; the end of the text is written (ISO-2022-JP returns to ASCII)
   (octet-test-list (si:octet-rule-encode '(#x3042) :encoding *encoding-jis*))
   ;; a raw byte is its own byte, with every encoding
   (octet-test-list (si:octet-rule-encode '(#x1100e9 #x3042) :encoding *encoding-utf8n*)))
  => (#xe3 #x81 #x82 #xc3 #xa9)
  => (#x61 #x82 #xa0)
  => (#x61 #xc3 #xa9)
  => (#xa4 #xa2)
  => (#x1b #x24 #x42 #x24 #x22 #x1b #x28 #x42)
  => (#xe9 #xe3 #x81 #x82))

(deftest octet-rule-encode-error ()
  (si:octet-rule-encode '(#x41) :encoding *encoding-auto*)
  !! type-error)

;;; :encoding of the octet functions (the same results with both values of
;;; the switch)

(deftest octet-base64-encoding ()
  (values
   (octet-test-trim (si:base64-encode (octet-test-string #x3042) nil nil
                                      :encoding *encoding-utf8n*))
   (octet-test-trim (si:base64-encode (octet-test-string #x3042) nil nil
                                      :encoding *encoding-sjis*))
   (octet-test-trim (si:base64-encode (octet-test-string #x3042) nil nil
                                      :encoding *encoding-euc-jp*))
   (octet-test-trim (si:base64-encode "abc" nil nil :encoding *encoding-utf8n*))
   (octet-test-trim (si:base64-encode "abc")))
  => "44GC"
  => "gqA="
  => "pKI="
  => "YWJj"
  => "YWJj")

(deftest octet-base64-encoding-stream ()
  (values
   (with-input-from-string (s (octet-test-string #x3042))
     (octet-test-trim (si:base64-encode s nil nil :encoding *encoding-utf8n*)))
   (with-input-from-string (s (octet-test-string #x3042))
     (octet-test-trim (si:base64-encode s nil nil :encoding *encoding-sjis*))))
  => "44GC"
  => "gqA=")

(deftest octet-base64-decode-length ()
  (values
   (si:octet-length (si:base64-decode "44GC"))
   (si:octet-length (si:base64-decode "gqA="))
   (map 'list #'char-code (si:base64-decode "YWJj")))
  => 3
  => 2
  => (#x61 #x62 #x63))

(deftest octet-decode-then-convert ()
  ;; a string of bytes is text through convert-encoding-to-internal
  (values
   (string= (convert-encoding-to-internal *encoding-utf8n* (si:base64-decode "44GC"))
            (octet-test-string #x3042))
   (string= (convert-encoding-to-internal *encoding-sjis* (si:base64-decode "gqA="))
            (octet-test-string #x3042))
   (string= (convert-encoding-to-internal *encoding-euc-jp* (si:base64-decode "pKI="))
            (octet-test-string #x3042)))
  => t
  => t
  => t)

(deftest octet-md5-encoding ()
  (let ((utf8 (octet-test-bytes-string #xe3 #x81 #x82))
        (sjis (octet-test-bytes-string #x82 #xa0))
        (jp (octet-test-string #x3042)))
    (values
     (string= (si:md5 jp :encoding *encoding-utf8n*) (si:md5 utf8))
     (string= (si:md5 jp :encoding *encoding-sjis*) (si:md5 sjis))
     (string= (si:md5 jp) (si:md5 sjis))
     (string= (si:sha-256 jp :encoding *encoding-utf8n*) (si:sha-256 utf8))
     (string= (si:md5 "abc" :encoding *encoding-utf8n*) (si:md5 "abc"))
     (si:md5 "abc" :encoding *encoding-sjis*)))
  => t
  => t
  => t
  => t
  => t
  => "900150983cd24fb0d6963f7d28e17f72")

(deftest octet-md5-stream-encoding ()
  (with-input-from-string (s (octet-test-string #x3042))
    (string= (si:md5 s :encoding *encoding-utf8n*)
             (si:md5 (octet-test-bytes-string #xe3 #x81 #x82))))
  => t)

(deftest octet-hmac-encoding ()
  ;; the key and the data are converted by the same rule
  (let ((jp (octet-test-string #x3042))
        (utf8 (octet-test-bytes-string #xe3 #x81 #x82)))
    (values
     (string= (si:hmac-sha-256 jp jp :encoding *encoding-utf8n*)
              (si:hmac-sha-256 utf8 utf8))
     (string= (si:hmac-md5 jp "data" :encoding *encoding-utf8n*)
              (si:hmac-md5 utf8 "data"))
     (string= (si:hmac-sha-1 jp jp) (si:hmac-sha-1 jp jp :encoding *encoding-sjis*))))
  => t
  => t
  => t)

(deftest octet-length-encoding-rule ()
  (values
   (si:octet-length (octet-test-string #x3042))
   (si:octet-length (octet-test-string #x3042) :encoding *encoding-utf8n*)
   (si:octet-length (octet-test-string #x3042 #x3044) :encoding *encoding-sjis*)
   (si:octet-length "abc" :start 1 :end 2))
  => 2
  => 3
  => 4
  => 1)

(deftest octet-url-encoding ()
  (values
   (string-upcase (si:www-url-encode (octet-test-string #x3042) nil nil :encoding *encoding-utf8n*))
   (string-upcase (si:www-url-encode (octet-test-string #x3042) nil nil :encoding *encoding-sjis*)))
  => "%E3%81%82"
  => "%82%A0")

(deftest octet-encoding-error ()
  (si:md5 "abc" :encoding *encoding-auto*)
  !! type-error)

(deftest octet-encoding-error-2 ()
  (si:base64-encode "abc" nil nil :encoding "utf8")
  !! type-error)

;;; MIME headers (decode-mime-header) and the decoders of the regions

(defun octet-test-decode-header (header)
  (let ((buf (create-new-buffer "*octet-test-mime*")))
    (unwind-protect
        (save-excursion
          (set-buffer buf)
          (insert header)
          (goto-char (point-min))
          (decode-mime-header)
          (buffer-substring (point-min) (point-max)))
      (delete-buffer buf))))

(defun octet-test-decoded-p (header)
  (let ((r (octet-test-decode-header header)))
    (list (and (find (unicode-char #x3042) r) t)
          (and (string-match "=\\?" r) t)
          (and (string-match "^Subject: " r) t))))

(deftest decode-mime-header-iso-2022-jp ()
  (octet-test-decoded-p (format nil "Subject: =?ISO-2022-JP?B?GyRCJCIbKEI=?=~%"))
  => (t nil t))

(deftest decode-mime-header-utf-8-b ()
  (octet-test-decoded-p (format nil "Subject: =?UTF-8?B?44GC?=~%"))
  => (t nil t))

(deftest decode-mime-header-utf-8-q ()
  (octet-test-decoded-p (format nil "Subject: =?UTF-8?Q?=E3=81=82?=~%"))
  => (t nil t))

(deftest decode-mime-header-shift-jis ()
  (octet-test-decoded-p (format nil "Subject: =?Shift_JIS?B?gqA=?=~%"))
  => (t nil t))

(deftest decode-mime-header-euc-jp ()
  (octet-test-decoded-p (format nil "Subject: =?EUC-JP?B?pKI=?=~%"))
  => (t nil t))

(deftest decode-mime-header-text-around ()
  ;; ASCII around an encoded word stays
  (let ((r (octet-test-decode-header
            (format nil "Subject: re: =?UTF-8?B?44GC?= end~%"))))
    (list (and (string-match "^Subject: re: " r) t)
          (and (find (unicode-char #x3042) r) t)
          (and (string-match " end" r) t)))
  => (t t t))

(deftest decode-mime-header-same-as-convert ()
  ;; the same characters as convert-encoding-to-internal gives
  (let ((r (octet-test-decode-header (format nil "Subject: =?UTF-8?B?44GC44GE?=~%"))))
    (and (search (octet-test-string #x3042 #x3044) r) t))
  => t)

(defun octet-test-decode-region (decode text encoding)
  (let ((buf (create-new-buffer "*octet-test-region*")))
    (unwind-protect
        (save-excursion
          (set-buffer buf)
          (insert text)
          (let ((*expected-fileio-encoding* encoding))
            (funcall decode (point-min) (point-max)))
          (buffer-substring (point-min) (point-max)))
      (delete-buffer buf))))

(deftest base64-decode-region-expected-encoding ()
  (values
   (string= (octet-test-decode-region #'base64-decode-region "44GC" *encoding-utf8n*)
            (octet-test-string #x3042))
   (string= (octet-test-decode-region #'base64-decode-region "gqA=" *encoding-sjis*)
            (octet-test-string #x3042))
   (string= (octet-test-decode-region #'base64-decode-region "pKI=" *encoding-euc-jp*)
            (octet-test-string #x3042))
   (octet-test-decode-region #'base64-decode-region "YWJj" *encoding-utf8n*))
  => t
  => t
  => t
  => "abc")

(deftest url-decode-region-expected-encoding ()
  (values
   (string= (octet-test-decode-region #'url-encoding-decode-region "%E3%81%82"
                                      *encoding-utf8n*)
            (octet-test-string #x3042))
   (string= (octet-test-decode-region #'url-encoding-decode-region "%82%A0"
                                      *encoding-sjis*)
            (octet-test-string #x3042)))
  => t
  => t)

(deftest quoted-printable-decode-region-expected-encoding ()
  (string= (octet-test-decode-region #'quoted-printable-decode-region "=E3=81=82"
                                     *encoding-utf8n*)
           (octet-test-string #x3042))
  => t)

;;; map-char-encoding-region: the characters of the buffer are the bytes of
;;; the file, as the encoding of the buffer reads them

(defun octet-test-map-region (buffer-encoding text encoding)
  (let ((buf (create-new-buffer "*octet-test-map*")))
    (unwind-protect
        (save-excursion
          (set-buffer buf)
          (set-buffer-fileio-encoding buffer-encoding)
          (insert text)
          (map-char-encoding-region (point-min) (point-max) encoding)
          (buffer-substring (point-min) (point-max)))
      (delete-buffer buf))))

(deftest map-char-encoding-region-same-encoding ()
  (values
   (string= (octet-test-map-region *encoding-sjis* (octet-test-string #x3042 #x41)
                                   *encoding-sjis*)
            (octet-test-string #x3042 #x41))
   (string= (octet-test-map-region *encoding-utf8n* (octet-test-string #x3042 #x41)
                                   *encoding-utf8n*)
            (octet-test-string #x3042 #x41)))
  => t
  => t)

(deftest map-char-encoding-region-reread ()
  ;; the bytes of EUC-JP of U+3042 (A4 A2), read as cp932: two half width kana
  (values
   (string= (octet-test-map-region *encoding-euc-jp* (octet-test-string #x3042)
                                   *encoding-sjis*)
            (octet-test-string #xff64 #xff62))
   ;; the bytes of UTF-8 of U+3042 (E3 81 82), read as UTF-8: the same
   (string= (octet-test-map-region *encoding-utf8n* (octet-test-string #x3042)
                                   *encoding-utf8n*)
            (octet-test-string #x3042)))
  => t
  => t)
