;;
;; Paren IRC bot
;; by Alex Iadicicco
;;

;; Default configuration values.
;; In the future, there may be a separate config file to be loaded.
(setq *conf-nick* "parenbot")
(setq *conf-name* "Paren IRC bot")
(setq *conf-host* "irc.interlinked.me")
(setq *conf-port* 6667)
(setq *conf-chan* "#code")

(setq *nickname* *conf-nick*)
(setq *channels* ())

(defun join (words sep)
  (let ((sep (if sep sep " ")))
    (if (cdr words)
        (strcat (car words) sep (join (cdr words) sep))
        (car words))))

(defun list-push (L e)
  (set L (cons e (eval L))))
(defun list-drop (L e)
  (set L (filter (lambda (x) (not (eq x e))) (eval L))))

;; Having data
(setq recvbuf "")
(defun is-not-return (c)
  (not (eq c #\return)))
(defun have-data (s)
  (setq recvbuf (filter is-not-return (strcat recvbuf s)))
  (process-next-line))
(defun process-next-line ()
  (let ((idx (index #\linefeed recvbuf)))
    (if idx
        (progn (process-line (substr recvbuf 0 idx))
               (setq recvbuf (substr recvbuf (+ idx 1)))
               (process-next-line)))))
(defun process-line (ln)
  (let ((msg (irc-parse ln)))
    (if msg
        (irc-recv msg))))
(defun irc-glue (words)
  (if words
      (if (eq (head (car words)) #\:)
          (list (join (cons (tail (car words)) (cdr words))))
          (cons (car words) (irc-glue (cdr words))))))
(defun irc-parse-source (src)
  (let ((idx (index #\! src)))
    (if idx
        (list 'u (substr src 0 idx) (substr src (+ idx 1)))
        (list 's src))))
(defun irc-parse (ln)
  (let ((spl (split ln #\space)))
    (if (eq (head (car spl)) #\:)
        (cons (irc-parse-source (tail (car spl))) (irc-glue (cdr spl)))
        (cons () (irc-glue spl)))))

;; General query functions
(defun is-me (nick)
  (eq nick *nickname*))

;; Message query functions
(defun msg-is-user (m)
  (eq (caar m) 'u))
(defun msg-is-server (m)
  (eq (caar m) 's))
(defun msg-is-me (m)
  (and (msg-is-user m) (is-me (cadar m))))

;; Message handling
(defun irc-handler (msg fn)
  (try irc-handlers (setq irc-handlers (hash-new)))
  (hash-put irc-handlers msg (cons fn (hash-get irc-handlers msg))))

(defun irc-recv (msg)
  (message " -> {}" msg)
  (let ((h (hash-get irc-handlers (cadr msg))))
    (map (lambda (fn) (fn msg)) h)))

(irc-handler "PING" (lambda (msg)
  (irc-send "PONG" () (caddr msg))))
(irc-handler "001" (lambda (msg)
  (irc-send "JOIN" (list *conf-chan*))))

(irc-handler "433" (lambda (msg)  ; "nick in use" message
  (setq *nickname* (strcat *nickname* "_"))
  (irc-send "NICK" (list *nickname*))))

;; Internal state management
(irc-handler "JOIN" (lambda (msg)
  (if (msg-is-me msg)
      (list-push '*channels* (caddr msg)))
  (message "chans={}" *channels*)))
(irc-handler "PART" (lambda (msg)
  (if (msg-is-me msg)
      (list-drop '*channels* (caddr msg)))
  (message "chans={}" *channels*)))
(irc-handler "KICK" (lambda (msg)
  (if (is-me (cadddr msg))
      (list-drop '*channels* (caddr msg)))
  (message "chans={}" *channels*)))

;; PRIVMSG handling
(irc-handler "PRIVMSG" (lambda (msg)
  (let ((src (cadar msg))
        (tgt (caddr msg))
        (txt (cadddr msg)))
    (if (eq (head tgt) #\#)
        (if (eq (substr txt 0 (strlen *nickname*)) *nickname*)
            (let ((cmdstr (cadr (split txt () 1))))
                 (if cmdstr (handle-command tgt cmdstr))))
        (handle-command src txt)))))

;; Command handling
(defun cmd-handler (cmd fn)
  (try cmd-handlers (setq cmd-handlers (hash-new)))
  (hash-put cmd-handlers cmd (cons fn (hash-get cmd-handlers cmd))))

(defun handle-command (reply line)
  (let* ((spl (split line () 1))
         (cmd (car spl))
         (arg (cadr spl))
         (hs  (hash-get cmd-handlers cmd))
         (rfn (lambda (txt)
                      (irc-send "PRIVMSG" (list reply) txt))))
    (message "handle({},{},{})" cmd arg h)
    (map (lambda (h) (h rfn arg)) hs)))

(cmd-handler "hello" (lambda (reply arg)
  (reply "hello :)")))

;; Function to send an IRC-formatted message
(defun irc-send (verb args trail)
  (let ((words (concat
                (list verb)
                (map (lambda (a) (strcat " " a)) args)
                (if trail (list (strcat " :" trail))))))
    (let ((msg (apply strcat words)))
      (message " <- {}" msg)
      (net:send sock (strcat msg "\r\n")))))

;; Attempt to connect
(setq sock
  (let ((sock (net:socket)))
       (print 'connect 'to *conf-host* 'port *conf-port*)
       (if (net:connect sock *conf-host* *conf-port*)
           sock nil)))
(if (nilp sock)
  (progn (print 'connect 'failed)
         (core:exit 1)))

;; Perform initial handshake, then enter recv() loop
(setq *nickname* *conf-nick*)
(setq *channels* (list *conf-chan*))

(irc-send "NICK" (list *nickname*))
(irc-send "USER" (list *conf-nick* "*" "*") *conf-name*)

(defun recv-loop ()
  (have-data (net:recv sock 512))
  (recv-loop))

(recv-loop)
