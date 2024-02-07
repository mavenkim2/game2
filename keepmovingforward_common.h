#include <stdint.h>

#if _MSC_VER
#define COMPILER_MSVC 1
#if defined(_WIN32)
#define WINDOWS 1
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

typedef float f32;
typedef double f64;

typedef i32 b32;
typedef i64 b64;

// clang-format off
//
// MACROS
//

#define ArrayLength(array) sizeof(array) / sizeof((array)[0])
#define kilobytes(value) ((value)*1024LL)
#define megabytes(value) (kilobytes(value) * 1024LL)
#define gigabytes(value) (megabytes(value) * 1024LL)
#define terabytes(value) (gigabytes(value) * 1024LL)

#if UNOPTIMIZED
#define Assert(expression) (!(expression) ? (*(volatile int *)0 = 0, 0) : 0)
#else
#define Assert(expression) (void)0
#endif

#define Unreachable Assert(!"Unreachable")
#define Swap(type, a, b) do { type _swapper_ = a; a = b; b = _swapper_; } while(0)

// NOTE: does it matter that this is a u64 instead of uintptr_t?
#define Offset(type, member) (u64)&(((type *)0)->member)

#define MemoryCopy memcpy
#define MemorySet memset
#define MemoryZero(ptr, size) MemorySet((ptr), 0, (size))

#define ArrayInit(arena, array, type, _cap) \
    do { array.cap = _cap; array.items = PushArray(arena, type, _cap); array.count; } while (0)

#define ArrayDef(type) struct Array_##type { type* items; u32 count; u32 cap; }
#define ArrayPush(array, item) (Assert((array)->count < (array)->cap), (array)->items[(array)->count++] = item) 

#define DO_STRING_JOIN(arg1, arg2) arg1 ## arg2
#define STRING_JOIN(arg1, arg2) DO_STRING_JOIN(arg1, arg2)
#define foreach(array, ptr) \
    for (u32 STRING_JOIN(i, __LINE__) = 0; STRING_JOIN(i, __LINE__) < (array)->count; STRING_JOIN(i, __LINE__)++) \
        if ((ptr = (array)->items + STRING_JOIN(i, __LINE__)) != 0)

#define foreach_index(array, ptr, index) \
    for (u32 index = 0; index < (array)->count; index++) \
        if ((ptr = (array)->items + index) != 0)

// clang-format on
