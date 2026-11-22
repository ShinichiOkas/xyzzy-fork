;;; S4-c: W 版のファイル API の層 (src/wfile.cc) の試験
;;;   - 260 文字を超えるパス（\\?\ を層の中で付ける）
;;;   - 4GB を超えるファイルの長さ（疎ファイルで作る）
;;;   - 8.3 の処理の削除
;;; 後始末は unwind-protect の中で自分で行う（run-tests.bat の rd /s /q は
;;; 長いパスを消せないことがある）。

(defvar *wfile-long-segment*
  "0123456789012345678901234567890123456789012345678901234567")

;; root の下に、全体で 300 文字を超えるまでディレクトリを掘る。
;; 作ったディレクトリを浅い順のリストで返す。
(defun wfile-make-long-dirs (root)
  (let ((dir (remove-trail-slash root))
        (dirs nil))
    (while (< (length dir) 300)
      (setq dir (concat dir "/" *wfile-long-segment*))
      (create-directory dir)
      (push dir dirs))
    (nreverse dirs)))

;; ディレクトリの中身（ファイル）を全部消してから、深い順にディレクトリを消す。
(defun wfile-remove-long-dirs (dirs)
  (dolist (dir (reverse dirs))
    (when (file-exist-p dir)
      (dolist (f (directory dir :absolute t :show-dots nil))
        (when (file-exist-p f)
          (if (file-directory-p f)
              (delete-directory f)
            (delete-file f))))
      (delete-directory dir))))

