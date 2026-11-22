;;; -*- mode: lisp; package: user -*-
;;;
;;; Tests that start another xyzzy (eval-in-another-xyzzy) and the way a child
;;; that never ends is handled (BUG-0060).

(in-package :user)

(defvar *child-xyzzy-test-var* nil)

(deftest child-xyzzy-reads-dump-image ()
  "A child started with -image reads the image that this xyzzy has written."
  (let ((image (make-temp-file-name "chd")))
    (setq *child-xyzzy-test-var* (list 1 2 3))
    (unwind-protect
        (progn
          (dump-xyzzy image)
          (eval-in-another-xyzzy
           '(list *child-xyzzy-test-var* (+ 1 2))
           :options (format nil "-image ~A -q" image)
           :timeout 60))
      (setq *child-xyzzy-test-var* nil)
      (ignore-errors (delete-file image))))
  => ((1 2 3) 3))

(deftest child-xyzzy-timeout-kills-the-child ()
  "A child that never ends is killed at the timeout; no process is left."
  (let ((before (length (test-live-subprocesses))))
    (list (handler-case
              (eval-in-another-xyzzy '(progn (sleep-for 1000) 1)
                                     :options "-q"
                                     :timeout 3)
            (error (e) (princ-to-string e)))
          (= before (length (test-live-subprocesses)))))
  => ("eval-in-another-xyzzy timeout." t))
