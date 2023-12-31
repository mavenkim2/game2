#ifndef KEEPMOVINGFORWARD_TYPES_H
#include <stdint.h>

// NOTE: so LSP doesn't grey out preproc directives, remove this when shippin
#define INTERNAL 1
#define UNOPTIMIZED 1

#define PI 3.14159265359f

#define global static
#define internal static

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

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

#define Unreachable Assert(false)

#define KEEPMOVINGFORWARD_TYPES_H
#endif
