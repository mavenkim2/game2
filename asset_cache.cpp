// NOTE: an asset is anything that is loaded from disk to be used in game.

// TODO: Features of asset system:
// - Hashes the asset filename, stores the data
// - If two requests to load an asset are issued, don't load them twice
// - Hot load assets
// - Later: if there isn't enough space, evict the least used
//
// - Be able to asynchronously load multiple textures at the same time (using probably a list of PBOs).
// - LRU for eviction

struct AssetSlot;
JOB_CALLBACK(AS_EntryPoint);

struct AS_State
{
    Arena *arena;

    // Must be power of 2
    u8 *ringBuffer;
    u64 ringBufferSize;
    u64 readPos;
    u64 writePos;

    // Synchronization primitives
    OS_Handle *threads;
    u32 threadCount;

    // Hash table for files
    u32 numSlots;
    AssetSlot *assetSlots;
};

struct AssetNode
{
    u64 hash;
    u64 lastModified;
    string path;
    string data;

    AssetNode *next;
};

struct AssetSlot
{
    AssetNode *first;
    AssetNode *last;
};

global AS_State *as_state = 0;

internal void helpmegod(string filename) {}

internal void AS_Init()
{
    Arena *arena    = ArenaAlloc(megabytes(8));
    as_state        = PushStruct(arena, AS_State);
    as_state->arena = arena;

    as_state->numSlots   = 1024;
    as_state->assetSlots = PushArray(arena, AssetSlot, as_state->numSlots);

    as_state->ringBufferSize = kilobytes(64);
    as_state->ringBuffer     = PushArray(arena, u8, as_state->ringBufferSize);

    as_state->threadCount = Max(2, OS_NumProcessors() - 1);
    as_state->threads     = PushArray(arena, OS_Handle, as_state->threadCount);
    for (u64 i = 0; i < as_state->threadCount; i++)
    {
        as_state->threads[i] = OS_ThreadStart(AS_EntryPoint, (void *)i);
    }

    // OS_ThreadStart();
}

internal u64 RingRead(u8 *base, u64 ringSize, u64 readPos, void *dest, u64 destSize)
{
    Assert(ringSize >= destSize);

    u64 cursor         = readPos % ringSize;
    u64 firstPartSize  = Min(ringSize - cursor, destSize);
    u64 secondPartSize = destSize - firstPartSize;

    MemoryCopy(dest, base + cursor, firstPartSize);
    MemoryCopy((u8 *)dest + firstPartSize, base, secondPartSize);
    return destSize;
}

#define RingReadStruct(base, size, readPos, ptr) RingRead((base), (size), (readPos), (ptr), sizeof(*(ptr)))

internal u64 RingWrite(u8 *base, u64 ringSize, u64 writePos, void *src, u64 srcSize)
{
    Assert(ringSize >= srcSize);

    u64 cursor         = writePos % ringSize;
    u64 firstPartSize  = Min(ringSize - cursor, srcSize);
    u64 secondPartSize = srcSize - firstPartSize;

    MemoryCopy(base + cursor, src, firstPartSize);
    MemoryCopy(base, (u8 *)src + firstPartSize, secondPartSize);

    return srcSize;
}

#define RingWriteStruct(base, size, writePos, ptr) RingWrite((base), (size), (writePos), (ptr), sizeof(*(ptr)))

// TODO: timeout if it takes too long for a file to be loaded?
internal b32 AS_EnqueueJob(string path)
{
    b32 sent      = 0;
    u64 writeSize = sizeof(path.size) + path.size;
    for (;;)
    {
        u64 curWritePos = as_state->writePos;
        u64 curReadPos  = as_state->readPos;

        u64 availableSize = as_state->ringBufferSize - (curWritePos - curReadPos);
        if (availableSize >= writeSize)
        {
            writeSize = AlignPow2(writeSize, 8);
            u64 check = AtomicCompareExchangeU64(&as_state->writePos, curWritePos + writeSize, curWritePos);
            if (check == curWritePos)
            {
                sent = 1;
                curWritePos +=
                    RingWriteStruct(as_state->ringBuffer, as_state->ringBufferSize, curWritePos, &path.size);
                curWritePos +=
                    RingWrite(as_state->ringBuffer, as_state->ringBufferSize, curWritePos, path.str, path.size);
                break;
            }
        }
        else OS_SignalWait(as_state->writeSemaphore);
    }
    if (sent)
    {
        OS_ReleaseSemaphore(as_state->readSemaphore, 1);
    }
    return sent;
}

internal string AS_DequeueFile(Arena *arena)
{
    string result = {};
    for (;;)
    {
        u64 curWritePos = as_state->writePos;
        u64 curReadPos  = as_state->readPos;

        u64 availableSize = curWritePos - curReadPos;
        if (availableSize >= sizeof(u64))
        {
            u64 newReadPos = curReadPos;
            newReadPos += RingReadStruct(as_state->ringBuffer, as_state->ringBufferSize, newReadPos, &result.size);
            u64 totalSize   = result.size + sizeof(u64);
            u64 alignedSize = AlignPow2(totalSize, 8);
            u64 check       = AtomicCompareExchangeU64(&as_state->readPos, curReadPos + alignedSize, curReadPos);
            if (check == curReadPos)
            {
                result.str = PushArrayNoZero(arena, u8, result.size);
                RingRead(as_state->ringBuffer, as_state->ringBufferSize, newReadPos, result.str, result.size);
                break;
            }
        }
        else OS_SignalWait(as_state->readSemaphore);
    }
    OS_ReleaseSemaphore(as_state->writeSemaphore, 1);
    return result;
}

// Loop infinitely, dequeue files to be read, kick off a task, and then go back to sleep
JOB_CALLBACK(AS_EntryPoint)
{
    for (;;)
}

// Dequeues file to be processed, sees if it has a hash
// How do i even want this to work?
// LoadModel(), which then loads corresponding textures, meshes, etc.
// how do you know when it's done loading if the loading is async?

// JOB_ENTRY_POINT(AS_ProcessFiles)
// {
//     for (;;)
//     {
//         TempArena temp  = ScratchBegin(0, 0);
//         string filename = AS_DequeueFile(temp.arena);
//         u64 hash        = HashFromString(filename);
//         AssetSlot *slot = as_state->assetSlots[hash];
//         AssetNode *n    = 0;
//         for (AssetNode *node = slot->first; node != 0; node = node->next)
//         {
//             if (node->hash == hash)
//             {
//                 n = node;
//                 break;
//             }
//         }
//     }
//
//     if (n == 0)
//     {
//         n = PushStruct(arena, AssetNode);
//         QueuePush(slot->first, slot->last, n);
//         n->hash = hash;
//         n->filename =
//     }
//
//     if (OS_GetLastWriteTime(filename))
//
//
// // should own the data personally
//
// string output = ReadEntireFile(filename);
// n->data       = output;
//
// ScratchEnd(temp);
//
// OS_QueueJob(
//}
