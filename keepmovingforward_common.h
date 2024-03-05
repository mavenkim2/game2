#include <cstdlib>
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
#include <intrin.h>
#else
#define readonly
#endif

#if COMPILER_MSVC
#define thread_global __declspec(thread)
#elif COMPILER_CLANG || COMPILER_GCC
#define thread_global __thread
#endif

// NOTE: so LSP doesn't grey out preproc directives, remove this when shippin
#define INTERNAL    1
#define UNOPTIMIZED 1

#define PI 3.14159265359f

#define global   static
#define internal static
#define U32Max   0xffffffff

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

//
// MACROS
//

#define ArrayLength(array) sizeof(array) / sizeof((array)[0])
#define kilobytes(value)   ((value) * 1024LL)
#define megabytes(value)   (kilobytes(value) * 1024LL)
#define gigabytes(value)   (megabytes(value) * 1024LL)
#define terabytes(value)   (gigabytes(value) * 1024LL)

#if UNOPTIMIZED
#define Assert(expression) (!(expression) ? (*(volatile int *)0 = 0, 0) : 0)
#else
#define Assert(expression) (void)0
#endif

#define Unreachable Assert(!"Unreachable")
#define Swap(type, a, b)                                                                                          \
    do                                                                                                            \
    {                                                                                                             \
        type _swapper_ = a;                                                                                       \
        a              = b;                                                                                       \
        b              = _swapper_;                                                                               \
    } while (0)

// NOTE: does it matter that this is a u64 instead of uintptr_t?
#define Offset(type, member) (u64) & (((type *)0)->member)

#define MemoryCopy            memcpy
#define MemorySet             memset
#define MemoryZero(ptr, size) MemorySet((ptr), 0, (size))

#define ArrayInit(arena, array, type, _cap)                                                                       \
    do                                                                                                            \
    {                                                                                                             \
        array.cap   = _cap;                                                                                       \
        array.items = PushArray(arena, type, _cap);                                                               \
    } while (0)

#define Array(type)                                                                                               \
    struct                                                                                                        \
    {                                                                                                             \
        type *items = 0;                                                                                          \
        u32 count   = 0;                                                                                          \
        u32 cap     = 0;                                                                                          \
    }
#define ArrayPush(array, item) (Assert((array)->count < (array)->cap), (array)->items[(array)->count++] = item)

// Loops
#define DO_STRING_JOIN(arg1, arg2) arg1##arg2
#define STRING_JOIN(arg1, arg2)    DO_STRING_JOIN(arg1, arg2)
#define foreach(array, ptr)                                                                                       \
    for (u32 STRING_JOIN(i, __LINE__) = 0; STRING_JOIN(i, __LINE__) < (array)->count; STRING_JOIN(i, __LINE__)++) \
        if ((ptr = (array)->items + STRING_JOIN(i, __LINE__)) != 0)

#define foreach_value(array, val)                                                                                 \
    for (u32 STRING_JOIN(i, __LINE__) = 0; STRING_JOIN(i, __LINE__) < (array)->count; STRING_JOIN(i, __LINE__)++) \
        if ((val = (array)->items[STRING_JOIN(i, __LINE__)]), 1)

#define foreach_index(array, ptr, index)                                                                          \
    for (u32 index = 0; index < (array)->count; index++)                                                          \
        if ((ptr = (array)->items + index) != 0)

#define loopi(start, end) for (u32 i = start; i < end; i++)
#define loopj(start, end) for (u32 j = start; j < end; j++)

#define AlignPow2(x, b) (((x) + (b)-1) & (~((b)-1)))

////////////////////////////////////////////////////////////////////////////////////////
// Linked list helpers
//
#define CheckNull(p) ((p) == 0)
#define SetNull(p)   ((p) = 0)
#define QueuePush_NZ(f, l, n, next, zchk, zset)                                                                   \
    (zchk(f) ? (((f) = (l) = (n)), zset((n)->next)) : ((l)->next = (n), (l) = (n), zset((n)->next)))
#define SLLStackPop_N(f, next)     ((f) = (f)->next)
#define SLLStackPush_N(f, n, next) ((n)->next = (f), (f) = (n))

#define DLLInsert_NPZ(f, l, p, n, next, prev, zchk, zset)                                                         \
    (zchk(f)   ? (((f) = (l) = (n)), zset((n)->next), zset((n)->prev))                                            \
     : zchk(p) ? (zset((n)->prev), (n)->next = (f), (zchk(f) ? (0) : ((f)->prev = (n))), (f) = (n))               \
               : ((zchk((p)->next) ? (0) : (((p)->next->prev) = (n))), (n)->next = (p)->next, (n)->prev = (p),    \
                  (p)->next = (n), ((p) == (l) ? (l) = (n) : (0))))
#define DLLPushBack_NPZ(f, l, n, next, prev, zchk, zset) DLLInsert_NPZ(f, l, l, n, next, prev, zchk, zset)
#define DLLRemove_NPZ(f, l, n, next, prev, zchk, zset)                                                            \
    (((f) == (n))   ? ((f) = (f)->next, (zchk(f) ? (zset(l)) : zset((f)->prev)))                                  \
     : ((l) == (n)) ? ((l) = (l)->prev, (zchk(l) ? (zset(f)) : zset((l)->next)))                                  \
                    : ((zchk((n)->next) ? (0) : ((n)->next->prev = (n)->prev)),                                   \
                       (zchk((n)->prev) ? (0) : ((n)->prev->next = (n)->next))))

