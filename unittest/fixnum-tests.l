;;; -*- mode: lisp; package: user -*-
;;;
;;; fixnum is 62-bit on x64 (decision 2, plan section 4.3).
;;;
;;; The boundaries checked here: 2^29 and 2^31 (the old 30-bit immediate and
;;; the old 32-bit boxed fixnum), 2^32, 2^61 (the new fixnum range) and 2^63
;;; (the int64_t range used inside the C code).  Every expected value is the
;;; mathematical value, written out as a literal.
;;;
;;;   2^29 = 536870912
;;;   2^31 = 2147483648
;;;   2^32 = 4294967296
;;;   2^60 = 1152921504606846976
;;;   2^61 = 2305843009213693952
;;;   2^62 = 4611686018427387904
;;;   2^63 = 9223372036854775808
;;;   2^64 = 18446744073709551616

;;;; limits and type predicates

(deftest fixnum62-limits ()
  (values most-positive-fixnum
          most-negative-fixnum
          (si:*fixnump most-positive-fixnum)
          (si:*fixnump most-negative-fixnum))
  => 2305843009213693951
  => -2305843009213693952
  => t
  => t)

(deftest fixnum62-fixnump-boundaries ()
  (mapcar #'si:*fixnump
          '(536870911 536870912 -536870913
            2147483647 2147483648 -2147483649
            4294967295 4294967296 -4294967296
            2305843009213693951 2305843009213693952
            -2305843009213693952 -2305843009213693953
            9223372036854775807 9223372036854775808 -9223372036854775808))
  => (t t t
      t t t
      t t t
      t nil
      t nil
      nil nil nil))

(deftest fixnum62-bignump-boundaries ()
  (mapcar #'si:*bignump
          '(2147483648 4294967296
            2305843009213693951 2305843009213693952
            -2305843009213693952 -2305843009213693953))
  => (nil nil nil t nil t))

