/*!
 * ## FFI - Foreign Function Interface
 *
 * A ForeignFunctionInterface (FFI) is an interface that allows calling code written
 * in one programming language, from another that is neither a superset nor a subset.
 *
 * Тут у нас реализация ffi механизма. Примеры в lib/opengl.scm, lib/sqlite.scm, etc
 *
 * FFI is Fatal Familial Insomnia too.
 *  Hmmm...
 */

// let's simplify our life - enable ffi by default
// #ifndef OLVM_FFI
// #define OLVM_FFI 1
// #endif

/*!
 * ### Source file: extensions/ffi.c
 */

// Libc and Unicode: http://www.tldp.org/HOWTO/Unicode-HOWTO-6.html
//
// The Plan9 operating system, a variant of Unix, uses UTF-8 as character encoding
//   in all applications. Its wide character type is called `Rune', not `wchar_t'.
//
// Design Issues for Foreign Function Interfaces
// http://autocad.xarch.at/lisp/ffis.html

#if OLVM_FFI

// defaults:
#ifndef OLVM_CALLABLES
#define OLVM_CALLABLES 1
#endif

// use virtual machine declaration from olvm source code
#define USE_OLVM_DECLARATION
struct heap_t;
#define ol_t heap_t

#include "olvm.c"

#define unless(...) if (! (__VA_ARGS__))

typedef struct ol_t OL;


#include <string.h>
#include <stdio.h> // temp


#if defined(__unix__) || defined(__APPLE__)
#	include <sys/mman.h>
#endif

#ifdef __ANDROID__
#	include <android/log.h>
#	define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "ol", __VA_ARGS__)
#	define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "ol", __VA_ARGS__)
#	define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "ol", __VA_ARGS__)
#else
#	define LOGE(...)
#	define LOGI(...)
#	define LOGD(...)
#endif

#ifndef OLVM_FFI_VECTORS
#define OLVM_FFI_VECTORS 1
#endif

#define TVOID         (48)
//efine TSTRING       (3)
//efine TSTRINGWIDE   (5)
//efine TBYTEVECTOR   (19)

#define TUNKNOWN      (62) // only for ffi, direct sending argument without processing
#define TANY          (63) // automatic conversion based on actual argument type

// ffi type system
#define TFLOAT        (46)
#define TDOUBLE       (47)

#define TINT8         (50)
#define TINT16        (51)
#define TINT32        (52)
#define TINT64        (53)
// 54 for 128 ?
// 55 for 256 ?

#define TUINT8        (55)
#define TUINT16       (56)
#define TUINT32       (57)
#define TUINT64       (58)
// 59 for 128 ?
// 60 for 256 ?


#define TMASK     0x00FFF

#define TCDECL    0x01000
//efine TSYSCALL    // OS/2, not supported/required
//efine TOPTLINK    // VisualAge, not supported/required
//efine TPASCAL     // Pascal, not supported/required
#define TSTDCALL  0x02000
#define TFASTCALL 0x03000
//efine TVECTORCALL // VS2013, not supported/required
//efine TDELPHI     // Delphi, not supported/required
//efine TWATCOM     // Watcom, not supported/required
//efine TSAFECALL   // Delphi/F-Pascal, not supported
//efine TTHISCALL   // c++, use in toplevel code

#define FFT_PTR   0x10000
#define FFT_REF   0x20000

// sizeof(intmax_t): // same as long long
//	arm, armv7a, armv8a, arm64: 8
//	avr: 8
//	risc-v 32, risc-v 64: 8
//	wasm: 8
//	x86, x86_64 (incl msvc): 8
//	kvx: 8
//	mips, mips64: 8
//	msp430: 8
//	ppc, ppc64, ppc64le: 8
//	raspbian, arduino: 8


#if defined(_WIN32)
#	define PUBLIC __declspec(dllexport)
#elif defined(__EMSCRIPTEN__)
#	define PUBLIC EMSCRIPTEN_KEEPALIVE
#elif defined(__unix__)
#	define PUBLIC __attribute__ ((__visibility__("default")))
#elif defined(__APPLE__)
#	define PUBLIC __attribute__ ((__visibility__("default")))
#endif


// https://en.wikipedia.org/wiki/Double-precision_floating-point_format
// However, on modern standard computers (i.e., implementing IEEE 754), one may
// in practice safely assume that the endianness is the same for floating-point
// numbers as for integers, making the conversion straightforward regardless of
// data type.
// (Small embedded systems using special floating-point formats may be another
// matter however.)
word d2ol(struct ol_t* ol, double v);        // implemented in olvm.c
double OL2D(word arg); float OL2F(word arg); // implemented in olvm.c

static
unsigned int llen(word list) 
{
	unsigned int i = 0;
	while (list != INULL)
		list = cdr(list), i++;
	return i;
}

static
unsigned int lenn(char *pos, size_t max) {
	unsigned int p = 0;
	while (p < max && *pos++) p++;
	return p;
}

static
unsigned int lenn16(short *pos, size_t max) {
	unsigned int i = 0;
	while (i < max && *pos++) i++;
	return i;
}

// note: invalid codepoint will be encoded as "?", so len is 1
#define codepoint_len(x) ({ int cp = x; \
	cp < 0x80 ? 1 : \
	cp < 0x0800 ? 2 : \
	cp < 0x10000 ? 3 : \
	cp < 0x110000 ? 4 : 1; })

static __inline__
int utf8_len(word widestr)
{
	int len = 0;
	for (int i = 1; i <= reference_size(widestr); i++)
		len += codepoint_len(value(ref(widestr, i)));
	return len;
}

#define list_length(x) llen(x)
#define vector_length(x) reference_size(x)
#define string_length(x) ({\
    word t = reference_type(x);\
    t == TSTRING ? rawstream_size(x) :\
    t == TSTRINGWIDE ? utf8_len(x) :\
    t == TSTRINGDISPATCH ? number(ref(x, 1)) :\
    0; })


// C preprocessor trick, some kind of "map":
// http://jhnet.co.uk/articles/cpp_magic
// http://stackoverflow.com/questions/319328/writing-a-while-loop-in-the-c-preprocessor
#define FIRST(a, ...) a
#define SECOND(a, b, ...) b
#define EMPTY()

#define EVAL(...)     EVAL1024(__VA_ARGS__)
#define EVAL1024(...) EVAL512(EVAL512(__VA_ARGS__))
#define EVAL512(...)  EVAL256(EVAL256(__VA_ARGS__))
#define EVAL256(...)  EVAL128(EVAL128(__VA_ARGS__))
#define EVAL128(...)  EVAL64(EVAL64(__VA_ARGS__))
#define EVAL64(...)   EVAL32(EVAL32(__VA_ARGS__))
#define EVAL32(...)   EVAL16(EVAL16(__VA_ARGS__))
#define EVAL16(...)   EVAL8(EVAL8(__VA_ARGS__))
#define EVAL8(...)    EVAL4(EVAL4(__VA_ARGS__))
#define EVAL4(...)    EVAL2(EVAL2(__VA_ARGS__))
#define EVAL2(...)    EVAL1(EVAL1(__VA_ARGS__))
#define EVAL1(...)    __VA_ARGS__

#define DEFER1(m) m EMPTY()
#define DEFER2(m) m EMPTY EMPTY()()
#define DEFER3(m) m EMPTY EMPTY EMPTY()()()
#define DEFER4(m) m EMPTY EMPTY EMPTY EMPTY()()()()

#define IS_PROBE(...) SECOND(__VA_ARGS__, 0)
#define PROBE() ~, 1

#define CAT(a,b) a ## b

#define NOT(x) IS_PROBE(CAT(_NOT_, x))
#define _NOT_0 PROBE()

#define BOOL(x) NOT(NOT(x))

#define IF_ELSE(condition) _IF_ELSE(BOOL(condition))
#define _IF_ELSE(condition) CAT(_IF_, condition)

#define _IF_1(...) __VA_ARGS__ _IF_1_ELSE
#define _IF_0(...)             _IF_0_ELSE

#define _IF_1_ELSE(...)
#define _IF_0_ELSE(...) __VA_ARGS__

#define HAS_ARGS(...) BOOL(FIRST(_END_OF_ARGUMENTS_ __VA_ARGS__)())
#define _END_OF_ARGUMENTS_() 0

#define MAP(m, first, ...)           \
  m(first)                           \
  IF_ELSE(HAS_ARGS(__VA_ARGS__))(    \
    DEFER2(_MAP)()(m, __VA_ARGS__)   \
  )()
#define _MAP() MAP
// end of C preprocessor trick

#define NEWLINE(x) x "\n\t"
#define __ASM__(...) __asm__(EVAL(MAP(NEWLINE, __VA_ARGS__)))

#ifndef __GNUC__
#define __asm__ asm
#endif


// platform defines:
// https://sourceforge.net/p/predef/wiki/Architectures/
//
// buildin assembly:
// http://locklessinc.com/articles/gcc_asm/
// http://www.agner.org/optimize/calling_conventions.pdf
// https://en.wikibooks.org/wiki/Embedded_Systems/Mixed_C_and_Assembly_Programming

// http://www.angelcode.com/dev/callconv/callconv.html
#if __amd64__ // x86-64 (LP64/LLP64)

// value returned in the rax
# if _WIN64 // Windows
// The x64 Application Binary Interface (ABI) uses a four register fast-call
// calling convention by default. Space is allocated on the call stack as a
// shadow store for callees to save those registers. There is a strict one-to-one
// correspondence between the arguments to a function call and the registers
// used for those arguments. Any argument that doesn’t fit in 8 bytes, or is
// not 1, 2, 4, or 8 bytes, must be passed by reference. There is no attempt
// to spread a single argument across multiple registers. The x87 register
// stack is unused. It may be used by the callee, but must be considered volatile
// across function calls. All floating point operations are done using the 16
// XMM registers. Integer arguments are passed in registers RCX, RDX, R8, and
// R9. Floating point arguments are passed in XMM0L, XMM1L, XMM2L, and XMM3L.
// 16-byte arguments are passed by reference. Parameter passing is described
// in detail in Parameter Passing. In addition to these registers, RAX, R10,
// R11, XMM4, and XMM5 are considered volatile. All other registers are non-volatile.
//
// The caller is responsible for allocating space for parameters to the callee,
// and must always allocate sufficient space to store four register parameters,
// even if the callee doesn’t take that many parameters.

// https://msdn.microsoft.com/en-us/library/9z1stfyw.aspx
// rcx, rdx, r8, r9 (xmm0, xmm1, ..., xmm3) are arguments,
// RAX, R10, R11, XMM4, and XMM5 are considered volatile

// http://www.gamasutra.com/view/news/171088/x64_ABI_Intro_to_the_Windows_x64_calling_convention.php

// rcx - argv
// edx - argc
// r8  - function
// r9d - type
intmax_t win64_call(int_t argv[], long argc, void* function, long type);

