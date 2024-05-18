#include <vector>

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
