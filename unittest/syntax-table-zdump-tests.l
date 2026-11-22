;;; -*- mode: lisp; package: user -*-
;;;
;;; The sparse overrides of a syntax table survive the dump (S4 R2).
;;; st-range-chars is in syntax-table-tests.l.

(in-package :user)

;; The overrides go into the dump (xyzzy.wxp) and come back.  The dump holds
;; the table as it is at startup, and lisp/ sets no character of 256 or more
;; in a table, so the test writes an image of this xyzzy, with a table that has
;; overrides in a variable, and reads it in another xyzzy.
;;
;; The image is written with *process-command-line-hook* cleared: the hook that
;; run-tests-helper.l added would otherwise take the arguments of the other
;; xyzzy (-e ...) and the other xyzzy would never run its form.  The wait for
;; the other xyzzy is bounded.
(defvar *syntax-dump-test-table* nil)

(defun st-eval-in-image (image form)
  "Evaluate FORM in another xyzzy that starts from IMAGE; wait 30 seconds at most."
  (let* ((tmpfile (make-temp-file-name))
         (command (eval-in-another-xyzzy--make-command-string
                   form (format nil "-image ~A -q" image) tmpfile))
         (tmpbuf (let ((b (create-new-buffer "*Temp*")))
                   (setup-temp-buffer b)
                   b))
         (proc (let ((*xyzzyenv-show-flag* :hide))
                 (make-process command :output tmpbuf :show :minimize))))
    (unwind-protect
        (progn
          (do ((n 0 (1+ n)))
              ((or (not (eql (process-status proc) :run)) (>= n 300)))
            (sleep-for 0.1))
          (when (eql (process-status proc) :run)
            (ignore-errors (signal-process proc))
            (error "the xyzzy that reads the image did not finish"))
          (eval-in-another-xyzzy--read-results tmpfile))
      (ignore-errors (delete-buffer tmpbuf))
      (ignore-errors (delete-file tmpfile)))))

(deftest syntax-nonascii-survives-dump ()
  "The sparse overrides are written to the dump and read back."
  (let ((tab (make-syntax-table))
        (src (make-syntax-table))
        (image (make-temp-file-name "syn")))
    (set-syntax-word tab (unicode-char #x3042))
    (set-syntax-punctuation tab (unicode-char #x00e9))
    (set-syntax-match tab (unicode-char #x300c) (unicode-char #x300d))
    (set-syntax-start-multi-comment tab (coerce (list (unicode-char #x3044) (unicode-char #x3046)) 'string))
    (dolist (ch (st-range-chars #x30a1 #x30f6))
      (set-syntax-word src ch))
    (copy-syntax-table src (make-syntax-table))
    (setq *syntax-dump-test-table* tab)
    (unwind-protect
        (progn
          (let ((*process-command-line-hook* nil))
            (dump-xyzzy image))
          (st-eval-in-image
           image
           '(let ((tab *syntax-dump-test-table*))
              (list (syntax-word-p (unicode-char #x3042) tab)
                    (syntax-punctuation-p (unicode-char #x00e9) tab)
                    (syntax-open-p (unicode-char #x300c) tab)
                    (syntax-close-p (unicode-char #x300d) tab)
                    (syntax-start-multi-comment-1-p (unicode-char #x3044) tab)
                    (syntax-start-multi-comment-2-p (unicode-char #x3046) tab)
                    (syntax-word-p (unicode-char #x3044) tab)
                    (syntax-word-p (unicode-char #x4e00) tab)
                    (progn (set-syntax-word tab (unicode-char #x4e00))
                           (syntax-word-p (unicode-char #x4e00) tab))))))
      (setq *syntax-dump-test-table* nil)
      (ignore-errors (delete-file image))))
  => (t t t t t t nil nil t))