__ASM__("win64_call:_win64_call:",  // "int $3",
	"pushq %rbp",
	"movq  %rsp, %rbp",

	"pushq %r9",
	"andl  $-16, %esp", // выравняем стек по 16-байтовой границе

	// get count of arguments
	"xor   %rax, %rax",
	"movl  %edx, %eax",

	// get last parameter
	"leaq  -8(%rcx, %rax,8), %r10",
	"subq  $4, %rax",
	"jbe   4f",

	// довыравняем стек, так как:
	//  The stack pointer must be aligned to 16 bytes in any region of code
	//  that isn’t part of an epilog or prolog, except within leaf functions.
	"movq  %rax, %rdx",
	"andq  $1, %rdx",
	"leaq  -16(%rsp,%rdx,8), %rsp",

"1:",
	"pushq (%r10)",
	"subq  $8, %r10",
	"decq  %rax",
	"jnz   1b",

	// 4. заполним обычные rcx, rdx, ... не проверяя количество аргументов и их
	// тип, так будет быстрее
"4:",
	"movq  %r8, %rax",
	"movsd 24(%rcx), %xmm3", "mov  24(%rcx), %r9",
	"movsd 16(%rcx), %xmm2", "mov  16(%rcx), %r8",
	"movsd  8(%rcx), %xmm1", "mov  8(%rcx), %rdx",
	"movsd  0(%rcx), %xmm0", "mov  0(%rcx), %rcx",
	"subq $32, %rsp", // extra free space for call
	"call *%rax",
	// вернем результат
	"cmpl $46, -8(%rbp)", // TFLOAT
	"je   52f",
	"cmpl $47, -8(%rbp)", // TDOUBLE
	"je   52f",
"9:",
	"leave",
	"ret",

// "51:",
// 	"cvtss2sd %xmm0, %xmm0", // float->double
"52:",
	"movsd %xmm0, (%rsp)",
	"pop   %rax", // можно попать, так как уже пушнули один r9 вверху (оптимизация)
	"jmp   9b");

# else      // System V (unix, linux, osx)
// rdi: argv[]
// rsi: ad[]
// edx: i
// ecx: d
// r8:  mask
// r9: function
// 16(rbp): type
intmax_t nix64_call(int_t argv[], double ad[], long i, long d, long mask, void* function, long type);

__ASM__("nix64_call:_nix64_call:", // "int $3",
	"pushq %rbp",
	"movq  %rsp, %rbp",

	"pushq %r9",
	"andq  $-16, %rsp",
	// 1. если есть флоаты, то заполним их
"1:",
	"testq %rcx, %rcx",
	"jz    2f",
	"movsd  0(%rsi), %xmm0",
	"movsd  8(%rsi), %xmm1",
	"movsd 16(%rsi), %xmm2",
	"movsd 24(%rsi), %xmm3",
	"movsd 32(%rsi), %xmm4",
	"movsd 40(%rsi), %xmm5",
	"movsd 48(%rsi), %xmm6",
	"movsd 56(%rsi), %xmm7",
//	"cmpq  $9,%rcx", // temp
//	"jne   2f",      // temp
//	"int   $3",      // temp
	// 2. проверим на "стековые" аргументы
"2:",
	"xorq  %rax, %rax",
	"subq  $8, %rcx",
	"cmovs %rax, %rcx", // = max(rcx-8, 0)
	"cmpl  $6, %edx",
	"cmova %rdx, %rax", //
	"addq  %rcx, %rax", // = max(rdx-6, 0) + max(rcx-8, 0)
	"testq %rax, %rax",
	"jz    4f", // no agruments for push to stack
	// забросим весь оверхед в стек с учетом очередности и маски
"3:",
	"andq  $1, %rax",
	"leaq  -16(%rsp,%rax,8),%rsp", // выравнивание стека по 16-байтной границе

	"leaq  -8(%rdi,%rdx,8), %rax",
	"leaq  56(%rsi,%rcx,8), %rsi", // last float argument  (if any) 56=8*8-8

	"subq  $6, %rdx",
"31:",
	"shrq  %r8",
	"jc    33f", // bit 0 was set, so push float value
"32:", // push fixed
	"cmpq  $0, %rdx",
	"jle   31b", // нету больше аргументов для fixed пуша
	"pushq (%rax)",
	"subq  $8, %rax",
	"decq  %rdx",
	"jnz   31b",
	"cmpq  $0, %rcx",
	"jle   4f",
	"jmp   31b",
"33:", // push float
	"cmpq  $0, %rcx",
	"jle   31b", // нету больше аргументов для float пуша
	"pushq (%rsi)",
	"subq  $8, %rsi",
	"decq  %rcx",
	"jnz   31b",
	"cmpq  $0, %rdx",
	"jle   4f",
	"jmp   31b",

	// 4. не проверяем количество аргументов, так будет быстрее
"4:",
	"movq  40(%rdi), %r9",
	"movq  32(%rdi), %r8",
	"movq  24(%rdi), %rcx",
	"movq  16(%rdi), %rdx",
	"movq   8(%rdi), %rsi",
	"movq   0(%rdi), %rdi",

	"callq *-8(%rbp)",

	// вернем результат
	"cmpl $46, 16(%rbp)", // TFLOAT
	"je   6f",
	"cmpl $47, 16(%rbp)", // TDOUBLE
	"je   6f",
"9:",
	"leave",
	"ret",

// "5:",
// 	"cvtss2sd %xmm0, %xmm0", // float->double
"6:",
	"movsd %xmm0, (%rsp)",
	"popq  %rax", // corresponded push not required (we already pushed %r9)
	"jmp   9b");

// RDI, RSI, RDX, RCX (R10 in the Linux kernel interface[17]:124), R8, and R9
// while XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6 and XMM7 are used for floats
# endif

#elif __i386__ // ILP32
// value returned in the edx:eax

// CDECL / STDCALL
// The cdecl (which stands for C declaration) is a calling convention
// that originates from the C programming language and is used by many
// C compilers for the x86 architecture.[1] In cdecl, subroutine
// arguments are passed on the stack. Integer values and memory
// addresses are returned in the EAX register, floating point values in
// the ST0 x87 register. Registers EAX, ECX, and EDX are caller-saved,
// and the rest are callee-saved. The x87 floating point registers ST0
// to ST7 must be empty (popped or freed) when calling a new function,
// and ST1 to ST7 must be empty on exiting a function. ST0 must also be
// empty when not used for returning a value.
//
// В нашем случае мы так или иначе восстанавливаем указатель стека, так что
// функция x86_call у нас будет универсальная cdecl/stdcall
intmax_t x86_call(int_t argv[], long i, void* function, long type);

__ASM__("x86_call:_x86_call:", //"int $3",
	"pushl %ebp",
	"movl  %esp, %ebp",

	"movl  12(%ebp), %ecx",
	"test  %ecx, %ecx",
	"jz    1f",
	"movl  8(%ebp), %eax",
	"leal  -4(%eax,%ecx,4),%eax",
"0:",
	"pushl (%eax)",
	"subl  $4, %eax",
	"decl  %ecx",
	"jnz   0b",
"1:",
	"call  *16(%ebp)",

	"movl  20(%ebp), %ecx", // проверка возвращаемого типа
	"cmpl  $46, %ecx",      // TFLOAT
	"je    3f",
	"cmpl  $47, %ecx",      // TDOUBLE
	"je    4f",
"9:",
	"leave",
	"ret",

// с плавающей точкой float
"3:", // float
	"pushl %eax",
	"fstps (%esp)",
	"popl  %eax",
	"jmp   9b",
// с плавающей точкой double
"4:", // double
	"pushl %edx",
	"pushl %eax",
	"fstpl (%esp)",
	"popl  %eax",
	"popl  %edx",
	"jmp   9b");


#elif __aarch64__

// current limitation: no more than 8 integer and 8 floating point values
__attribute__((naked))
uintmax_t arm64_call(int_t argv[], double ad[],
                     long i, long d,
                     void* function, long type)
// x0: argv
// x1: ad
// x2: i
// x3: d
// x4: function
// x5: type
{
__ASM__(//"brk #0",
	"stp  x29, x30, [sp, -16]!",
	"mov  x29, sp",

	"mov  x9, x4",

	// будем заполнять регистры с плавающей запятой по 4 или 8 (в целях оптимизации)
	"cmp x3, #0",  // (count of fp values)
	"beq .Lno_more_floats",
	"ldr d0, [x1]",
	"ldr d1, [x1, #8]",
	"ldr d2, [x1, #16]",
	"ldr d3, [x1, #24]",

	"cmp x3, #4",
	"beq .Lno_more_floats",
	"ldr d4, [x1, #32]",
	"ldr d5, [x1, #40]",
	"ldr d6, [x1, #48]",
	"ldr d7, [x1, #56]",

	// "cmp r3, #8",
	// "ble .Lnofloats",
	// "vldr.32 s8, [r1, #32]",
	// "vldr.32 s9, [r1, #36]",
	// "vldr.32 s10, [r1, #40]",
	// "vldr.32 s11, [r1, #44]",
	// "vldr.32 s12, [r1, #48]",
	// "vldr.32 s13, [r1, #52]",
	// "vldr.32 s14, [r1, #56]",
	// "vldr.32 s15, [r1, #60]",

	// аналогично сделаем с целочисленными аргументами
".Lno_more_floats:",
	"cmp x2, #0",
	"beq .Lno_integers",

	"cmp x2, #4",
	"ble .Lno_more_integers",

	"ldr x4, [x0, #32]",
	"ldr x5, [x0, #40]",
	"ldr x6, [x0, #48]",
	"ldr x7, [x0, #56]",

	// ...

".Lno_more_integers:",
	"ldr x3, [x0, #24]",
	"ldr x2, [x0, #16]",
	"ldr x1, [x0, #8]",
	"ldr x0, [x0]",

".Lno_integers:",
	"blr x9", // SP mod 16 = 0.  The stack must be quad-word aligned.

	// ...
	"ldp  x29, x30, [sp], 16",
	"ret");
}

#elif __arm__
// http://ru.osdev.wikia.com/wiki/Категория:Архитектура_ARM
// https://msdn.microsoft.com/ru-ru/library/dn736986.aspx - Обзор соглашений ABI ARM (Windows)
// Procedure Call Standard for the ARM®  Architecture
//  http://infocenter.arm.com/help/topic/com.arm.doc.ihi0042f/IHI0042F_aapcs.pdf

# ifndef __ARM_PCS_VFP
// gcc-arm-linux-gnueabi: -mfloat-abi=softfp options (and -mfloat-abi=soft ?)

__attribute__((naked))
uintmax_t arm32_call(int_t argv[], long i,
                     void* function)
{
__ASM__(
#ifndef __ANDROID__
	"arm32_call:_arm32_call:",
#endif
	// "BKPT",
	// !!! stack must be 8-bytes aligned, so let's push even arguments count
	"push {r4, r5, r6, lr}",
	"mov r4, sp", // save sp

	// note: at public interface stack must be double-word aligned (SP mod 8 = 0).
	// finally, sending regular (integer) arguments
	"cmp r1, #4",  // if (i > 4)
	"ble .Lnoextraregs",      // todo: do the trick -> jmp to corresponded "ldrsh" instruction based on r3 value

	// "add r5, r0, r1, asl #2",
	"lsl r5, r1, #2",
	"add r5, r0, r5",
	"sub r5, r5, #4",

	// stack alignment
	"and r3, r1, #1",         // попадет ли новый стек на границу двойного слова?
	"sub sp, sp, r3, asl #2", // если да, то скорректируем стек на 4 байта вниз.

".Lextraregs:", // push argv[i]
	"ldr r3, [r5]",
	//"rev16 r3, r3",
	"push {r3}",
	"sub r1, r1, #1",
	"sub r5, r5, #4",
	"cmp r1, #4",
	"bgt .Lextraregs",

// 	"mov r5, r0",
// ".Lextraregs:", // push argv[i]
// 	"mov r3, 0",
// 	"push {r3}",
// 	"sub r1, r1, #1",
// 	"add r5, r5, #4",
// 	"cmp r1, #4",
// 	"bgt .Lextraregs",

".Lnoextraregs:",
	"mov r5, r2", // function

	"ldr r3, [r0,#12]", // save all 4 registers without checking
	"ldr r2, [r0, #8]",
	"ldr r1, [r0, #4]",
	"ldr r0, [r0, #0]",
	// call the function

#ifdef __ARM_ARCH_4T__ // armv4t
	"mov lr, pc",
	"bx r5",
#else
	"blx r5",
#endif
	"mov sp, r4", // restore sp

	// all values: int, long, float and double returning in r0+r1
	"pop {r4, r5, r6, pc}");
//	"bx lr");
}

