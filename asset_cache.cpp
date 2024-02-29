// NOTE: an asset is anything that is loaded from disk to be used in game.

// TODO: Features of asset system:
// - Hashes the asset filename, stores the data
// - If two requests to load an asset are issued, don't load them twice
// - Hot load assets
// - Later: if there isn't enough space, evict the least used
//
// - Be able to asynchronously load multiple textures at the same time (using probably a list of PBOs).
// - LRU for eviction

global AS_CacheState *as_state = 0;

internal void AS_Init()
{
    Arena *arena    = ArenaAlloc(megabytes(8));
    as_state        = PushStruct(arena, AS_CacheState);
    as_state->arena = arena;

    as_state->numSlots   = 1024;
    as_state->assetSlots = PushArray(arena, AS_Slot, as_state->numSlots);

    as_state->ringBufferSize = kilobytes(64);
    as_state->ringBuffer     = PushArray(arena, u8, as_state->ringBufferSize);

    as_state->threadCount    = Min(2, OS_NumProcessors() - 1);
    as_state->readSemaphore  = OS_CreateSemaphore(as_state->threadCount);
    as_state->writeSemaphore = OS_CreateSemaphore(as_state->threadCount);

    as_state->threads = PushArray(arena, OS_Handle, as_state->threadCount);
    for (u64 i = 0; i < as_state->threadCount; i++)
    {
        as_state->threads[i] = OS_ThreadStart(AS_EntryPoint, (void *)i);
    }
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
internal b32 AS_EnqueueFile(string path)
{
    b32 sent      = 0;
    u64 writeSize = sizeof(path.size) + path.size;
    for (;;)
    {
        BeginTicketMutex(&as_state->mutex);

        u64 availableSize = as_state->ringBufferSize - (as_state->writePos - as_state->readPos);
        if (availableSize >= writeSize)
        {
            writeSize = AlignPow2(writeSize, 8);
            sent      = 1;
            as_state->writePos +=
                RingWriteStruct(as_state->ringBuffer, as_state->ringBufferSize, as_state->writePos, &path.size);
            as_state->writePos +=
                RingWrite(as_state->ringBuffer, as_state->ringBufferSize, as_state->writePos, path.str, path.size);
            as_state->writePos = AlignPow2(as_state->writePos, 8);
            EndTicketMutex(&as_state->mutex);
            break;
        }
        EndTicketMutex(&as_state->mutex);
        OS_SignalWait(as_state->writeSemaphore);
    }
    if (sent)
    {
        OS_ReleaseSemaphore(as_state->readSemaphore);
    }
    return sent;
}

internal string AS_DequeueFile(Arena *arena)
{
    string result = {};
    for (;;)
    {
        BeginTicketMutex(&as_state->mutex);

        u64 availableSize = as_state->writePos - as_state->readPos;
        if (availableSize >= sizeof(u64))
        {
            as_state->readPos +=
                RingReadStruct(as_state->ringBuffer, as_state->ringBufferSize, as_state->readPos, &result.size);
            result.str = PushArrayNoZero(arena, u8, result.size + 1);
            as_state->readPos += RingRead(as_state->ringBuffer, as_state->ringBufferSize, as_state->readPos,
                                          result.str, result.size);
            as_state->readPos = AlignPow2(as_state->readPos, 8);
            EndTicketMutex(&as_state->mutex);
            break;
        }
        EndTicketMutex(&as_state->mutex);
        OS_SignalWait(as_state->readSemaphore);
    }
    OS_ReleaseSemaphore(as_state->writeSemaphore);
    return result;
}

//////////////////////////////
// Handle
//
internal AS_Handle AS_GetAssetHandle(string path)
{
    u64 hash      = HashFromString(path);
    AS_Slot *slot = &as_state->assetSlots[(hash & (as_state->numSlots - 1))];
    AS_Node *n    = 0;
    BeginMutex(&slot->mutex);
    for (AS_Node *node = slot->first; node != 0; node = node->next)
    {
        if (node->path == path)
        {
            n = node;
            break;
        }
    }
}

//////////////////////////////
// Asset Thread Entry Points
//
internal void AS_EntryPoint(void *p)
{
    SetThreadName(Str8Lit("[AS] Scanner"));
    for (;;)
    {
        TempArena scratch   = ScratchStart(0, 0);
        string path         = AS_DequeueFile(scratch.arena);
        path.str[path.size] = 0;

        u64 hash      = HashFromString(path);
        AS_Slot *slot = &as_state->assetSlots[(hash & (as_state->numSlots - 1))];
        AS_Node *n    = 0;
        BeginMutex(&slot->mutex);
        for (AS_Node *node = slot->first; node != 0; node = node->next)
        {
            if (node->path == path)
            {
                n = node;
                break;
            }
        }
        EndMutex(&slot->mutex);

        if (n == 0)
        {
            TicketMutexScope(&as_state->mutex)
            {
                n = as_state->freeNode;
                if (n == 0)
                {
                    n = PushStruct(as_state->arena, AS_Node);
                }
                else
                {
                    StackPop(as_state->freeNode);
                }
            }

            MutexScope(&slot->mutex)
            {
                QueuePush(slot->first, slot->last, n);
            }
            n->hash = hash;
            n->path = path;
        }

        OS_Handle handle             = OS_OpenFile(OS_AccessFlag_Read | OS_AccessFlag_ShareRead, path);
        OS_FileAttributes attributes = OS_AttributesFromFile(handle);
        // // Hot load
        // // TODO: this is no bueno
        if (n != 0 && attributes.lastModified != 0 && n->lastModified != attributes.lastModified)
        {
            ArenaRelease(n->arena);
        }

        n->lastModified = attributes.lastModified;
        n->arena        = ArenaAlloc(attributes.size);
        n->data.str     = PushArrayNoZero(n->arena, u8, attributes.size);
        n->data.size    = OS_ReadEntireFile(handle, n->data.str);
        OS_CloseFile(handle);

        JS_Kick(AS_LoadAsset, n, 0, Priority_Low);
        ScratchEnd(scratch);
    }
}

internal void AS_HotloadEntryPoint(void *p)
{
    u64 threadIndex = (u64)p;
    TempArena temp  = ScratchStart(0, 0);
    SetThreadName(PushStr8F(temp.arena, "[AS] Hotload %u", threadIndex));
    ScratchEnd(temp);

    for (;;)
    {
        for (u32 i = 0; i < as_state->numSlots; i++)
        {
            AS_Slot *slot = as_state->assetSlots + i;
            MutexScope(&slot->mutex)
            {
                for (AS_Node *node = slot->first; node != 0; node = node->next)
                {
                }
            }
        }
    }
}

//////////////////////////////
// Specific asset loading callbacks
//
// TODO: create a pack file so you don't have to check the file extension
JOB_CALLBACK(AS_LoadAsset)
{
    AS_Node *node    = (AS_Node *)data;
    string extension = GetFileExtension(node->path);
    int stop         = 5;
    if (extension == Str8Lit("model"))
    {
    }
    else if (extension == Str8Lit("anim"))
    {
    }
    else if (extension == Str8Lit("skel"))
    {
        SkeletonHelp skeleton;
        Tokenizer tokenizer;
        tokenizer.input  = node->data;
        tokenizer.cursor = tokenizer.input.str;

        u32 version;
        u32 count;
        GetPointer(&tokenizer, &version);
        GetPointer(&tokenizer, &count);
        skeleton.count = count;

        if (version == 1)
        {
            loopi(0, count)
            {
                ReadLine(&tokenizer);
            }
            skeleton.parents = GetTokenCursor(&tokenizer, i32);
            Advance(&tokenizer, sizeof(skeleton.parents[0]) * count);
            skeleton.inverseBindPoses = GetTokenCursor(&tokenizer, Mat4);
            Advance(&tokenizer, sizeof(skeleton.inverseBindPoses[0]) * count);
            skeleton.transformsToParent = GetTokenCursor(&tokenizer, Mat4);
            Advance(&tokenizer, sizeof(skeleton.transformsToParent[0]) * count);

            Assert(EndOfBuffer(&tokenizer));
        }
        node->type = AS_Skeleton;
    }
    // TODO: I wish I could just stream the texture from this thread, but opengl won't let me
    else if (extension == Str8Lit("png"))
    {
        node->type = AS_Texture;
        if (FindSubstring(node->path, Str8Lit("diffuse"), MatchFlag_CaseInsensitive) != node->path.size)
        {
            node->texture.type = TextureType_Diffuse;
        }
        else if (FindSubstring(node->path, Str8Lit("normal"), MatchFlag_CaseInsensitive) != node->path.size)
        {
            node->texture.type = TextureType_Normal;
        }
        PushTextureQueue(node);
    }
    else if (extension == Str8Lit("vs"))
    {
    }
    else if (extension == Str8Lit("fs"))
    {
    }
    else
    {
        Assert(!"Asset type not supported");
    }
    return 0;
}
