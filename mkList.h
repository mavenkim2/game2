#include <vector>
#include <atomic>
#include "mkCrack.h"
#ifdef LSP_INCLUDE
#include "mkCommon.h"
#endif

template <typename T, typename A = std::allocator<T>>
using list = std::vector<T, A>;

// template <typename T>
// struct list
// {
//     i32 size;
//     i32 capacity;
//     T *data;
//
//     inline list()
//     {
//         size     = 0;
//         capacity = 0;
//         data     = 0;
//     }
//
//     inline void clear()
//     {
//         size     = 0;
//         capacity = 0;
//         free(data);
//         data = 0;
//     }
// };

template <u32 hashSize, u32 indexSize>
struct AtomicFixedHashTable
{
    std::atomic<u8> locks[hashSize]; // TODO: maybe could have "stripes", which would save memory but some buckets
                                     // would be locked even if they are unused
    u32 hash[hashSize];
    u32 nextIndex[hashSize];

    AtomicFixedHashTable();
    void Clear();
    void BeginLock(u32 lockIndex);
    void EndLock(u32 lockIndex);
    u32 First(u32 key) const;
    u32 FirstAndLock(u32 key) const;
    u32 Next(u32 index) const;
    b8 IsValid(u32 index) const;
    b8 IsValidLock(u32 key, u32 index) const;

    void Add(u32 key, u32 index);
    void Remove(u32 key, u32 index);
};

template <u32 hashSize, u32 indexSize>
inline AtomicFixedHashTable<hashSize, indexSize>::AtomicFixedHashTable()
{
    StaticAssert(indexSize < 0xffffffff, IndexIsNotU32Max);
    StaticAssert((hashSize & (hashSize - 1)) == 0, HashIsPowerOfTwo);
    Clear();
}

template <u32 hashSize, u32 indexSize>
inline b8 AtomicFixedHashTable<hashSize, indexSize>::IsValid(u32 index) const
{
    return index != 0xffffffff;
}

template <u32 hashSize, u32 indexSize>
inline b8 AtomicFixedHashTable<hashSize, indexSize>::IsValidLock(u32 key, u32 index) const
{
    if (index == 0xffff)
    {
        key &= (hashSize - 1);
        EndLock(key);
    }
    return index != 0xffff;
}

template <u32 hashSize, u32 indexSize>
inline void AtomicFixedHashTable<hashSize, indexSize>::Clear()
{
    MemorySet(hash, 0xff, sizeof(hash));
    MemorySet(locks, 0, sizeof(locks));
}

template <u32 hashSize, u32 indexSize>
inline void AtomicFixedHashTable<hashSize, indexSize>::BeginLock(u32 lockIndex)
{
    u8 val = 0;
    while (!locks[lockIndex].compare_exchange_weak(val, val + 1))
    {
        _mm_pause();
    }
}

template <u32 hashSize, u32 indexSize>
inline void AtomicFixedHashTable<hashSize, indexSize>::EndLock(u32 lockIndex)
{
    locks[lockIndex].store(0);
}

template <u32 hashSize, u32 indexSize>
inline u32 AtomicFixedHashTable<hashSize, indexSize>::FirstAndLock(u32 key) const
{
    key &= (hashSize - 1);
    BeginLock(key);
    return hash[key];
}

template <u32 hashSize, u32 indexSize>
inline u32 AtomicFixedHashTable<hashSize, indexSize>::First(u32 key) const
{
    key &= (hashSize - 1);
    return hash[key];
}

template <u32 hashSize, u32 indexSize>
inline u32 AtomicFixedHashTable<hashSize, indexSize>::Next(u32 index) const
{
    return nextIndex[index];
}

template <u32 hashSize, u32 indexSize>
inline void AtomicFixedHashTable<hashSize, indexSize>::Add(u32 key, u32 index)
{
    Assert(index < indexSize);
    key &= (hashSize - 1);
    BeginLock(key);
    nextIndex[index] = hash[key];
    hash[key]        = index;
    EndLock(key);
}

template <u32 hashSize, u32 indexSize>
inline void AtomicFixedHashTable<hashSize, indexSize>::Remove(u32 key, u32 index)
{
    key &= (hashSize - 1);
    BeginLock(key);
    if (hash[key] == index)
    {
        hash[key] = nextIndex[index];
    }
    else
    {
        for (u32 i = hash[key]; IsValidLock(key, i); i = Next(i))
        {
            if (nextIndex[i] == index)
            {
                nextIndex[i] = nextIndex[index];
                break;
            }
        }
    }
}

struct HashIndex
{
    i32 *hash;
    i32 *indexChain;
    i32 hashSize;
    i32 indexChainSize;
    i32 hashMask;

    void Init(i32 inHashSize, i32 inChainSize)
    {
        hashSize = inHashSize;
        Assert((hashSize & (hashSize - 1)) == 0); // pow 2
        indexChainSize = inChainSize;
        hashMask       = inHashSize - 1;
        hash           = new i32[inHashSize];
        MemorySet(hash, 0xff, sizeof(hash[0]) * hashSize);
        indexChain = new i32[inHashSize];
        MemorySet(indexChain, 0xff, sizeof(indexChain[0]) * indexChainSize);
    }

    void Init()
    {
        Init(1024, 1024);
    }

    i32 FirstInHash(i32 inHash)
    {
        i32 result = hash[inHash & hashMask];
        return result;
    }

    i32 NextInHash(i32 in)
    {
        i32 result = indexChain[in];
        return result;
    }

    void AddInHash(i32 key, i32 index)
    {
        i32 slot          = key & hashMask;
        indexChain[index] = hash[slot];
        hash[slot]        = index;
    }

    void AddInHash(i32 key, i32 index, i32 value)
    {
        i32 slot          = key & hashMask;
        indexChain[index] = hash[slot];
        hash[slot]        = value;
    }

    b32 RemoveFromHash(i32 key, i32 index)
    {
        b32 result = 0;
        i32 slot   = key & hashMask;
        if (hash[slot] == index)
        {
            hash[slot] = -1;
            result     = 1;
        }
        else
        {
            for (i32 i = hash[slot]; i != -1; i = indexChain[i])
            {
                if (indexChain[i] == index)
                {
                    indexChain[i] = indexChain[index];
                    result        = 1;
                    break;
                }
            }
        }
        indexChain[index] = -1;
        return result;
    }

    i32 Hash(i32 value)
    {
        return value & hashMask;
    }

    i32 Hash(string string)
    {
        i32 result = 0;
        for (u64 i = 0; i < string.size; i++)
        {
            result += (string.str[i]) * ((i32)i + 119);
        }
        return result;
    }
};