# else
// gcc-arm-linux-gnueabihf: -mfloat-abi=hard and -D__ARM_PCS_VFP options

// __ARM_PCS_VFP, __ARM_PCS
// __ARM_ARCH_7A__
//

__attribute__((naked))
uintmax_t arm32_call(int_t argv[], float af[],
                     long i, long f,
                     void* function, long type)
__ASM__(
#ifndef __ANDROID__
	"arm32_call:_arm32_call:",
#endif
	// "BKPT",
	// r0: argv, r1: af, r2: i, r3: ad, f: [sp, #12], g: [sp, #16]
	// r4: saved sp
	// r5: temporary
	"stmfd   sp!, {r4, r5, lr}",

	// check floats
#ifdef __ARM_PCS_VFP // gnueabihf, -mfloat-abi=hard
	"cmp r3, #0",  // f (count of floats)
	"beq .Lnofloats",
	// будем заполнять регистры с плавающей запятой по 4 или 8 (в целях оптимизации)
	// https://developer.arm.com/technologies/floating-point
	"vldr.32 s0, [r1]",
	"vldr.32 s1, [r1, #4]",
	"vldr.32 s2, [r1, #8]",
	"vldr.32 s3, [r1, #12]",
	"cmp r3, #4",
	"ble .Lnofloats",
	"vldr.32 s4, [r1, #16]",
	"vldr.32 s5, [r1, #20]",
	"vldr.32 s6, [r1, #24]",
	"vldr.32 s7, [r1, #28]",
	"cmp r3, #8",
	"ble .Lnofloats",
	"vldr.32 s8, [r1, #32]",
	"vldr.32 s9, [r1, #36]",
	"vldr.32 s10, [r1, #40]",
	"vldr.32 s11, [r1, #44]",
	"vldr.32 s12, [r1, #48]",
	"vldr.32 s13, [r1, #52]",
	"vldr.32 s14, [r1, #56]",
	"vldr.32 s15, [r1, #60]",
	// todo: s16-s32 must be preserved across subroutine calls (ARM ABI)!
	//  save this situation as flag and restore this regs after function call if so
	// "cmp r3, 16",
	// "ble .Lnofloats",
	// "vldr.32 s16, [r1, #48]",
	// "vldr.32 s17, [r1, #52]",
	// "vldr.32 s18, [r1, #56]",
	// "vldr.32 s19, [r1, #60]",
	// "vldr.32 s20, [r1, #48]",
	// "vldr.32 s21, [r1, #52]",
	// "vldr.32 s22, [r1, #56]",
	// "vldr.32 s23, [r1, #60]",
	// "vldr.32 s24, [r1, #48]",
	// "vldr.32 s25, [r1, #52]",
	// "vldr.32 s26, [r1, #56]",
	// "vldr.32 s27, [r1, #60]",
	// "vldr.32 s28, [r1, #48]",
	// "vldr.32 s29, [r1, #52]",
	// "vldr.32 s30, [r1, #56]",
	// "vldr.32 s31, [r1, #60]",
".Lnofloats:",
#endif

	"mov r4, sp", // save sp
	// note: at public interface stack must be double-word aligned (SP mod 8 = 0).
	"sub r1, sp, r2, asl #2",// try to predict stack alighnment (к текущему стеку прибавим количество аргументов * размер слова)
	"and r1, r1, #4", // попадает ли стек на границу слова?
	"sub sp, sp, r1", // если да, то на слово его и опустим
	// finally, sending regular (integer) arguments
	"cmp r2, #4",  // if (i > 4)
	"ble .Lnoextraregs",      // todo: do the trick -> jmp to corrsponded "ldrsh" instruction based on r3 value
	"add r1, r0, r2, asl #2",
	"sub r1, r1, #4",
".Lextraregs:", // push argv[i]
	"ldr r3, [r1]",
	"push {r3}",
	"sub r2, r2, #1",
	"sub r1, r1, #4",
	"cmp r2, #4",
	"bgt .Lextraregs",
	// todo: arrange stack pointer to dword

".Lnoextraregs:",
	"ldr r3, [r0,#12]", // save all 4 registers without checking
	"ldr r2, [r0, #8]",
	"ldr r1, [r0, #4]",
	"ldr r0, [r0, #0]",
	// call the function
	"ldr r5, [r4,#12]", // function
#ifdef __ARM_ARCH_4T__ // armv4t
	"mov lr, pc",
	"bx r5",
#else
	"blx r5", // call this function
#endif
	"mov sp, r4", // restore sp

#ifdef __ARM_PCS_VFP
	"ldr r5, [r4,#16]", // return type
	"cmp r5, #46",          // TFLOAT
	"beq .Lfconv",
	"cmp r5, #47",          // TDOUBLE
	"beq .Lfconv",
".Lfconv:",
	"vmov r0, s0",
	"vmov r1, s1",
#endif

".Lret:",
	// all values: int, long, float and double returns in r0+r1
	"ldmfd   sp!, {r4, r5, pc}",
);
# endif
#elif __EMSCRIPTEN__

typedef intmax_t ret_t;
static
ret_t asmjs_call(int_t args[], int fmask, void* function, int type)
{
//	printf("asmjs_call(%p, %d, %p, %d)\n", args, fmask, function, type);

	switch (fmask) {
	case 0b1:
		return (ret_t)(word)((word (*)  ())
					 function) ();

	case 0b10:
		return (ret_t)(word)((word (*)  (int_t))
					 function) (args[ 0]);

	case 0b100:
		return (ret_t)(word)((word (*)  (int_t, int_t))
		             function) (args[ 0], args[ 1]);
	case 0b1000:
		return (ret_t)(word)((word (*)  (int_t, int_t, int_t))
		             function) (args[ 0], args[ 1], args[ 2]);
	case 0b10000:
		return (ret_t)(word)((word (*)  (int_t, int_t, int_t, int_t))
		             function) (args[ 0], args[ 1], args[ 2], args[ 3]);
	case 0b11111:
//		printf("%f/%f/%f/%f\n", *(float*)&args[ 0], *(float*)&args[ 1], *(float*)&args[ 2], *(float*)&args[ 3]);
		return (ret_t)(word)((word (*)  (float, float, float, float))
		             function) (*(float*)&args[ 0], *(float*)&args[ 1], *(float*)&args[ 2], *(float*)&args[ 3]);
	case 0b100000:
		return (ret_t)(word)((word (*)  (int_t, int_t, int_t, int_t, int_t))
		             function) (args[ 0], args[ 1], args[ 2], args[ 3],
		                        args[ 4]);
	case 0b1000000:
		return (ret_t)(word)((word (*)  (int_t, int_t, int_t, int_t, int_t, int_t))
		             function) (args[ 0], args[ 1], args[ 2], args[ 3],
		                        args[ 4], args[ 5]);
	case 0b10000000:
		return (ret_t)(word)((word (*)  (int_t, int_t, int_t, int_t, int_t, int_t,
	                                  int_t))
	                 function) (args[ 0], args[ 1], args[ 2], args[ 3],
	                            args[ 4], args[ 5], args[ 6]);
	case 0b100000000:
		return (ret_t)(word)((word (*)  (int_t, int_t, int_t, int_t, int_t, int_t,
	                                  int_t, int_t))
	                 function) (args[ 0], args[ 1], args[ 2], args[ 3],
	                            args[ 4], args[ 5], args[ 6], args[ 7]);
	case 0b1000000000:
		return (ret_t)(word)((word (*)  (int_t, int_t, int_t, int_t, int_t, int_t,
	                                  int_t, int_t, int_t))
	                 function) (args[ 0], args[ 1], args[ 2], args[ 3],
	                            args[ 4], args[ 5], args[ 6], args[ 7],
	                            args[ 8]);
	// 10:
	case 0b10000000000:
		return (ret_t)(word)((word (*)  (int_t, int_t, int_t, int_t, int_t, int_t,
	                                  int_t, int_t, int_t, int_t))
	                 function) (args[ 0], args[ 1], args[ 2], args[ 3],
	                            args[ 4], args[ 5], args[ 6], args[ 7],
	                            args[ 8], args[ 9]);
	case 0b100000000000:
		return (ret_t)(word)((word (*)  (int_t, int_t, int_t, int_t, int_t, int_t,
	                                  int_t, int_t, int_t, int_t, int_t))
	                 function) (args[ 0], args[ 1], args[ 2], args[ 3],
	                            args[ 4], args[ 5], args[ 6], args[ 7],
	                            args[ 8], args[ 9], args[10]);
	case 0b1000000000000:
		return (ret_t)(word)((word (*)  (int_t, int_t, int_t, int_t, int_t, int_t,
	                                  int_t, int_t, int_t, int_t, int_t, int_t))
	                 function) (args[ 0], args[ 1], args[ 2], args[ 3],
	                            args[ 4], args[ 5], args[ 6], args[ 7],
	                            args[ 8], args[ 9], args[10], args[11]);
	default: fprintf(stderr, "Unsupported parameters count for ffi function: %ud", fmask);
		return 0;
	};
}

#else // other platforms

// http://byteworm.com/2010/10/12/container/ (lambdas in c)

// https://en.wikipedia.org/wiki/X86_calling_conventions
// x86 conventions: cdecl, syscall(OS/2), optlink(IBM)
// pascal(OS/2, MsWin 3.x, Delphi), stdcall(Win32),
// fastcall(ms), vectorcall(ms), safecall(delphi),
// thiscall(ms)

	// todo: ограничиться количеством функций поменьше
	//	а можно сделать все в одной switch:
	// todo: а можно лямбдой оформить и засунуть эту лябмду в функцию еще в get-proc-address
	// todo: проанализировать частоту количества аргументов и переделать все в
	//   бинарный if

	// __attribute__((stdcall))
/*						__stdcall // gcc style for lambdas in pure C
	int (*stdcall[])(char*) = {
			({ int $(char *str){ printf("Test: %s\n", str); } $; })
	};*/
	// http://www.agner.org/optimize/calling_conventions.pdf
	#define CALL(conv) \
		switch (i) {\
		case  0: return (ret_t)(word)((conv word (*)  ())\
						 function) ();\
		case  1: return (ret_t)(word)((conv word (*)  (word))\
						 function) (args[ 0]);\
		case  2: return (ret_t)(word)((conv word (*)  (word, word))\
		                 function) (args[ 0], args[ 1]);\
		case  3: return (ret_t)(word)((conv word (*)  (word, word, word))\
		                 function) (args[ 0], args[ 1], args[ 2]);\
		case  4: return (ret_t)(word)((conv word (*)  (word, word, word, word))\
		                 function) (args[ 0], args[ 1], args[ 2], args[ 3]);\
		case  5: return (ret_t)(word)((conv word (*)  (word, word, word, word, word))\
		                 function) (args[ 0], args[ 1], args[ 2], args[ 3],\
		                            args[ 4]);\
		case  6: return (ret_t)(word)((conv word (*)  (word, word, word, word, word, word))\
		                 function) (args[ 0], args[ 1], args[ 2], args[ 3],\
		                            args[ 4], args[ 5]);\
		case  7: return (ret_t)(word)((conv word (*)  (word, word, word, word, word, word, \
		                                  word))\
		                 function) (args[ 0], args[ 1], args[ 2], args[ 3],\
		                            args[ 4], args[ 5], args[ 6]);\
		case  8: return (ret_t)(word)((conv word (*)  (word, word, word, word, word, word, \
		                                  word, word))\
		                 function) (args[ 0], args[ 1], args[ 2], args[ 3],\
		                            args[ 4], args[ 5], args[ 6], args[ 7]);\
		case  9: return (ret_t)(word)((conv word (*)  (word, word, word, word, word, word, \
		                                  word, word, word))\
		                 function) (args[ 0], args[ 1], args[ 2], args[ 3],\
		                            args[ 4], args[ 5], args[ 6], args[ 7],\
		                            args[ 8]);\
		case 10: return (ret_t)(word)((conv word (*)  (word, word, word, word, word, word, \
		                                  word, word, word, word))\
		                 function) (args[ 0], args[ 1], args[ 2], args[ 3],\
		                            args[ 4], args[ 5], args[ 6], args[ 7],\
		                            args[ 8], args[ 9]);\
		case 11: return (ret_t)(word)((conv word (*)  (word, word, word, word, word, word, \
		                                  word, word, word, word, word))\
		                 function) (args[ 0], args[ 1], args[ 2], args[ 3],\
		                            args[ 4], args[ 5], args[ 6], args[ 7],\
		                            args[ 8], args[ 9], args[10]);\
		case 12: return (ret_t)(word)((conv word (*)  (word, word, word, word, word, word, \
		                                  word, word, word, word, word, word))\
		                 function) (args[ 0], args[ 1], args[ 2], args[ 3], \
		                            args[ 4], args[ 5], args[ 6], args[ 7], \
		                            args[ 8], args[ 9], args[10], args[11]);\
		default: fprintf(stderr, "Unsupported parameters count for ffi function: %d", i);\
			return 0;\
		};
