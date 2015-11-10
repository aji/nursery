(defun tco-demo (n)
  (if (< n 500000)
    (progn (print n)
           (tco-demo (+ n 1)))))

(tco-demo 0)
