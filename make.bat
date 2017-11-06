@echo off
set PATH~=%PATH%
set MINGW32=C:\mingw\i686-6.2.0-posix-dwarf-rt_v5-rev1\mingw32\bin\
set MINGW64=C:\mingw-w64\x86_64-6.1.0-posix-seh-rt_v5-rev1\mingw64\bin\

echo -=( building %1 )=-------------------------------------------------------------------
IF "%1"==""   GOTO ALL
IF "%1"=="vm" GOTO VM
IF "%1"=="vm32" GOTO VM32
IF "%1"=="ol" GOTO OL
IF "%1"=="ol32" GOTO OL32
IF "%1"=="repl" GOTO REPL
IF "%1"=="repl32" GOTO REPL32
IF "%1"=="boot" GOTO BOOT
IF "%1"=="slim" GOTO SLIM
IF "%1"=="talkback" GOTO TALKBACK
IF "%1"=="js" GOTO JS
IF "%1"=="wasm" GOTO WASM
IF "%1"=="release" GOTO RELEASE
IF "%1"=="tests" GOTO TESTS
IF "%1"=="/help"  GOTO HELP
IF "%1"=="--help" GOTO HELP
IF "%1"=="/h"     GOTO HELP
IF "%1"=="101"    GOTO 101
IF "%1"=="111"    GOTO 111
IF "%1"=="121"    GOTO 121
IF "%1"=="android" GOTO ANDROID
GOTO:EOF


:ALL
for %%a in (
   vm.exe
   src\repl.o
   ol.exe
   src\slim.c
   olvm.js
) do if exist %%a erase %%a
CALL :VM    & if not exist vm.exe goto :fail
CALL :REPL  & fc /b repl boot.fasl > nul & if errorlevel 1 goto :fail
CALL :OL    & if not exist ol.exe goto :fail
CALL :TESTS
CALL :SLIM  & if not exist src/slim.c goto :fail
CALL :JS    & if not exist olvm.js goto :fail

echo '  `___`  '
echo '  (o,o)  '
echo '  \)  )  '
echo '___"_"___'
echo 'Build Ok.'
::CALL :101
::CALL :111
::CALL :121
GOTO:EOF

rem 101 - NetBSD x64
rem 111 - FreeBSD x64
rem 121 - OpenBSD x64

:HELP

echo "  repl                                "
echo "+------+                  ol  +------+"
echo "| REPL |--------------------->|  OL  |"
echo "+------+           ^          +------+"
echo "                   |              ^   "
echo "   vm              |              |   "
echo "+------+           |              |   "
echo "|  VM  |-----------+--------------/   "
echo "+------+                              "
echo:
GOTO:EOF

:BOOT
vm repl <src/to-c.scm >src/boot.c
GOTO:EOF

:: ======================================
:VM
echo.   *** Making virtual machine:
gcc -std=c99 -g0 -O2 -Wall -fmessage-length=0 -fno-exceptions -Wno-strict-aliasing -DNAKED_VM ^
   src/olvm.c -o "vm.exe" -lws2_32 -DHAS_PINVOKE=1 -DNDEBUG -s
GOTO:EOF

:: ======================================
:VM32
echo.   *** Making 32-bit virtual machine:
set PATH=%MINGW32%;%PATH%

gcc -std=c99 -g0 -O2 -Wall -fmessage-length=0 -fno-exceptions -Wno-strict-aliasing -DNAKED_VM ^
   src/olvm.c -o "vm32.exe" -lws2_32 -DHAS_PINVOKE=1 -m32 -DNDEBUG -s

set PATH=%PATH~%
GOTO:EOF

:: ======================================
:VM64
echo.   *** Making 64-bit virtual machine:
set PATH=%MINGW64%;%PATH%

gcc -std=c99 -g0 -O2 -Wall -fmessage-length=0 -fno-exceptions -Wno-strict-aliasing -DNAKED_VM ^
   src/olvm.c -o "vm64.exe" -lws2_32 -DHAS_PINVOKE=1 -m64 -DNDEBUG -s

set PATH=%PATH~%
GOTO:EOF


:REPL32
set PATH=%MINGW32%;%PATH%
ld -r -b binary -o src/repl32.o repl
set PATH=%PATH~%
GOTO:EOF

:REPL64
set PATH=%MINGW64%;%PATH%
ld -r -b binary -o src/repl32.o repl
set PATH=%PATH~%
GOTO:EOF


:OL
echo.   *** Making Otus Lisp:
gcc -std=c99 -g3 -Wall -fmessage-length=0 -Wno-strict-aliasing src/repl.o src/olvm.c -o "ol.exe" -lws2_32 -O2 -g2 -DHAS_PINVOKE=1
GOTO:EOF

:OL32
echo.   *** Making 32-bit Otus Lisp:
set PATH=%MINGW32%;%PATH%
gcc -std=c99 -g3 -Wall -fmessage-length=0 -Wno-strict-aliasing src/repl.o src/olvm.c -o "ol.exe" -lws2_32 -O2 -g2 -DHAS_PINVOKE=1 -m32
set PATH=%PATH~%
GOTO:EOF

