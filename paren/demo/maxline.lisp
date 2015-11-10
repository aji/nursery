; check we have arguments!
(if (nilp (core:args))
    (progn (print "need arguments!")
           (core:exit 1)))

; try to open the file
(if (nilp (setq f (io:open (car (core:args)))))
    (progn (print "that file doesn't exist")
           (core:exit 1)))

(setq maxline "")
(map (lambda (ln)
             (if (> (strlen ln) (strlen maxline))
                 (setq maxline ln)))
     (gen io:gets (list f)))
(print maxline)