#endif


// ol->c convertors
static
int_t from_uint(word arg) {
	assert (is_reference(arg));
	// так как в стек мы все равно большое число сложить не сможем,
	// то возьмем только то, что влазит (первые два члена)
	return (car(arg) >> 8) | ((car(cdr(arg)) >> 8) << VBITS);
}

#if UINTPTR_MAX != 0xffffffffffffffff // 32-bit platform math
static
intmax_t from_ulong(word arg) {
	assert (is_reference(arg));
	intmax_t v = car(arg) >> 8;
	int shift = VBITS;
	while (cdr(arg) != INULL) {
		v |= ((intmax_t)(car(cdr(arg)) >> 8) << shift);
		shift += VBITS;
		arg = cdr(arg);
	}
	return v;
};
#endif

static
#if UINTPTR_MAX != 0xffffffffffffffff // 32-bit machines (__SIZEOF_PTRDIFF_T__ == 4)
long
#endif
long from_rational(word arg) {
	word* pa = (word*)car(arg);
	word* pb = (word*)cdr(arg);

	#if UINTPTR_MAX != 0xffffffffffffffff // 32-bit machines
	long
	#endif
	long a = 0;
	if (is_value(pa))
		a = enum(pa);
	else {
		switch (reference_type(pa)) {
		case TINTP:
			a = +untoi(pa);
			break;
		case TINTN:
			a = -untoi(pa);
			break;
		}
	}

	#if UINTPTR_MAX != 0xffffffffffffffff // 32-bit machines
	long
	#endif
	long b = 1;
	if (is_value(pb))
		b = enum(pb);
	else {
		switch (reference_type(pb)) {
		case TINTP:
			b = +untoi(pb);
			break;
		case TINTN:
			b = -untoi(pb);
			break;
		}
	}

	return (a / b);
}

static
int to_int(word arg) {
	if (is_enum(arg))
		return enum(arg);

	switch (reference_type(arg)) {
	case TINTP:
		return (int)+from_uint(arg);
	case TINTN:
		return (int)-from_uint(arg);
	case TRATIONAL:
		return (int) from_rational(arg);
	case TCOMPLEX:
		return to_int(car(arg)); // return real part of value
	default:
		E("can't get int from %d", reference_type(arg));
	}

	return 0;
}

/*
static
long to_long(word arg) {
	if (is_enum(arg))
		return enum(arg);

	switch (reference_type(arg)) {
	case TINTP:
		return (long)+from_int(arg);
	case TINTN:
		return (long)-from_int(arg);
	case TRATIONAL:
		return (long) from_rational(arg);
	case TCOMPLEX:
		return to_long(car(arg)); // return real part of value
	default:
		E("can't get int from %d", reference_type(arg));
	}

	return 0;
}*/

#	ifndef __EMSCRIPTEN__
// stupid clang does not support builtin in functions function,
// so we need to do this stupid workaround
static __inline__
word* new_npair(word**fpp, int type, intmax_t value) {
    word* fp = *fpp;
    intmax_t a = value >> VBITS;
    word* b = (a > VMAX) ? new_npair(&fp, TPAIR, a) : new_pair(TPAIR, I(a & VMAX), INULL);
    word* p = new_pair(type, I(value & VMAX), b);
    *fpp = fp;
    return p;
}
static __inline__
word* ll2ol(word**fpp, intmax_t val) {
    intmax_t x5 = val;
    intmax_t x6 = x5 < 0 ? -x5 : x5;
    int type = x5 < 0 ? TINTN : TINTP;
    return (x6 > VMAX) ? new_npair(fpp, type, x6) : (word*)make_value(x5 < 0 ? TENUMN : TENUMP, x6);
};

static __inline__
word* new_unpair(word**fpp, int type, uintmax_t value) {
    word* fp = *fpp;
    uintmax_t a = value >> VBITS;
    word* b = (a > VMAX) ? new_unpair(&fp, TPAIR, a) : new_pair(TPAIR, I(a & VMAX), INULL);
    word* p = new_pair(type, I(value & VMAX), b);
    *fpp = fp;
    return p;
}
static __inline__
word* ul2ol(word**fpp, intmax_t val) {
    uintmax_t x = val;
    return (x > VMAX) ? new_unpair(fpp, TINTP, x) : (word*)make_value(x < 0 ? TENUMN : TENUMP, x);
};

#	endif

