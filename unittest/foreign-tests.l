(require "foreign")


;; int32

(c:define-dll-entry
  c:int
  (_atoi :convention :cdecl) ((c:void *))
  "msvcrt" "atoi")

(deftest define-dll-entry-return-int32 ()
  (flet ((atoi (str)
           (_atoi (si:make-string-chunk str))))
    (values
     (atoi "0")
     (atoi "1")
     (atoi "-1")
     (atoi "2147483647")))
  => 0
  => 1
  => -1
  => #x7FFFFFFF)


(c:define-dll-entry
  (c:void *)
  (_itoa :convention :cdecl) (c:int (c:void *) c:int)
  "msvcrt")

(deftest define-dll-entry-int32-arg ()
  (flet ((itoa (n)
           (let ((c (si:make-chunk nil 30)))
             (_itoa n c 10)
             (si:unpack-string c 0))))
    (values
     (itoa 0)
     (itoa 1)
     (itoa -1)
     (itoa #x7FFFFFFF)))
  => "0"
  => "1"
  => "-1"
  => "2147483647")


;; uint32

(c:define-dll-entry
  c:u_int
  (strtoul :convention :cdecl) ((c:void *) (c:void *) c:int)
  "msvcrt")

(deftest define-dll-entry-return-uint32 ()
  (flet ((atoi (str)
           (strtoul (si:make-string-chunk str) 0 10)))
    (values
     (atoi "0")
     (atoi "1")
     (atoi "-1")
     (atoi "2147483647")
     (atoi "4294967295")))
  => 0
  => 1
  => #xFFFFFFFF
  => #x7FFFFFFF
  => #xFFFFFFFF)

(c:define-dll-entry
  (c:void *)
  (_ultoa :convention :cdecl) (c:u_int (c:void *) c:int)
  "msvcrt")

(deftest define-dll-entry-uint32-arg ()
  (flet ((itoa (n)
           (let ((c (si:make-chunk nil 30)))
             (_ultoa n c 10)
             (si:unpack-string c 0))))
    (values
     (itoa 0)
     (itoa 1)
     (itoa -1)
     (itoa #x7FFFFFFF)
     (itoa #xFFFFFFFF)))
  => "0"
  => "1"
  => "4294967295"
  => "2147483647"
  => "4294967295")


;; int64

(c:define-dll-entry
  c:int64
  (_atoi64 :convention :cdecl) ((c:void *))
  "msvcrt")

(deftest define-dll-entry-return-int64 ()
  (flet ((atoi (str)
           (_atoi64 (si:make-string-chunk str))))
    (values
     (atoi "0")
     (atoi "1")
     (atoi "-1")
     (atoi "2147483647")
     (atoi "4294967295")
     (atoi "9223372036854775807")))
  => 0
  => 1
  => -1
  => #x7FFFFFFF
  => #xFFFFFFFF
  => #x7FFFFFFFFFFFFFFF)


(c:define-dll-entry
  (c:void *)
  (_i64toa :convention :cdecl) (c:int64 (c:void *) c:int)
  "msvcrt")

(deftest define-dll-entry-int64-arg ()
  (flet ((itoa (n)
           (let ((c (si:make-chunk nil 30)))
             (_i64toa n c 10)
             (si:unpack-string c 0))))
    (values
     (itoa 0)
     (itoa 1)
     (itoa -1)
     (itoa #x7FFFFFFF)
     (itoa #xFFFFFFFF)
     (itoa #x7FFFFFFFFFFFFFFF)))
  => "0"
  => "1"
  => "-1"
  => "2147483647"
  => "4294967295"
  => "9223372036854775807")


;; u_int64

(c:define-dll-entry
  c:u_int64
  (_strtoui64 :convention :cdecl) ((c:void *) (c:void *) c:int)
  "msvcrt")

(deftest define-dll-entry-return-uint64 ()
  (flet ((atoi (str)
           (_strtoui64 (si:make-string-chunk str) 0 10)))
    (values
     (atoi "0")
     (atoi "1")
     (atoi "-1")
     (atoi "2147483647")
     (atoi "4294967295")
     (atoi "9223372036854775807")
     (atoi "18446744073709551615")))
  => 0
  => 1
  => #xFFFFFFFFFFFFFFFF
  => #x7FFFFFFF
  => #xFFFFFFFF
  => #x7FFFFFFFFFFFFFFF
  => #xFFFFFFFFFFFFFFFF)


(c:define-dll-entry
  (c:void *)
  (_ui64toa :convention :cdecl) (c:u_int64 (c:void *) c:int)
  "msvcrt")

(deftest define-dll-entry-uint64-arg ()
  (flet ((itoa (n)
           (let ((c (si:make-chunk nil 30)))
             (_ui64toa n c 10)
             (si:unpack-string c 0))))
    (values
     (itoa 0)
     (itoa 1)
     (itoa -1)
     (itoa #x7FFFFFFF)
     (itoa #xFFFFFFFF)
     (itoa #x7FFFFFFFFFFFFFFF)
     (itoa #xFFFFFFFFFFFFFFFF)))
  => "0"
  => "1"
  => "18446744073709551615"
  => "2147483647"
  => "4294967295"
  => "9223372036854775807"
  => "18446744073709551615")


;; c:last-win32-error

(c:define-dll-entry
  c:int
  MultiByteToWideChar (c:u_int         ; code page
                       c:u_long        ; character-type options
                       (c:void *)      ; string to map
                       c:int           ; number of bytes in string
                       (c:void *)      ; wide-character buffer
                       c:int)          ; size of buffer
  "kernel32")

(deftest last-win32-error ()
  (let ((a (si:make-string-chunk "abc"))
        (u (si:make-chunk nil 9)))
    (si:clear-chunk u)
    (values
     (progn
       (setf (c:last-win32-error) 0)
       (MultiByteToWideChar 932 0
                            a (si:chunk-size a)
                            u (si:chunk-size u))
       (c:last-win32-error))
     (progn
       (setf (c:last-win32-error) 0)
       (MultiByteToWideChar 932932 0
                            a (si:chunk-size a)
                            u (si:chunk-size u))
       (c:last-win32-error))
     (progn
       (setf (c:last-win32-error) 0)
       (MultiByteToWideChar 932 0
                            a (si:chunk-size a)
                            u (si:chunk-size u))
       (c:last-win32-error))
     (progn
       (setf u (si:make-chunk nil 1))
       (setf (c:last-win32-error) 0)
       (MultiByteToWideChar 932 0
                            a (si:chunk-size a)
                            u (si:chunk-size u))
       (c:last-win32-error))
     ))
  => 0
  => 87   ; ERROR_INVALID_PARAMETER
  => 0
  => 122) ; ERROR_INSUFFICIENT_BUFFER

(c:define-dll-entry
  c:int
  GetLastError ()
  "kernel32")

(deftest last-win32-error-GetLastError ()
  (let ((a (si:make-string-chunk "abc"))
        (u (si:make-chunk nil 9)))
    (si:clear-chunk u)
    (setf (c:last-win32-error) 0)
    (MultiByteToWideChar 932932 0
                         a (si:chunk-size a)
                         u (si:chunk-size u))
    (values
     (c:last-win32-error)
     (GetLastError)))
  => 87   ; ERROR_INVALID_PARAMETER
  => 87)  ; ERROR_INVALID_PARAMETER


(c:define-dll-entry
  c:u_int64 ; ほんとは div_t だが構造体の戻り値は未サポート
  (div :convention :cdecl)
  (c:int c:int)
  "msvcrt")

(deftest handle-divide-by-zero ()
  (flet ((unpack-div_t (div_t)
           (list (logand #xFFFF div_t)
                 (logand #xFFFF (ash div_t -32)))))
    (values
     (unpack-div_t (div 10 3))
     (handler-case
         (div 10 0)
       (win32-exception (e)
         (list (win32-exception-description e)
               (win32-exception-code e))))))
  => (3 1)
  => ("Integer divide by zero" #xc0000094))


(c:define-dll-entry
  (c:char *)
  (strcat :convention :cdecl)
  ((c:char *) (c:char *))
  "msvcrt")

(deftest handle-access-violation ()
  (let ((c (si:make-chunk nil 5)))
    (si:clear-chunk c)
    (strcat c (si:make-string-chunk "hoge"))
    (values
     (si:unpack-string c 0)
     (handler-case
         (strcat 0 (si:make-string-chunk "fuga"))
       (win32-exception (e)
         (list (win32-exception-description e)
               (win32-exception-code e))))))
  => "hoge"
  => ("Access violation" #xc0000005))


(c:define-dll-entry
  c:int
  GetModuleHandleA ((c:void *))
  "kernel32")

(deftest handle-access-violation-2 ()
  (handler-case
      (GetModuleHandleA 32)
    (win32-exception (e)
      (list (win32-exception-description e)
            (win32-exception-code e))))
  => ("Access violation" #xc0000005))


(c:define-dll-entry
  c:int
  (sprintf :convention :cdecl)
  ((c:char *) (c:char *) &rest)
  "msvcrt")


(deftest expand-c-vaargs ()
  (equal (c:expand-c-vaargs '((c:int 123)
                              (c:double 1.23)
                              (c:string 0)))
         (list (list :int32 123)
               (list :double 1.23)
               (list :uint64 0)))
  => t)

(deftest vaargs-macro ()
  (equal (c:c-vaargs (c:int 123)
                     (c:double 1.23)
                     (c:string 0))
         (list (list :int32 123)
               (list :double 1.23)
               (list :uint64 0)))
  => t)

(c:define-c-struct INVALID_VAARGS
  (c:int a))

(deftest expand-c-vaargs-struct ()
  (handler-case
      (c:expand-c-vaargs `((INVALID_VAARGS 123)))
    (error (c)
      (si:*condition-string c)))
  => "不正な可変長引数です: ((INVALID_VAARGS 123))")

(deftest funcall-expand-vaargs ()
  (let ((b (si:make-chunk nil 200)))
    (values
     (sprintf b (si:make-string-chunk "foo: %d, %.3f, %s")
              (c:expand-c-vaargs `((c:int 123)
                                   (c:double 1.23)
                                   (c:string ,(si:make-string-chunk "bar")))))
     (si:unpack-string b 0)))
  => 20
  => "foo: 123, 1.230, bar")

(deftest funcall-vaargs ()
  (let ((b (si:make-chunk nil 200)))
    (values
     (sprintf b (si:make-string-chunk "foo: %d, %.3f, %s")
              (c:c-vaargs (c:int 123)
                        (c:double 1.23)
                        (c:string (si:make-string-chunk "bar"))))
     (si:unpack-string b 0)))
  => 20
  => "foo: 123, 1.230, bar")

(deftest funcall-vaargs-2 ()
  (let ((b (si:make-chunk nil 200)))
    (values
     (sprintf b (si:make-string-chunk "foo: %d, %.3f, %s")
              (append
               (c:c-vaargs (c:int 123))
               (c:c-vaargs (c:double 1.23)
                         (c:string (si:make-string-chunk "bar")))))
     (si:unpack-string b 0)))
  => 20
  => "foo: 123, 1.230, bar")

(deftest funcall-vaargs-default-argument-promotions ()
  (let ((b (si:make-chunk nil 200)))
    (values
     (sprintf b (si:make-string-chunk "foo: %d, %d, %.3f")
              (c:c-vaargs (c:char 123)
                        (c:short 123)
                        (c:float 1.23)))
     (si:unpack-string b 0)))
  => 20
  => "foo: 123, 123, 1.230")

(deftest funcall-vaargs-noarg ()
  (let ((b (si:make-chunk nil 200)))
    (values
     (sprintf b (si:make-string-chunk "foo"))
     (si:unpack-string b 0)
     (sprintf b (si:make-string-chunk "foobar") nil)
     (si:unpack-string b 0)))
  => 3
  => "foo"
  => 6
  => "foobar")

(deftest c-vaargs-invalid-vaargs-0 ()
  (handler-case
      (let ((b (si:make-chunk nil 200)))
        (sprintf b (si:make-string-chunk "foo: %d, %.3f, %d")
                 (append
                  (c:c-vaargs (c:int 123))
                  (c:c-vaargs (c:double 1.23))
                  (list 123))))
    (error (c)
      (si:*condition-string c)))
  => "不正な可変長引数です: (123)")

(deftest c-vaargs-invalid-vaargs-1 ()
  (handler-case
      (c:c-vaargs 123)
    (error (c)
      (si:*condition-string c)))
  => "不正な可変長引数です: (123)")

(deftest c-vaargs-invalid-vaargs-2 ()
  (handler-case
      (c:c-vaargs (123))
    (error (c)
      (si:*condition-string c)))
  => "不正な可変長引数です: ((123))")

(deftest c-vaargs-invalid-vaargs-3 ()
  (handler-case
      (c:c-vaargs :int32 123)
    (error (c)
      (si:*condition-string c)))
  => "不正な可変長引数です: (:int32 123)")

(deftest c-vaargs-invalid-vaargs-4 ()
  (handler-case
      (c:c-vaargs (:int32))
    (error (c)
      (si:*condition-string c)))
  => "不正な可変長引数です: ((:int32))")

(deftest c-vaargs-invalid-vaargs-5 ()
  (handler-case
      (c:c-vaargs (:int32 123 234))
    (error (c)
      (si:*condition-string c)))
  => "不正な可変長引数です: ((:int32 123 234))")

(deftest c-vaargs-invalid-vaargs-6 ()
  (handler-case
      (c:c-vaargs nil (:int32 123))
    (error (c)
      (si:*condition-string c)))
  => "不正な可変長引数です: (nil (:int32 123))")

(deftest c-vaargs-invalid-vaargs-7 ()
  (handler-case
      (c:c-vaargs (:void 123))
    (error (c)
      (si:*condition-string c)))
  => "不正な可変長引数です: ((:void 123))")


(deftest funcall-invalid-vaargs-1 ()
  (handler-case
      (sprintf 0 0 123)
    (error (c)
      (si:*condition-string c)))
  => "不正な可変長引数です: 123")

(deftest funcall-invalid-vaargs-2 ()
  (handler-case
      (sprintf 0 0 '(123))
    (error (c)
      (si:*condition-string c)))
  => "不正な可変長引数です: (123)")

(deftest funcall-invalid-vaargs-3 ()
  (handler-case
      (sprintf 0 0 '(:int32 123))
    (error (c)
      (si:*condition-string c)))
  => "不正な可変長引数です: (:int32 123)")

(deftest funcall-invalid-vaargs-4 ()
  (handler-case
      (sprintf 0 0 '((:int32)))
    (error (c)
      (si:*condition-string c)))
  => "不正な可変長引数です: ((:int32))")

(deftest funcall-invalid-vaargs-5 ()
  (handler-case
      (sprintf 0 0 '((:int32 123 234)))
    (error (c)
      (si:*condition-string c)))
  => "不正な可変長引数です: ((:int32 123 234))")

(deftest funcall-invalid-vaargs-6 ()
  (handler-case
      (sprintf 0 0 '(nil (:int32 123)))
    (error (c)
      (si:*condition-string c)))
  => "不正な可変長引数です: (nil (:int32 123))")

(deftest funcall-invalid-vaargs-7 ()
  (handler-case
      (sprintf 0 0 '((:void 123)))
    (error (c)
      (si:*condition-string c)))
  => "不正な可変長引数です: ((:void 123))")


(deftest make-c-function-stdcall-rest ()
  (handler-case
      (si:make-c-function (si:load-dll-module "msvcrt")
                          "sprintf"
                          '(:int32 :int32 :int32)
                          :int32
                          :vaarg t :convention :stdcall)
    (error (c)
      (si:*condition-string c)))
  => "stdcall呼び出し規約の場合、可変長引数は利用できません")

(deftest define-dll-entry-stdcall-rest ()
  (handler-case
      (c:define-dll-entry
        c:int
        sprintf_stdcall ((c:char *) (c:char *) &rest)
        "msvcrt" "sprintf")
    (error (c)
      (si:*condition-string c)))
  => "stdcall呼び出し規約の場合、可変長引数は利用できません")


(c:define-dll-entry
  c:int
  (sprintf_no_vaarg :convention :cdecl)
  ((c:char *) (c:char *))
  "msvcrt" "sprintf")

(deftest funcall-invalid-c-args ()
  (sprintf_no_vaarg 0 0 '((:int32 123)))
  !! too-many-arguments)


(deftest c-size-of-premitive ()
  (values
   (c:c-size-of :void)
   (c:c-size-of :int8)
   (c:c-size-of :uint8)
   (c:c-size-of :int16)
   (c:c-size-of :uint16)
   (c:c-size-of :int32)
   (c:c-size-of :uint32)
   (c:c-size-of :int64)
   (c:c-size-of :uint64)
   (c:c-size-of :float)
   (c:c-size-of :double))
  => 0
  => 1
  => 1
  => 2
  => 2
  => 4
  => 4
  => 8
  => 8
  => 4
  => 8)

(deftest c-size-of-define-c-type ()
  (values
   (c:c-size-of c:void)
   (c:c-size-of c:char)
   (c:c-size-of c:short)
   (c:c-size-of c:int)
   (c:c-size-of c:long)
   (c:c-size-of c:int64)
   (c:c-size-of c:u_char)
   (c:c-size-of c:u_short)
   (c:c-size-of c:u_int)
   (c:c-size-of c:u_long)
   (c:c-size-of c:u_int64)
   (c:c-size-of c:float)
   (c:c-size-of c:double)

   (c:c-size-of c:wchar_t)
   (c:c-size-of c:size_t)
   (c:c-size-of c:time_t)
   (c:c-size-of c:ptrdiff_t)
   (c:c-size-of c:clock_t)
   (c:c-size-of c:off_t))
  => 0
  => 1
  => 2
  => 4
  => 4
  => 8
  => 1
  => 2
  => 4
  => 4
  => 8
  => 4
  => 8
  => 2
  => 8
  => 8
  => 8
  => 4
  => 4)

(deftest c-size-of-pointer ()
  (let ((size 8))
    (values
     (= (c:c-size-of c:void*) size)
     (= (c:c-size-of c:char*) size)
     (= (c:c-size-of c:string) size)))
  => t
  => t
  => t)


(c:define-c-struct c-size-of-test
  (c:int a)
  (c:double a)
  (c:char a))

(deftest c-size-of-struct ()
  (values
   (c:c-size-of c-size-of-test)
   (c:c-struct-size-of c-size-of-test))
  => 24
  => 24)


;; BUG-0028: size_t, int64, ptrdiff_t and time_t are 8 bytes on x64.
;; Values above 32 bits must survive the argument and the return value.
;; _abs64 (msvcrt) takes and returns a 64-bit integer.

(deftest c-size-of-64bit-types ()
  (values
   (c:c-size-of c:size_t)
   (c:c-size-of c:int64)
   (c:c-size-of c:ptrdiff_t)
   (c:c-size-of c:time_t))
  => 8
  => 8
  => 8
  => 8)

(c:define-dll-entry
  c:size_t
  (abs64-size :convention :cdecl) (c:size_t)
  "msvcrt" "_abs64")

(c:define-dll-entry
  c:int64
  (abs64-int64 :convention :cdecl) (c:int64)
  "msvcrt" "_abs64")

(c:define-dll-entry
  c:ptrdiff_t
  (abs64-ptrdiff :convention :cdecl) (c:ptrdiff_t)
  "msvcrt" "_abs64")

(c:define-dll-entry
  c:time_t
  (abs64-time :convention :cdecl) (c:time_t)
  "msvcrt" "_abs64")

(c:define-dll-entry
  c:size_t
  (strtoui64-size :convention :cdecl) ((c:void *) (c:void *) c:int)
  "msvcrt" "_strtoui64")

(deftest define-dll-entry-size_t-roundtrip ()
  (values
   (abs64-size 0)
   (abs64-size 1)
   (abs64-size #xFFFFFFFF)
   (abs64-size #x100000000)
   (abs64-size 4294967297)
   (abs64-size (expt 2 40))
   (abs64-size #x7FFFFFFFFFFFFFFF))
  => 0
  => 1
  => #xFFFFFFFF
  => #x100000000
  => 4294967297
  => #x10000000000
  => #x7FFFFFFFFFFFFFFF)

(deftest define-dll-entry-size_t-return-above-int64 ()
  (strtoui64-size (si:make-string-chunk "18446744073709551615") 0 10)
  => #xFFFFFFFFFFFFFFFF)

(deftest define-dll-entry-int64-roundtrip ()
  (values
   (abs64-int64 -4294967297)
   (abs64-int64 4294967297)
   (abs64-int64 (- (expt 2 40)))
   (abs64-int64 (expt 2 40))
   (abs64-int64 -2147483649)
   (abs64-int64 -1))
  => 4294967297
  => 4294967297
  => #x10000000000
  => #x10000000000
  => 2147483649
  => 1)

(deftest define-dll-entry-ptrdiff_t-roundtrip ()
  (values
   (abs64-ptrdiff 2147483648)
   (abs64-ptrdiff -2147483649)
   (abs64-ptrdiff -4294967297)
   (abs64-ptrdiff #x100000000))
  => 2147483648
  => 2147483649
  => 4294967297
  => #x100000000)

(deftest define-dll-entry-time_t-roundtrip ()
  (values
   (abs64-time 2147483648)
   (abs64-time -2147483649)
   (abs64-time -4294967297)
   (abs64-time #x100000000))
  => 2147483648
  => 2147483649
  => 4294967297
  => #x100000000)


;; BUG-0031: define-c-struct lays members out by the C (MSVC x64) rules.
;; Expected values are measured with MSVC x64 (sizeof / offsetof / alignof),
;; not computed from the Lisp rules.

(c:define-c-struct ctest-rect
  (c:long left)
  (c:long top)
  (c:long right)
  (c:long bottom))

;; { int a; double b; char c; }
(c:define-c-struct ctest-s1
  (c:int a)
  (c:double b)
  (c:char c))

;; { char c; char *p; char d; }
(c:define-c-struct ctest-s2
  (c:char c)
  (c:char * p)
  (c:char d))

;; { char c; RECT r; char d; }  -- RECT is 16 bytes, aligned to 4
(c:define-c-struct ctest-n1
  (c:char c)
  (ctest-rect r)
  (c:char d))

;; { RECT r; double d; char c; }
(c:define-c-struct ctest-n2
  (ctest-rect r)
  (c:double d)
  (c:char c))

;; { short s; N1 n; char e; }  -- struct in struct in struct
(c:define-c-struct ctest-nn
  (c:short s)
  (ctest-n1 n)
  (c:char e))

;; { char c; short a[3]; char d; }
(c:define-c-struct ctest-a1
  (c:char c)
  (c:short a 3)
  (c:char d))

;; { char c; RECT r[2]; char d; }
(c:define-c-struct ctest-a2
  (c:char c)
  (ctest-rect r 2)
  (c:char d))

;; { char c; double d[2]; }
(c:define-c-struct ctest-a3
  (c:char c)
  (c:double d 2))

;; { char c; __int64 q; }
(c:define-c-struct ctest-q1
  (c:char c)
  (c:int64 q))

;; { char c; float f; short s; }
(c:define-c-struct ctest-f1
  (c:char c)
  (c:float f)
  (c:short s))

;; PAINTSTRUCT
(c:define-c-struct ctest-paintstruct
  (c:void* hdc)
  (c:int fErase)
  (ctest-rect rcPaint)
  (c:int fRestore)
  (c:int fIncUpdate)
  (c:u_char rgbReserved 32))

(deftest c-struct-layout-scalar ()
  (values
   (c:c-struct-size-of ctest-s1)
   (c:c-struct-offset-of ctest-s1 a)
   (c:c-struct-offset-of ctest-s1 b)
   (c:c-struct-offset-of ctest-s1 c)
   (c:c-struct-size-of ctest-s2)
   (c:c-struct-offset-of ctest-s2 c)
   (c:c-struct-offset-of ctest-s2 p)
   (c:c-struct-offset-of ctest-s2 d)
   (c:c-struct-size-of ctest-q1)
   (c:c-struct-offset-of ctest-q1 q)
   (c:c-struct-size-of ctest-f1)
   (c:c-struct-offset-of ctest-f1 f)
   (c:c-struct-offset-of ctest-f1 s))
  => 24
  => 0
  => 8
  => 16
  => 24
  => 0
  => 8
  => 16
  => 16
  => 8
  => 12
  => 4
  => 8)

(deftest c-struct-layout-nested ()
  (values
   (c:c-struct-size-of ctest-n1)
   (c:c-struct-offset-of ctest-n1 r)
   (c:c-struct-offset-of ctest-n1 d)
   (c:c-struct-size-of ctest-n2)
   (c:c-struct-offset-of ctest-n2 d)
   (c:c-struct-offset-of ctest-n2 c)
   (c:c-struct-size-of ctest-nn)
   (c:c-struct-offset-of ctest-nn n)
   (c:c-struct-offset-of ctest-nn e))
  => 24
  => 4
  => 20
  => 32
  => 16
  => 24
  => 32
  => 4
  => 28)

(deftest c-struct-layout-array-member ()
  (values
   (c:c-struct-size-of ctest-a1)
   (c:c-struct-offset-of ctest-a1 a)
   (c:c-struct-offset-of ctest-a1 d)
   (c:c-struct-size-of ctest-a2)
   (c:c-struct-offset-of ctest-a2 r)
   (c:c-struct-offset-of ctest-a2 d)
   (c:c-struct-size-of ctest-a3)
   (c:c-struct-offset-of ctest-a3 d))
  => 10
  => 2
  => 8
  => 40
  => 4
  => 36
  => 24
  => 8)

(deftest c-struct-layout-paintstruct ()
  (values
   (c:c-struct-size-of ctest-paintstruct)
   (c:c-struct-offset-of ctest-paintstruct hdc)
   (c:c-struct-offset-of ctest-paintstruct fErase)
   (c:c-struct-offset-of ctest-paintstruct rcPaint)
   (c:c-struct-offset-of ctest-paintstruct fRestore)
   (c:c-struct-offset-of ctest-paintstruct fIncUpdate)
   (c:c-struct-offset-of ctest-paintstruct rgbReserved))
  => 72
  => 0
  => 8
  => 12
  => 28
  => 32
  => 36)

;; the generated accessors read and write at those offsets
(deftest c-struct-accessor-paintstruct ()
  (let ((ps (make-ctest-paintstruct)))
    (setf (ctest-paintstruct-fRestore ps) 7)
    (setf (ctest-paintstruct-fIncUpdate ps) 9)
    (setf (ctest-rect-bottom (ctest-paintstruct-rcPaint ps)) 5)
    (values
     (si:unpack-int32 ps 28)
     (si:unpack-int32 ps 32)
     (si:unpack-int32 ps (+ 12 12))
     (si:chunk-size ps)))
  => 7
  => 9
  => 5
  => 72)

;; *c-structure-packing-align* (#pragma pack) limits the alignment of the
;; members and of the struct itself.
(deftest c-struct-layout-pack ()
  (let ((c:*c-structure-packing-align* 4))
    ;; #pragma pack(4) { char c; double d; char e; }
    (eval '(c:define-c-struct ctest-p4a
            (c:char c)
            (c:double d)
            (c:char e)))
    ;; #pragma pack(4) { char c; RECT r; char *p; }
    (eval '(c:define-c-struct ctest-p4b
            (c:char c)
            (ctest-rect r)
            (c:char * p))))
  (let ((c:*c-structure-packing-align* 1))
    ;; #pragma pack(1) { char c; int i; double d; }
    (eval '(c:define-c-struct ctest-p1a
            (c:char c)
            (c:int i)
            (c:double d))))
  (values
   (c:c-struct-size-of ctest-p4a)
   (c:c-struct-offset-of ctest-p4a d)
   (c:c-struct-offset-of ctest-p4a e)
   (c:c-struct-size-of ctest-p4b)
   (c:c-struct-offset-of ctest-p4b r)
   (c:c-struct-offset-of ctest-p4b p)
   (c:c-struct-size-of ctest-p1a)
   (c:c-struct-offset-of ctest-p1a i)
   (c:c-struct-offset-of ctest-p1a d))
  => 16
  => 4
  => 12
  => 28
  => 4
  => 20
  => 13
  => 1
  => 5)
