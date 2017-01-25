#!/usr/bin/ol
;(define *interactive* #t)
;(define *debug* #t)

(import (lib http))
;(import (lib html))

,load "db.lisp"
,load "lib1.lisp"
,load "html.lisp"

(define (ne? . args)
   (not (apply eq? args)))

(define unurl (string->regex "s/+/ /g"))

; ==============================================================
; набор общей игровой математики
(define (get-advantage-points-left prt lrts)
   (+ prt lrts))

; пара мелкий упрощений синтаксиса
; -


; todo: сделать такой string-split
;       который, если в строке есть сумволы кроме чисел
;       возвращает url-decoded строки, в ином случае - числа через (string->number)
; а вообще, просто добавить сигнатуру вызова и автоматическую его парсилку

(define-syntax actions
   (syntax-rules (else action account simple al string-split string-split-with-auth close)
      ((actions) #false)
      ((actions (else exp . rest))
         ((lambda () exp . rest)))        ; (begin ...)
      ((actions (simple url (args...) .body) .rest)
         (if (starts-with al url)
            (apply (lambda (args...)
                     .body)
               (cddr (split-url al)))
            (actions .rest)))
      ((actions (action url (args...) .body) .rest)
         (if (starts-with al url)
            (apply (lambda (account args...)
                     .body)
               (string-split-with-auth al))
            (actions .rest)))
      ((actions (clause exp . rest-exps) .rest)
         (if clause
            ((lambda () exp . rest-exps)) ; (begin ...)
            (actions . rest)))))

;====================================================
(http:run 8080 (lambda (fd request headers send close)
   (print "Request: " request)
;  (print "Headers: " headers)

   ; todo: rename to answer
   (define (respond status-code . args)
      (print "Sending " status-code)
      (send "HTTP/1.0 " status-code "\r\n")
      (send "Content-Type: text/html\r\n"
            "Server: " (car *version*) "/" (cdr *version*) "\r\n"
            "\r\n")
      (for-each (lambda (x) (write-to fd x)) args)
      (close #t))
   (define (send-200) (respond "200 OK"))
   (define (send-204) (respond "204 No Content"))
   (define (send-400) (respond "400 Bad Request"))
   (define (send-401) (respond "401 Unauthorized"))
   (define (send-404) (respond "404 Not Found"))
   (define (send-405) (respond "405 Method Not Allowed"))

   ; обходит дерево рекурсивно и отправляет его в выходной поток
   ; #null и #false в выходной поток НЕ отправляются!
   (define (html . args)
      (let html ((args args))
         (for-each (lambda (arg)
            (if arg
               (if (list? arg)
                  (html arg)
                  (if (not (null? arg))
                     (display-to fd arg)))))
            args)))

   ; отправить файл в сокет
   (define (sendfile content-type filename)
      (print "Sending as '" content-type "' " filename)
      (if (has-two-dots? filename)
         (send-204) ; нельзя выходить за границу папки
         ; else
         (let*((path (str-app "." (c-string filename)))
               (stat (syscall 4 path #f #f)))
            (if stat (let ((file (fopen path 0)))
               (print "Sending 200 OK, file size is " (ref stat 8) ", name is " path)
               (send "HTTP/1.0 200 OK\r\n"
                     "Connection: close\r\n"
                     "Content-Type: " content-type "\r\n"
                     "Content-Length: " (ref stat 8) "\r\n"
                     "Server: " (car *version*) "/" (cdr *version*) "\r\n"
                     "\r\n")
               (syscall 40 fd file (ref stat 8)) ;(write-vector (file->vector path) fd)
               (fclose file)
               (print "File sent."))
            ;else
            (begin
               (print "Sending 404 Not Found, file name is " path)
               (send "HTTP/1.0 404 Not Found\r\n"
                     "Connection: close\r\n"
                     "Content-Type: text/html\r\n"
                     "Server: " (car *version*) "/" (cdr *version*) "\r\n"
                     "\r\n")
               (send "<html><body>"
                     "<h1>404 Not Found</h1>"
                     "</body></html>"))))))
   ; %->%25, etc.

; парсит, проверяя аутентификацию пользователя
   (define (string-split-with-auth url)
      (let ((args (cdr (split-url url))))
      (let ((account (db:value "SELECT id FROM accounts WHERE session = ? AND remote_address = ?"
                        (list-ref args 1) ;session
                        (car (syscall 51 fd #f #f))))) ;remote peer address
         (print "account: " account ", from: " (car (syscall 51 fd #f #f)) ", args: " args)
         (if (not account)
            (close (send-401))) ; todo: return only args, change apply to case-lambda

         (cons account (cdr args)))))

   ; обработчик запросов
   (cond
      ((string-eq? (ref request 1) "GET")
         (let*((al (ref request 2))) ; al - address line
            (actions
               ; static web content:
               ((starts-with al "/javascripts/")
                  (sendfile "application/javascript" al))
               ((starts-with al "/stylesheets/")
                  (sendfile "text/css" al))

               ; -----------------------------------
               ; обработка логина пользователя:
               ((or  (string-eq? al "/")
                     (string-eq? al "/index.html")
                     (string-eq? al "/login.html"))
                  (sendfile "text/html" "/index.html"))

               ; временное решение вопроса логина
               (simple "/login/" (username password)
                  (let ((remote_address (car (syscall 51 fd #f #f)))
                        (session (db:value "SELECT lower(hex(randomblob(16)))"))) ; сеансовый ключ
                     (print "session: " session)
                     (if (db:value "UPDATE accounts SET session=?, remote_address=? WHERE name=? AND password=?"
                                    session remote_address username password)
                        (respond "200 OK" session)
                        (respond "401 Unauthorized"))))

               ; ===============================================================
               ; домашний экран пользователя
               (action "/home/" (session)
                  ; отправим username '(игры полльзователя погруппированные по расам)
                  (respond "200 OK"
                     (db:value "SELECT name FROM accounts WHERE id = ?" account)
                     (if #true ;
                        (db:map (db:query
                           "SELECT  id,name  FROM races WHERE account = ?" account)
                           (lambda (id name)
                              (print id ", " name)
                              (list id name '(
                                 (1 "game 1" 0)
                                 (2 "game 2" 0)))))
                        ; тестовые данные
                        '(;id name
                           (1 "race 1" (
                              ;id name state
                              (1 "game 1" 0)
                              (2 "game 2" 0)))
                           (2 "race 2" (
                              (3 "game 3" 1)
                              (4 "game 4" 0)))))))


               (action "/create-new-race/" (session)
                  (print "Creating new race. ")
                  (let ((race (db:value "INSERT INTO races (name, account) VALUES (?,?)" "new race" account)))
                     (if race
                        (respond "200 OK" race)
                        (respond "400 Server error"))))


               ; ===============================================================
               ; всякая серверная математика
               (simple "/get-advantage-points-left/" (prt lrts)
                  (send-200)

                  (send (+ prt lrts)))

               ;else
               (else
                  (send-404)
                  (send "<html><body>"
                        "<h1>404 Not Found</h1>"
                        "</body></html>")))))
      (else
         (send-405)
         (send "<html><body>"
               "<h1>405 Method Not Allowed</h1>"
               "</body></html>")))))