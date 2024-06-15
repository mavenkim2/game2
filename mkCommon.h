#ifndef COMMON_H
#define COMMON_H

#include <cstdlib>
#include <emmintrin.h>
#include <stdint.h>
#include <type_traits>
#include <atomic>
#include <thread>

#if _MSC_VER
#define COMPILER_MSVC 1
#if defined(_WIN32)
#define WINDOWS 1
#define VULKAN  1
#endif
#endif

#if COMPILER_MSVC
#pragma section(".roglob", read)
#define readonly __declspec(allocate(".roglob"))
#include <intrin.h>
#else
#define readonly
#endif

#if COMPILER_MSVC
#define thread_global __declspec(thread)
#elif COMPILER_CLANG || COMPILER_GCC
#define thread_global __thread
#endif

//////////////////////////////
// SSE
//
#define SSE42

// NOTE: so LSP doesn't grey out preproc directives, remove this when shippin
#define INTERNAL    1
#define UNOPTIMIZED 1

#define PI 3.14159265359f

#define global   static
#define internal static
#define U32Max   0xffffffff
#define U64Max   0xffffffffffffffff

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

typedef i8 b8;
typedef i16 b16;
typedef i32 b32;
typedef i64 b64;

//
// MACROS
//

#define ArrayLength(array) sizeof(array) / sizeof((array)[0])
#define kilobytes(value)   ((value)*1024LL)
#define megabytes(value)   (kilobytes(value) * 1024LL)
#define gigabytes(value)   (megabytes(value) * 1024LL)
#define terabytes(value)   (gigabytes(value) * 1024LL)

typedef void print_func(char *fmt, ...);
print_func *Printf;

