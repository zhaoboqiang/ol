; * Copyright(c) 2014 - 2017 Yuriy Chumak
; *
; * -------------------------------------
; * This program is free software;  you can redistribute it and/or
; * modify it under the terms of the GNU General Public License as
; * published by the Free Software Foundation; either version 3 of
; * the License, or (at your option) any later version.
; *
; * This program is distributed in the hope that it will be useful,
; * but WITHOUT ANY WARRANTY; without even the implied warranty of
; * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

(define-library (otus ffi)
   (export
      dlopen
      dlclose
      dlsym dlsym+
      ffi uname

      RTLD_LAZY
      RTLD_NOW
      RTLD_BINDING_MASK
      RTLD_NOLOAD
      RTLD_DEEPBIND
      RTLD_GLOBAL
      RTLD_LOCAL
      RTLD_NODELETE


      type-short ; 16-bit integer
      type-int   ; 32-bit integer
      type-long  ; 32 for 32-bit platform, 64 for 64-bit

      type-int16
      type-int32
      type-int64 ; 64-bit integer

      type-float
      type-double

      type-void type-void* type-void**

      type-unknown
      type-callable
      type-any


      load-dynamic-library
      ; по-поводу calling convention:
      ; под Windows дефолтный конвеншен - __stdcall, под линукс - __cdecl
      ;  пока что пусть остается так.
      __stdcall __cdecl __fastcall
   )

   (import
      (r5rs core)
      (owl io)
      (owl math)
      (owl string))

   (begin

; принимаются типы:
; int (type-int+)
; float (type-rational)
; char* (type-string)
; void** (type-tuple)
; handle (новый тип type-handle)
;(define INTEGER type-int+)     ; todo: rename to the TINTEGER or similar
;(define FLOAT   type-rational) ; todo: same


; The MODE argument to `dlopen' contains one of the following:
(define RTLD_LAZY       #x00001); Lazy function call binding.
(define RTLD_NOW        #x00002); Immediate function call binding.
(define RTLD_BINDING_MASK   #x3); Mask of binding time value.
(define RTLD_NOLOAD     #x00004); Do not load the object.
(define RTLD_DEEPBIND   #x00008); Use deep binding.

; If the following bit is set in the MODE argument to `dlopen',
; the symbols of the loaded object and its dependencies are made
; visible as if the object were linked directly into the program.
(define RTLD_GLOBAL     #x00100)

; Unix98 demands the following flag which is the inverse to RTLD_GLOBAL.
; The implementation does this by default and so we can define the
; value to zero.
(define RTLD_LOCAL      0)

; Do not delete object when closed.
(define RTLD_NODELETE   #x01000)

; функция dlopen ищет динамическую библиотеку *name* (если она не загружена - загружает)
;  и возвращает ее уникальный handle (type-port)
(define dlopen (case-lambda
   ((name flag) (syscall 174 (if (string? name) (c-string name) name) flag      #false))
   ((name)      (syscall 174 (if (string? name) (c-string name) name) RTLD_LAZY #false))
   (()          (syscall 174 '()                                      RTLD_LAZY #false))))
(define (dlclose module) (syscall 176 module #f #f))

(define ffi (syscall 177 (dlopen) "ffi" #f))

; функция dlsym связывает название функции с самой функцией и позволяет ее вызывать
(define (dlsym+ dll name)
   (let ((function (syscall 177 dll (c-string name) #false)))
      (if function
      (lambda args
         (exec function args #false)))))

(define (dlsym  dll type name . prototype)
   ; todo: add arguments to the call of function and use as types
   ; должно быть так: если будет явное преобразование типа в аргументе функции, то пользовать его
   ; иначе использовать указанное в arguments; обязательно выводить предупреждение, если количество аргументов не
   ; совпадает (возможно еще во время компиляции)
   (let ((rtty (cons type prototype))
         (function (syscall 177 dll (c-string name) #false)))
      (if function
      (lambda args
         (exec ffi  function rtty args)))))

(define (load-dynamic-library name)
   (let ((dll (dlopen name)))
      (if dll
         (lambda (type name . prototype)
            (let ((rtty (cons type prototype))
                  (function (syscall 177 dll (c-string name) #f))) ; todo: избавиться от (c-string)
               (if function
                  (lambda args
                     (exec ffi  function rtty args))))))))


;(define (dlsym+ dll type name . prototype) (dlsym dll type name 44 prototype))
;; dlsym-c - аналог dlsym, то с правилом вызова __cdecl
;;(define (dlsym-c type dll name . prototype)
;;; todo: отправлять тип функции третим параметром (syscall 177) и в виртуальной машине
;;;   возвращать структуру с (byte-vector адрес-функции адрес-вызыватора-с-соответвующей-конвенцией) ?
;;   (let ((function (cons '((bor type 64) . prototype) (syscall 171 dll (c-string name) #false)))) ; todo: избавиться от (c-string)
;;;;;(let ((function (cons (bor type 64) (syscall 177 dll (c-string name) #false)))) ; todo: переделать 64 во что-то поприятнее
;;      (lambda args ;  function       type          ;arguments
;;         (syscall 59 (cdr function) (car function) args))))

; Calling Conventions
; default call is __stdcall for windows and __cdecl for linux (for x32)
; you can directly provide required calling convention:
(define (__cdecl    arg) (+ arg #b01000000))
(define (__stdcall  arg) (+ arg #b10000000))
(define (__fastcall arg) (+ arg #b11000000))

; а тут система типов функций, я так думаю, что проверку аргументов надо забабахать сюда?
;(define (INTEGER arg) (cons 45 arg))
;(define (FLOAT arg)   (cons 46 arg))
;(define (DOUBLE arg)  '(47 arg))

; для результата, что превышает x00FFFFFF надо использовать type-handle
; 44 - is socket but will be free
;(define type-handle 45)
; todo: (vm:cast type-constant) and start from number 1?
(define type-float  46)
(define type-double 47)
(define type-void   48)
(define type-void*  49)  ; same as type-vptr
(define type-void** 113) ; 49 + #x40

(define type-word   50)  (define type-long  type-word)

(define type-int16  51)  (define type-short type-int16)
(define type-int32  52)  (define type-int   type-int32)
(define type-int64  53)
;define type-int128 54)
;define type-int256 55)
;define type-int512 56)

(define type-unknown 62)
(define type-callable 61)
(define type-any 63)


;; OS detection
(define (uname) (syscall 63 #f #f #f))

; see also: http://www.boost.org/doc/libs/1_55_0/libs/predef/doc/html/predef/reference/boost_os_operating_system_macros.html
))