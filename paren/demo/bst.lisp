; creates a new binary search tree
(defun bst-new ()
  nil)

; retrieves the value in the tree associated with key 'k'
(defun bst-get (bst k)
  (if (eq (caar bst) k)
    (cdar bst)
    (if (< k (caar bst))
      (bst-get (cadr bst) k)
      (bst-get (cddr bst) k))))

; creates a new tree where 'k' is associated with the value 'v'
(defun bst-put (bst k v)
  (if (nilp bst)
    (cons (cons k v) (cons nil nil))
    (if (eq (caar bst) k)
      (cons (cons k v) (cdr bst))
      (if (< k (caar bst))
        (cons (car bst) (cons (bst-put (cadr bst) k v) (cddr bst)))
        (cons (car bst) (cons (cadr bst) (bst-put (cddr bst) k v)))))))

; calls 'fn' once for each node in the tree, using in-order traversal.
; invokes 'fn' like (fn key val)
(defun bst-each (bst fn)
  (if (nilp bst)
    nil
    (progn (bst-each (cadr bst) fn)
           (fn (caar bst) (cdar bst))
           (bst-each (cddr bst) fn))))

(setq bst (bst-new))
(setq bst (bst-put bst 1 "one"))
(setq bst (bst-put bst 5 "five"))
(setq bst (bst-put bst 3 "three"))
(setq bst (bst-put bst 2 "two"))
(setq bst (bst-put bst 4 "four"))

(print bst)
(print (bst-get bst 1))
(print (bst-get bst 2))
(print (bst-get bst 3))
(print (bst-get bst 4))
(print (bst-get bst 5))

(bst-each bst print)
