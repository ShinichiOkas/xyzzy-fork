(in-package "editor")

(defvar *g-log* (concat (si:getenv "G_SP") "/log.txt"))

(defun g-log (s)
  (with-open-file (o *g-log* :direction :output :if-exists :append
                     :if-does-not-exist :create)
    (princ s o)
    (terpri o)))

(when (si:getenv "G_OLD")
  (defvar *rename-alternate-file-name* nil)
  (load (si:getenv "G_OLD"))
  (g-log "loaded old optprop"))

(defun g-dlg ()
  (interactive)
  (setq *option-prop-page-no*
        (position 'filer (reverse *option-prop-page*)
                  :key #'(lambda (x) (if (consp x) (car x) x))))
  (g-log (format nil "dialog page no ~S" *option-prop-page-no*))
  (option-property-sheet)
  (g-log "dialog closed"))

(defun g-filer ()
  (interactive)
  (setq *filer-modal* nil)
  (when (si:getenv "G_DIR")
    (set-default-directory (si:getenv "G_DIR")))
  (g-log (format nil "filer dir ~S modal ~S" (default-directory) *filer-modal*))
  (handler-case (filer (default-directory) t nil t t)
    (error (c) (g-log (format nil "filer error ~A" (si:*condition-string c)))))
  (g-log "filer called"))

(defun g-bufs ()
  (interactive)
  (g-log (format nil "---- buffers ~D" (length (buffer-list))))
  (dolist (b (buffer-list))
    (let ((f (get-buffer-file-name b)))
      (g-log (format nil "buf ~S file=~S len=~D" (buffer-name b) f (if f (length f) 0))))))

(defun g-disk ()
  (interactive)
  (let ((d (or (si:getenv "G_DIR") (default-directory))))
    (multiple-value-bind (total used free) (get-disk-usage d)
      (g-log (format nil "get-disk-usage ~S total=~S used=~S free=~S" d total used free)))))

(defun g-text ()
  (interactive)
  (g-log (format nil "text of ~S: codes ~S" (buffer-name (selected-buffer))
                 (map 'list #'char-code (buffer-substring (point-min) (point-max))))))

(defun g-clear ()
  (interactive)
  (erase-buffer (selected-buffer))
  (g-log "buffer cleared"))

(define-key *global-keymap* #\F5 'g-text)
(define-key *global-keymap* #\F4 'g-clear)

(defun g-done () (interactive) (g-log "done") (kill-xyzzy))

(define-key *global-keymap* #\F6 'g-disk)
(define-key *global-keymap* #\F7 'g-dlg)
(define-key *global-keymap* #\F8 'g-filer)
(define-key *global-keymap* #\F9 'g-bufs)
(define-key *global-keymap* #\F12 'g-done)
(g-log "script loaded")
