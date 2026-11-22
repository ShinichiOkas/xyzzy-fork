;;; -*- mode: lisp; package: user -*-
;;;
;;; The exchange between xyzzycli.exe and xyzzy.exe (S4 stage 3, R8; design
;;; 10.4).  xyzzycli makes UTF-8 from GetCommandLineW and writes it to the
;;; shared memory ("xyzzy-server v3"); xyzzy reads it as UTF-8.
;;;
;;; The test starts xyzzycli.exe against the xyzzy that runs the test, with a
;;; file whose name is Japanese, and looks at the buffer that is opened.  It
;;; needs no window of the user: the test runs in the session of the CI.
;;;
;;; This file is ASCII: the characters are asked for by their Unicode.

(in-package :user)

(defun xyzzycli-test-name (&rest codes)
  (coerce (mapcar #'unicode-char codes) 'string))

(defun xyzzycli-test-open (path)
  "Runs xyzzycli.exe with PATH, and returns the buffer of PATH (or nil)."
  (let* ((cli (merge-pathnames "xyzzycli.exe" (si:system-root)))
         (tmpbuf (create-new-buffer "*xyzzycli-test*"))
         (proc nil)
         (buffer nil))
    (start-xyzzy-server)
    (unwind-protect
        (progn
          (setq proc (make-process (format nil "\"~A\" \"~A\"" cli path)
                                   :output tmpbuf :show :hide))
          (let ((deadline (+ (get-universal-time) 40)))
            (loop
              (setq buffer (get-file-buffer path))
              (when (or buffer (> (get-universal-time) deadline))
                (return))
              (sleep-for 0.1)))
          ;; xyzzycli returns when the server has read the text
          (let ((deadline (+ (get-universal-time) 10)))
            (while (and (eql (process-status proc) :run)
                        (< (get-universal-time) deadline))
              (sleep-for 0.1)))
          (and buffer (buffer-name buffer)))
      (when (and proc (eql (process-status proc) :run))
        (ignore-errors (signal-process proc)))
      (let ((b (get-file-buffer path)))
        (when b
          (ignore-errors (delete-buffer b))))
      (ignore-errors (delete-buffer tmpbuf)))))

(deftest xyzzycli-opens-file-with-japanese-name ()
  (let* ((tmp (make-temp-file-name))
         (name (concatenate 'string
                            (xyzzycli-test-name #x65e5 #x672c #x8a9e)
                            "-" (pathname-name tmp) ".txt"))
         (path (merge-pathnames name (directory-namestring tmp))))
    (unwind-protect
        (progn
          (with-open-file (s path :direction :output :if-exists :supersede)
            (write-string "x" s))
          (if (file-exist-p (merge-pathnames "xyzzycli.exe" (si:system-root)))
              (equal (xyzzycli-test-open path) name)
            :no-xyzzycli))
      (ignore-errors (delete-file path))
      (ignore-errors (delete-file tmp))))
  => t)

(deftest xyzzycli-opens-file-with-ascii-name ()
  (let* ((tmp (make-temp-file-name))
         (name (concatenate 'string "xyzzycli-" (pathname-name tmp) ".txt"))
         (path (merge-pathnames name (directory-namestring tmp))))
    (unwind-protect
        (progn
          (with-open-file (s path :direction :output :if-exists :supersede)
            (write-string "x" s))
          (if (file-exist-p (merge-pathnames "xyzzycli.exe" (si:system-root)))
              (equal (xyzzycli-test-open path) name)
            :no-xyzzycli))
      (ignore-errors (delete-file path))
      (ignore-errors (delete-file tmp))))
  => t)