/////////////////////////////////////////////////////////////////////////////////////
// Главная функция механизма ffi:
PUBLIC
__attribute__((used))
word* OL_ffi(OL* this, word* arguments)
{
	// a - function address
	// b - arguments (may be a pair with type in car and argument in cdr - not yet done)
	// c - '(return-type . argument-types-list)
	word A = car(arguments); arguments = (word*)cdr(arguments); // function
	word B = car(arguments); arguments = (word*)cdr(arguments); // rtty
	word C = car(arguments); arguments = (word*)cdr(arguments); // args

	assert (is_vptr(A));
	assert (B != INULL && (is_reference(B) && reference_type(B) == TPAIR));
	assert (C == INULL || (is_reference(C) && reference_type(C) == TPAIR));

	void *function = (void*)car(A);  assert (function);
	int returntype = value(car(B));

	// note: not working under netbsd. should be fixed.
	// static_assert(sizeof(float) <= sizeof(word), "float size should not exceed the word size");

#ifdef __EMSCRIPTEN__
	int fmask = 0; // маска для типа аргументов, (0-int, 1-float) + старший бит-маркер (установим в конце)
#endif

#if (__amd64__ && (__linux__ || __APPLE__)) || __aarch64__ // LP64
	// *nix x64 содержит отдельный массив чисел с плавающей запятой
	double ad[18];
	int d = 0;     // количество аргументов для ad
	long floatsmask = 0; // маска для аргументов с плавающей запятой
#elif __ARM_EABI__ && __ARM_PCS_VFP // -mfloat-abi=hard (?)
	// арм int и float складывает в разные регистры (r?, s?), если сопроцессор есть
	float af[18]; // для флоатов отдельный массив
	int f = 0;     // количество аргументов для af
#elif _WIN32
	// nothing special for Windows (both x32 and x64)
#endif

	// 1. do memory precalculations and count number of arguments
	int words = 0;
	int i = 0;     // актуальное количество аргументов

	word* p = (word*)C;   // ol arguments
	word* t = (word*)cdr (B); // rtty

	while ((word)p != INULL && (word)t != INULL) { // пока есть аргументы
		int type = value(car(t)); // destination type
		word arg = (word)car(p);

//		again:
		if (is_reference(arg)) {
			if (type & (FFT_PTR|FFT_REF)) {
                int cnt =
                    reference_type(arg) == TPAIR ? list_length(arg) :
                    reference_type(arg) == TVECTOR ? vector_length(arg) :
                    0;
                int listq = reference_type(arg) == TPAIR;
				int atype = type &~(FFT_REF|FFT_PTR); // c data type
                
                int len = // in bytes
                atype == TSTRING ? ({
                    int l = 0;
                    for (int i = 0; i < cnt; i++) {
                        word str = car(arg);
                        l += reference_type(str) == TSTRING ? rawstream_size(str) + 1 + W :
                             reference_type(str) == TSTRINGWIDE ? utf8_len(str) + 1  + W :
                             reference_type(str) == TSTRINGDISPATCH ? number(ref(str, 1)) + 1 + W :
                             0;
                        arg = listq ? cdr(arg) : arg++;
                    }
                    l + cnt*W; // size for array + size of all strings
                }) : cnt;

				words += ((len *
					atype == TINT8 || atype == TUINT8 ? sizeof(int8_t) :
					atype == TINT16 || atype == TUINT16 ? sizeof(int16_t) :
					atype == TINT32 || atype == TUINT32 ? sizeof(int32_t) :
					atype == TINT64 || atype == TUINT64 ? sizeof(int64_t) :
					atype == TFLOAT ? sizeof(float) :
					atype == TDOUBLE ? sizeof(double) :
					atype == TVPTR && !(reference_type(arg) == TVPTR || reference_type(arg) == TBYTEVECTOR) ? sizeof(void*) :
                    atype == TSTRING ? sizeof(char) :
					0) + W - 1) / W + 1;
			}
			else
			switch (type) {
				case TANY:
					D("TODO: TANY processing");
					break;
				case TSTRING:
					switch (reference_type (arg)) {
						case TSTRING:
							words += reference_size(arg) + 1;
							break;
						case TSTRINGWIDE:
							words += (utf8_len(arg) + W - 1) / W + 1;
							break;
                        case TSTRINGDISPATCH:
                            words += (number(ref(arg, 1)) + W - 1) / W + 1;
                            break;
					}
					break;
				case TSTRINGWIDE:
                    // todo: calculate string and stringdispatch
						if (reference_type (arg) == TSTRINGWIDE)
							words += 2 * reference_size(arg) + 1;
					break;
			}
		}

		i++;
#if UINT64_MAX > UINTPTR_MAX // 32-bit machines
		if (type == TINT64 || type == TUINT64 || type == TDOUBLE)
			i++;
#endif
		p = (word*)cdr(p); // (cdr p)
		t = (word*)cdr(t); // (cdr t)
	}
	if ((word)t != INULL || (word)p != INULL) {
		E("Arguments count mismatch", 0);
		return (word*)IFALSE;
	}

	// ensure that all arguments will fit in heap
	if (words > (this->end - this->fp)) {
        size_t a = OL_pin(this, A);
        size_t b = OL_pin(this, B);
        size_t c = OL_pin(this, C);

		this->gc(this, words);

        A = OL_unpin(this, a);
        B = OL_unpin(this, b);
        C = OL_unpin(this, c);
	}

	word* fp = this->fp;
	int_t* args = __builtin_alloca((i > 16 ? i : 16) * sizeof(int_t)); // minimum - 16 words for arguments
	i = 0;

	// 2. prepare arguments to push
	p = (word*)C;   // ol arguments
	t = (word*)cdr (B); // rtty
	int has_wb = 0; // has write-back in arguments (speedup)

	while ((word)p != INULL) { // пока есть аргументы
		assert (reference_type(p) == TPAIR); // assert(list)
		assert (reference_type(t) == TPAIR); // assert(list)

		int type = value(car(t)); // destination type
		word arg = (word) car(p);

/*		// todo: add argument overriding as PAIR as argument value
		if (thetype (p[1]) == TPAIR) {
			type = value (((word*)p[1])[1]);
			arg = ((word*)p[1])[2];
		}*/

		args[i] = 0; // обнулим (теперь дальше сможем симулировать обнуление через break)
#if (__amd64__ && (__linux__ || __APPLE__)) || __aarch64__ // LP64
		floatsmask <<= 1; // подготовим маску к следующему аргументу
#endif

		if (arg == IFALSE) // #false is universal "0" value
		switch (type) {
#if UINT64_MAX > UINTPTR_MAX
		case TINT64:
		case TUINT64:
		case TDOUBLE:
			args[++i] = 0; 	// for 32-bits: double and longlong fills two words
			break;
#endif
#if (__amd64__ && (__linux__ || __APPLE__)) || __aarch64__
		case TFLOAT:
		case TDOUBLE:
			ad[d++] = 0;
			floatsmask|=1; --i;
			break;
#endif
		}
		else
		switch (type) {

		// целочисленные типы:
		case TINT8:  case TUINT8:
		case TINT16: case TUINT16:
		case TINT32: case TUINT32:
#if UINT64_MAX == UINTPTR_MAX // 64-bit machines
		case TINT64: case TUINT64:
#endif
		tint:
			if (is_enum(arg))
				args[i] = enum(arg);
			else
			switch (reference_type(arg)) {
			case TINTP:
				args[i] = +from_uint(arg);
				break;
			case TINTN:
				args[i] = -from_uint(arg);
				break;
			case TRATIONAL: //?
				*(int64_t*)&args[i] = from_rational(arg);
#if UINT64_MAX > UINTPTR_MAX // sizeof(long long) > sizeof(word)
				i++;
#endif
				break;
			default:
				E("can't cast %d to int", type);
				args[i] = 0; // todo: error
			}
			break;

#if UINT64_MAX > UINTPTR_MAX // 32-bit machines
			case TINT64: case TUINT64:
				if (is_enum(arg))
					*(int64_t*)&args[i] = enum(arg);
				else
				switch (reference_type(arg)) {
				case TINTP: // source type
					*(int64_t*)&args[i] = +from_ulong(arg);
					break;
				case TINTN:
					*(int64_t*)&args[i] = -from_ulong(arg);
					break;
				case TRATIONAL:
					*(int64_t*)&args[i] = from_rational(arg);
					break;
				default:
					E("can't cast %d to int64", type);
				}
				#if UINT64_MAX > UINTPTR_MAX
					i++; // for 32-bits: long long values fills two words
				#endif
			break;
#endif

		//
		case TINT8 + FFT_REF:
		case TUINT8 + FFT_REF:
			has_wb = 1;
			//no break
		case TINT8 + FFT_PTR:
		case TUINT8 + FFT_PTR:
		tint8ptr: {
			// todo: add vectors pushing
			if (arg == INULL) // empty array will be sent as nullptr
				break;

			int c = llen(arg); // todo: remove this precounting
			char* p = (char*) &new_bytevector(c * sizeof(char))[1];
			args[i] = (word)p;

			word l = arg;
			while (c--)
				*p++ = (char)to_int(car(l)), l = cdr(l);
			break;
		}

		case TINT16 + FFT_REF:
		case TUINT16 + FFT_REF:
			has_wb = 1;
			//no break
		case TINT16 + FFT_PTR:
		case TUINT16 + FFT_PTR:
		tint16ptr: {
			// todo: add vectors pushing
			if (arg == INULL) // empty array will be sent as nullptr
				break;

			int c = llen(arg);
			short* p = (short*) &new_bytevector(c * sizeof(short))[1];
			args[i] = (word)p;

			word l = arg;
			while (c--)
				*p++ = (short)to_int(car(l)), l = cdr(l);
			break;
		}

		case TINT32 + FFT_REF:
		case TUINT32 + FFT_REF:
			has_wb = 1;
			//no break
		case TINT32 + FFT_PTR:
		case TUINT32 + FFT_PTR:
		tint32ptr: {
			// todo: add vectors pushing
			if (arg == INULL) // empty array will be sent as nullptr
				break;

			int c = llen(arg);
			int* p = (int*) &new_bytevector(c * sizeof(int))[1];
			args[i] = (word)p;

			word l = arg;
			while (c--)
				*p++ = to_int(car(l)), l = cdr(l);
			break;
		}

		case TINT64 + FFT_REF:
		case TUINT64 + FFT_REF:
			has_wb = 1;
			//no break
		case TINT64 + FFT_PTR:
		case TUINT64 + FFT_PTR:
		tint64ptr: {
			// todo: add vectors pushing
			if (arg == INULL) // empty array will be sent as nullptr
				break;

			int c = llen(arg);
			int64_t* p = (int64_t*) &new_bytevector(c * sizeof(int64_t))[1];
			args[i] = (word)p;

			word l = arg;
			while (c--)
				*p++ = to_int(car(l)), l = cdr(l); // todo: to_longlong
			break;
		}

		// с плавающей запятой:
		case TFLOAT:
			#if (__amd64__ && (__linux__ || __APPLE__)) || __aarch64__
				*(float*)&ad[d++] = OL2F(arg); --i;
				floatsmask|=1;
			#elif __ARM_EABI__ && __ARM_PCS_VFP // only for -mfloat-abi=hard (?)
				*(float*)&af[f++] = OL2F(arg); --i;
			#else
				*(float*)&args[i] = OL2F(arg);
			# ifdef __EMSCRIPTEN__
				fmask |= 1 << i;
			# endif
			#endif
			break;
		case TFLOAT + FFT_REF:
		tfloatref:
			has_wb = 1;
			//no break
		case TFLOAT + FFT_PTR:
		tfloatptr: {
			if (arg == INULL) // empty array will be sent as nullptr
				break;

			switch (reference_type(arg)) {
				case TPAIR: {
					int c = llen(arg);
					float* p = (float*) &new_bytevector(c * sizeof(float))[1];
					args[i] = (word)p;

					word l = arg;
					while (c--)
						*p++ = OL2F(car(l)), l = cdr(l);
					break;
				}
				case TVECTOR: // let's support not only lists as float*
				case TVECTORLEAF: { // todo: bytevector?
					int c = header_size(*(word*)arg) - 1;
					float* p = (float*) &new_bytevector(c * sizeof(float))[1];
					args[i] = (word)p;

					word* l = &car(arg);
					while (c--)
						*p++ = OL2F(*l++);
					break;
				}
			}

			break;
		}

		case TDOUBLE:
		tdouble:
			#if (__amd64__ && (__linux__ || __APPLE__)) || __aarch64__
				*(double*)&ad[d++] = OL2D(arg); --i;
				floatsmask|=1;
			#elif __ARM_EABI__ && __ARM_PCS_VFP // only for -mfloat-abi=hard (?)
				*(double*)&af[f++] = OL2D(arg); --i; f++;
			#else
				*(double*)&args[i] = OL2D(arg);
			# if __SIZEOF_DOUBLE__ > __SIZEOF_PTRDIFF_T__ // UINT64_MAX > SIZE_MAX // sizeof(double) > sizeof(float) //__LP64__
				++i; 	// for 32-bits: double fills two words
			# endif
			#endif
			break;

		case TDOUBLE + FFT_REF:
			has_wb = 1;
			// no break
		case TDOUBLE + FFT_PTR:
		tdoubleptr: {
			if (arg == INULL) // empty array will be sent as nullptr
				break;

			int c = llen(arg);
			double* p = (double*) &new_bytevector(c * sizeof(double))[1];
			args[i] = (word)p;

			word l = arg;
			while (c--)
				*p++ = OL2D(car(l)), l = cdr(l);
			break;
		}

		// поинтер на данные
		case TUNKNOWN:
			args[i] = arg;
			break;

		// automatically change the type to argument type, if fft-any
		case TANY: {
			// если передали какое-либо число, то автоматически надо привести к размеру
			// машинного слова (того, что будет лежать в стеке)
			if (is_number(arg))
				goto tint;
			else
			switch (reference_type(arg)) {
			case TVPTR: // value of vptr
			case TCALLABLE: // same as ^
				args[i] = car(arg);
				break;
			case TBYTEVECTOR: // address of bytevector data (no copying to stack)
				args[i] = (word) &car(arg);
				break;
			case TPAIR: // sending type override
				if (is_enump(car(arg))) // should be positive enum number
				switch (value(car(arg))) {
					// (cons fft-int8 '(...))
					case TINT8 + FFT_PTR:
					case TUINT8 + FFT_PTR:
						arg = cdr(arg);
						goto tint8ptr;
					// (cons fft-int16 '(...))
					case TINT16 + FFT_PTR:
					case TUINT16 + FFT_PTR:
						arg = cdr(arg);
						goto tint16ptr;
					// (cons fft-int32 '(...))
					case TINT32 + FFT_PTR:
					case TUINT32 + FFT_PTR:
						arg = cdr(arg);
						goto tint32ptr;
					// (cons fft-float '(...))
					case TFLOAT + FFT_PTR:
						arg = cdr(arg);
						goto tfloatptr;
					// TBD.
					// todo: add list with custom types and a function with
					// pushing structures
					// case TPAIR: ...
					default:
						E("unsupported list types for void*");
				}
				else
					E("No type conversion selected");
				break;
			case TSTRING:
				goto tstring;
			// case TSTRINGWIDE:
			// 	goto tstringwide;
			}
			break;
		}

		// vptr should accept only vptr!
		case TVPTR: tvptr:
			// temporary. please, change to "if (is_reference(arg) && reference_type(arg) == TVPTR)"
			if (is_reference(arg))
				switch (reference_type(arg)) {
				case TVPTR:
					args[i] = car(arg);
					break;
				case TBYTEVECTOR: // can be used instead of vptr
					args[i] = (word) &car(arg);
					break;
				default:
					E("invalid parameter value (requested vptr)");
				}
			else
				E("invalid parameter value (requested vptr)");
			break;
		case TVPTR + FFT_REF:
			has_wb = 1;
			// no break
		case TVPTR + FFT_PTR: {
			if (arg == INULL) // empty array will be sent as nullptr
				break;
			if (reference_type(arg) == TVPTR || reference_type(arg) == TBYTEVECTOR) // single vptr value or bytevector (todo: add bytevector size check)
				args[i] = (word) &car(arg);
			else {
				int c = llen(arg);
				void** p = (void**) __builtin_alloca(c * sizeof(void*));
				args[i] = (word)p;

				word l = arg;
				while (c--) // todo: add type check
					*p++ = (void*)car(l), l = cdr(l);
			}
			break;
		}

		case TBYTEVECTOR:
//		case TBYTEVECTOR + FFT_PTR:
//		case TBYTEVECTOR + FFT_REF:
		tbytevector:
			switch (reference_type(arg)) {
			case TBYTEVECTOR:
			case TSTRING: // ansi strings marshaling to bytevector data "as is" without conversion
				args[i] = (word) &car(arg);
				break;
			default:
				E("invalid parameter values (requested bytevector)");
			}
			break;

		// todo: change to automaticly converting to 0-ending string
		case TSTRING:
		tstring:
			switch (reference_type(arg)) {
			case TSTRING: {
				// todo: add check to not copy the zero-ended string
				int size = rawstream_size(arg);
				char* zerostr = (char*) &car(new_bytevector (size));
				memcpy(zerostr, &car(arg), size); zerostr[size] = 0;
				args[i] = (word)zerostr;
				break;
			}
			case TSTRINGWIDE: {
				// utf-8 encoding
				char* ptr = (char*) (args[i] = (int_t) &fp[1]);
				for (int i = 1; i <= reference_size(arg); i++) {
					int cp = value(ref(arg, i));
					if (cp < 0x80)
						*ptr++ = cp;
					else
					if (cp < 0x0800) {
						*ptr++ = 0xC0 | (cp >> 6);
						*ptr++ = 0x80 | (cp & 0x3F);
					}
					else
					if (cp < 0x10000) {
						*ptr++ = 0xE0 | (cp >> 12);
						*ptr++ = 0x80 | ((cp >> 6) & 0x3F);
						*ptr++ = 0x80 | (cp & 0x3F);
					}
					else
					if (cp < 0x110000) {
						*ptr++ = 0xF0 | (cp >> 18);
						*ptr++ = 0x80 | ((cp >> 12) & 0x3F);
						*ptr++ = 0x80 | ((cp >> 6) & 0x3F);
						*ptr++ = 0x80 | (cp & 0x3F);
					}
					else {
						// printf("invalid codepoint");
						*ptr++ = 0x7F;
					}
				}
				*ptr++ = 0;
				new_bytevector(ptr - (char*)&fp[1]);
				break;
			}
			default:
				E("invalid parameter values (requested string)");
			}
			break;
		case TSTRING + FFT_PTR: {
			int size = llen(arg);

			// TODO: check the available memory and gun GC if necessary
			word* p = new (TBYTEVECTOR, size); // yes, size - because this vector will store the raw system words
			args[i] = (word) ++p;

            size++;

			word src = arg;
			while (--size) {
                word str = car(src);
                // todo: move to function, use universal function to push objects
                switch (reference_type(str)) {
                case TSTRING: {
                    // todo: add check to not copy the zero-ended string
                    int length = rawstream_size(str);
                    char* zerostr = (char*) &car(new_bytevector (length+1));
                    memcpy(zerostr, &car(str), length); zerostr[length] = 0;
                    *p++ = (word)zerostr;
                    break;
                }
                case TSTRINGWIDE: {
                    // utf-8 encoding
                    char* ptr = (char*) (*p++ = (int_t) &fp[1]);
                    for (int i = 1; i <= reference_size(arg); i++) {
                        int cp = value(ref(arg, i));
                        if (cp < 0x80)
                            *ptr++ = cp;
                        else
                        if (cp < 0x0800) {
                            *ptr++ = 0xC0 | (cp >> 6);
                            *ptr++ = 0x80 | (cp & 0x3F);
                        }
                        else
                        if (cp < 0x10000) {
                            *ptr++ = 0xE0 | (cp >> 12);
                            *ptr++ = 0x80 | ((cp >> 6) & 0x3F);
                            *ptr++ = 0x80 | (cp & 0x3F);
                        }
                        else
                        if (cp < 0x110000) {
                            *ptr++ = 0xF0 | (cp >> 18);
                            *ptr++ = 0x80 | ((cp >> 12) & 0x3F);
                            *ptr++ = 0x80 | ((cp >> 6) & 0x3F);
                            *ptr++ = 0x80 | (cp & 0x3F);
                        }
                        else {
                            // printf("invalid codepoint");
                            *ptr++ = 0x7F;
                        }
                    }
                    *ptr++ = 0;
                    new_bytevector(ptr - (char*)&fp[1]);
                    break;
                }
                case TSTRINGDISPATCH: {
                    int length = number(ref(str, 1)); // for last-one zero
                    int parts = reference_size(str) - 1;

                    char* zerostr = (char*) &car(new_bytevector (length+1)); // one more for the trailing '\0'
                    *p++ = (word)zerostr;

                    for (int i = 0; i < parts; i++) {
                        word substr = ref(str, i + 2);

                        switch (reference_type(substr)) {
                        case TSTRING: {
                            // todo: add check to not copy the zero-ended string
                            int length = rawstream_size(substr);
                            memcpy(zerostr, &car(substr), length);
                            zerostr += length;
                            break;
                        }
                        case TSTRINGWIDE: {
                            // utf-8 encoding
                            char* ptr = zerostr;
                            for (int i = 1; i <= reference_size(arg); i++) {
                                int cp = value(ref(arg, i));
                                if (cp < 0x80)
                                    *ptr++ = cp;
                                else
                                if (cp < 0x0800) {
                                    *ptr++ = 0xC0 | (cp >> 6);
                                    *ptr++ = 0x80 | (cp & 0x3F);
                                }
                                else
                                if (cp < 0x10000) {
                                    *ptr++ = 0xE0 | (cp >> 12);
                                    *ptr++ = 0x80 | ((cp >> 6) & 0x3F);
                                    *ptr++ = 0x80 | (cp & 0x3F);
                                }
                                else
                                if (cp < 0x110000) {
                                    *ptr++ = 0xF0 | (cp >> 18);
                                    *ptr++ = 0x80 | ((cp >> 12) & 0x3F);
                                    *ptr++ = 0x80 | ((cp >> 6) & 0x3F);
                                    *ptr++ = 0x80 | (cp & 0x3F);
                                }
                                else {
                                    // printf("invalid codepoint");
                                    *ptr++ = 0x7F;
                                }
                            }
                            zerostr = ptr;
                            break;
                        }
                        default:
                            E("invalid parameter values (requested string*), %d", reference_type(substr));
                        }
                    }
                    *zerostr = '\0'; // trailing zero
                    break;
                }
                default:
                    *p++ = 0;
                    E("invalid parameter values (requested string*), %d", reference_type(str));
                }
                src = cdr(src);
            }
			break;
		}

		// todo: do fix for windows!
		//    Windows uses UTF-16LE encoding internally as the memory storage
		//    format for Unicode strings, it considers this to be the natural
		//    encoding of Unicode text. In the Windows world, there are ANSI
		//    strings (the system codepage on the current machine, subject to
		//    total unportability) and there are Unicode strings (stored
		//    internally as UTF-16LE).
//		#ifdef _WIN32
		case TSTRINGWIDE:
		tstringwide:
			switch (reference_type(arg)) {
			case TBYTEVECTOR:
			case TSTRING: {
				word hdr = deref(arg);
				int len = (header_size(hdr)-1)*sizeof(word) - header_pads(hdr);
				short* unicode = (short*) __builtin_alloca(len * sizeof(short));

				short* p = unicode;
				char* s = (char*)&car(arg);
				for (int i = 1; i < len; i++)
					*p++ = (short)(*s++);
				*p = 0;

				args[i] = (word) unicode;
				break;
			}
			case TSTRINGWIDE: {
				int len = header_size(deref(arg));
				short* unicode = (short*) __builtin_alloca(len * sizeof(short));
				// todo: use new()
				// check the available memory in the heap
				// use builtin unlucky for calling the gc if no more memory

				short* p = unicode;
				word* s = &car(arg);
				for (int i = 1; i < len; i++)
					*p++ = (short)value(*s++);
				*p = 0;

				args[i] = (word) unicode;
				break;
			}
			default:
				E("invalid parameter values (requested string)");
			}
			break;
//		#endif

		case TCALLABLE: { // todo: maybe better to merge type-callable and type-vptr ?
			if (is_callable(arg))
				args[i] = (word)car(arg);
			else
				E("invalid parameter values (requested callable)");
			break;
		}
/*
		case TTUPLE:
			switch (reference_type(arg)) {
			case TTUPLE: { // ?
				// аллоцировать массив и сложить в него указатели на элементы кортежа
				int size = hdrsize(*(word*)arg);
				*fp++ = make_raw_header(TBYTEVECTOR, size, 0);
				args[i] = (word)fp; // ссылка на массив указателей на элементы

				word* src = &car(arg);
				while (--size)
					*fp++ = (word)((word*)*src++ + 1);
				}
				break;
			default:
				args[i] = INULL; // todo: error
			}
			break;*/
//		case TRAWVALUE:
//			args[i] = (word)arg;
//			break;
		case TPORT: {
			if (arg == make_enum(-1)) { // ?
				args[i] = -1;
				break;
			}
			int portfd = port(arg);
			switch (portfd) {
			case 0: // stdin
				args[i] = (word) stdin;
				break;
			case 1: // stdout
				args[i] = (word) stdout;
				break;
			case 2: // stderr
				args[i] = (word) stderr;
				break;
			default:
				args[i] = portfd;
				break;
			}
			break;
		}
		case TVOID:
			args[i] = 0; // do nothing, just for better readability
			break;
		default:
			E("can't recognize %d type", type);
		}

		i++;
		p = (word*)cdr(p); // (cdr p)
		t = (word*)cdr(t); // (cdr t)
	}
#ifdef __EMSCRIPTEN__
	fmask |= 1 << i;
#endif
	assert ((word)t == INULL); // количество аргументов совпало!

	unsigned long long got = 0; // результат вызова функции (64 бита для возможного double)

	size_t pB = OL_pin(this, B);
	size_t pC = OL_pin(this, C);
	this->fp = fp; // сохраним, так как в call могут быть вызваны коллейблы, и они попортят fp

//	if (floatsmask == 15)
//		__asm__("int $3");

#if  __amd64__
#	if (__linux__ || __APPLE__)
		got = nix64_call(args, ad, i, d, floatsmask, function, returntype & 0x3F);
#	elif _WIN64
		got = win64_call(args, i, function, returntype & 0x3F);
#	else
#		error "Unsupported platform"
#	endif
#elif __i386__
#	if (__linux__ || __APPLE__)
		got = x86_call(args, i, function, returntype & 0x3F);
#	elif _WIN32
		// cdecl and stdcall in our case are same, so...
		got = x86_call(args, i, function, returntype & 0x3F);
#		error "Unsupported platform"
#	endif
#elif __aarch64__
	got = arm64_call(args, ad, i, d, function, returntype & 0x3F);
#elif __arm__
	// arm calling abi http://infocenter.arm.com/help/topic/com.arm.doc.ihi0042f/IHI0042F_aapcs.pdf
#	ifndef __ARM_PCS_VFP
		got = arm32_call(args, i, function);
#	else // (?)
		got = arm32_call(args, NULL,
	        i, 0,
	        function, returntype & 0x3F); //(?)
#	endif
#elif __mips__
	// https://acm.sjtu.edu.cn/w/images/d/db/MIPSCallingConventionsSummary.pdf
	typedef long long ret_t;
	inline ret_t call(word args[], int i, void* function, int type) {
		CALL();
	}
	got = call(args, i, function, returntype & 0x3F);
#elif __EMSCRIPTEN__
	got = asmjs_call(args, fmask, function, returntype & 0x3F);
#else // ALL other
	typedef long long ret_t;
	inline ret_t call(word args[], int i, void* function, int type) {
		CALL();
	}
	got = call(args, i, function, returntype & 0x3F);
/*	inline ret_t call_cdecl(word args[], int i, void* function, int type) {
		CALL(__cdecl);
	}
	inline ret_t call_stdcall(word args[], int i, void* function, int type) {
		CALL(__stdcall);
	}

	switch (returntype >> 6) {
	case 0:
	case 1:
		got = call_cdecl(args, i, function, returntype & 0x3F);
		break;
	case 2:
		got = call_stdcall(args, i, function, returntype & 0x3F);
		break;
	// FASTCALL: (3)
	case 3:
	// THISCALL: (4)
	// ...
	default:
		E("Unsupported calling convention %d", returntype >> 6);
		break;
	}*/
#endif

	// где гарантия, что C и B не поменялись?
	fp = this->fp;
	B = OL_unpin(this, pB);
	C = OL_unpin(this, pC);

	if (has_wb) {
		// еще раз пробежимся по аргументам, может какие надо будет вернуть взад
		p = (word*)C;   // сами аргументы
		t = (word*)cdr(B); // rtti

		i = 0;
		while ((word)p != INULL) { // пока есть аргументы
			assert (reference_type(p) == TPAIR); // assert(list)
			assert (reference_type(t) == TPAIR); // assert(list)

			int type = value(car(t));
			word arg = (word) car(p);

			// destination type
			//if (arg != IFALSE) - излишен
			switch (type) {

			// simplest cases - all shorts are fits as value
			case TINT8 + FFT_REF: {
				// вот тут попробуем заполнить переменные назад
				int c = llen(arg);
				signed char* f = (signed char*)args[i];

				word l = arg;
				while (c--) {
					signed char value = *f++;
					word* ptr = &car(l);

					*ptr = make_enum(value);
					l = cdr(l);
				}
				break;
			}
			case TUINT8 + FFT_REF: {
				// вот тут попробуем заполнить переменные назад
				int c = llen(arg);
				unsigned char* f = (unsigned char*)args[i];

				word l = arg;
				while (c--) {
					unsigned char value = *f++;
					word* ptr = &car(l);

					*ptr = I(value);
					l = cdr(l);
				}
				break;
			}

			case TINT16 + FFT_REF: {
				// вот тут попробуем заполнить переменные назад
				int c = llen(arg);
				signed short* f = (signed short*)args[i];

				word l = arg;
				while (c--) {
					short value = *f++;
					word* ptr = &car(l);

					*ptr = make_enum(value);
					l = cdr(l);
				}
				break;
			}
			case TUINT16 + FFT_REF: {
				// вот тут попробуем заполнить переменные назад
				int c = llen(arg);
				unsigned short* f = (unsigned short*)args[i];

				word l = arg;
				while (c--) {
					unsigned short value = *f++;
					word* ptr = &car(l);

					*ptr = I(value);
					l = cdr(l);
				}
				break;
			}

			case TINT32 + FFT_REF: {
				// вот тут попробуем заполнить переменные назад
				int c = llen(arg);
				int* f = (int*)args[i];

				word l = arg;
				while (c--) {
					int value = *f++;
					word* ptr = &car(l);

				#if UINTPTR_MAX != 0xffffffffffffffff  // 32-bit machines
					if (value > VMAX) {
						if (is_value(*ptr))
							E("got too large number to store");
						else {
							assert (is_npairp(ptr) || is_npairn(ptr));
							if (value < 0) {
								*ptr = make_header(value < 0 ? TINTN : TINTP, 3);
								value = -value;
							}
							else
								*ptr = make_header(TINTP, 3);
							*(word*)&car(ptr) = I(value & VMAX);
							*(word*)&cadr(ptr) = I(value >> VBITS);
						}
					}
					else
				#endif
					*ptr = make_enum(value);
					l = cdr(l);
				}
				break;
			}

			case TUINT32 + FFT_REF: {
				// вот тут попробуем заполнить переменные назад
				int c = llen(arg);
				unsigned int* f = (unsigned int*)args[i];

				word l = arg;
				while (c--) {
					unsigned int value = *f++;
					word* ptr = &car(l);

				#if UINTPTR_MAX != 0xffffffffffffffff  // 32-bit machines
					if (value > VMAX) {
						if (is_value(*ptr))
							E("got too large number to store");
						else {
							assert (is_npairp(ptr) || is_npairn(ptr));
							*ptr = make_header(TINTP, 3);
							*(word*)&car(ptr) = I(value & VMAX);
							*(word*)&cadr(ptr) = I(value >> VBITS);
						}
					}
					else
				#endif
					*ptr = I(value);

					l = cdr(l);
				}
				break;
			}

			case TFLOAT + FFT_REF: {
				// todo: перепроверить!
				// вот тут попробуем заполнить переменные назад
				int c = llen(arg);
				float* f = (float*)args[i];

				word l = arg;
				while (c--) {
					float value = *f++;
					word num = car(l);
					switch (reference_type(num)) {
						// case TRATIONAL: {
						// 	// максимальная читабельность (todo: change like fto..)
						// 	long n = value * 10000;
						// 	long d = 10000;
						// 	car(num) = make_enum(n);
						// 	cdr(num) = make_enum(d);
						// 	// максимальная точность (fixme: пока не работает как надо)
						// 	//car(num) = make_enum(value * VMAX);
						// 	//cdr(num) = I(VMAX);
						// 	break;
						// }
						case TINEXACT: {
							*(inexact_t*)&car(num) = value;
							break;
						}
						default:
							assert (0 && "Invalid return variables.");
							break;
					}
					l = cdr(l);
				}
				break;
			}
			case TDOUBLE + FFT_REF: {
				// todo: перепроверить!
				// вот тут попробуем заполнить переменные назад
				int c = llen(arg);
				double* f = (double*)args[i];

				word l = arg;
				while (c--) {
					double value = *f++;
					word num = car(l);
					switch (reference_type(num)) {
						// case TRATIONAL: {
						// 	self->heap.fp = fp;
						// 	word v = D2OL(self, value);
						// 	fp = self->heap.fp;

						// 	if (is_value (v))
						// 		car(l) = v;
						// 	else {
						// 		assert (is_value(car(v)) && is_value(cdr(v)));
						// 		car(num) = car(v);
						// 		cdr(num) = cdr(v);
						// 	}
						// 	break;
						// }
						case TINEXACT: {
							*(inexact_t*)&car(num) = value;
							break;
						}
						default:
							assert (0 && "Invalid return variables.");
							break;
					}
					l = cdr(l);
				}
				break;
			}

			case TVPTR + FFT_REF: {
				int c = llen(arg);
				void** f = (void**)args[i];

				word l = arg;
				while (c--) {
					void* value = *f++;
					word num = car(l);
					assert (reference_type(num) == TVPTR);
					*(void**)&car(num) = value;

					l = cdr(l);
				}
				break;
			} }

			p = (word*) cdr(p);
			t = (word*) cdr(t);
			i++;
		}
	}

	word* result = (word*)IFALSE;
	returntype &= 0x3F;
	switch (returntype) {
		// TENUMP - deprecated
		case TENUMP: // type-enum+ - если я уверен, что число заведомо меньше 0x00FFFFFF! (или сколько там в x64)
			result = (word*) make_enum(got);
			break;

		case TINT8:
			// little-endian:
			result = (word*) make_number (*(char*)&got);  // TODO: change to __INT8_TYPE__
			break;
		case TINT16:
			result = (word*) make_number (*(short*)&got); // TODO: change to __INT16_TYPE__
			break;
		case TINT32:
			result = (word*) make_number (*(int*)&got);   // TODO: change to __INT32_TYPE__
			break;
		case TINT64: {
#if UINTPTR_MAX == 0xffffffffffffffff
			result = (word*) make_number (*(long long*)&got); // TODO: change to __INT64_TYPE__
#else
#	ifndef __EMSCRIPTEN__
			result = (word*) ll2ol (&fp, *(long long*)&got);
#	endif
#endif
			break;
		}

		case TUINT8:
			result = (word*) itoun (*(unsigned char*)&got); // TODO: change to __UINT8_TYPE__
			break;
		case TUINT16:
			result = (word*) itoun (*(unsigned short*)&got);// TODO: change to __UINT16_TYPE__
			break;
		case TUINT32:
			result = (word*) itoun (*(unsigned int*)&got);  // TODO: change to __UINT32_TYPE__
			break;
		case TUINT64: {
#if UINTPTR_MAX == 0xffffffffffffffff
			result = (word*) itoun (*(unsigned long long*)&got); // TODO: change to __UINT32_TYPE__
#else
#	ifndef __EMSCRIPTEN__
			result = (word*) ul2ol (&fp, *(unsigned long long*)&got);
#	endif
#endif
			break;
		}


		case TPORT:
			result = (word*) make_port ((long)got); // todo: make port like in SYSCALL_OPEN
			break;
		case TVOID:
			result = (word*) ITRUE;
			break;

		case TVPTR:
			if ((word)got)
				result = new_vptr (got);
			break;

		case TSTRING:
			if (got) {
				int l = lenn((char*)(word)got, VMAX+1);
				/* TODO: enable again!
				if (fp + (l/sizeof(word)) > heap->end) {
					self->gc(self, l/sizeof(word));
					heap = &self->heap;
					fp = heap->fp;
				}*/
				result = new_string ((char*)(word)got, l);
			}
			break;
		case TSTRINGWIDE:
			if (got) {
				int l = lenn16((short*)(word)got, VMAX+1);
				/* TODO: enable again!
				if (fp + (l/sizeof(word)) > heap->end) {
					self->gc(self, l/sizeof(word));
					heap = &self->heap;
					fp = heap->fp;
				}*/
				word* p = result = new (TSTRINGWIDE, l);
				unsigned short* s = (unsigned short*)(word)got;
				while (l--) {
					*++p = I(*s++);
				}
			}
			break;

		// возвращаемый тип не может быть TRATIONAL, так как непонятна будет точность
		case TFLOAT:
		case TDOUBLE: {
			inexact_t value =
				(returntype == TFLOAT)
					? *(float* )&got
					: *(double*)&got;

#if OLVM_INEXACTS
			result = new_inexact(value);
#else
			this->fp = fp;
			result = (word*) d2ol(self, value);
			fp = this->fp;
#endif
			break;
		}
	}

	this->fp = fp;
	return result;
}

