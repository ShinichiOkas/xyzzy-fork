;;; -*- mode: lisp; package: user -*-
;;;
;;; S4 R6: the reader and the writer of ini files (src/ini.cc).
;;;
;;; They must do what GetPrivateProfileString and WritePrivateProfileString
;;; did. The expected values below are what those API returned and wrote
;;; (probed on Windows before the replacement), apart from three things that
;;; are on purpose:
;;;   - a UTF-8 file (with a byte order mark) and a UTF-16 file can be read
;;;     (the A API took the mark as a part of the first line);
;;;   - a delete on a file that does not exist does not create the file;
;;;   - cp932 bytes that are not defined (and the codes that Windows does
;;;     not write for a character) are kept as they are.
;;;
;;; The tests use si:ini-read and si:ini-write, which work on any file; they
;;; never touch the settings of the user. The tests that check the bytes
;;; written by the 0 side of XYZZY_CHAR_UNICODE (cp932, no byte order mark)
;;; have 0 in the name.
;;;
;;; The notation of a file in the tests: a list of lines joined by CRLF.

(in-package :user)

(defvar *ini-t-cr* (string (code-char 13)))
(defvar *ini-t-lf* (string (code-char 10)))
(defvar *ini-t-crlf* (concat *ini-t-cr* *ini-t-lf*))
(defvar *ini-t-tab* (string (code-char 9)))

