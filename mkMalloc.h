#ifndef MALLOC_H
#define MALLOC_H

namespace Memory
{
struct MallocArena;
const u32 cChunkSize = megabytes(4);
const u32 cPageSize  = kilobytes(4); // 2 ^ 12

// TODO: in order to actually choose size groups I need to see the allocation statistics of this app
// Small: [8], [16, 32, 48, ..., 512], [1 KiB, 2 KiB]
const u32 cNumSmallGroups = 1 + (((512 - 16) >> 4) + 1) + 2;

const u32 cMaxLargeSizeShift = 21;
const u32 cMaxLargeSizeGroup = 1 << 21; // kilobytes(2048);
// Large: [4 KiB, 8 KiB, 16 KiB, 2048 KiB]
const u32 cNumLargeGroups = cMaxLargeSizeShift - 10 - 1; // 1 << 10 = 1 KiB
const u32 cTotalNumGroups = cNumSmallGroups + cNumLargeGroups;

// const u32 cMaxSizeShift              = 12; // 1 << 12 = 4096
// const u32 cMaxSize                   = 1 << cMaxSizeShift;
const u32 cMinimumSizeGroupDiffShift = 3; // 1 << 3 = 8

global u32 indexToSizeTable[cTotalNumGroups];
global u32 sizeToIndexTable[cMaxLargeSizeGroup >> cMinimumSizeGroupDiffShift];

inline u32 GetSizeFromIndex(u32 index)
{
    Assert(index < ArrayLength(indexToSizeTable));
    return indexToSizeTable[index];
}

inline u32 ConvertSizeToTableIndex(u32 size)
{
    return (size + (1 << cMinimumSizeGroupDiffShift) - 1) >> cMinimumSizeGroupDiffShift;
}

inline u32 GetIndexFromSize(u32 size)
{
    u32 sizeIndex = ConvertSizeToTableIndex(size);
    Assert(sizeIndex < ArrayLength(sizeToIndexTable));
    u32 result = sizeToIndexTable[sizeIndex];
    return result;
}

enum class MemoryTag : u32
{
    Global,
    Asset,
    Render,
    Count,
};

#if 0
struct Chunk
{
    void *memory;
    u32 chunkSize; // default 4MB?
    Chunk *prev;
    Chunk *next;

    Run *runs;
};

struct SmallRun
{
    u32 bitmask[4096 / ?];
};

struct MallocArena
{
    Chunk *first;
    Chunk *last;
};

// Runs are no smaller than 1 page, and contain a single size class
struct Run
{
    u32 numPages;
    u32 sizeClass;
    u64 bitmap[4]; // 256
    void *ptr;
    // u32 bitmap; // TODO
};
#endif

const u32 cBinSize = kilobytes(512);
struct ThreadLocalBin
{
    u16 numElementsAllocated;
    u32 maxElementCount;
    u32 numGroups;
};

ThreadLocalBin *CreateBin(u32 binElementSize);
void *Allocate(ThreadLocalBin *bin);
void Free(ThreadLocalBin *bin, void *ptr);

// void *GetFreeAllocation(Run *run)
// {
//     u32 i = 0;
//     while (run->bitmap[i] != 0)
//     {
//         i++;
//     }
//     u32 freeRegion = GetLowestSetBit(run->bitmap[i]) + 64 * i;
//     u8 *ptr        = (u8 *)run->ptr + freeRegion * sizeClass;
//     return ptr;
// }
//
// internal void SplitRun(Run *run, u32 numPages)
// {
//     while (numPages > run->numPages)
//     {
//     }
// }

struct ThreadLocalCache
{
    ThreadLocalBin *bins[cTotalNumGroups];
    u32 totalSizeAllocated;
};

thread_global ThreadLocalCache *threadLocalCache;

void Init();
void *Malloc(u32 size);
void Free(void *ptr);
void *Realloc(void *ptr, u32 size);
#if INTERNAL
u32 GetAllocationAmount(MemoryTag tag = MemoryTag::Global);
#endif
} // namespace Memory

#if 0
struct MallocAllocator
{
    MallocArena internalArenas[16]; // TODO: make this not fixed?
    std::atomic<u8> numArenas;

    // Small < 4 KiB
    // Medium < 4 MiB
    // Large >= 4 MiB
    u8 GetSizeGroup(u32 size)
    {
        u32 isSmall  = (size & ~(pageSize - 1)) == 0;
        u32 isMedium = (size & ~(chunkSize - 1)) == 0;
        return isSmall ? 0 : (isMedium ? 1 : 2);
    }

    void *Alloc(u32 size)
    {
        if (!threadArena)
        {
            u32 arenaIndex = numArenas.fetch_add(1);
            Assert(arenaIndex < ArrayLength(internalArenas));
        }
        MallocArena *arena = threadArena ? threadArena : internalArenas
    }

    void Free(void *ptr)
    {
    }
};
#endif

#endif