/** This function returns size of basic types:
 * 1 - char (signed, unsigned)
 * 2 - short (signed, unsigned)
 * 3 - int (signed, unsigned)
 * 4 - long (signed, unsigned)
 * ...
 * 9 - long
 * 10 - float, 11 - double
 * 20 - void*
 */
PUBLIC
word OL_sizeof(OL* self, word* arguments)
{
	word* A = (word*)car(arguments); // type
	switch (value(A)) {
		// primitive integer types
		case 1: return I(sizeof(char));
		case 2: return I(sizeof(short));
		case 3: return I(sizeof(int));
		case 4: return I(sizeof(long));
		case 5: return I(sizeof(long long));

		// floating point types
		case 10: return I(sizeof(float));
		case 11: return I(sizeof(double));

		// general types:
		case 20: return I(sizeof(void*));

		default:
			return IFALSE;
	}
}

#endif//OLVM_FFI


// --=( CALLABLES support )=-----------------------------
// --
#if OLVM_CALLABLES

// http://man7.org/tlpi/code/faq.html#use_default_source
//  glibc version 6+ uses __GLIBC__/__GLIBC_MINOR__
// #ifndef _DEFAULT_SOURCE
// # ifndef __APPLE__
// #	error "Required -std=gnu11 (we use anonymous mmap)"
// # endif
// #endif

