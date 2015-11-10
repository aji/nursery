; quicksort demo
; the quicksort function numerically sorts the given list

(defun quicksort (seq)
  (if (listp seq)
    (let ((pivot (car seq))
          (rest  (cdr seq)))
      (concat
        (quicksort (filter (lambda (n) (<  n pivot)) rest))
        (list pivot)
        (quicksort (filter (lambda (n) (>= n pivot)) rest))))
    nil))

(print (quicksort '(1 7 3 2 8 1 1 5 2 3 6 3)))