#ifdef COMPILER_MSVC
#define Trap() __debugbreak()
#else
#error compiler not implemented
#endif
#if UNOPTIMIZED
// #define Assert(expression) (!(expression) ? (*(volatile int *)0 = 0, 0) : 0)
#define Assert(expression)                                                                                 \
    if (expression)                                                                                        \
    {                                                                                                      \
    }                                                                                                      \
    else                                                                                                   \
    {                                                                                                      \
        Printf("Assert fired\nExpression: %s\nFile: %s\nLine Num: %u\n", #expression, __FILE__, __LINE__); \
        Trap();                                                                                            \
    }
#else
#define Assert(expression) (void)(expression)
#endif

//////////////////////////////
// Macros
//
#ifdef COMPILER_MSVC
#define FUNCTION_NAME __FUNCTION__
#else
#error compiler not supported
#endif

#define Unreachable Assert(!"Unreachable")
#define Swap(type, a, b)            \
    do                              \
    {                               \
        type _swapper_ = a;         \
        a              = b;         \
        b              = _swapper_; \
    } while (0)

// NOTE: does it matter that this is a u64 instead of uintptr_t?
#define Offset(type, member) (u64) & (((type *)0)->member)

#define MemoryCopy            memcpy
#define MemorySet             memset
#define MemoryCompare         memcmp
#define MemoryZero(ptr, size) MemorySet((ptr), 0, (size))
#define MemoryZeroStruct(ptr) MemorySet((ptr), 0, sizeof(*(ptr)))

#define ArrayInit(arena, array, type, _cap)         \
    do                                              \
    {                                               \
        array.cap   = _cap;                         \
        array.items = PushArray(arena, type, _cap); \
    } while (0)

#define Array(type)      \
    struct               \
    {                    \
        type *items = 0; \
        u32 count   = 0; \
        u32 cap     = 0; \
    }
#define ArrayPush(array, item)             \
    Assert((array)->count < (array)->cap); \
    (array)->items[(array)->count++] = item

// Loops
#define DO_STRING_JOIN(arg1, arg2) arg1##arg2
#define STRING_JOIN(arg1, arg2)    DO_STRING_JOIN(arg1, arg2)
#define foreach(array, ptr)                                                                                       \
    for (u32 STRING_JOIN(i, __LINE__) = 0; STRING_JOIN(i, __LINE__) < (array)->count; STRING_JOIN(i, __LINE__)++) \
        if ((ptr = (array)->items + STRING_JOIN(i, __LINE__)) != 0)

#define foreach_value(array, val)                                                                                 \
    for (u32 STRING_JOIN(i, __LINE__) = 0; STRING_JOIN(i, __LINE__) < (array)->count; STRING_JOIN(i, __LINE__)++) \
        if ((val = (array)->items[STRING_JOIN(i, __LINE__)]), 1)

#define foreach_index(array, ptr, index)                 \
    for (u32 index = 0; index < (array)->count; index++) \
        if ((ptr = (array)->items + index) != 0)

#define loopi(start, end) for (u32 i = start; i < end; i++)
#define loopj(start, end) for (u32 j = start; j < end; j++)

#define AlignPow2(x, b) (((x) + (b)-1) & (~((b)-1)))

template <typename T>
inline T Align(T value, T alignment)
{
    return ((value + alignment - T(1)) / alignment) * alignment;
}

////////////////////////////////////////////////////////////////////////////////////////
// Linked list helpers
//
#define CheckNull(p) ((p) == 0)
#define SetNull(p)   ((p) = 0)
#define QueuePush_NZ(f, l, n, next, zchk, zset) \
    (zchk(f) ? (((f) = (l) = (n)), zset((n)->next)) : ((l)->next = (n), (l) = (n), zset((n)->next)))
#define SLLStackPop_N(f, next)     ((f) = (f)->next)
#define SLLStackPush_N(f, n, next) ((n)->next = (f), (f) = (n))

#define DLLInsert_NPZ(f, l, p, n, next, prev, zchk, zset)                                                      \
    (zchk(f)   ? (((f) = (l) = (n)), zset((n)->next), zset((n)->prev))                                         \
     : zchk(p) ? (zset((n)->prev), (n)->next = (f), (zchk(f) ? (0) : ((f)->prev = (n))), (f) = (n))            \
               : ((zchk((p)->next) ? (0) : (((p)->next->prev) = (n))), (n)->next = (p)->next, (n)->prev = (p), \
                  (p)->next = (n), ((p) == (l) ? (l) = (n) : (0))))
#define DLLPushBack_NPZ(f, l, n, next, prev, zchk, zset) DLLInsert_NPZ(f, l, l, n, next, prev, zchk, zset)
#define DLLRemove_NPZ(f, l, n, next, prev, zchk, zset)                           \
    (((f) == (n))   ? ((f) = (f)->next, (zchk(f) ? (zset(l)) : zset((f)->prev))) \
     : ((l) == (n)) ? ((l) = (l)->prev, (zchk(l) ? (zset(f)) : zset((l)->next))) \
                    : ((zchk((n)->next) ? (0) : ((n)->next->prev = (n)->prev)),  \
                       (zchk((n)->prev) ? (0) : ((n)->prev->next = (n)->next))))

#define QueuePush(f, l, n) QueuePush_NZ(f, l, n, next, CheckNull, SetNull)
#define StackPop(f)        SLLStackPop_N(f, next)
#define StackPush(f, n)    SLLStackPush_N(f, n, next)

#define DLLPushBack(f, l, n)  DLLPushBack_NPZ(f, l, n, next, prev, CheckNull, SetNull)
#define DLLPushFront(f, l, n) DLLPushBack_NPZ(l, f, n, prev, next, CheckNull, SetNull)
#define DLLInsert(f, l, p, n) DLLInsert_NPZ(f, l, p, n, next, prev, CheckNull, SetNull)
#define DLLRemove(f, l, n)    DLLRemove_NPZ(f, l, n, next, prev, CheckNull, SetNull)

//////////////////////////////
// Atomics
//
#if COMPILER_MSVC
#define AtomicCompareExchangeU32(dest, src, expected) \
    (u32)(_InterlockedCompareExchange((long volatile *)dest, src, expected))
#define AtomicCompareExchangeU64(dest, src, expected) \
    (u64) _InterlockedCompareExchange64((__int64 volatile *)dest, src, expected)

// NOTE: returns the initial value
inline u32 AtomicExchange(u32 *dest, u32 src)
{
    return (u32)_InterlockedExchange((long volatile *)dest, src);
}
inline i32 AtomicCompareExchange(i32 *dest, i32 src, i32 expected)
{
    return (i32)_InterlockedCompareExchange((long volatile *)dest, src, expected);
}

typedef void *PVOID;
#define AtomicCompareExchangePtr(dest, src, expected) \
    _InterlockedCompareExchangePointer((volatile PVOID *)dest, src, expected)

inline u32 AtomicIncrementU32(u32 volatile *dest)
{
    u32 result = _InterlockedIncrement((long volatile *)dest);
    return result;
}

// NOTE: returns the resulting incremented value
inline i32 AtomicIncrementI32(i32 *dest)
{
    i32 result = _InterlockedIncrement((long volatile *)dest);
    return result;
}

// NOTE: returns the resulting decremented value
inline i32 AtomicDecrementI32(i32 *dest)
{
    i32 result = _InterlockedDecrement((long volatile *)dest);
    return result;
}

// returns the initial value
inline i32 AtomicAddI32(i32 volatile *dest, i32 addend)
{
    i32 result = _InterlockedExchangeAdd((long volatile *)dest, addend);
    return result;
}

#define AtomicIncrementU64(dest) _InterlockedIncrement64((__int64 volatile *)dest)

#define AtomicDecrementU32(dest)   _InterlockedDecrement((long volatile *)dest)
#define AtomicDecrementU64(dest)   _InterlockedDecrement64((__int64 volatile *)dest)
#define AtomicAddU32(dest, addend) _InterlockedExchangeAdd((long volatile *)dest, addend)
#define AtomicAddU64(dest, addend) _InterlockedExchangeAdd64((__int64 volatile *)dest, addend)
#define WriteBarrier() \
    _WriteBarrier();   \
    _mm_sfence();
#define ReadBarrier() _ReadBarrier();

struct TicketMutex
{
    std::atomic<u64> ticket = 0;
    std::atomic<u64> serving = 0;
    // u64 volatile ticket;
    // u64 volatile serving;
};

inline void BeginTicketMutex(TicketMutex *mutex)
{
    u64 ticket = mutex->ticket.fetch_add(1);
    while (ticket != mutex->serving)
    {
        _mm_pause();
    }
}

inline void EndTicketMutex(TicketMutex *mutex)
{
    mutex->serving.fetch_add(1);
}

struct Mutex
{
    u32 volatile count;
};

inline void BeginMutex(Mutex *mutex)
{
    while (AtomicCompareExchangeU32(&mutex->count, 1, 0))
    {
        _mm_pause();
    }
}

// TODO: use memory barrier instead, _mm_sfence()?
inline void EndMutex(Mutex *mutex)
{
    WriteBarrier();
    mutex->count = 0;
}

// TODO: ????
inline void BeginRMutex(Mutex *mutex)
{
    for (;;)
    {
        u32 oldValue = (mutex->count & 0x7fffffff);
        u32 newValue = oldValue + 1;
        if (AtomicCompareExchangeU32(&mutex->count, newValue, oldValue) == (i32)oldValue)
        {
            break;
        }
        _mm_pause();
    }
}

inline void EndRMutex(Mutex *mutex)
{
    // TODO: this doesn't have to be a full barier I don't think
    u32 out = AtomicDecrementU32(&mutex->count);
    Assert(out >= 0);
}

inline void BeginWMutex(Mutex *mutex)
{
    while (AtomicCompareExchangeU32(&mutex->count, 0x80000000, 0))
    {
        _mm_pause();
    }
}

inline void EndWMutex(Mutex *mutex)
{
    // TODO: this doesn't have to be a full barrier I don't think
    // also are these reads/writes atomic?
    Assert(mutex->count == 0x80000000);
    WriteBarrier();
    mutex->count = 0;
}

// Fake mutex
struct FakeLock
{
    volatile b8 mLocked;
};

inline void BeginLock(FakeLock *lock)
{
    Assert(!lock->mLocked);
    lock->mLocked = 1;
}

inline void EndLock(FakeLock *lock)
{
    Assert(lock->mLocked);
    lock->mLocked = 0;
}

//////////////////////////////
// Flags
//

template <typename E>
inline b32 HasFlags(E lhs, E rhs)
{
    return (lhs & rhs) == rhs;
}

#if INTERNAL
#define BeginFakeLock(lock) BeginLock(lock)
#define EndFakeLock(lock)   EndLock(lock)
#else
#define BeginFakeLock(lock)
#define EndFakeLock(lock)
#endif

#else
#error Atomics not supported
#endif

//////////////////////////////
// Defer Loop/Scopes
//
#define DeferLoop(begin, end)   for (int _i_ = ((begin), 0); !_i_; _i_ += 1, (end))
#define TicketMutexScope(mutex) DeferLoop(BeginTicketMutex(mutex), EndTicketMutex(mutex))
#define MutexScope(mutex)       DeferLoop(BeginMutex(mutex), EndMutex(mutex))

//////////////////////////////
// Asserts
//
#define Glue(a, b)             a##b
#define StaticAssert(expr, ID) global u8 Glue(ID, __LINE__)[(expr) ? 1 : -1]

#endif