// todo: добавить возможность вызова колбека как сопрограммы (так и назвать - сопрограмма)
//       который будет запускать отдельный поток и в контексте колбека ВМ сможет выполнять
//       все остальные свои сопрограммы.
// ret is ret address to the caller function
static
int64_t callback(OL* ol, size_t id, int_t* argi
	#if __amd64__
		, inexact_t* argf, int_t* rest
	#endif
	);


// todo: удалить userdata api за ненадобностью (?) и использовать пин-api
PUBLIC
word OL_mkcb(OL* self, word* arguments)
{
	word* A = (word*)car(arguments);

	unless (is_value(A))
		return IFALSE;

	int pin = untoi(A);

	char* ptr = 0;
#ifdef __i386__ // x86
	// JIT howto: http://eli.thegreenplace.net/2013/11/05/how-to-jit-an-introduction
	static char bytecode[] =
			"\x90"  // nop
			"\x8D\x44\x24\x04" // lea eax, [esp+4]
			"\x50"     // push eax
			// 6
			"\x68----" // push $0
			"\x68----" // push ol
			"\xB8----" // mov eax, ...
			"\xFF\xD0" // call eax
			"\x83\xC4\x0C" // add esp, 3*4
			"\xC3"; // ret
	#ifdef _WIN32
		HANDLE mh = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_EXECUTE_READWRITE,
				0, sizeof(bytecode), NULL);
		if (!mh)
			return IFALSE;
		ptr = MapViewOfFile(mh, FILE_MAP_ALL_ACCESS,
				0, 0, sizeof(bytecode));
		CloseHandle(mh);
		if (!ptr)
			return IFALSE;
	#else
		ptr = mmap(0, sizeof(bytecode), PROT_READ | PROT_WRITE | PROT_EXEC,
				MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
		if (ptr == (char*) -1)
			return IFALSE;
	#endif

	memcpy(ptr, &bytecode, sizeof(bytecode));
	*(long*)&ptr[ 7] = pin;
	*(long*)&ptr[12] = (long)self;
	*(long*)&ptr[17] = (long)&callback;
#elif __amd64__
	// Windows x64
	#ifdef _WIN32
	//long long callback(OL* ol, int id, long long* argi, double* argf, long long* rest)
	static char bytecode[] =
			"\x90" // nop
			"\x48\x8D\x44\x24\x28"  // lea rax, [rsp+40] (rest)
			"\x55"                  // push rbp
			"\x48\x89\xE5"          // mov ebp, rsp
			"\x41\x51"              // push r9
			"\x41\x50"              // push r8
			"\x52"                  // push rdx
			"\x51"                  // push rcx
			"\x49\x89\xE0"          // mov r8, esp         // argi
			// 19
			"\x48\x83\xEC\x20"      // sub esp, 32
			"\x67\xF2\x0F\x11\x44\x24\x00"    // movsd [esp+ 0], xmm0
			"\x67\xF2\x0F\x11\x4C\x24\x08"    // movsd [esp+ 8], xmm1
			"\x67\xF2\x0F\x11\x54\x24\x10"    // movsd [esp+16], xmm2
			"\x67\xF2\x0F\x11\x5C\x24\x18"    // movsd [esp+24], xmm3
			"\x49\x89\xE1"          // mov r9, esp         // argf
			// 54
			"\x48\xBA--------"      // mov rdx, 0          // id
			"\x48\xB9--------"      // mov rcx, 0          // ol
			// 74
			"\x50"	                // push rax // dummy
			"\x50"                  // push rax            // rest
			"\x48\x83\xEC\x20"      // sub rsp, 32         // free space
			// 80
			"\x48\xB8--------"      // mov rax, callback
			"\xFF\xD0"              // call rax
			"\xC9"                  // leave
			"\xC3";                 // ret

	HANDLE mh = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_EXECUTE_READWRITE,
			0, sizeof(bytecode), NULL);
	if (!mh)
		return IFALSE;
	ptr = MapViewOfFile(mh, FILE_MAP_ALL_ACCESS | FILE_MAP_EXECUTE,
			0, 0, sizeof(bytecode));
	CloseHandle(mh);
	if (!ptr)
		return IFALSE;

	memcpy(ptr, &bytecode, sizeof(bytecode));
	*(long long*)&ptr[56] = pin;
	*(long long*)&ptr[66] = (long long)self;
	*(long long*)&ptr[82] = (long long)&callback;

	#else // System V (linux, unix, macos, android, ...)
	//long long callback(OL* ol, int id, long long* argi, double* argf, long long* rest)
	// rdi: ol, rsi: id, rdx: argi, rcx: argf, r8: rest
	// not used: r9

	static char bytecode[] =
			"\x90" // nop
			"\x55"                  // push rbp
			"\x48\x89\xE5"          // mov rbp, rsp
			"\x41\x51"              // push r9
			"\x41\x50"              // push r8
			"\x51"                  // push rcx
			"\x52"                  // push rdx
			"\x56"                  // push rsi
			"\x57"                  // push rdi
			"\x48\x89\xE2"          // mov rdx, rsp         // argi
			"\x4C\x8D\x44\x24\x40"  // lea r8, [rsp+64]     // rest
			// 21
			"\x48\x83\xEC\x40"      // sub rsp, 8*8
			"\xF2\x0F\x11\x44\x24\x00"    // movsd [esp+ 0], xmm0
			"\xF2\x0F\x11\x4C\x24\x08"    // movsd [esp+ 8], xmm1
			"\xF2\x0F\x11\x54\x24\x10"    // movsd [esp+16], xmm2
			"\xF2\x0F\x11\x5C\x24\x18"    // movsd [esp+24], xmm3
			"\xF2\x0F\x11\x54\x24\x20"    // movsd [esp+32], xmm4
			"\xF2\x0F\x11\x6C\x24\x28"    // movsd [esp+40], xmm5
			"\xF2\x0F\x11\x74\x24\x30"    // movsd [esp+48], xmm6
			"\xF2\x0F\x11\x7C\x24\x38"    // movsd [esp+56], xmm7
			"\x48\x89\xE1"          // mov rcx, rsp         // argf
			// 76
			"\x48\xBE--------"      // mov rsi, 0          // id
			// 86
			"\x48\xBF--------"      // mov rdi, 0          // ol
			// 96
			"\x48\xB8--------"      // mov rax, callback
			"\xFF\xD0"              // call rax
			"\xC9"                  // leave
			"\xC3";                 // ret
	ptr = mmap(0, sizeof(bytecode), PROT_READ | PROT_WRITE | PROT_EXEC,
			MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (ptr == (char*) -1)
		return IFALSE;

	memcpy(ptr, &bytecode, sizeof(bytecode));
	*(long long*)&ptr[78] = pin;
	*(long long*)&ptr[88] = (long long)self;
	*(long long*)&ptr[98] = (long long)&callback;
	#endif
#endif

	word*
	fp = self->fp;
	word result = (word) new_callable(ptr);
	self->fp = fp;
	return result;
}