;; lines joined by CRLF, with CRLF at the end
(defun ini-t-text (&rest lines)
  (apply #'concat (mapcar (lambda (l) (concat l *ini-t-crlf*)) lines)))

;; lines joined by LF, with LF at the end
(defun ini-t-lf-text (&rest lines)
  (apply #'concat (mapcar (lambda (l) (concat l *ini-t-lf*)) lines)))

(defun ini-t-put-bytes (file bytes)
  (with-open-file (s file :direction :output :encoding :binary
                     :if-exists :supersede)
    (dolist (b bytes)
      (write-char (code-char b) s))))

(defun ini-t-get-bytes (file)
  (with-open-file (s file :direction :input :encoding :binary)
    (let ((r nil) c)
      (while (setq c (read-char s nil nil))
        (push (char-code c) r))
      (nreverse r))))

(defun ini-t-ascii-bytes (string)
  (map 'list #'char-code string))

;; the file as a string, if all bytes are ASCII
(defun ini-t-get-text (file)
  (map 'string #'code-char (ini-t-get-bytes file)))

;; initial: a string of ASCII (nil: no file)
(defmacro with-ini-file ((var initial) &body body)
  `(with-temp-files (,var)
     (if ,initial
         (ini-t-put-bytes ,var (ini-t-ascii-bytes ,initial))
       (when (file-exist-p ,var)
         (delete-file ,var)))
     ,@body))
(setf (get 'with-ini-file 'ed:lisp-indent-hook) 1)

(defun ini-t-read (text section key)
  (with-ini-file (f text)
    (si:ini-read f section key)))

;; Applies the writes (SECTION KEY VALUE) in order and returns the file.
(defun ini-t-apply (text &rest ops)
  (with-ini-file (f text)
    (dolist (op ops)
      (si:ini-write f (first op) (second op) (third op)))
    (if (file-exist-p f)
        (ini-t-get-text f)
      :nofile)))

;;; ------------------------------------------------------------------
;;; read

(defvar *ini-t-read-file*
  (ini-t-text "[Sec]"
              "key=value"
              "  spaced  =  v  v  "
              "q=\"quoted\""
              "sq='single'"
              "half=\"open"
              "mix=\"a'"
              ";comment=1"
              "; c2"
              "noeq"
              "dup=1"
              "dup=2"
              "empty="
              "semi=a;b"
              "KEY=upper"
              "[ Sp ]"
              "x=1"
              "[sec]"
              "low=1"
              "  [Ind]"
              "ind=1"
              "[Br]ac"
              "br=1"
              "[Un"
              "un=1"))

(deftest ini-read-rules ()
  (mapcar (lambda (q) (ini-t-read *ini-t-read-file* (first q) (second q)))
          '(("Sec" "key") ("sec" "KEY") ("Sec" "Key")
            ("Sec" "spaced") ("Sec" "q") ("Sec" "sq")
            ("Sec" "half") ("Sec" "mix")
            ("Sec" "comment") ("Sec" ";comment") ("Sec" "noeq")
            ("Sec" "dup") ("Sec" "empty") ("Sec" "semi")
            ("Sp" "x") (" Sp " "x")
            ("sec" "low") ("Ind" "ind") ("Br" "br") ("Un" "un")
            ("Nope" "x") ("Sec" "nokey")))
  => ("value" "value" "value"
      "v  v" "quoted" "single"
      "\"open" "\"a'"
      nil nil nil
      "1" "" "a;b"
      "1" "1"
      nil "1" "1" "1"
      nil nil))

(deftest ini-read-tab-and-more ()
  (let ((tab *ini-t-tab*))
    (values
     (ini-t-read (ini-t-text "[S]" (concat "tab" tab "=" tab "v" tab)) "S" "tab")
     (ini-t-read (ini-t-text "[S]" "a=b=c") "S" "a")
     (ini-t-read (ini-t-text "[S]" "x=1 ; trail") "S" "x")
     (ini-t-read (ini-t-text "[S]" "a=\"x\"y\"") "S" "a")
     (ini-t-read (ini-t-text "[S]" "b=\"\"") "S" "b")
     (ini-t-read (ini-t-text "[S]" "c=\"") "S" "c")
     (ini-t-read (ini-t-text "[S]" "d=' x '") "S" "d")
     (ini-t-read (ini-t-text "[S]" "a=1" "[S2]" "a=2") "S2" "a")
     (ini-t-read (ini-t-text "[S]" "abc=1" "ab=2") "S" "ab")))
  => "v"
  => "b=c"
  => "1 ; trail"
  => "x\"y"
  => ""
  => "\""
  => " x "
  => "2"
  => "2")

(deftest ini-read-first-section-and-key-only ()
  (values
   (ini-t-read (ini-t-text "[S]" "a=1" "[S]" "a=2" "b=3") "S" "a")
   (ini-t-read (ini-t-text "[S]" "a=1" "[S]" "a=2" "b=3") "S" "b")
   (ini-t-read (ini-t-text "a=1" "[S]" "b=1") "" "a")
   (ini-t-read (ini-t-text "[S]" "a=1" "[T]" "" "  [S]" "c=3") "S" "c"))
  => "1"
  => nil
  => nil
  => nil)

(deftest ini-read-line-ends ()
  ;; CR alone, LF alone, CRLF and no end of line at the end of the file
  (let ((cr *ini-t-cr*) (lf *ini-t-lf*))
    (values
     (ini-t-read (concat "[S]" cr "a=1" cr "b=2" cr) "S" "b")
     (ini-t-read (concat "[S]" lf "a=1" lf "b=2") "S" "b")
     (ini-t-read (concat "[S]" *ini-t-crlf* "a=1" *ini-t-crlf* "b=2") "S" "b")))
  => "2"
  => "2"
  => "2")

(deftest ini-read-no-file ()
  (with-ini-file (f nil)
    (values (si:ini-read f "S" "a")
            (file-exist-p f)))
  => nil
  => nil)

(deftest ini-read-sees-a-file-that-changes ()
  ;; The last file is cached; a change of the file (by another program) is
  ;; seen.
  (with-ini-file (f (ini-t-text "[S]" "a=1"))
    (let ((r1 (si:ini-read f "S" "a")))
      (ini-t-put-bytes f (ini-t-ascii-bytes (ini-t-text "[S]" "a=22222")))
      (list r1 (si:ini-read f "S" "a"))))
  => ("1" "22222"))

;;; ------------------------------------------------------------------
;;; write

(deftest ini-write-new-file-and-add ()
  (ini-t-apply nil
               '("A" "k" "v")
               '("A" "k2" "v2")
               '("B" "k" "v")
               '("A" "K" "new")
               '("a" "k3" "v3"))
  => #.(ini-t-text "[A]" "k=new" "k2=v2" "k3=v3" "[B]" "k=v"))

(deftest ini-write-replaces-in-place ()
  (values
   ;; the value after '=' is replaced; the white space after it stays
   (ini-t-apply (ini-t-text "[S]" "a =  1  ")
                '("S" "a" "2"))
   (ini-t-apply (ini-t-text "[S]" "a =  1  ")
                '("S" "  a  " "3"))
   (ini-t-apply (ini-t-text "[S]" "  a  =1")
                '("S" "a" "2"))
   ;; the spelling of the key is the one in the file
   (ini-t-apply (ini-t-text "[S]" "A=1")
                '("S" "a" "2"))
   ;; the quotes of the old value go with it
   (ini-t-apply (ini-t-text "[S]"
                            "a=  v  v  "
                            "b=\"quoted\" "
                            "c="
                            (concat "d=" *ini-t-tab* " x " *ini-t-tab*)
                            "e = \"lit\"")
                '("S" "a" "N") '("S" "b" "N") '("S" "c" "N")
                '("S" "d" "N") '("S" "e" "N"))
   (ini-t-apply (ini-t-text "[S]" "a=1") '("S" "a" ""))
   (ini-t-apply (ini-t-text "[S]" "a=1") '("S" "a" "  lead"))
   (ini-t-apply (ini-t-text "[S]" "abc=1" "ab=2") '("S" "ab" "9"))
   (ini-t-apply (ini-t-text "[S]" "x=1 ; trail") '("S" "x" "2")))
  => #.(ini-t-text "[S]" "a =2  ")
  => #.(ini-t-text "[S]" "a =3  ")
  => #.(ini-t-text "[S]" "  a  =2")
  => #.(ini-t-text "[S]" "A=2")
  => #.(ini-t-text "[S]" "a=N  " "b=N " "c=N"
                   (concat "d=N " *ini-t-tab*) "e =N")
  => #.(ini-t-text "[S]" "a=")
  => #.(ini-t-text "[S]" "a=  lead")
  => #.(ini-t-text "[S]" "abc=1" "ab=9")
  => #.(ini-t-text "[S]" "x=2"))

;; a replaced line keeps its LF; a new line has CRLF; the last line gets
;; the end of line that it did not have
(deftest ini-write-keeps-the-end-of-line-of-a-replaced-line ()
  (ini-t-apply (concat (ini-t-lf-text "[S]" "a=1" "b=2" "[T]") "t=1")
               '("S" "a" "9"))
  => #.(concat (ini-t-lf-text "[S]" "a=9" "b=2" "[T]") "t=1" *ini-t-crlf*))

(deftest ini-write-adds-a-line-with-crlf-to-a-file-with-lf ()
  (ini-t-apply (concat (ini-t-lf-text "[S]" "a=1" "b=2" "[T]") "t=1")
               '("S" "a" "9") '("S" "n" "0"))
  => #.(concat (ini-t-lf-text "[S]" "a=9" "b=2") "n=0" *ini-t-crlf*
               (ini-t-lf-text "[T]") "t=1" *ini-t-crlf*))

(deftest ini-write-adds-a-key-after-the-last-key ()
  (values
   ;; before the blank lines and the comments that follow the last key
   (ini-t-apply (ini-t-text "[S]" "a=1" "" "; c" "b=2" "" "" "[T]" "t=1")
                '("S" "c" "3"))
   (ini-t-apply (ini-t-text "[S]" "a=1" ";x" "" "[T]" "b=2")
                '("S" "n" "1"))
   (ini-t-apply (ini-t-text "[S]" "a=1" ";x" "[T]" "b=2")
                '("S" "n" "1"))
   (ini-t-apply (ini-t-text "[S]" "a=1" "noeq" "[T]" "b=2")
                '("S" "n" "1"))
   (ini-t-apply (ini-t-text "[S]" "a=1" ";c=1" "[T]" "c=3")
                '("S" "n" "1"))
   ;; an empty section
   (ini-t-apply (ini-t-text "[S]" "[T]" "b=2")
                '("S" "n" "1"))
   (ini-t-apply (ini-t-text "[S]" "" "[T]" "b=2")
                '("S" "n" "1"))
   ;; the first of the sections of the name
   (ini-t-apply (ini-t-text "[S]" "a=1" "[S]" "a=2" "b=3")
                '("S" "a" "9") '("S" "b" "9") '("S" "c" "9"))
   ;; the lines before the first section stay
   (ini-t-apply (ini-t-text "a=1" "[S]" "b=1")
                '("S" "c" "1")))
  => #.(ini-t-text "[S]" "a=1" "" "; c" "b=2" "c=3" "" "" "[T]" "t=1")
  => #.(ini-t-text "[S]" "a=1" "n=1" ";x" "" "[T]" "b=2")
  => #.(ini-t-text "[S]" "a=1" "n=1" ";x" "[T]" "b=2")
  => #.(ini-t-text "[S]" "a=1" "n=1" "noeq" "[T]" "b=2")
  => #.(ini-t-text "[S]" "a=1" "n=1" ";c=1" "[T]" "c=3")
  => #.(ini-t-text "[S]" "n=1" "[T]" "b=2")
  => #.(ini-t-text "[S]" "n=1" "" "[T]" "b=2")
  => #.(ini-t-text "[S]" "a=9" "b=9" "c=9" "[S]" "a=2" "b=3")
  => #.(ini-t-text "a=1" "[S]" "b=1" "c=1"))

(deftest ini-write-adds-a-section-at-the-end ()
  (values
   (ini-t-apply (ini-t-text "[S]" "a=1") '("T" "a" "1"))
   (ini-t-apply (concat (ini-t-text "[S]") "a=1") '("T" "a" "1"))
   (ini-t-apply (ini-t-text "; head" "" "[S]" "a=1") '("T" "a" "1"))
   (ini-t-apply (ini-t-text "[S]" "a=1" "") '("T" "a" "1"))
   (ini-t-apply "" '("S" "a" "1"))
   (ini-t-apply (ini-t-text "" "") '("S" "a" "1")))
  => #.(ini-t-text "[S]" "a=1" "[T]" "a=1")
  => #.(ini-t-text "[S]" "a=1" "[T]" "a=1")
  => #.(ini-t-text "; head" "" "[S]" "a=1" "[T]" "a=1")
  => #.(ini-t-text "[S]" "a=1" "" "[T]" "a=1")
  => #.(ini-t-text "[S]" "a=1")
  => #.(ini-t-text "" "" "[S]" "a=1"))

(deftest ini-write-deletes ()
  (values
   ;; a key (the first of the name)
   (ini-t-apply (ini-t-text "[S]" "a=1" "b=2" "[T]" "c=3") '("S" "a" nil))
   (ini-t-apply (ini-t-text "[S]" "a=1" "b=2" "b=3") '("S" "b" nil))
   (ini-t-apply (ini-t-text "[S]" "a=1") '("S" "a" nil))
   (ini-t-apply (ini-t-text "[S]" "a=1") '("S" "zz" nil) '("Q" "zz" nil))
   ;; a section: from the header to the last key line
   (ini-t-apply (ini-t-text "[S]" "a=1" "b=2" "[T]" "c=3") '("S" nil nil))
   (ini-t-apply (ini-t-text "[S]" "a=1" ";mid" "b=2" ";tail" "" "[T]" "c=3")
                '("S" nil nil))
   (ini-t-apply (ini-t-text "[S]" "a=1" ";c=1" "[T]" "c=3") '("S" nil nil))
   (ini-t-apply (ini-t-text "[S]" "a=1" "[T]") '("T" nil nil))
   (ini-t-apply (ini-t-text "[S]" "a=1") '("Q" nil nil))
   ;; no file: not created
   (ini-t-apply nil '("S" "a" nil) '("S" nil nil)))
  => #.(ini-t-text "[S]" "b=2" "[T]" "c=3")
  => #.(ini-t-text "[S]" "a=1" "b=3")
  => #.(ini-t-text "[S]")
  => #.(ini-t-text "[S]" "a=1")
  => #.(ini-t-text "[T]" "c=3")
  => #.(ini-t-text ";tail" "" "[T]" "c=3")
  => #.(ini-t-text ";c=1" "[T]" "c=3")
  => #.(ini-t-text "[S]" "a=1")
  => #.(ini-t-text "[S]" "a=1")
  => :nofile)

(deftest ini-write-reads-back-quoted-values ()
  (with-ini-file (f nil)
    (si:ini-write f "S" "y" "\"q 1\"")
    (list (ini-t-get-text f)
          (si:ini-read f "S" "y")))
  => #.(list (ini-t-text "[S]" "y=\"q 1\"") "q 1"))

(deftest ini-write-does-not-rewrite-when-nothing-changes ()
  ;; The file has a UTF-8 mark: it stays (a write would turn it into cp932).
  (with-ini-file (f nil)
    (let ((bytes (append '(#xef #xbb #xbf)
                         (ini-t-ascii-bytes (ini-t-text "[S]" "a=1")))))
      (ini-t-put-bytes f bytes)
      (si:ini-write f "S" "a" "1")
      (si:ini-write f "S" "zz" nil)
      (si:ini-write f "Q" nil nil)
      (equal (ini-t-get-bytes f) bytes)))
  => t)

;;; ------------------------------------------------------------------
;;; the encodings of the file

;; ‚l‚r ƒSƒVƒbƒN
(defvar *ini-t-font-name* "‚l‚r ƒSƒVƒbƒN")
(defvar *ini-t-font-cp932*
  '(#x82 #x6c #x82 #x72 #x20 #x83 #x53 #x83 #x56 #x83 #x62 #x83 #x4e))
(defvar *ini-t-font-utf8*
  '(#xef #xbc #xad #xef #xbc #xb3 #x20 #xe3 #x82 #xb4 #xe3 #x82 #xb7
    #xe3 #x83 #x83 #xe3 #x82 #xaf))

(defun ini-t-bytes-of (&rest parts)
  (apply #'append
         (mapcar (lambda (p) (if (stringp p) (ini-t-ascii-bytes p) p)) parts)))

(deftest ini-read-cp932-file ()
  (with-ini-file (f nil)
    (ini-t-put-bytes f (ini-t-bytes-of "[Font]" *ini-t-crlf*
                                      "japanese=-16,\"" *ini-t-font-cp932*
                                      "\",128" *ini-t-crlf*))
    (si:ini-read f "Font" "japanese"))
  => #.(concat "-16,\"" *ini-t-font-name* "\",128"))

(deftest ini-read-utf8-file-with-mark ()
  (with-ini-file (f nil)
    (ini-t-put-bytes f (ini-t-bytes-of '(#xef #xbb #xbf)
                                      "[Font]" *ini-t-crlf*
                                      "japanese=-16,\"" *ini-t-font-utf8*
                                      "\",128" *ini-t-crlf*))
    (si:ini-read f "Font" "japanese"))
  => #.(concat "-16,\"" *ini-t-font-name* "\",128"))

(deftest ini-read-utf16-file-with-mark ()
  (with-ini-file (f nil)
    ;; "[S]" CRLF "a=" and the two characters, in UTF-16LE:
    ;; ‚l = ff2d, ‚r = ff33
    (ini-t-put-bytes f (append '(#xff #xfe)
                               (mapcan (lambda (c) (list c 0))
                                       (ini-t-ascii-bytes
                                        (concat "[S]" *ini-t-crlf* "a=")))
                               '(#x2d #xff #x33 #xff #x0d 0 #x0a 0)))
    (si:ini-read f "S" "a"))
  => "‚l‚r")

(deftest ini-write-japanese-0 ()
  ;; The 0 side writes cp932 without a mark. The name of a font goes out and
  ;; comes back.
  (with-ini-file (f nil)
    (si:ini-write f "Font" "japanese" (concat "-16,\"" *ini-t-font-name* "\",128"))
    (list (ini-t-get-bytes f)
          (si:ini-read f "Font" "japanese")))
  => #.(list (ini-t-bytes-of "[Font]" *ini-t-crlf* "japanese=-16,\""
                             *ini-t-font-cp932* "\",128" *ini-t-crlf*)
             (concat "-16,\"" *ini-t-font-name* "\",128")))

(deftest ini-japanese-names-round-trip ()
  (with-ini-file (f nil)
    (si:ini-write f "Ý’è" "ƒL[" "’l")
    (si:ini-write f "Ý’è" "ƒL[2" *ini-t-font-name*)
    (list (si:ini-read f "Ý’è" "ƒL[")
          (si:ini-read f "Ý’è" "ƒL[2")
          (si:ini-read f "Ý’è" "none")))
  => ("’l" #.*ini-t-font-name* nil))

(deftest ini-write-turns-a-utf8-file-into-cp932-0 ()
  ;; A change of a UTF-8 file with a mark is written in cp932 (the 0 side).
  ;; The text of the other lines is kept.
  (with-ini-file (f nil)
    (ini-t-put-bytes f (ini-t-bytes-of '(#xef #xbb #xbf)
                                      "[Font]" *ini-t-crlf*
                                      "japanese=" *ini-t-font-utf8* *ini-t-crlf*))
    (si:ini-write f "Font" "n" "1")
    (list (ini-t-get-bytes f)
          (si:ini-read f "Font" "japanese")))
  => #.(list (ini-t-bytes-of "[Font]" *ini-t-crlf* "japanese=" *ini-t-font-cp932*
                             *ini-t-crlf* "n=1" *ini-t-crlf*)
             *ini-t-font-name*))

(deftest ini-write-keeps-undefined-and-duplicated-cp932-bytes-0 ()
  ;; #xed #x40 is a code that Windows does not write (NEC selected IBM
  ;; extension): the code of the file stays. #xfd is not a character of
  ;; cp932: it stays as a byte.
  (with-ini-file (f nil)
    (let ((bytes (ini-t-bytes-of "[S]" *ini-t-crlf*
                                 "a=" '(#xed #x40) *ini-t-crlf*
                                 "z=" '(#xfd) "x" *ini-t-crlf*)))
      (ini-t-put-bytes f bytes)
      (si:ini-write f "S" "n" "1")
      (ini-t-get-bytes f)))
  => #.(ini-t-bytes-of "[S]" *ini-t-crlf*
                       "a=" '(#xed #x40) *ini-t-crlf*
                       "z=" '(#xfd) "x" *ini-t-crlf*
                       "n=1" *ini-t-crlf*))

;;; ------------------------------------------------------------------
;;; xyzzy.ini of a started xyzzy (end to end)

(deftest ini-xyzzy-restart-keeps-the-font-name-0 ()
  ;; A folder (XYZZYCONFIGPATH) with a cp932 xyzzy.ini that has a Japanese
  ;; name of a font. xyzzy starts, reads it, changes the size and saves; the
  ;; file has the name in cp932; xyzzy starts again and reads the name.
  (with-temp-dirs (cfg)
    (let ((ini (merge-pathnames "xyzzy.ini" cfg))
          (env `(("XYZZYCONFIGPATH" . ,cfg))))
      (unwind-protect
          (progn
            (ini-t-put-bytes ini (ini-t-bytes-of "[Font]" *ini-t-crlf*
                                                 "japanese=-16,\"" *ini-t-font-cp932*
                                                 "\",128" *ini-t-crlf*))
            (let* ((r1 (eval-in-another-xyzzy
                        '(progn
                           (set-text-font :japanese
                                          :face (getf (get-text-font :japanese) :face)
                                          :size 15 :size-pixel-p nil)
                           (getf (get-text-font :japanese) :face))
                        :options "-q" :environ env))
                   (bytes (ini-t-get-bytes ini))
                   (found (and (search (ini-t-bytes-of "japanese=30,\""
                                                        *ini-t-font-cp932* "\",128")
                                       bytes)
                               t))
                   (r2 (multiple-value-list
                        (eval-in-another-xyzzy
                         '(let ((f (get-text-font :japanese)))
                            (values (getf f :face) (getf f :size)))
                         :options "-q" :environ env))))
              (list (equal r1 *ini-t-font-name*)
                    found
                    (equal (first r2) *ini-t-font-name*)
                    (second r2))))
        (when (file-exist-p ini)
          (delete-file ini)))))
  => (t t t 15))