#define QueuePush(f, l, n) QueuePush_NZ(f, l, n, next, CheckNull, SetNull)
#define StackPop(f)        SLLStackPop_N(f, next)
#define StackPush(f, n)    SLLStackPush_N(f, n, next)

#define DLLPushBack(f, l, n)  DLLPushBack_NPZ(f, l, n, next, prev, CheckNull, SetNull)
#define DLLPushFront(f, l, n) DLLPushBack_NPZ(l, f, n, prev, next, CheckNull, SetNull)
#define DLLInsert(f, l, p, n) DLLInsert_NPZ(f, l, p, n, next, prev, CheckNull, SetNull)
#define DLLRemove(f, l, n)    DLLRemove_NPZ(f, l, n, next, prev, CheckNull, SetNull)

////////////////////////////////////////////////////////////////////////////////////////
// Array List
// TODO: probably going to get rid of this
struct AHeader
{
    u32 count;
    u32 cap;
};

inline void *ArrayGrow(void *a, u32 size, u32 length, u32 minCap);

#define ArrayHeader(a)      ((AHeader *)(a)-1)
#define ArrayPut(a, item)   (ArrayMayGrow((a), 1), (a)[ArrayHeader(a)->count++] = item)
#define ArrayLen(a)         ((a) ? ArrayHeader(a)->count : 0)
#define ArrayCap(a)         ((a) ? ArrayHeader(a)->cap : 0)
#define ArraySetCap(a, cap) (ArrayGrowWrap(a, 0, cap))
#define ArraySetLen(a, len)                                                                                       \
    ((ArrayCap(a) < (len) ? ArraySetCap((a), (len)), 0 : 0), (a) ? (ArrayHeader(a)->count = (len)) : 0)

#define ArrayMayGrow(a, n)                                                                                        \
    ((!(a) || (ArrayHeader(a)->count) + (n) > ArrayHeader(a)->cap) ? (ArrayGrowWrap((a), (n), 0), 0) : 0)
#define ArrayGrowWrap(a, b, c) ((a) = ArrayGrowWrapper((a), (sizeof(*a)), (b), (c)))

template <class T> internal T *ArrayGrowWrapper(T *a, u32 size, u32 length, u32 minCap)
{
    return (T *)ArrayGrow(a, size, length, minCap);
}

inline void *ArrayGrow(void *a, u32 size, u32 length, u32 minCap)
{
    void *b;
    u32 minCount = ArrayLen(a) + length;
    if (minCap < minCount)
    {
        minCap = minCount;
    }
    if (minCap < 2 * ArrayCap(a)) minCap = 2 * ArrayCap(a);
    else if (minCap < 4) minCap = 4;

    b = realloc((a) ? ArrayHeader(a) : 0, size * minCap + sizeof(AHeader));
    b = (u8 *)b + sizeof(AHeader);
    if (a == 0)
    {
        ArrayHeader(b)->count = 0;
    }
    ArrayHeader(b)->cap = minCap;
    return b;
}

#define forEach(array, ptr)                                                                                       \
    for (u32 STRING_JOIN(i, __LINE__) = 0; STRING_JOIN(i, __LINE__) < ArrayLen(array);                            \
         STRING_JOIN(i, __LINE__)++)                                                                              \
        if ((ptr = (array) + STRING_JOIN(i, __LINE__)) != 0)

// Atomics
#if COMPILER_MSVC
#define AtomicCompareExchangeU32(dest, src, expected)                                                             \
    _InterlockedCompareExchange((long volatile *)dest, src, expected)
#define AtomicCompareExchangeU64(dest, src, expected)                                                             \
    _InterlockedCompareExchange64((__int64 volatile *)dest, src, expected)

#define AtomicIncrementU32(dest) _InterlockedIncrement((long volatile *)dest)
#define AtomicIncrementU64(dest) _InterlockedIncrement64((__int64 volatile *)dest)

#define AtomicDecrementU32(dest)   _InterlockedDecrement((long volatile *)dest)
#define AtomicDecrementU64(dest)   _InterlockedDecrement64((__int64 volatile *)dest)
#define AtomicAddU64(dest, addend) _InterlockedExchangeAdd64((__int64 volatile *)dest, addend)
#define WriteBarrier()             _mm_sfence()

struct TicketMutex
{
    u64 volatile ticket;
    u64 volatile serving;
};

inline void BeginTicketMutex(TicketMutex *mutex)
{
    u64 ticket = AtomicAddU64(&mutex->ticket, 1);
    while (ticket != mutex->serving)
    {
        _mm_pause();
    }
}

inline void EndTicketMutex(TicketMutex *mutex)
{
    AtomicIncrementU64(&mutex->serving);
}

struct Mutex
{
    u32 count;
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

// #define WriteLock 1 << 63
//
// inline void BeginRLock(ReadWriteLock *lock)
// {
//     while (lock->count & WriteLock)
//         _mm_pause();
//
//     AtomicIncrementU64(&lock->count);
// }
//
// inline void EndRLock(ReadWriteLock *lock)
// {
//     AtomicDecrementU64(&lock->count);
// }
//
// inline void BeginWLock(ReadWriteLock *lock)
// {
//     while (AtomicCompareExchangeU64(lock->count, WriteLock, 0))
//         _mm_pause();
// }
//
// inline void EndWLock(ReadWriteLock *lock)
// {
//     lock->count = 0;
// }
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