//long long callback(OL* ol, int id, word* args) // win32
//long long callback(OL* ol, int id, long long* argi, double* argf, long long* others) // linux
// notes:
//	http://stackoverflow.com/questions/11270588/variadic-function-va-arg-doesnt-work-with-float

static
__attribute__((used))
int64_t callback(OL* ol, size_t id, int_t* argi // TODO: change "ol" to "this"
#if __amd64__
		, inexact_t* argf, int_t* rest //win64
#endif
		)
{
	word* fp;
//	__asm("int $3");

	word cb = OL_deref(ol, id);
	word types = car(cb);
	word function = cdr(cb);

	// let's count of arguments
	word args = types;
	size_t a = 0;
	while (args != INULL) {
		a++;
		args = cdr(args);
	}

	word R[a]; // сложим все в стек, потом заполним оттуда список
	size_t count = a;

	fp = ol->fp;

	int i = 0;
#if __amd64__ && (__linux__ || __APPLE__)
	int j = 0;
#endif
/*#if __amd64__  // !!!
	#if _WIN64
//	rest -= 4;
	#else
	rest -= 6;
	#endif
#endif*/
//	int f = 0; // linux
	while (types != INULL) {
		--a;
		// шаблон транслятора аргументов C -> OL
		#if __amd64__
			#if _WIN64
			# define c2ol_value(type) \
			type value = i < 5 \
					? *(type*) &argi[i] \
					: *(type*) &rest[i-5];
			#else
			# define c2ol_value(type) \
			type value = i < 6 \
					? *(type*) &argi[i] \
					: *(type*) &rest[i-6]; \
			i++;
			#endif
		#else
			# define c2ol_value(type) \
			type value = *(type*) &argi[i++];
		#endif


		switch (car(types)) {
		case I(TVPTR): {
			c2ol_value(void*);
			R[a] = (word) new_vptr(value);
			break;
		}
		case I(TUINT8): {
			c2ol_value(unsigned char);
			R[a] = I(value);
			break; }
		case I(TUINT16): {
			c2ol_value(unsigned short);
			R[a] = I(value);
			break; }
		case I(TUINT32): {
			c2ol_value(unsigned int);
			R[a] = (word) make_number(value);
			break;
		}
		case I(TUINT64): {
			c2ol_value(unsigned long long);
			R[a] = (word) make_number(value);
			break;
		}

		case I(TINT8): {
			c2ol_value(signed char);
			R[a] = (word) make_number(value);
			break; }
		case I(TINT16): {
			c2ol_value(signed short);
			R[a] = (word) make_number(value);
			break; }
		case I(TINT32): {
			c2ol_value(signed int);
			R[a] = (word) make_number(value);
			break;
		}
		case I(TINT64): {
			c2ol_value(signed long long);
			R[a] = (word) make_number(value);
			break;
		}

		case I(TFLOAT): {
			float
			#if __amd64__
				#if _WIN64
				value = i <= 4
				        ? *(float*) &argf[i]
				        : *(float*) &rest[i-6];
				i++;
				#else
				value = j <= 8
						? *(float*) &argf[j]
						: *(float*) &rest[j-8]; // ???
				j++;
				#endif
			#else
				value =   *(float*) &argi[i++];
			#endif
			ol->fp = fp;
			R[a] = d2ol(ol, value);
			fp = ol->fp;
			break;
		}
		case I(TDOUBLE): {
			double
			#if __amd64__
				#if _WIN64
				value = i <= 4
				        ? *(double*) &argf[i]
				        : *(double*) &rest[i-4];
				i++;
				#else
				value = i <= 8
						? *(double*) &argf[j]
						: *(double*) &rest[j-8]; // ???
				j++;
				#endif
			#else
				value =   *(double*) &argi[i++]; i++;
			#endif
			ol->fp = fp;
			R[a] = d2ol(ol, value);
			fp = ol->fp;
			break;
		}
		case I(TSTRING): {
			void*
			#if __amd64__
				#if _WIN64
				value = i <= 4
				        ? *(void**) &argi[i]
				        : *(void**) &rest[i-4];
				#else
				value = i <= 6
						? *(void**) &argi[i]
						: *(void**) &rest[i-6]; // ???
				#endif
				i++;
			#else
				value =   *(void**) &argi[i++];
			#endif
			R[a] = value ? (word)new_string(value) : IFALSE;
			break;
		}
//		case I(TVOID):
//			R[a] = IFALSE;
//			i++;
//			break;
		default: {
			void*
			#if __amd64__
				#if _WIN64
				value = i <= 4
				        ? *(void**) &argi[i]
				        : *(void**) &rest[i-4];
				#else
				value = i <= 6
						? *(void**) &argi[i]
						: *(void**) &rest[i-6]; // ???
				#endif
				i++;
			#else
				value =   *(void**) &argi[i++];
			#endif

			if (is_pair(car(types))) {
				//int l = llen(car(types));
				word tail = INULL;
				void** values = (void**)value;

				word argv = car(types);
				while (argv != INULL) {
					switch (car (argv)) {
					case I(TSTRING):
						tail = (word) new_pair(new_string(*values++), tail);
						break;
					case I(TVPTR):
						tail = (word) new_pair(new_vptr(*values++), tail);
						break;
					}
					argv = cdr (argv);
				}
				R[a] = tail;
				break;
			}

			// else error
			E("unknown argument type");
			break; }
		}
		types = cdr(types);
	}
	assert (a == 0);

	// let's put all arguments into list
	args = INULL;
	for (size_t i = 0; i < count; i++)
		args = (word) new_pair(R[i], args);

	ol->fp = fp; // well, done

	word r = OL_apply(ol, function, args);

	if (is_value (r))
		return value(r);
	else
	switch (reference_type (r)) {
		case TVPTR:
			return r;
		// return type override
		case TPAIR: ;
			// if expected float or double result,
			// do the __ASM__ with loading the result into fpu/xmm register
			//switch (value_type (car(r))) {
			//	case TFLOAT:
			//	case TDOUBLE:
			//}
	}
	// default:
	return 0;
}

#endif//OLVM_CALLABLES