(deftest fixnum62-typep ()
  (list (typep 2147483648 'fixnum)
        (typep most-positive-fixnum 'fixnum)
        (typep most-negative-fixnum 'fixnum)
        (typep 2305843009213693952 'fixnum)
        (typep 2305843009213693952 'bignum)
        (typep 4294967296 'bignum)
        (typep 4294967296 '(integer 0 4294967296))
        (type-of 4294967296)
        (type-of 2305843009213693952))
  => (t t t nil t nil t integer integer))

(deftest fixnum62-limit-neighbours ()
  "1+ and 1- across the limits switch the representation"
  (let ((above (1+ most-positive-fixnum))
        (below (1- most-negative-fixnum)))
    (list above (si:*bignump above)
          below (si:*bignump below)
          (1- above) (si:*fixnump (1- above))
          (1+ below) (si:*fixnump (1+ below))))
  => (2305843009213693952 t
      -2305843009213693953 t
      2305843009213693951 t
      -2305843009213693952 t))

;;;; addition and subtraction

(deftest fixnum62-add ()
  (list (+ 2147483647 1)
        (+ 4294967295 1)
        (+ 1152921504606846976 1152921504606846975)
        (+ 1152921504606846976 1152921504606846976)
        (+ most-positive-fixnum most-positive-fixnum)
        (+ most-negative-fixnum most-negative-fixnum)
        (+ most-positive-fixnum most-negative-fixnum)
        (+ 2305843009213693952 -1)
        (+ -2305843009213693953 1))
  => (2147483648
      4294967296
      2305843009213693951
      2305843009213693952
      4611686018427387902
      -4611686018427387904
      -1
      2305843009213693951
      -2305843009213693952))

(deftest fixnum62-add-results-are-normalized ()
  (mapcar #'si:*fixnump
          (list (+ 1152921504606846976 1152921504606846975)
                (+ 2305843009213693952 -1)
                (+ -2305843009213693953 1)
                (+ 4611686018427387904 -4611686018427387904)))
  => (t t t t))

(deftest fixnum62-subtract ()
  (list (- -2147483648 1)
        (- most-negative-fixnum 1)
        (- most-positive-fixnum -1)
        (- 0 most-negative-fixnum)
        (- most-negative-fixnum most-positive-fixnum)
        (- 2305843009213693952 1))
  => (-2147483649
      -2305843009213693953
      2305843009213693952
      2305843009213693952
      -4611686018427387903
      2305843009213693951))

(deftest fixnum62-negate ()
  (list (- most-negative-fixnum)
        (si:*bignump (- most-negative-fixnum))
        (- most-positive-fixnum)
        (- 2305843009213693952)
        (si:*fixnump (- 2305843009213693952))
        (abs most-negative-fixnum)
        (- -2147483648))
  => (2305843009213693952 t
      -2305843009213693951
      -2305843009213693952 t
      2305843009213693952
      2147483648))

;;;; multiplication

(deftest fixnum62-multiply ()
  (list (* 65536 65536)
        (* 1073741824 1073741824)
        (* 1073741824 2147483648)
        (* 2147483648 2147483648)
        (* 3037000499 3037000499)
        (* 3037000500 3037000500)
        (* -3037000500 3037000500)
        (* 4294967296 4294967296)
        (* most-positive-fixnum -1)
        (* most-negative-fixnum -1)
        (* most-positive-fixnum most-positive-fixnum)
        (* most-negative-fixnum most-negative-fixnum)
        (* most-positive-fixnum most-negative-fixnum)
        (* -4611686018427387904 -2)
        (* 4611686018427387904 0))
  => (4294967296
      1152921504606846976
      2305843009213693952
      4611686018427387904
      9223372030926249001
      9223372037000250000
      -9223372037000250000
      18446744073709551616
      -2305843009213693951
      2305843009213693952
      5316911983139663487003542222693990401
      5316911983139663491615228241121378304
      -5316911983139663489309385231907684352
      9223372036854775808
      0))

(deftest fixnum62-multiply-results-representation ()
  (list (si:*fixnump (* 1073741824 1073741824))
        (si:*bignump (* 1073741824 2147483648))
        (si:*fixnump (* 4611686018427387904 0))
        (si:*fixnump (* 2305843009213693952 -1)))
  => (t t t t))

;;;; division

(deftest fixnum62-floor-of-most-negative-by-minus-one ()
  (floor most-negative-fixnum -1)
  => 2305843009213693952
  => 0)

(deftest fixnum62-truncate ()
  (truncate most-positive-fixnum 2)
  => 1152921504606846975
  => 1)

(deftest fixnum62-floor ()
  (floor most-negative-fixnum 3)
  => -768614336404564651
  => 1)

(deftest fixnum62-ceiling ()
  (ceiling most-positive-fixnum 2)
  => 1152921504606846976
  => -1)

(deftest fixnum62-round ()
  "round to even at .5"
  (round most-positive-fixnum 2)
  => 1152921504606846976
  => -1)

(deftest fixnum62-rem-mod ()
  (list (rem most-negative-fixnum 7)
        (mod most-negative-fixnum 7)
        (mod -1 4294967296)
        (rem 4294967297 4294967296)
        (truncate 2305843009213693952 -1))
  => (-2 5 4294967295 1 -2305843009213693952))

(deftest fixnum62-divide ()
  (list (/ 4294967296 2)
        (/ most-positive-fixnum most-positive-fixnum)
        (/ 6 4294967296)
        (/ most-negative-fixnum -1)
        (/ 4611686018427387904 2))
  => (2147483648 1 3/2147483648 2305843009213693952 2305843009213693952))

(deftest fixnum62-gcd-lcm-isqrt ()
  (list (gcd 4294967296 6442450944)
        (gcd most-negative-fixnum 6)
        (gcd most-negative-fixnum 0)
        (lcm 4294967296 3)
        (isqrt most-positive-fixnum)
        (isqrt 4294967296)
        (isqrt 4294967295))
  => (2147483648 2 2305843009213693952 12884901888 1518500249 65536 65535))

(deftest fixnum62-expt ()
  (list (expt 2 61)
        (si:*bignump (expt 2 61))
        (expt 2 60)
        (si:*fixnump (expt 2 60))
        (expt -2 61)
        (si:*fixnump (expt -2 61))
        (expt 1 4294967296)
        (expt -1 4294967297)
        (expt 0 4294967296))
  => (2305843009213693952 t 1152921504606846976 t
      -2305843009213693952 t 1 -1 0))

;;;; shifts and bit operations

(deftest fixnum62-ash-left ()
  (list (ash 1 32)
        (ash 1 60)
        (ash 1 61)
        (ash 1 63)
        (ash -1 61)
        (ash -1 62)
        (ash most-positive-fixnum 1)
        (ash 3 60))
  => (4294967296
      1152921504606846976
      2305843009213693952
      9223372036854775808
      -2305843009213693952
      -4611686018427387904
      4611686018427387902
      3458764513820540928))

(deftest fixnum62-ash-left-representation ()
  (mapcar #'si:*fixnump
          (list (ash 1 60) (ash 1 61) (ash -1 61) (ash -1 62)))
  => (t nil t nil))

(deftest fixnum62-ash-right ()
  (list (ash most-positive-fixnum -60)
        (ash most-negative-fixnum -61)
        (ash most-negative-fixnum -60)
        (ash 5 -32)
        (ash 5 -33)
        (ash 5 -64)
        (ash -5 -64)
        (ash 5 -100)
        (ash -5 -100)
        (ash 4294967296 -32)
        (ash 4611686018427387904 -1)
        (ash 4611686018427387904 -2)
        (ash 1 -4294967296)
        (ash -1 -4294967296))
  => (1 -1 -2 0 0 0 -1 0 -1 1 2305843009213693952 1152921504606846976 0 -1))

(deftest fixnum62-ash-overflow ()
  "a shift count beyond the range of long cannot make a representable number"
  (ash 1 4294967296)
  !! arithmetic-error)

(deftest fixnum62-logical ()
  (list (logand most-positive-fixnum -1)
        (logior 2147483648 1)
        (logxor most-positive-fixnum most-negative-fixnum)
        (lognot most-positive-fixnum)
        (lognot most-negative-fixnum)
        (logand 18446744073709551615 4294967295)
        (logior 4294967296 2305843009213693952)
        (logandc2 most-positive-fixnum 4294967295))
  => (2305843009213693951
      2147483649
      -1
      -2305843009213693952
      2305843009213693951
      4294967295
      2305843013508661248
      2305843004918726656))

(deftest fixnum62-logcount-logbitp-integer-length ()
  (list (logcount 4294967295)
        (logcount most-positive-fixnum)
        (logcount -4294967296)
        (logbitp 40 1099511627776)
        (logbitp 40 1)
        (logbitp 62 -1)
        (logbitp 100 -1)
        (logbitp 100 most-positive-fixnum)
        (logbitp 60 most-positive-fixnum)
        (logbitp 4294967296 -1)
        (logbitp 4294967296 1)
        (integer-length most-positive-fixnum)
        (integer-length most-negative-fixnum)
        (integer-length 4294967296)
        (integer-length 2305843009213693952))
  => (32 61 32 t nil t t nil t t nil 61 61 33 62))

;;;; comparison, eql, equal, hash

(deftest fixnum62-compare ()
  (list (< most-positive-fixnum 2305843009213693952)
        (> -2305843009213693953 most-negative-fixnum)
        (< 2147483647 2147483648 4294967296 2305843009213693951 2305843009213693952)
        (= 4294967296 4294967296.0d0)
        (max 4294967296 2147483648 -2305843009213693953)
        (min 4294967296 2147483648 -2305843009213693953)
        (/= most-positive-fixnum 2305843009213693952)
        (zerop (- 4294967296 (* 65536 65536)))
        (plusp most-positive-fixnum)
        (minusp most-negative-fixnum)
        (evenp most-negative-fixnum)
        (oddp most-positive-fixnum))
  => (t nil t t 4294967296 -2305843009213693953 t t t t t t))

(deftest fixnum62-eql-across-representations ()
  "values that reach the same number by different paths are eql"
  (list (eql 4294967296 (* 65536 65536))
        (eql (expt 2 40) (ash 1 40))
        (eql (1- (expt 2 61)) most-positive-fixnum)
        (eql (- (expt 2 61)) most-negative-fixnum)
        (eql (expt 2 61) (1+ most-positive-fixnum))
        (eql (/ (* 2305843009213693952 3) 3) (ash 1 61))
        (equal (list 4294967296 (ash 1 61)) (list (* 65536 65536) (expt 2 61)))
        (equalp most-positive-fixnum (1- (expt 2 61))))
  => (t t t t t t t t))

(deftest fixnum62-sxhash ()
  (list (= (sxhash 4294967296) (sxhash (* 65536 65536)))
        (= (sxhash most-positive-fixnum) (sxhash (1- (expt 2 61))))
        (= (sxhash most-negative-fixnum) (sxhash (- (expt 2 61))))
        (= (sxhash 2305843009213693952) (sxhash (ash 1 61)))
        (= (sxhash (list 1099511627776)) (sxhash (list (expt 2 40))))
        (every #'si:*fixnump
               (mapcar #'sxhash (list 0 -1 4294967296 most-positive-fixnum
                                      most-negative-fixnum (ash 1 61)))))
  => (t t t t t t))

(deftest fixnum62-hash-tables ()
  (let ((eqlh (make-hash-table :test 'eql))
        (equalh (make-hash-table :test 'equal))
        (equalph (make-hash-table :test 'equalp)))
    (dolist (h (list eqlh equalh equalph))
      (setf (gethash (expt 2 40) h) 'a)
      (setf (gethash (1- (expt 2 61)) h) 'b)
      (setf (gethash (expt 2 61) h) 'c)
      (setf (gethash (- (expt 2 61)) h) 'd))
    (mapcar #'(lambda (h)
                (list (gethash (ash 1 40) h)
                      (gethash most-positive-fixnum h)
                      (gethash (1+ most-positive-fixnum) h)
                      (gethash most-negative-fixnum h)
                      (hash-table-count h)))
            (list eqlh equalh equalph)))
  => ((a b c d 4) (a b c d 4) (a b c d 4)))

;;;; reading and printing

(deftest fixnum62-read ()
  (let ((xs (mapcar #'read-from-string
                    '("2305843009213693951" "2305843009213693952"
                      "-2305843009213693952" "-2305843009213693953"
                      "#x1fffffffffffffff" "#x2000000000000000"
                      "#x-2000000000000000" "#o77777777777777777777"
                      "#b1111111111111111111111111111111111111111111111111111111111111"
                      "4294967296." "+4294967296"))))
    (list xs (mapcar #'si:*fixnump xs)))
  => ((2305843009213693951 2305843009213693952
       -2305843009213693952 -2305843009213693953
       2305843009213693951 2305843009213693952
       -2305843009213693952 1152921504606846975
       2305843009213693951
       4294967296 4294967296)
      (t nil t nil t nil t t t t t)))

(deftest fixnum62-parse-integer ()
  (list (parse-integer "2305843009213693951")
        (si:*fixnump (parse-integer "2305843009213693951"))
        (parse-integer "-2305843009213693953")
        (parse-integer "1fffffffffffffff" :radix 16))
  => (2305843009213693951 t -2305843009213693953 2305843009213693951))

(deftest fixnum62-print ()
  (list (prin1-to-string most-positive-fixnum)
        (prin1-to-string most-negative-fixnum)
        (prin1-to-string 4294967296)
        (princ-to-string (list 4294967296 -4294967296))
        (format nil "~D" most-positive-fixnum)
        (format nil "~X" most-positive-fixnum)
        (format nil "~O" most-negative-fixnum)
        (format nil "~B" most-positive-fixnum)
        (format nil "~36R" most-positive-fixnum)
        (format nil "~:D" most-positive-fixnum)
        (format nil "~@D" 4294967296)
        (format nil "~25D" most-negative-fixnum)
        (write-to-string most-positive-fixnum :base 16 :radix t)
        (write-to-string most-positive-fixnum :base 10 :radix t)
        (write-to-string most-negative-fixnum :base 2 :radix t)
        (let ((*print-base* 16)) (prin1-to-string most-negative-fixnum)))
  => ("2305843009213693951"
      "-2305843009213693952"
      "4294967296"
      "(4294967296 -4294967296)"
      "2305843009213693951"
      "1fffffffffffffff"
      "-200000000000000000000"
      "1111111111111111111111111111111111111111111111111111111111111"
      "hio94mrrlk3j"
      "2,305,843,009,213,693,951"
      "+4294967296"
      "     -2305843009213693952"
      "#x1FFFFFFFFFFFFFFF"
      "2305843009213693951."
      "#b-10000000000000000000000000000000000000000000000000000000000000"
      "-2000000000000000"))

(deftest fixnum62-format-radix-words ()
  "~R prints large integers in digits, as it did when they were bignums"
  (list (format nil "~R" 1000000000000)
        (format nil "~R" 2147483648)
        (format nil "~@R" 4294967296)
        (format nil "~R" 12))
  => ("1000000000000" "2147483648" "4294967296" "twelve"))

(deftest fixnum62-print-read-roundtrip ()
  (every #'(lambda (x)
             (let ((y (read-from-string (prin1-to-string x))))
               (and (eql x y)
                    (eq (si:*fixnump x) (si:*fixnump y)))))
         (list 0 1 -1 536870911 536870912 2147483647 2147483648 -2147483649
               4294967296 most-positive-fixnum most-negative-fixnum
               (1+ most-positive-fixnum) (1- most-negative-fixnum)
               9223372036854775807 -9223372036854775808))
  => t)

;;;; conversion to and from floats

(deftest fixnum62-float-conversion ()
  (list (float 4294967296 1d0)
        (= (float most-positive-fixnum 1d0) (float (expt 2 61) 1d0))
        (truncate 2.0d18)
        (si:*fixnump (truncate 2.0d18))
        (truncate 4.0d18)
        (si:*bignump (truncate 4.0d18))
        (floor -2.0d18)
        (rational 4294967296.0d0)
        (si:*fixnump (rational 4294967296.0d0))
        (+ 4294967296 0.5d0))
  => (4.294967296d9 t 2000000000000000000 t 4000000000000000000 t
      -2000000000000000000 4294967296 t 4.2949672965d9))

;;;; random

(deftest fixnum62-random-range ()
  "random of a wide fixnum stays in [0, n) and uses the high bits"
  (let ((ok t)
        (wide nil))
    (dolist (n (list 2147483647 4294967296 1099511627776 most-positive-fixnum))
      (dotimes (i 50)
        (let ((r (random n)))
          (unless (and (integerp r) (<= 0 r) (< r n))
            (setq ok nil)))))
    (dotimes (i 50)
      (when (> (random 1099511627776) 4294967296)
        (setq wide t)))
    (list ok wide))
  => (t t))

;;;; integers given to C functions that take a long

(deftest fixnum62-index-out-of-long-range-substring ()
  (substring "abc" 4294967297)
  !! range-error)

(deftest fixnum62-index-out-of-long-range-nth ()
  (nth 4294967296 '(a b))
  !! range-error)

(deftest fixnum62-index-out-of-long-range-elt ()
  (elt "abc" 4294967296)
  !! range-error)

(deftest fixnum62-index-out-of-long-range-svref ()
  (svref (vector 1 2) 4294967296)
  !! range-error)

(deftest fixnum62-code-char-out-of-long-range ()
  (code-char 4294967296)
  !! range-error)

(deftest fixnum62-make-vector-out-of-long-range ()
  (make-vector 4294967296)
  !! range-error)

(deftest fixnum62-scale-float-out-of-long-range ()
  (scale-float 1.0 4294967296)
  !! range-error)

(deftest fixnum62-goto-char-out-of-long-range ()
  "goto-char does not wrap 4294967297 to 1; point stays where it was"
  (let ((b (create-new-buffer "*fixnum62-test*")))
    (unwind-protect
        (save-excursion
          (set-buffer b)
          (insert "abcdef")
          (goto-char 3)
          (list (handler-case (progn (goto-char 4294967297) :moved)
                  (type-error () :type-error)
                  (error () :error))
                (point)
                (progn (goto-char 2147483647) (point))))
      (delete-buffer b)))
  => (:type-error 3 6))

;;;; byte-compiled code

(deftest fixnum62-compiled-arithmetic ()
  (let ((f (compile nil '(lambda (a b)
                           (list (+ a b) (- a b) (* a b) (1+ a) (1- b) (- a)
                                 (< a b) (= a b))))))
    (list (compiled-function-p f)
          (funcall f most-positive-fixnum 1)
          (funcall f 3037000500 3037000500)
          (funcall f most-negative-fixnum -1)))
  => (t
      (2305843009213693952 2305843009213693950 2305843009213693951
       2305843009213693952 0 -2305843009213693951 nil nil)
      (6074001000 0 9223372037000250000 3037000501 3037000499
       -3037000500 nil t)
      (-2305843009213693953 -2305843009213693951 2305843009213693952
       -2305843009213693951 -2 2305843009213693952 t nil)))

(deftest fixnum62-compile-file-constants ()
  "62-bit constants survive compile-file (written to .lc and read back)"
  (let ((src (make-temp-file-name "fx62" "l"))
        (lc nil))
    (unwind-protect
        (progn
          (with-open-file (s src :direction :output :if-exists :supersede)
            (format s "(defun fixnum62-test-from-lc (x) (list (+ x 2305843009213693951) (* x 4294967296) -2305843009213693952 2305843009213693952))~%"))
          (let ((*standard-output* (make-string-output-stream)))
            (compile-file src)
            (setq lc (compile-file-pathname src))
            (load lc))
          (list (compiled-function-p #'fixnum62-test-from-lc)
                (fixnum62-test-from-lc 1)
                (mapcar #'si:*fixnump (fixnum62-test-from-lc 0))))
      (when (file-exist-p src)
        (delete-file src))
      (when (and lc (file-exist-p lc))
        (delete-file lc))))
  => (t
      (2305843009213693952 4294967296 -2305843009213693952 2305843009213693952)
      (t t t nil)))

;;;; chunks (the integer conversions that the foreign interface uses)

(deftest fixnum62-chunk-int64-roundtrip ()
  (let ((c (si:make-chunk nil 8)))
    (mapcar #'(lambda (x)
                (si:pack-int64 c 0 x)
                (let ((y (si:unpack-int64 c 0)))
                  (list y (si:*fixnump y))))
            (list 1099511627776 -1099511627776 most-positive-fixnum
                  most-negative-fixnum 4611686018427387904
                  9223372036854775807 -9223372036854775808)))
  => ((1099511627776 t) (-1099511627776 t) (2305843009213693951 t)
      (-2305843009213693952 t) (4611686018427387904 nil)
      (9223372036854775807 nil) (-9223372036854775808 nil)))

(deftest fixnum62-chunk-unsigned-roundtrip ()
  (let ((c (si:make-chunk nil 8)))
    (list (progn (si:pack-uint64 c 0 18446744073709551615) (si:unpack-uint64 c 0))
          (progn (si:pack-uint64 c 0 2305843009213693951) (si:unpack-uint64 c 0))
          (progn (si:pack-int32 c 0 -1) (si:unpack-uint32 c 0))
          (si:*fixnump (si:unpack-uint32 c 0))
          (progn (si:pack-uint32 c 0 4294967295) (si:unpack-int32 c 0))))
  => (18446744073709551615 2305843009213693951 4294967295 t -1))
