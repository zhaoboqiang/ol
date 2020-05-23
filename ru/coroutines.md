---
layout: page
title:  Сопрограммы
date: 2016-02-27 12:42:07 UTC
categories: ru
---
> Внимание, эта статья находится в процессе создания; ее содержание может (и будет) меняться, пока полностью не удовлетворит автора. А до тех пор я не ручаюсь за стопроцентную достоверность приведенной информации.


### Сопрограммы

Чисто функциональные языки - это хорошо и великолепно, но что делать, если хочется или надо написать что-нибудь императивненькое, а функциональный аналог слишком громоздок и/или малопонятен?

Ol предоставляет возможность не нарушая функциональной парадигмы создавать объекты с динамическим внутренним состоянием; называется этот механизм - сопрограммы.

Сопрограмма - это специально написанная лямбда функция, которая выполняется в отдельном потоке, и с которой можно обмениваться данными путем посылки (и принятия) сообщений; причем делать это может как основная программа, так и другие сопрограммы. Скажу больше, на самом деле главная сессия [REPL](?ru/repl) тоже является сопрограммой с, вот совпадение, именем 'repl.

Для работы с сопрограммами Ol содержит функции fork, fork-server, mail, check-mail, wait-mail, interact.

Разберем все на примере. Допустим, нам надо работать с неким пулом объектов, причем каждый запрошенный объект должен иметь уникальный номер, а доступ к пулу происходит в разных функциях и по разному поводу. Как можно написать отдельную функцию, у которой будет внутреннее состояние, если язык у нас чисто функциональный и все функции без исключения - чистые (pure)?

Для начала, запустим сопрограмму - это делается с помощью операции (fork-server имя лямбда). Пускай эта сопрограмма называется get-counter и содержит цикл, который будет на каждой итерации увеличивать свой счетчик на 1 (естественно, с использованием рекурсии - Ol вполне аккуратно обращается с хвостовой рекурсией и можно не бояться переполнения кучи).

<pre><code data-language="ol">
(fork-server 'get-counter (lambda ()
   (let loop ((n 1))
      (let*((envelope (wait-mail))
            (from msg envelope))
         (mail from n))
      (loop (+ n 1)))
))
</code></pre>

С остальным миром эта сопрограмма общается с помощью приема и посылки сообщений.

Посылка сообщения происходит с помощью функции mail,если нам не нужен ответ или interact, если ответ нужен. Прием - с помощью (wait-mail), которая возращает values пару '(отправитель cообщение). Разделить values нам поможет форма let*.

Основная часть программы будет выглядеть так:

<pre><code data-language="ol">
(define (getc)
   (interact 'get-counter #f))

(print (getc)(getc)(getc)(getc)(getc)(getc)(getc))
</code></pre>

Что в этой программе происходит:

* запускается сопрограмма 'get-counter, которая инициализирует внутренний символ 'n единицей и ждет на сообщение (wait-mail, ждать-почту),
* основная программа в функции getc через (interact получатель сообщение) отправляет сообщение #f (нам на самом деле сейчас все равно, что отправлять - нам важен сам факт отправки и получатель) и остается ждать ответа,
* сопрограмма просыпается, получает сообщение в envelope и с помощью формы let* разделяет его на from и msg,
* сопрограмма отправляет текущее значение n отправителю - from, после чего незамедлительно (mail не ждет ответа) уходит в рекурсию с увеличенным на 1 n), и снова засыпает в ожидании нового сообщения (wait-mail),
* тем временем основная программа просыпается, получая от сопрограммы значение n (в данном случае - 1), после чего повторяет процесс еще 6 раз, по количеству вызовов getc в print.

В результате мы получим вывод "1234567" в консоль.

Вот таким простым и незамысловатым образом можно с помощью дополнительных, совершенно незначительных накладных расходов, организовать функции "с состоянием". Сопрограммы в Ol довольно широко используются, например в поставляемой библиотеке lib/opengl, которая через набор функций, таких как create-window позволяет в императивном режиме организовать работу с opengl.

Для проверки работы вышеприведенного кода я сложу его здесь вместе с возможностью сразу отправить в терминал:

<pre><button class="doit" onclick="doit(numbers.textContent)">отправить в терминал</button>
<code data-language="ol" id="numbers">
; пример cопрограммы
(fork-server 'get-counter (lambda ()
   (let loop ((n 1))
      (let*((envelope (wait-mail))
            (from msg envelope))
         (mail from n))
      (loop (+ n 1)))
))

(define (getc)
   (interact 'get-counter #f))

(print (getc)(getc)(getc)(getc)(getc)(getc)(getc))
</code></pre>