(defmacro with-wfile-long-dir ((var) &body body)
  (let ((dirs (gensym)) (root (gensym)))
    `(with-temp-dirs (,root)
       (let ((,dirs nil))
         (unwind-protect
             (progn
               (setq ,dirs (wfile-make-long-dirs ,root))
               (let ((,var (append-trail-slash (car (last ,dirs)))))
                 ,@body))
           (wfile-remove-long-dirs ,dirs))))))
(setf (get 'with-wfile-long-dir 'ed:lisp-indent-hook) 1)

(deftest wfile-long-path-access-denied-is-an-error-not-a-dialog ()
  ;; \\?\C:\... を共有（\\server\share）と取り違えて、ネットワークの資格情報の
  ;; ダイアログを出さないこと。読み取り専用のファイルの削除は access-denied になる。
  (with-wfile-long-dir (dir)
    (let ((file (concat dir "readonly.txt")))
      (with-open-file (s file :direction :output)
        (princ "ro" s))
      (set-file-attributes file ed::*file-attribute-readonly*)
      (let ((r (handler-case (progn (delete-file file) :deleted)
                 (error () :error))))
        (set-file-attributes file ed::*file-attribute-archive*)
        r)))
  => :error)
(defconstant *wfile-big-size* (+ (* 5 1024 1024 1024) 1))

;; 疎ファイルを作る（ディスクを食わない）。作れなければ nil
(defun wfile-make-sparse-file (file size)
  (handler-case
      (progn
        (when (file-exist-p file)
          (delete-file file))
        (with-open-file (s file :direction :output)
          (princ "" s))
        (call-process (format nil "fsutil sparse setflag \"~A\""
                              (map-slash-to-backslash file))
                      :wait t :show :hide)
        ;; 末尾の 1 バイトだけ書く（疎ファイルなので実際のディスクは食わない）
        (with-open-file (s file :direction :io :if-exists :overwrite)
          (file-position s (1- size))
          (princ "z" s))
        (eql (file-length file) size))
    (error () nil)))

(deftest wfile-long-path-directory ()
  (with-wfile-long-dir (dir)
    (values
     (> (length dir) 300)
     (< (length dir) 500)
     (file-directory-p dir)
     (file-exist-p dir)))
  => t
  => t
  => t
  => t)

(deftest wfile-long-path-write-and-read ()
  (with-wfile-long-dir (dir)
    (let ((file (concat dir "long-file.txt")))
      (with-open-file (s file :direction :output :if-exists :supersede)
        (princ "hello" s))
      (values
       (> (length file) 300)
       (file-exist-p file)
       (file-length file)
       (with-open-file (s file :direction :input)
         (read-line s)))))
  => t
  => t
  => 5
  => "hello")

(deftest wfile-long-path-open-new-and-append ()
  (with-wfile-long-dir (dir)
    (let ((file (concat dir "append.txt")))
      (with-open-file (s file :direction :output :if-exists :error)
        (princ "ab" s))
      (with-open-file (s file :direction :output :if-exists :append)
        (princ "cd" s))
      (values
       (file-length file)
       (with-open-file (s file :direction :input)
         (read-line s)))))
  => 4
  => "abcd")

(deftest wfile-long-path-directory-list ()
  (with-wfile-long-dir (dir)
    (let ((file (concat dir "listed.txt")))
      (with-open-file (s file :direction :output)
        (princ "xyz" s))
      (values
       (directory dir)
       (equal (get-file-info file) (cdr (car (directory dir :file-info t))))
       (nth 2 (get-file-info file)))))
  => ("listed.txt")
  => t
  => 3)

(deftest wfile-long-path-truename ()
  (with-wfile-long-dir (dir)
    (let ((file (concat dir "Truename.txt")))
      (with-open-file (s file :direction :output)
        (princ "x" s))
      (values
       (file-namestring (truename (concat dir "truename.txt")))
       ;; Compare with the truename of FILE, not FILE itself: DIR comes from
       ;; TEMP, which can hold 8.3 names (a service's TEMP has NETWOR~1), and
       ;; truename gives the long names.
       (string-equal (truename (concat dir "truename.txt")) (truename file)))))
  => "Truename.txt"
  => t)

(deftest wfile-long-path-buffer-read-and-save ()
  (with-wfile-long-dir (dir)
    (let ((file (concat dir "buffer.txt")))
      (with-open-file (s file :direction :output)
        (princ "one" s))
      (with-temp-buffer
        (insert-file-contents file)
        (let ((before (buffer-substring (point-min) (point-max))))
          (goto-char (point-max))
          (insert "-two")
          (write-region (point-min) (point-max) file)
          (values before (file-length file))))))
  => "one"
  => 7)

(deftest wfile-long-path-save-buffer ()
  ;; 保存は一時ファイルに書いて MoveFile で差し替える経路（長いパス）を通る
  (with-wfile-long-dir (dir)
    (let ((file (concat dir "saved.txt")))
      (with-open-file (s file :direction :output)
        (princ "old" s))
      (with-temp-buffer
        (insert-file-contents file)
        (set-buffer-file-name file)
        (goto-char (point-max))
        (insert "-new")
        (save-buffer)
        (values
         (with-open-file (s file :direction :input) (read-line s))
         (file-length file)))))
  => "old-new"
  => 7)

(deftest wfile-long-path-copy-rename-delete ()
  (with-wfile-long-dir (dir)
    (let ((a (concat dir "a.txt"))
          (b (concat dir "b.txt"))
          (c (concat dir "c.txt")))
      (with-open-file (s a :direction :output)
        (princ "copy me" s))
      (copy-file a b)
      (rename-file b c)
      (let ((r (list (file-length a) (file-exist-p b) (file-length c))))
        (delete-file a)
        (delete-file c)
        (append r (list (file-exist-p a) (file-exist-p c))))))
  => (7 nil 7 nil nil))

(deftest wfile-long-path-make-directory-tree-and-delete ()
  (with-wfile-long-dir (dir)
    (let ((sub (concat dir "sub1/sub2/")))
      (create-directory sub)
      (let ((r (file-directory-p sub)))
        (delete-directory (concat dir "sub1/sub2"))
        (delete-directory (concat dir "sub1"))
        (list r (file-exist-p (concat dir "sub1"))))))
  => (t nil))

(deftest wfile-long-path-compare-and-attributes ()
  (with-wfile-long-dir (dir)
    (let ((a (concat dir "a.txt"))
          (b (concat dir "b.txt")))
      (with-open-file (s a :direction :output) (princ "same" s))
      (with-open-file (s b :direction :output) (princ "same" s))
      (set-file-attributes a ed::*file-attribute-readonly*)
      (let ((ro (logand (get-file-attributes a) ed::*file-attribute-readonly*)))
        (set-file-attributes a ed::*file-attribute-archive*)
        (list (compare-file a b) ; 内容は同じ
              (/= 0 ro)
              (path-equal a (concat dir "A.TXT"))))))
  => (t t t))

;;; 4GB を超えるファイルの長さ

(deftest wfile-file-length-over-4gb ()
  (with-temp-files (file)
    (if (wfile-make-sparse-file file *wfile-big-size*)
        (values
         (file-length file)
         (> (file-length file) #xffffffff)
         (nth 2 (get-file-info file))
         (with-open-file (s file :direction :input)
           (file-position s :end)
           (file-position s)))
      (values *wfile-big-size* t *wfile-big-size* *wfile-big-size*)))
  => 5368709121
  => t
  => 5368709121
  => 5368709121)

(deftest wfile-file-length-over-4gb-directory-listing ()
  (with-temp-dirs (dir)
    (let ((file (concat (append-trail-slash dir) "big.bin")))
      (unwind-protect
          (if (wfile-make-sparse-file file *wfile-big-size*)
              (list (nth 2 (cdr (car (directory dir :file-info t))))
                    (nth 2 (get-file-info file)))
            (list *wfile-big-size* *wfile-big-size*))
        (when (file-exist-p file) (delete-file file)))))
  => (5368709121 5368709121))

;;; 8.3 の削除

(deftest wfile-short-name-functions-removed ()
  (values
   (fboundp 'get-short-path-name)
   (fboundp 'file-system-supports-long-file-name-p)
   (boundp '*rename-alternate-file-name*)
   (length (get-file-info (si:system-root))))
  => nil
  => nil
  => nil
  => 3)

;;; compare-file maps whole files (BUG-0055): 2GB-4GB and over 4GB

(defun wfile-compare-sparse-files (size)
  (let ((a (make-temp-file-name))
        (b (make-temp-file-name)))
    (unwind-protect
        (if (and (wfile-make-sparse-file a size)
                 (wfile-make-sparse-file b size))
            (let ((same (compare-file a b)))
              ;; change the last byte of b only
              (with-open-file (s b :direction :io :if-exists :overwrite)
                (file-position s (1- size))
                (princ "y" s))
              (list same (compare-file a b) (file-length b)))
          (list t nil size))
      (when (file-exist-p a) (delete-file a))
      (when (file-exist-p b) (delete-file b)))))

(deftest wfile-compare-file-3gb ()
  (wfile-compare-sparse-files (+ (* 3 1024 1024 1024) 1))
  => (t nil 3221225473))

(deftest wfile-compare-file-5gb ()
  (wfile-compare-sparse-files *wfile-big-size*)
  => (t nil 5368709121))
