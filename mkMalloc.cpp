namespace Memory
{
ThreadLocalBin *CreateBin(u32 binElementSize)
{
    // Allocate space for header, free list bitmap, and all bin allocations
    u32 maxElementCount = (cBinSize / binElementSize);
    u32 numGroups       = (maxElementCount + 31) >> 5;
    ThreadLocalBin *bin = (ThreadLocalBin *)platform.Alloc(sizeof(ThreadLocalBin) + sizeof(u32) * numGroups + cBinSize);
    MemorySet((u8 *)bin + sizeof(ThreadLocalBin), 0xff, sizeof(u32) * numGroups);
    bin->numElementsAllocated = 0;
    bin->numGroups            = numGroups;
    bin->maxElementCount      = maxElementCount;
    return bin;
}

void *Allocate(ThreadLocalBin *bin)
{
    u32 *bitmap   = (u32 *)((u8 *)bin + sizeof(ThreadLocalBin));
    u32 freeIndex = 0xffffffff;
    for (u32 i = 0; i < bin->numGroups; i++)
    {
        if (bitmap[i] != 0)
        {
            u32 lowestBit = GetLowestSetBit(bitmap[i]);
            freeIndex     = 32 * i + lowestBit;
            bitmap[i] &= ~(1u << lowestBit);
            break;
        }
    }
    Assert(freeIndex != 0xffffffff);
    u8 *ptr = (u8 *)bin + sizeof(ThreadLocalBin) + sizeof(u32) * bin->numGroups;
    bin->numElementsAllocated++;
    return ptr;
}

void Free(ThreadLocalBin *bin, void *ptr)
{
    uintptr freedIndex = (uintptr)((u8 *)ptr - ((u8 *)bin + sizeof(ThreadLocalBin) + sizeof(u32) * bin->numGroups));
    Assert(freedIndex < U32Max);
    Assert(bin->numElementsAllocated > 0);
    bin->numElementsAllocated--;
    u32 *bitmap = (u32 *)((u8 *)bin + sizeof(ThreadLocalBin));
    bitmap[freedIndex >> 5] |= (1u << (freedIndex & 31));
}

#if INTERNAL
global u32 totalAllocationAmounts[MemoryTag::Count];
#endif

void Init()
{
    indexToSizeTable[0] = 8;
    for (u32 i = 1; i < cNumSmallGroups - 2; i++)
    {
        indexToSizeTable[i] = 16 * i;
    }
    indexToSizeTable[cNumSmallGroups - 2] = kilobytes(1);
    indexToSizeTable[cNumSmallGroups - 1] = kilobytes(2);

    for (u32 i = 0; i < cNumLargeGroups; i++) // cNumSmallGroups; i < cTotalNumGroups; i++)
    {
        indexToSizeTable[cNumSmallGroups + i] = kilobytes(4) * Pow(2, i);
    }

    u32 prevStart = 0;
    for (u32 i = 0; i < cTotalNumGroups; i++)
    {
        u32 index = ConvertSizeToTableIndex(indexToSizeTable[i]);
        for (u32 j = prevStart; j <= index; j++)
        {
            sizeToIndexTable[j] = i;
        }
        prevStart = index + 1;
    }
}

void *Malloc(u32 size)
{
    void *result = 0;
    if (size < cBinSize)
    {
        // Fast thread local cache path
        u32 index         = GetIndexFromSize(size + sizeof(u32));
        u32 quantizedSize = GetSizeFromIndex(index);
        if (!threadLocalCache)
        {
            // NOTE: this must be all zeroes
            threadLocalCache                     = (ThreadLocalCache *)platform.Alloc(sizeof(ThreadLocalCache));
            threadLocalCache->totalSizeAllocated = 0;
        }
        ThreadLocalBin *bin = threadLocalCache->bins[index];
        if (!bin)
        {
            threadLocalCache->bins[index] = CreateBin(quantizedSize);
            bin                           = threadLocalCache->bins[index];
        }
        result         = Allocate(bin);
        *(u32 *)result = quantizedSize;
        result         = (u32 *)result + 1;
#if INTERNAL
        totalAllocationAmounts[(u32)MemoryTag::Global] += quantizedSize;
#endif
    }
    else
    {
        Assert(0);
    }
    return result;
}

void Free(void *ptr)
{
    // TODO: I don't really like this. maybe need a hash table or something
    u32 *sizeGroup      = (u32 *)ptr - 1;
    u32 index           = GetIndexFromSize(*sizeGroup);
    ThreadLocalBin *bin = threadLocalCache->bins[index];
    Assert(bin);
    Free(bin, sizeGroup);
#if INTERNAL
    totalAllocationAmounts[(u32)MemoryTag::Global] -= *sizeGroup;
#endif
}

void *Realloc(void *ptr, u32 size)
{
    u32 *sizeGroup = (u32 *)ptr - 1;
    u32 index      = GetIndexFromSize(*sizeGroup);
    void *newPtr   = Malloc(size);
    MemoryCopy(newPtr, ptr, *sizeGroup);
    Free(ptr);
    return newPtr;
}

#if INTERNAL
u32 GetAllocationAmount(MemoryTag tag)
{
    u32 amount = totalAllocationAmounts[(u32)tag];
    return amount;
}
#endif
} // namespace Memory
