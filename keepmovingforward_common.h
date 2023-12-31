#ifndef KEEPMOVINGFORWARD_COMMON_H
#include <stdint.h>

#if _MSC_VER
#define COMPILER_MSVC 1
#if defined(_WIN32)
#define OS_WINDOWS 1
#endif
#endif

#if COMPILER_MSVC
#pragma section(".roglob", read)
#define readonly __declspec(allocate(".roglob"))
#else
#define readonly
#endif

// NOTE: so LSP doesn't grey out preproc directives, remove this when shippin
#define INTERNAL 1
#define UNOPTIMIZED 1

#define PI 3.14159265359f

#define global static
#define internal static

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define ArrayLength(array) sizeof(array) / sizeof((array)[0])
#define kilobytes(value) ((value)*1024LL)
#define megabytes(value) (kilobytes(value) * 1024LL)
#define gigabytes(value) (megabytes(value) * 1024LL)
#define terabytes(value) (gigabytes(value) * 1024LL)

#if UNOPTIMIZED
#define Assert(expression)                                                                              \
    if (!(expression))                                                                                  \
    {                                                                                                   \
        *(volatile int *)0 = 0;                                                                         \
    }
#else
#define Assert(expression) (void)0
#endif

#define Unreachable Assert(!"Unreachable")
#define Swap(type, a, b) do { type _swapper_ = a; a = b; b = _swapper_; } while(0)

#define RESX 640
#define RESY 360

#define KEEPMOVINGFORWARD_COMMON_H
#endif
