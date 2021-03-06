;;;
;;; $ mxtx.el $
;;;
;;; Author: Tomi Ollila -- too ät iki piste fi
;;;
;;;	Copyright (c) 2017 Tomi Ollila
;;;	    All rights reserved
;;;
;;; Created: Tue 05 Sep 2017 21:50:02 EEST too
;;; Last modified: Fri 08 Dec 2017 21:33:08 +0200 too

;;; This particular file is licenced under GPL v3 (and probably later)...

;;; execute emacs -l ./mxtx.el when testing (and mxtx-apu.sh emacs works later)

(require 'tramp)

(add-to-list 'tramp-methods
  '("mxtx" ;; based on "rsync" method
    (tramp-login-program        "mxtx-rsh")
    ;; executing /bin/sh is more robust than relying default (login) shell
    (tramp-login-args           (("-t") ("%h") (".") ("/bin/sh" "-il")))
    ;;(tramp-login-args           (("%h")))
    ;;(tramp-login-args           (("-e" "none") ("%h")))
    (tramp-async-args           (("-q")))
    (tramp-remote-shell         "/bin/sh")
    (tramp-remote-shell-login   ("-l"))
    (tramp-remote-shell-args    ("-c"))
    (tramp-copy-program         "rsync")
    (tramp-copy-args            (("-t" "%k") ("-r") ("-e" "mxtx-io")))
    (tramp-copy-keep-date       t)
    ;;(tramp-copy-keep-tmpfile    t)
    (tramp-copy-recursive       t)))

;; tramp-verbose -- some hints to mxtx components debugging...?
;; (setq tramp-verbose 6) ;; M-x describe-variable ... to see levels

(setq tramp-mode t
      tramp-copy-size-limit nil
      )

(defun mxtx! ()
    "speedup tramp (a bit), modifies global env"
  (interactive)
  (require 'tramp)
  (setq vc-ignore-dir-regexp
	(format "\\(%s\\)\\|\\(%s\\)"
		vc-ignore-dir-regexp tramp-file-name-regexp)
	remote-file-name-inhibit-cache nil))