:OL64
echo.   *** Making 32-bit Otus Lisp:
set PATH=%MINGW64%;%PATH%
gcc -std=c99 -g3 -Wall -fmessage-length=0 -Wno-strict-aliasing src/repl.o src/olvm.c -o "ol.exe" -lws2_32 -O2 -g2 -DHAS_PINVOKE=1 -m64
set PATH=%PATH~%
GOTO:EOF

:REPL
set VERSION=1.2
::for /f "delims=" %%a in ('git describe') do @set VERSION=%%a
vm repl - --version %VERSION% < src/ol.scm
FOR %%I IN (repl) DO FOR %%J IN (boot.fasl) DO echo ":: %%~zI -> %%~zJ"
fc /b repl boot.fasl > nul
if errorlevel 1 goto again
ld -r -b binary -o src/repl.o repl
GOTO:EOF
:again
copy boot.fasl repl
GOTO :REPL

:REPL32
set PATH=%MINGW32%;%PATH%
ld -r -b binary -o src/repl32.o -m32 repl
set PATH=%PATH~%
GOTO:EOF

:REPL64
set PATH=%MINGW64%;%PATH%
ld -r -b binary -o src/repl64.o -m32 repl
set PATH=%PATH~%
GOTO:EOF

:SLIM
echo.   *** Making slim:
vm repl src/slim.lisp >src/slim.c
GOTO:EOF

:TALKBACK
echo.   *** Making talkback:
ld -r -b binary -o ffi.o otus/ffi.scm
gcc -std=c99 -g3 -Wall -DEMBEDDED_VM -DNAKED_VM -DOLVM_FFI=1 ^
    -fmessage-length=0 -Wno-strict-aliasing -I src ^
    -D INTEGRATED_FFI ^
    src/olvm.c src/repl.o ffi.o extensions/talkback/talkback.c extensions/talkback/sample.c -o "talkback.exe" ^
    -lws2_32 -O2 -g2
GOTO:EOF

GOTO:EOF

:JS
echo.   *** Making virtual machine on js:
@set PATH=C:\Program Files\Emscripten\python\2.7.5.3_64bit\;C:\Program Files\Emscripten\emscripten\1.35.0\;%PATH%
ol src/to-c.scm >src/repl.c
call emcc src/olvm.c src/repl.c -o olvm.js -s ASYNCIFY=1 -Oz ^
     -s NO_EXIT_RUNTIME=1 ^
     -fno-exceptions -fno-rtti ^
     --memory-init-file 0 --llvm-opts "['-O3']"
GOTO:EOF

:WASM
echo.   *** Making virtual machine for webassembly:
@set PATH=C:\Program Files\Emscripten\python\2.7.5.3_64bit\;C:\Program Files\Emscripten\emscripten\1.35.0\;%PATH%
call emcc src/slim.c src/olvm.c -o olvm.html -s ASYNCIFY=1 -s WASM=1 -O1 --memory-init-file 0 --llvm-opts "['-O2']"
GOTO:EOF


:RELEASE
gcc -std=c99 -O2 -s -Wall -fmessage-length=0 -DNAKED_VM src/olvm.c -o "vm.exe" -lws2_32
gcc -std=c99 -O2 -s -Wall -fmessage-length=0 src/repl.o src/olvm.c -o "ol.exe" -lws2_32
GOTO:EOF


:101
CALL :REMOTE 10122 "NetBSD 7.0 x86-64"   gmake
GOTO:EOF

:111
CALL :REMOTE 11122 "FreeBSD 10.2 x86-64" gmake
GOTO:EOF

:121
CALL :REMOTE 12122 "OpenBSD 5.8 x86-64"  gmake
GOTO:EOF

:TEST
echo|set /p=Testing %1 ...
vm32.exe repl %1 >C:\TEMP\out
fc C:\TEMP\out %1.ok > nul
if errorlevel 1 goto fail1

vm64.exe repl %1 >C:\TEMP\out
fc C:\TEMP\out %1.ok > nul
if errorlevel 1 goto fail1

echo. Ok.
GOTO:EOF
:fail1
echo. Failed.
GOTO:EOF


:: let's do full package testing (32- and 64-bit binaries)
:TESTS

call :VM32
call :VM64
call :REPL32
call :REPL64

:: internal
set PATH=%MINGW32%;%PATH%
echo 32-bit internal test:
gcc -std=c99 -g0 -O2 -Wall -fmessage-length=0 -fno-exceptions -Wno-strict-aliasing -DNAKED_VM ^
   src/olvm.c tests/vm.c -o "test-vm32.exe" -lws2_32 -DEMBEDDED_VM=1 -m32 -DNDEBUG -s -Isrc
test-vm32.exe
if errorlevel 1 goto fail

set PATH=%PATH~%
set PATH=%MINGW64%;%PATH%
echo 64-bit internal test:
gcc -std=c99 -g0 -O2 -Wall -fmessage-length=0 -fno-exceptions -Wno-strict-aliasing -DNAKED_VM ^
   src/olvm.c tests/vm.c -o "test-vm64.exe" -lws2_32 -DEMBEDDED_VM=1 -m64 -DNDEBUG -s -Isrc
test-vm64.exe
if errorlevel 1 goto fail

set PATH=%PATH~%

:: ffi
set PATH=%MINGW32%;%PATH%
echo|set /p=32-bit ffi testing ...
ld -r -b binary -o src/repl32.o repl
gcc -std=c99 -g3 -Wall -fmessage-length=0 -Wno-strict-aliasing -I src ^
    -DHAS_PINVOKE=1 ^
    src/olvm.c src/repl32.o tests/ffi.c -o "test-ffi32.exe" -lws2_32 -O2 -g2 -m32
test-ffi32.exe tests/ffi.scm > C:\TEMP\out
fc C:\TEMP\out tests/ffi.scm.ok > nul
if errorlevel 1 (
   echo. Failed.
   goto fail
)
echo. Ok.

set PATH=%PATH~%
set PATH=%MINGW64%;%PATH%
echo|set /p=64-bit ffi testing ...
ld -r -b binary -o src/repl64.o repl
gcc -std=c99 -g3 -Wall -fmessage-length=0 -Wno-strict-aliasing -I src ^
    -DHAS_PINVOKE=1 ^
    src/olvm.c src/repl64.o tests/ffi.c -o "test-ffi64.exe" -lws2_32 -O2 -g2 -m64
test-ffi64.exe tests/ffi.scm > C:\TEMP\out
fc C:\TEMP\out tests/ffi.scm.ok > nul
if errorlevel 1 (
   echo. Failed.
   goto fail
)
echo. Ok.

set PATH=%PATH~%

:: Other tests
call :TEST tests\apply.scm
call :TEST tests\banana.scm
call :TEST tests\callcc.scm
call :TEST tests\case-lambda.scm
call :TEST tests\echo.scm
call :TEST tests\ellipsis.scm
call :TEST tests\eval.scm
call :TEST tests\factor-rand.scm
call :TEST tests\factorial.scm
call :TEST tests\fasl.scm
call :TEST tests\ff-call.scm
call :TEST tests\ff-del-rand.scm
call :TEST tests\ff-rand.scm
call :TEST tests\fib-rand.scm
call :TEST tests\hashbang.scm
call :TEST tests\iff-rand.scm
call :TEST tests\library.scm
call :TEST tests\macro-capture.scm
call :TEST tests\macro-lambda.scm
call :TEST tests\mail-order.scm
call :TEST tests\math-rand.scm
call :TEST tests\par-nested.scm
call :TEST tests\par-nested-rand.scm
call :TEST tests\par-rand.scm
call :TEST tests\perm-rand.scm
call :TEST tests\por-prime-rand.scm
call :TEST tests\por-terminate.scm
call :TEST tests\queue-rand.scm
call :TEST tests\record.scm
call :TEST tests\rlist-rand.scm
call :TEST tests\seven.scm
call :TEST tests\share.scm
call :TEST tests\stable-rand.scm
call :TEST tests\str-quote.scm
call :TEST tests\string.scm
call :TEST tests\suffix-rand.scm
call :TEST tests\theorem-rand.scm
call :TEST tests\toplevel-persist.scm
call :TEST tests\utf-8-rand.scm
call :TEST tests\vararg.scm
call :TEST tests\vector-rand.scm
call :TEST tests\numbers.scm
GOTO:EOF

:: ====================================================================================

:ANDROID
C:\android-ndk-r10e\ndk-build.cmd
GOTO:EOF

:REMOTE
echo Starting %~2...
"C:\Program Files\Oracle\VirtualBox\VBoxManage" startvm "%~2" --type headless
:: --type headless

echo Connecting to the host...
:wait
echo:>%TEMP%\empty
plink -ssh -2 -l ol -pw ol 127.0.0.1 -P %~1 -m %TEMP%\empty
if errorlevel 1 goto wait

echo Copying source files...
call :cp 127.0.0.1 %~1 Makefile
call :cp 127.0.0.1 %~1 src/olvm.c
call :cp 127.0.0.1 %~1 src/olvm.h
call :cp 127.0.0.1 %~1 repl

echo Running make...
echo %~3>%TEMP%\gmake
plink -ssh -2 -l ol -pw ol 127.0.0.1 -P %~1 -m %TEMP%\gmake

"C:\Program Files\Oracle\VirtualBox\VBoxManage" controlvm "%~2" savestate
GOTO:EOF


:cp
pscp -l ol -pw ol -P %~2 %~3 %~1:%~3
goto:eof

:fail
echo. *** Build failed!!! ***
exit