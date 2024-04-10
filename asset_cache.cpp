// NOTE: an asset is anything that is loaded from disk to be used in game.

// TODO: Features of asset system:
// - Hashes the asset filename, stores the data
// - If two requests to load an asset are issued, don't load them twice
// - Hot load assets
// - Later: if there isn't enough space, evict the least used
//
// - Be able to asynchronously load multiple textures at the same time (using probably a list of PBOs).
// - LRU for eviction
#include "crack.h"
#ifdef LSP_INCLUDE
#include "keepmovingforward_common.h"
#include "render/render.h"
#include "asset.h"
#include "asset_cache.h"
#include "./offline/asset_processing.h"
#include "font.h"
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "third_party/stb_image.h"

//////////////////////////////
// Globals
//
global AS_CacheState *as_state = 0;

// const i32 invalidIndex = -1;

internal void AS_Init()
{
    Arena *arena    = ArenaAlloc(megabytes(512));
    as_state        = PushStruct(arena, AS_CacheState);
    as_state->arena = arena;

    // as_state->numSlots   = 1024;
    // as_state->assetSlots = PushArray(arena, AS_Slot, as_state->numSlots);

    as_state->assetCapacity = 1024;
    as_state->assets        = PushArray(arena, AS_Asset *, as_state->assetCapacity);
    // Reserve null slot
    as_state->assets[0] = PushStruct(arena, AS_Asset);
    as_state->assetCount++;
    as_state->assetEndOfList++;
    as_state->freeAssetList = PushArray(arena, i32, as_state->assetCapacity);

    // Hash table
    {
        as_state->fileHash.hashCount = 1024;
        as_state->fileHash.hash      = PushArray(arena, i32, as_state->fileHash.hashCount);
        // Sets array to be full of -1s
        MemorySet(as_state->fileHash.hash, 0xff,
                  sizeof(as_state->fileHash.hash[0]) * as_state->fileHash.hashCount);
        as_state->fileHash.hashMask = as_state->fileHash.hashCount - 1;

        as_state->fileHash.indexCount = 1024;
        as_state->fileHash.indexChain = PushArray(arena, i32, as_state->fileHash.indexCount);
        MemorySet(as_state->fileHash.indexChain, 0xff,
                  sizeof(as_state->fileHash.indexChain[0]) * as_state->fileHash.indexCount);

        // Stripes
#if 0
        as_state->numStripes = 64;
        as_state->stripes    = PushArray(arena, AS_Stripe, as_state->numStripes);
#endif
    }

    as_state->ringBufferSize = kilobytes(64);
    as_state->ringBuffer     = PushArray(arena, u8, as_state->ringBufferSize);

    as_state->threadCount    = 1; // Min(1, OS_NumProcessors() - 1);
    as_state->readSemaphore  = OS_CreateSemaphore(as_state->threadCount);
    as_state->writeSemaphore = OS_CreateSemaphore(as_state->threadCount);

    as_state->threads = PushArray(arena, AS_Thread, as_state->threadCount);
    for (u64 i = 0; i < as_state->threadCount; i++)
    {
        as_state->threads[i].handle = OS_ThreadStart(AS_EntryPoint, (void *)i);
        as_state->threads[i].arena  = ArenaAlloc();
    }
#if 0
    as_state->hotloadThread = OS_ThreadStart(AS_HotloadEntryPoint, 0);
#endif

#if 0
    // Block allocator
    // 64 2 megabyte blocks
    // SEPARATE linked list of headers that point to spot in memory,

    as_state->numBlocks           = 128;
    as_state->blockSize           = megabytes(1);
    as_state->blockBackingBuffer  = PushArray(arena, u8, as_state->numBlocks * as_state->blockSize);
    as_state->memoryHeaderNodes   = PushArray(arena, AS_MemoryHeaderNode, as_state->numBlocks);
    AS_MemoryHeaderNode *sentinel = &as_state->freeBlockSentinel;
    sentinel->next                = sentinel;
    sentinel->prev                = sentinel;
    for (u32 i = 0; i < as_state->numBlocks; i++)
    {
        AS_MemoryHeaderNode *headerNode = &as_state->memoryHeaderNodes[i];
        AS_MemoryHeader *header         = &headerNode->header;
        header->buffer                  = as_state->blockBackingBuffer + as_state->blockSize * i;

        headerNode->next       = sentinel;
        headerNode->prev       = sentinel->prev;
        headerNode->next->prev = headerNode;
        headerNode->prev->next = headerNode;
    }
#endif
    AS_InitializeAllocator();

    // Asset tag trees
    as_state->tagMap.maxSlots = AS_TagKey_Count;
    as_state->tagMap.slots    = PushArray(arena, AS_TagSlot, as_state->tagMap.maxSlots);
    for (i32 i = 0; i < AS_TagKey_Count; i++)
    {
        as_state->tagMap.slots[i].maxCount = 256;
        as_state->tagMap.slots[i].nodes    = PushArray(arena, AS_TagNode, as_state->tagMap.slots[i].maxCount);
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

// TODO: timeout if it takes too long for a file to be loaded?
internal b32 AS_EnqueueFile(string path)
{
    b32 sent      = 0;
    u64 writeSize = sizeof(path.size) + path.size;
    Assert(writeSize < as_state->ringBufferSize);
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
            as_state->readPos       = AlignPow2(as_state->readPos, 8);
            result.str[result.size] = 0;
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
// Asset Thread Entry Points
//

// TODO: this being multithreaded feels a bit awkward. maybe look at this later to figure if it can be
// single-threaded, or if stuff can be timed so that it doesn't overlap somehow?
internal void AS_EntryPoint(void *p)
{
    SetThreadName(Str8Lit("[AS] Scanner"));
    for (;;)
    {
        TempArena scratch = ScratchStart(0, 0);
        string path       = AS_DequeueFile(scratch.arena);

        OS_Handle handle             = OS_OpenFile(OS_AccessFlag_Read | OS_AccessFlag_ShareRead, path);
        OS_FileAttributes attributes = OS_AttributesFromFile(handle);
        // If the file doesn't exist, abort
        if (attributes.lastModified == 0 && attributes.size == 0)
        {
            continue;
        }

        // u64 hash = HashFromString(path);
        // as_state->fileHash.

        AS_Asset *asset = 0;

        i32 hash = HashFromString(path);
        Assert(((as_state->assetCapacity) & (as_state->assetCapacity - 1)) == 0);

        // The asset should've been added to the cache already.
        for (i32 i = AS_FirstInHash(hash); i != -1; i = AS_NextInHash(i))
        {
            AS_Asset *nextAsset = as_state->assets[i];
            if (nextAsset->path == path)
            {
                asset = nextAsset;
                break;
            }
        }

        if (asset == 0)
        {
            Assert(!"Space for asset not made");
        }

        // Freed/new assets have a "lastModified" timestamp of 0. Assets to be hotloaded have a timestamp != 0.
        if (asset->lastModified != 0)
        {
            // If the model is reloaded while it's still in the process of loading, cancel the request. (the
            // hotload)
            if (AtomicCompareExchangeU32(&asset->status, AS_Status_Unloaded, AS_Status_Loaded) != AS_Status_Loaded)
            {
                continue;
            }

            BeginTicketMutex(&as_state->allocator.ticketMutex);
            AS_Free(asset);
            Printf("Asset freed");
            EndTicketMutex(&as_state->allocator.ticketMutex);
        }
        asset->lastModified = attributes.lastModified;

#if 0
        AS_Slot *slot = &as_state->assetSlots[(hash & (as_state->numSlots - 1))];
        AS_Node *n        = 0;
        // Find node with matching filename
        BeginRMutex(&slot->mutex);
        for (AS_Node *node = slot->first; node != 0; node = node->next)
        {
            if (node->asset.hash == hash)
            {
                n = node;
                break;
            }
        }
        // EndRMutex(&slot->mutex);

        // If node doesn't exist, add it to the hash table
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
                n->asset.path = PushStr8Copy(as_state->arena, path);
            }

            BeginWMutex(&slot->mutex);
            QueuePush(slot->first, slot->last, n);
            EndWMutex(&slot->mutex);

            n->asset.hash = hash;
        }
        // If node does exist, then it needs to be hot reloaded.
        else
        {
            // Don't hot load if the texture is loading.
            if (AtomicCompareExchangeU32(&n->asset.status, AS_Status_Unloaded, AS_Status_Loaded) !=
                AS_Status_Loaded)
            {
                continue;
            }
            // Puts on free list
            FreeBlocks(n);
        }

        // Update the node
        n->asset.lastModified = attributes.lastModified;
#endif

        u64 readCursor = 0;
#if 0
        struct AssetFileHeader
        {
            AssetFileSectionHeader headers[4];
        };
        AssetFileHeader header;

        // TODO: do I have to manually keep track of the offset?
        readCursor += OS_ReadFile(handle, &header, readCursor, sizeof(header));

        // TODO: maybe it would just be better to have a temporary buffer hold the contents of the entire file
        // instead of issuing multiple os read calls, but I feel like worrying about this is a waste of time.
        for (i32 i = 0; i < ArrayLength(header.headers); i++)
        {
            AssetFileSectionHeader *sectionHeader = header.headers + i;
            if (sectionHeader->size > 0)
            {
                string tag = Str8((u8 *)sectionHeader->tag, 4);
                // temporary memory
                if (tag == Str8Lit("GPU ") || tag == Str8Lit("TEMP") || tag == Str8Lit("DBG"))
                {
                    // TODO: temporary block allocator or something? or allocating a piece of memory from the
                    // renderer (i.e. the renderer owns the memory instead of the asset system?)

                    u8 *memory = (u8 *)malloc(sectionHeader->size);
                    Assert(sectionHeader->offset == readCursor);
                    readCursor += OS_ReadFile(handle, memory, readCursor, sectionHeader->size);
                }
                else if (tag == Str8Lit("MAIN"))
                {
                    // TODO: this unfortunately has to be locked.
                    BeginTicketMutex(&as_state->allocator.ticketMutex);
                    asset->memoryBlock = AS_Alloc((i32)attributes.size);
                    EndTicketMutex(&as_state->allocator.ticketMutex);

                    asset->size =
                        OS_ReadFile(handle, AS_GetMemory(asset->memoryBlock), readCursor, sectionHeader->size);
                    Assert(asset->size == attributes.size);
                    readCursor += asset->size;
                }
                else
                {
                    Assert(!"Invalid tag");
                }
            }
#endif

        BeginTicketMutex(&as_state->allocator.ticketMutex);
        asset->memoryBlock = AS_Alloc((i32)attributes.size);
        EndTicketMutex(&as_state->allocator.ticketMutex);

        asset->size = OS_ReadEntireFile(handle, AS_GetMemory(asset));
        OS_CloseFile(handle);

        // TODO IMPORTANT: virtual protect the memory so it can only be read

        // Process the raw asset data
        JS_Kick(AS_LoadAsset, asset, 0, Priority_Low);
        ScratchEnd(scratch);
    }
}

// TODO BUG: every once in a while hot loading just fails? I'm pretty sure this is an issue with
// how I'm checking the filetimes. also for some reason every time the file is modified, the
// write time is modified twice sometimes
internal void AS_HotloadEntryPoint(void *p)
{
    SetThreadName(Str8Lit("[AS] Hotload"));

    for (;;)
    {
        for (i32 i = 0; i < as_state->assetEndOfList; i++)
        {
            AS_Asset *asset = as_state->assets[i];
            if (asset->lastModified)
            {
                // If the asset was modified, its write time changes. Need to hotload.
                OS_FileAttributes attributes = OS_AttributesFromPath(asset->path);
                u64 lastModified             = asset->lastModified;
                if (attributes.lastModified != 0 && attributes.lastModified != lastModified)
                {
                    // Printf("Old last modified: %u\nNew last modified: %u\n\n", node->lastModified,
                    //        attributes.lastModified);
                    AtomicCompareExchangeU64(&asset->lastModified, attributes.lastModified, lastModified);

                    AS_EnqueueFile(asset->path);
                }
            }
        }
        OS_Sleep(100);
    }
}

//////////////////////////////
// Specific asset loading callbacks
//
// TODO: create a pack file so you don't have to check the file extension
// assets should either be loaded directly into main memory, or into a temp storage
JOB_CALLBACK(AS_LoadAsset)
{
    AS_Asset *asset = (AS_Asset *)data;
    if (AtomicCompareExchangeU32(&asset->status, AS_Status_Queued, AS_Status_Unloaded) != AS_Status_Unloaded)
    {
        return 0;
    }
    string extension = GetFileExtension(asset->path);
    if (extension == Str8Lit("model"))
    {
        string directory = Str8PathChopPastLastSlash(asset->path);

        LoadedModel *model = &asset->model;
        asset->type        = AS_Model;

        Tokenizer tokenizer;
        tokenizer.input.str  = AS_GetMemory(asset);
        tokenizer.input.size = asset->size;
        tokenizer.cursor     = tokenizer.input.str;

        GetPointerValue(&tokenizer, &model->vertexCount);
        GetPointerValue(&tokenizer, &model->indexCount);

        // TODO: if the data is freed, this instantly goes bye bye. use a handle.
        // what I'm thinking is that the memory itself is wrapped in a structure that is pointed to by a
        // handle, instead of just pointing to the asset node which contains information you don't really need?
        model->vertices = GetTokenCursor(&tokenizer, MeshVertex);
        Advance(&tokenizer, sizeof(model->vertices[0]) * model->vertexCount);
        model->indices = GetTokenCursor(&tokenizer, u32);
        Advance(&tokenizer, sizeof(model->indices[0]) * model->indexCount);

        // Materials
        GetPointerValue(&tokenizer, &model->materialCount);
        // Printf("material count: %u\n", model->materialCount);
        model->materials = PushArray(arena, Material, model->materialCount);
        for (u32 i = 0; i < model->materialCount; i++)
        {
            Material *material = model->materials + i;
            GetPointerValue(&tokenizer, &material->startIndex);
            GetPointerValue(&tokenizer, &material->onePlusEndIndex);
            // Printf("material start index: %u\n", material->startIndex);
            // Printf("material end index: %u\n", material->onePlusEndIndex);
            for (u32 j = 0; j < TextureType_Count; j++)
            {
                char marker[6];
                Get(&tokenizer, &marker, 6);
                // Printf("Marker: %s\n", marker);

                string path;
                path.str = GetPointer(&tokenizer, u8);
                // Printf("Offset: %u\n", path.str);
                if (path.str != tokenizer.input.str)
                {
                    GetPointerValue(&tokenizer, &path.size);
                    Advance(&tokenizer, (u32)path.size);
                    // Printf("Size: %u\n", path.size);
                    model->materials[i].textureHandles[j] = AS_GetAsset(path);
                    // Printf("Texture Type: %u, File: %S\n", j, path);
                }
                else
                {
                    // Printf("Texture Type: %u Not found\n", j);
                }
                // Printf("\n");
            }
        }

        // Skeleton
        {
            string path;
            path.str = GetPointer(&tokenizer, u8);
            GetPointerValue(&tokenizer, &path.size);
            Advance(&tokenizer, (u32)path.size);

            // Printf("Skeleton file name: %S\n", path);
            model->skeleton = AS_GetAsset(path);
        }

        Assert(EndOfBuffer(&tokenizer));

        // TODO: these should be part of megabuffers
        model->vertexBuffer = R_AllocateBuffer(R_BufferType_Vertex, model->vertices,
                                               sizeof(model->vertices[0]) * model->vertexCount);
        model->indexBuffer =
            R_AllocateBuffer(R_BufferType_Index, model->indices, sizeof(model->indices[0]) * model->indexCount);

        WriteBarrier();
        asset->status = AS_Status_Loaded;
    }
    else if (extension == Str8Lit("anim"))
    {
        asset->type = AS_Anim;
        u8 *buffer  = AS_GetMemory(asset);

        // NOTE: crazy town incoming
        KeyframedAnimation **animation = &asset->anim;
        *animation                     = (KeyframedAnimation *)buffer;
        KeyframedAnimation *a          = asset->anim;

        // CompressedKeyframedAnimation *a = (CompressedKeyframedAnimation *)buffer;
        Printf("Num nodes: %u\n", a->numNodes);
        // Printf("offset: %u\n", (u64)(a->boneChannels));
        a->boneChannels = (BoneChannel *)(buffer + (u64)(a->boneChannels));
        for (u32 i = 0; i < a->numNodes; i++)
        {
            BoneChannel *boneChannel = a->boneChannels + i;
            ConvertOffsetToPointer(buffer, &boneChannel->name.str, u8);
            ConvertOffsetToPointer(buffer, &boneChannel->positions, AnimationPosition);
            ConvertOffsetToPointer(buffer, &boneChannel->scales, AnimationScale);
            ConvertOffsetToPointer(buffer, &boneChannel->rotations, AnimationRotation);
        }
        WriteBarrier();
        asset->status = AS_Status_Loaded;
    }
    else if (extension == Str8Lit("skel"))
    {
        LoadedSkeleton skeleton;
        Tokenizer tokenizer;
        tokenizer.input.str  = AS_GetMemory(asset);
        tokenizer.input.size = asset->size;
        tokenizer.cursor     = tokenizer.input.str;

        u32 version;
        u32 count;
        GetPointerValue(&tokenizer, &version);
        GetPointerValue(&tokenizer, &count);
        skeleton.count = count;

        if (version == 1)
        {
            // NOTE: How this works for future me:
            // When written, pointers are converted to offsets in file. Offset + base file address is the new
            // pointer location. For now, I am storing the string data right after the offset and size, but
            // this could theoretically be moved elsewhere.
            // TODO: get rid of these types of allocations
            skeleton.names = PushArray(arena, string, skeleton.count);
            for (u32 i = 0; i < count; i++)
            {
                string *boneName = &skeleton.names[i];
                boneName->str    = GetPointer(&tokenizer, u8);
                GetPointerValue(&tokenizer, &boneName->size);
                Advance(&tokenizer, (u32)boneName->size);
            }
            skeleton.parents = GetTokenCursor(&tokenizer, i32);
            Advance(&tokenizer, sizeof(skeleton.parents[0]) * count);
            skeleton.inverseBindPoses = GetTokenCursor(&tokenizer, Mat4);
            Advance(&tokenizer, sizeof(skeleton.inverseBindPoses[0]) * count);
            skeleton.transformsToParent = GetTokenCursor(&tokenizer, Mat4);
            Advance(&tokenizer, sizeof(skeleton.transformsToParent[0]) * count);

            Assert(EndOfBuffer(&tokenizer));
        }
        asset->type     = AS_Skeleton;
        asset->skeleton = skeleton;
        WriteBarrier();
        asset->status = AS_Status_Loaded;
    }
    else if (extension == Str8Lit("png"))
    {
        asset->type = AS_Texture;
        if (FindSubstring(asset->path, Str8Lit("diffuse"), 0, MatchFlag_CaseInsensitive) != asset->path.size)
        {
            asset->texture.type = TextureType_Diffuse;
        }
        else if (FindSubstring(asset->path, Str8Lit("normal"), 0, MatchFlag_CaseInsensitive) != asset->path.size)
        {
            asset->texture.type = TextureType_Normal;
        }
        R_TexFormat format = R_TexFormat_RGBA8;
        switch (asset->texture.type)
        {
            case TextureType_Diffuse: format = R_TexFormat_SRGB; break;
            case TextureType_Normal:
            default: format = R_TexFormat_RGBA8; break;
        }
        i32 width, height, nComponents;
        void *texData =
            stbi_load_from_memory(AS_GetMemory(asset), (i32)asset->size, &width, &height, &nComponents, 4);
        asset->texture.width  = width;
        asset->texture.height = height;

        asset->texture.handle = R_AllocateTexture(texData, width, height, format);
        asset->status         = AS_Status_Loaded;
    }
    else if (extension == Str8Lit("ttf"))
    {
        asset->type          = AS_Font;
        u8 *buffer           = AS_GetMemory(asset);
        asset->font.fontData = F_InitializeFont(buffer);
        asset->status        = AS_Status_Loaded;
        // FreeBlocks(node);
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

// #if 0
// internal AS_Handle AS_HandleFromAsset(AS_Node *node)
// {
//     AS_Handle result = {};
//     result.u64[0]    = (u64)(node);
//     result.u64[1]    = asset->generation;
//     return result;
// }
//
// internal AS_Node *AS_AssetFromHandle(AS_Handle handle)
// {
//     AS_Node *node = (AS_Node *)(handle.u64[0]);
//     if (node == 0 || asset->generation != handle.u64[1])
//     {
//         node = &as_nodeNil;
//     }
//     return node;
// }
// #endif

internal void AS_UnloadAsset(AS_Asset *asset)
{
    switch (asset->type)
    {
        case AS_Null: break;
        case AS_Mesh: break;
        case AS_Texture:
        {
            // R_DeleteTexture2D(node->texture.handle);
            break;
        }
        case AS_Skeleton: break;
        case AS_Model: break;
        case AS_Shader: break;
        default: Assert(!"Invalid asset type");
    }
}

//////////////////////////////
// Handles
//
// #if 0
// internal AS_Handle AS_GetAssetByTag(string tag)
// {
//     u64 hash = HashFromString(tag);
// }
// #endif

internal AS_Asset *AS_GetAssetFromHandle(AS_Handle handle)
{
    Assert(sizeof(handle) <= 16);

    AS_Asset *result = 0;
    i32 index        = handle.i32[0];

    result = as_state->assets[index];
    if (result->generation != handle.i32[1] || result->status != AS_Status_Loaded)
    {
        result = 0;
    }
    return result;
}

// NOTE: I refuse to synchronize this.
internal AS_Asset *AS_AllocAsset(const string inPath)
{
    BeginFakeLock(&as_state->fakeLock);

    AS_Asset *asset = 0;
    i32 hash        = HashFromString(inPath);

    as_state->assetCount++;
    // If there is an asset on the free list, use that

    if (as_state->freeAssetCount != 0)
    {
        asset = as_state->assets[as_state->freeAssetList[--as_state->freeAssetCount]];
        asset->generation += 1;
        Assert(asset->memoryBlock == 0);
    }
    else
    {
        i32 assetId               = as_state->assetEndOfList++;
        as_state->assets[assetId] = PushStruct(as_state->arena, AS_Asset);
        // NOTE: all strings will have a fixed backing buffer of 256 bytes.
        asset           = as_state->assets[assetId];
        asset->path.str = PushArray(as_state->arena, u8, MAX_OS_PATH);
        StringCopy(&asset->path, inPath);
        asset->id = assetId;
    }
    AS_AddInHash(hash, asset->id);

    EndFakeLock(&as_state->fakeLock);

    AS_EnqueueFile(inPath);

    return asset;
}

internal void AS_FreeAsset(AS_Handle handle)
{
    BeginFakeLock(&as_state->fakeLock);
    AS_Asset *asset = AS_GetAssetFromHandle(handle);
    if (asset)
    {
        as_state->assetCount--;
        AS_Free(asset);
        asset->lastModified = 0;
        as_state->assetCount--;

        i32 id = as_state->freeAssetCount++;
        Assert(id >= 0);
        as_state->freeAssetList[id] = asset->id;
    }
    EndFakeLock(&as_state->fakeLock);
}

internal AS_Handle AS_GetAsset_(const string inPath, const b32 inLoadIfNotFound = 1)
{
    AS_Handle result = {};
    result.i32[0]    = -1;

    i32 hash = HashFromString(inPath);
    for (i32 i = AS_FirstInHash(hash); i != -1; i = AS_NextInHash(i))
    {
        if (as_state->assets[i]->path == inPath)
        {
            result.i32[0] = i;
            result.i32[1] = as_state->assets[i]->generation;
            break;
        }
    }

    // Unloaded
    if (result.i32[0] == -1 && inLoadIfNotFound)
    {
        // TODO: growth strategy?
        Assert(as_state->assetCount < as_state->assetCapacity);
        AS_Asset *asset = AS_AllocAsset(inPath);
        result.i32[0]   = asset->id;
        result.i32[1]   = asset->generation;
    }
    return result;
}

internal AS_Handle AS_GetAsset(const string inPath)
{
    return AS_GetAsset_(inPath, true);
}

internal Font *GetFont(AS_Handle handle)
{
    AS_Asset *asset = AS_GetAssetFromHandle(handle);
    Font *result    = &fontNil;
    if (asset)
    {
        Assert(asset->type == AS_Font);
        result = &asset->font;
    }
    return result;
}

internal LoadedSkeleton *GetSkeleton(AS_Handle handle)
{
    AS_Asset *asset        = AS_GetAssetFromHandle(handle);
    LoadedSkeleton *result = &skeletonNil;
    if (asset)
    {
        Assert(asset->type == AS_Skeleton);
        result = &asset->skeleton;
    }
    return result;
}

internal Texture *GetTexture(AS_Handle handle)
{
    AS_Asset *asset = AS_GetAssetFromHandle(handle);
    Texture *result = &textureNil;
    if (asset)
    {
        Assert(asset->type == AS_Texture);
        result = &asset->texture;
    }
    return result;
}

internal LoadedModel *GetModel(AS_Handle handle)
{
    AS_Asset *asset     = AS_GetAssetFromHandle(handle);
    LoadedModel *result = &modelNil;
    if (asset)
    {
        Assert(asset->type == AS_Model);
        result = &asset->model;
    }
    return result;
}
// TODO: calling GetSkeleton() on an nil model should return a nil skeleton
internal LoadedSkeleton *GetSkeletonFromModel(AS_Handle handle)
{
    LoadedModel *model     = GetModel(handle);
    LoadedSkeleton *result = model == &modelNil ? &skeletonNil : GetSkeleton(model->skeleton);
    return result;
}
internal KeyframedAnimation *GetAnim(AS_Handle handle)
{
    AS_Asset *asset            = AS_GetAssetFromHandle(handle);
    KeyframedAnimation *result = &animNil;
    if (asset)
    {
        Assert(asset->type == AS_Anim);
        result = result = asset->anim;
    }
    return result;
}

internal R_Handle GetTextureRenderHandle(AS_Handle input)
{
    Texture *texture = GetTexture(input);
    R_Handle handle  = texture->handle;
    return handle;
}

// TODO: similar function that tries to get it from the hash first before loading it smiley face :)
inline b32 IsModelHandleNil(AS_Handle handle)
{
    LoadedModel *model = GetModel(handle);
    b32 result         = (model == 0 || model == &modelNil);
    return result;
}
inline b8 IsAnimNil(KeyframedAnimation *anim)
{
    b8 result = (anim == 0 || anim == &animNil);
    return result;
}

//////////////////////////////
// Memory Helpers
//

// Removes the first element from free list
// #if 0
// internal AS_MemoryHeaderNode *AllocateBlock()
// {
//     AS_MemoryHeaderNode *sentinel   = &as_state->freeBlockSentinel;
//     AS_MemoryHeaderNode *headerNode = sentinel->next;
//     headerNode->next->prev          = headerNode->prev;
//     headerNode->prev->next          = headerNode->next;
//     headerNode->next = headerNode->prev = 0;
//     return headerNode;
// }
// // Removes the input element from the free list (for >2MB block allocation)
// internal AS_MemoryHeaderNode *AllocateBlock(AS_MemoryHeaderNode *headerNode)
// {
//     headerNode->next->prev = headerNode->prev;
//     headerNode->prev->next = headerNode->next;
//     headerNode->next = headerNode->prev = 0;
//     return headerNode;
// }
// // Adds to free list
// internal void FreeBlock(AS_MemoryHeaderNode *headerNode)
// {
//     AS_MemoryHeaderNode *sentinel = &as_state->freeBlockSentinel;
//     headerNode->next              = sentinel;
//     headerNode->prev              = sentinel->prev;
//     headerNode->next->prev        = headerNode;
//     headerNode->prev->next        = headerNode;
// }
//
// internal void FreeBlocks(AS_Asset *asset)
// {
//     TicketMutexScope(&as_state->mutex)
//     {
//         i32 numBlocks = (i32)((asset->size / as_state->blockSize) + (asset->size % as_state->blockSize !=
//         0)); AS_MemoryHeaderNode *node = asset->memoryBlock;
//
//         Assert(node + numBlocks <= as_state->memoryHeaderNodes + as_state->numBlocks);
//         for (i32 i = 0; i < numBlocks; i++)
//         {
//             FreeBlock(node + i);
//         }
//         asset->memoryBlock = 0;
//     }
// }
//
// internal AS_MemoryHeaderNode *AllocateBlocks(u64 size)
// {
//     AS_MemoryHeaderNode *result = 0;
//     TicketMutexScope(&as_state->mutex)
//     {
//         AS_MemoryHeaderNode *sentinel = &as_state->freeBlockSentinel;
//         Assert(sentinel->next != 0);
//         // Assert(attributes.size <= 2 * as_state->blockSize);
//         // If block is small enough, just get off free list
//         if (size <= as_state->blockSize)
//         {
//             AS_MemoryHeaderNode *block = sentinel->next;
//             result                     = AllocateBlock();
//         }
//         // Otherwise
//         else
//         {
//             u32 s = SafeTruncateU64(size);
//             // Find a set of consecutive blocks that fits the needed size
//             i32 numBlocks  = (i32)((s / as_state->blockSize) + (s % as_state->blockSize != 0));
//             i32 startIndex = -1;
//             i32 count      = 0;
//             for (i32 i = 0; i < (i32)as_state->numBlocks - (i32)numBlocks; i++)
//             {
//                 AS_MemoryHeaderNode *n = &as_state->memoryHeaderNodes[i];
//                 b32 isFree             = (n->next != 0);
//                 b32 isChaining         = (startIndex != -1 && count != 0);
//                 // Start a chain of contiguous blocks
//                 if (isFree && !isChaining)
//                 {
//                     startIndex = i;
//                     count      = 1;
//                 }
//                 // If there aren't enough contiguous blocks, restart
//                 else if (!isFree && isChaining)
//                 {
//                     startIndex = -1;
//                     count      = 0;
//                 }
//                 // If block is free and part of an ongoing chain, increment and continue
//                 else if (isFree && isChaining)
//                 {
//                     count += 1;
//                 }
//                 if (count == numBlocks)
//                 {
//                     Assert(as_state->memoryHeaderNodes[startIndex].next != 0);
//                     Assert(&as_state->memoryHeaderNodes[startIndex] + count <=
//                            as_state->memoryHeaderNodes + as_state->numBlocks);
//                     for (i32 j = startIndex; j < startIndex + count; j++)
//                     {
//                         AllocateBlock(&as_state->memoryHeaderNodes[j]);
//                     }
//                     result = &as_state->memoryHeaderNodes[startIndex];
//                     break;
//                 }
//             }
//         }
//     }
//     return result;
// }
//
// inline u8 *GetAssetBuffer(AS_Asset *asset)
// {
//     u8 *result = 0;
//     if (asset->memoryBlock)
//     {
//         result = asset->memoryBlock->header.buffer;
//     }
//     return result;
// }
//
// inline void EndTemporaryMemory(AS_Asset *asset)
// {
//     FreeBlocks(asset);
// }
// #endif

//////////////////////////////
// Asset tags
//
// #if 0
// internal void AS_AddAssetTag(AS_TagKey tagKey, f32 tagValue, AS_Handle handle)
// {
//     AS_TagSlot *slot   = as_state->tagMap.slots + tagKey;
//     AS_TagNode *parent = 0;
//     // Root nodes
//     u32 nodeIndex    = 0;
//     AS_TagNode *node = slot->nodes + nodeIndex;
//     for (;;)
//     {
//         i32 index = -1;
//         for (i32 i = 0; i < node->count; i++)
//         {
//             AS_TagKeyValuePair *pair = node->pairs + i;
//             if (tagValue <= pair->value)
//             {
//                 index = i;
//                 break;
//             }
//         }
//
//         index = index == -1 ? node->count : index;
//         // Assumption: node has no children until "nodes" is filled. then children are created
//         if (node->count < ArrayLength(node->pairs))
//         {
//             // Shift all elements up by 1
//             if (index != node->count)
//             {
//                 MemoryCopy(node->pairs + index + 1, node->pairs + index, node->count - index);
//             }
//             node->pairs[node->count++] = {handle, tagValue};
//             break;
//         }
//         // Find new child to iterate to, adding node if necesary
//         else
//         {
//             nodeIndex = node->children[index];
//             if (nodeIndex == 0)
//             {
//                 Assert(slot->count < slot->maxCount);
//                 node->children[index] = slot->count++;
//                 nodeIndex             = node->children[index];
//             }
//             parent = node;
//             node   = slot->nodes + nodeIndex;
//         }
//     }
// }
//
// internal AS_Handle AS_GetAssetByTag(AS_TagKey tagKey, f32 tagValue)
// {
//     AS_TagSlot *slot = as_state->tagMap.slots + tagKey;
//     AS_TagNode *node = slot->nodes + 0;
//
//     f32 bestMatchValue        = FLT_MAX;
//     AS_Handle bestMatchHandle = {};
//
//     for (;;)
//     {
//         i32 nodeIndex = -1;
//         for (i32 i = 0; i < node->count; i++)
//         {
//             AS_TagKeyValuePair *pair = node->pairs + i;
//             if (tagValue == pair->value)
//             {
//                 return pair->assetHandle;
//             }
//             if (Abs(pair->value - tagValue) < bestMatchValue)
//             {
//                 bestMatchValue  = pair->value;
//                 bestMatchHandle = pair->assetHandle;
//             }
//             if (tagValue < pair->value)
//             {
//                 nodeIndex = node->children[i];
//                 break;
//             }
//         }
//         nodeIndex = -1 ? node->children[ArrayLength(node->children) - 1] : nodeIndex;
//         node      = slot->nodes + nodeIndex;
//     }
//
//     return bestMatchHandle;
// }
// #endif

//////////////////////////////
// Iterate hash
//
internal i32 AS_FirstInHash(i32 hash)
{
    i32 result = as_state->fileHash.hash[hash & as_state->fileHash.hashMask];
    return result;
}

internal i32 AS_NextInHash(i32 index)
{
    i32 result = as_state->fileHash.indexChain[index];
    return result;
}

internal i32 AS_FirstInHash(string path)
{
    i32 hash = HashFromString(path);
    return AS_FirstInHash(hash);
}

internal void AS_AddInHash(i32 key, i32 index)
{
    i32 hash                             = key & as_state->fileHash.hashMask;
    as_state->fileHash.indexChain[index] = as_state->fileHash.hash[hash];
    as_state->fileHash.hash[hash]        = index;
}

internal void AS_RemoveFromHash(i32 key, i32 index)
{
    i32 hash = key & as_state->fileHash.hashMask;
    if (as_state->fileHash.hash[hash] == index)
    {
        as_state->fileHash.hash[hash] = -1;
    }
    else
    {
        for (i32 i = as_state->fileHash.hash[hash]; i != -1; i = as_state->fileHash.indexChain[i])
        {
            if (as_state->fileHash.indexChain[i] == index)
            {
                as_state->fileHash.indexChain[i] = as_state->fileHash.indexChain[index];
            }
        }
    }
}

//////////////////////////////
// B-tree memory allocation
//

// How this allocator works:
//
// Allocation
//
// 1. Every time an asset is allocated, a size is passed in and a buffer is returned. A b-tree is used to
// keep track of previous allocations. The smallest memory block with an allocation size greater than the
// requested memory size is retrieved. If such a memory block exists, then it is used for the next steps.
// Otherwise, a new chunk of memory (w/ a size defined at startup) is used. This new chunk of memory is a "base
// block."
//
// 2. Once a new block is obtained, the memory block is split if the remaining size in the memory block is
// greater than the block size. A new block is created from these leftovers, and added to the b-tree.
//
// Free
//
// 1. When freeing a block, the next and previous nodes in the allocation chain are tested. If they are part of
// the same chain (i.e they share a base block) and are freed, then they are combined with the block. This
// block is then added to the b-tree.

// heavily inspired by: https://github.com/id-Software/DOOM-3-BFG/blob/master/neo/idlib/containers/BTree.h
// and https://github.com/id-Software/DOOM-3-BFG/blob/1caba1979589971b5ed44e315d9ead30b278d8b4/neo/idlib/Heap.h
internal AS_BTreeNode *AS_AllocNode()
{
    AS_BTree *tree        = &as_state->allocator.bTree;
    AS_BTreeNode *newNode = tree->free;

    if (newNode == 0)
    {
        newNode = PushStruct(as_state->allocator.arena, AS_BTreeNode);
    }
    else
    {
        StackPop(tree->free);
        newNode->memoryBlock = 0;
        newNode->key         = 0;
        newNode->numChildren = 0;
        newNode->parent      = 0;
        newNode->next        = 0;
        newNode->prev        = 0;
        newNode->first       = 0;
        newNode->last        = 0;
    }
    return newNode;
}

internal void AS_FreeNode(AS_BTreeNode *node)
{
    StackPush(as_state->allocator.bTree.free, node);
}

// NOTE: All children of a node have a key <= to that of their parent. Only the leaves contain objects.
// This simplifies the control flow complexity.
internal AS_BTreeNode *AS_AddNode(AS_MemoryBlockNode *memNode)
{
    AS_BTree *tree        = &as_state->allocator.bTree;
    AS_BTreeNode *newNode = 0;

    // If the root is null, allocate
    if (tree->root == 0)
    {
        tree->root = AS_AllocNode();
    }
    AS_BTreeNode *root = tree->root;

    // If the root is full, add another level (shift level of all current nodes down by 1, so that
    // memory blocks are found only in leaf nodes)
    if (root->numChildren >= tree->maxChildren)
    {
        newNode              = AS_AllocNode();
        newNode->first       = root;
        newNode->last        = root;
        newNode->key         = root->key;
        newNode->numChildren = 1;
        root->parent         = newNode;
        AS_SplitNode(root);
        tree->root = newNode;
    }

    newNode              = AS_AllocNode();
    newNode->memoryBlock = memNode;
    i32 key              = memNode->size;
    newNode->key         = key;

    // How this works:
    // 1. the keys of the children of a node are <= to the key of their parent
    // Iterate down to leaf node

    AS_BTreeNode *child = 0;
    for (AS_BTreeNode *node = tree->root; node->first != 0; node = child)
    {
        if (node->key < key)
        {
            node->key = key;
        }
        for (child = node->first; child->next != 0; child = child->next)
        {
            if (child->key >= key)
            {
                break;
            }
        }
        if (child->memoryBlock)
        {
            if (child->key >= key)
            {
                // Add node before child
                newNode->prev = child->prev;
                newNode->next = child;
                if (newNode->prev == 0)
                {
                    node->first = newNode;
                }
                else
                {
                    newNode->prev->next = newNode;
                }
                newNode->next->prev = newNode;
            }
            else
            {
                // Add node after child
                newNode->prev = child;
                newNode->next = child->next;
                if (newNode->next == 0)
                {
                    node->last = newNode;
                }
                else
                {
                    newNode->next->prev = newNode;
                }
                newNode->prev->next = newNode;
            }
            newNode->parent = node;
            node->numChildren++;

            return newNode;
        }
        if (child->numChildren > tree->maxChildren)
        {
            AS_SplitNode(child);
            if (child->prev->key >= key)
            {
                child = child->prev;
            }
        }
    }

    Assert(root->numChildren == 0);
    newNode->parent = root;
    root->first     = newNode;
    root->last      = newNode;
    root->numChildren++;
    return newNode;
}

internal void AS_RemoveNode(AS_BTreeNode *node)
{
    Assert(node->memoryBlock != 0);
    // Unlink from parent
    if (node->prev)
    {
        node->prev->next = node->next;
    }
    else
    {
        node->parent->first = node->next;
    }

    if (node->next)
    {
        node->next->prev = node->prev;
    }
    else
    {
        node->parent->last = node->prev;
    }
    node->parent->numChildren--;

    AS_BTreeNode *parent = 0;
    AS_BTree *tree       = &as_state->allocator.bTree;
    for (parent = node->parent; parent != tree->root && parent->numChildren < tree->maxChildren;
         parent = parent->parent)
    {
        if (parent->next)
        {
            parent = AS_MergeNodes(parent, parent->next);
        }
        else if (parent->prev)
        {
            parent = AS_MergeNodes(parent->prev, parent);
        }
        if (parent->key > parent->last->key)
        {
            parent->key = parent->last->key;
        }
        if (parent->numChildren > tree->maxChildren)
        {
            // Splitting the node will make the parent's parent have at least 2 children
            AS_SplitNode(parent);
            break;
        }
    }

    for (; parent != 0 && parent->last != 0; parent = parent->parent)
    {
        if (parent->key > parent->last->key)
        {
            parent->key = parent->last->key;
        }
    }
    AS_FreeNode(node);
    if (tree->root->numChildren == 1 && tree->root->memoryBlock == 0)
    {
        AS_BTreeNode *oldRoot     = tree->root;
        tree->root->first->parent = 0;
        tree->root                = tree->root->first;
        AS_FreeNode(oldRoot);
    }
}

internal void AS_SplitNode(AS_BTreeNode *node)
{

    AS_BTreeNode *newNode = AS_AllocNode();
    newNode->parent       = node->parent;
    AS_BTreeNode *child   = node->first;

    Assert(node->parent->numChildren < as_state->allocator.bTree.maxChildren);

    for (i32 i = 0; i < node->numChildren; i += 2)
    {
        child->parent = newNode;
        child         = child->next;
    }

    // All nodes before the current child node are now children of the newnode.
    newNode->key         = child->prev->key;
    newNode->numChildren = (node->numChildren + 1) / 2;
    newNode->next        = node;
    newNode->prev        = node->prev;
    newNode->first       = node->first;
    newNode->last        = child->prev;

    child->prev->next = 0;
    child->prev       = 0;

    if (node->prev)
    {
        node->prev->next = newNode;
    }
    else
    {
        node->parent->first = newNode;
    }
    node->prev  = newNode;
    node->first = child;
    node->numChildren -= newNode->numChildren;
    Assert(node->numChildren <= as_state->allocator.bTree.maxChildren);

    node->parent->numChildren++;
}

internal AS_BTreeNode *AS_MergeNodes(AS_BTreeNode *node1, AS_BTreeNode *node2)
{
    Assert(node1->parent == node2->parent);
    Assert(node1->next == node2 && node2->prev == node1);
    Assert(node1->memoryBlock == 0 && node2->memoryBlock == 0);
    Assert(node1->numChildren <= (as_state->allocator.bTree.maxChildren + 1) / 2 &&
           node2->numChildren <= (as_state->allocator.bTree.maxChildren + 1) / 2);

    AS_BTreeNode *child = 0;
    for (child = node1->first; child->next != 0; child = child->next)
    {
        child->parent = node2;
    }

    child->parent     = node2;
    child->next       = node2->first;
    child->next->prev = child;
    node2->first      = node1->first;
    node2->prev       = node1->prev;
    node2->parent->numChildren--;
    node2->numChildren += node1->numChildren;

    if (node1->prev)
    {
        node1->prev->next = node2;
    }
    else
    {
        node1->parent->first = node2;
    }

    AS_FreeNode(node1);
    return node2;
}

internal AS_BTreeNode *AS_FindMemoryBlock(i32 size)
{
    AS_BTreeNode *root = as_state->allocator.bTree.root;

    AS_BTreeNode *result = 0;
    if (root != 0)
    {
        for (AS_BTreeNode *node = root->first; node != 0; node = node->first)
        {
            while (node->next != 0)
            {
                if (node->key >= size)
                {
                    break;
                }
                node = node->next;
            }
            if (node->memoryBlock)
            {
                if (node->key >= size)
                {
                    result = node;
                }
                break;
            }
        }
    }
    return result;
}

internal void AS_InitializeAllocator()
{
    Arena *arena                          = ArenaAlloc();
    as_state->allocator.arena             = arena;
    as_state->allocator.bTree.root        = PushStruct(arena, AS_BTreeNode);
    as_state->allocator.bTree.maxChildren = 4;
    as_state->allocator.baseBlockSize     = megabytes(2);
    as_state->allocator.minBlockSize      = kilobytes(4);
}

internal u8 *AS_GetMemory(AS_MemoryBlockNode *node)
{
    u8 *result = (u8 *)(node) + sizeof(AS_MemoryBlockNode);
    return result;
}

internal u8 *AS_GetMemory(AS_Asset *asset)
{
    return AS_GetMemory(asset->memoryBlock);
}

internal AS_MemoryBlockNode *AS_Alloc(i32 size)
{
    i32 alignedSize                     = AlignPow2(size, 16);
    AS_DynamicBlockAllocator *allocator = &as_state->allocator;
    allocator->usedBlocks++;
    allocator->usedBlockMemory += alignedSize;
    allocator->numAllocs++;
    u8 *result                      = 0;
    AS_BTreeNode *node              = AS_FindMemoryBlock(size);
    AS_MemoryBlockNode *memoryBlock = 0;
    // Use an existing memory block for the allocation. Split it to prevent excessive leaks.
    if (node)
    {
        memoryBlock = node->memoryBlock;
        AS_RemoveNode(node);
        memoryBlock->node = 0;
        allocator->freeBlocks--;
        allocator->freeBlockMemory -= memoryBlock->size;

        // not yet
        result = AS_GetMemory(node->memoryBlock);
    }
    // Create an entirely separate block.
    else
    {
        i32 newSize              = Max(allocator->baseBlockSize, alignedSize + sizeof(AS_MemoryBlockNode));
        memoryBlock              = (AS_MemoryBlockNode *)PushArray(allocator->arena, u8, newSize);
        memoryBlock->isBaseBlock = true;
        memoryBlock->size        = newSize - sizeof(AS_MemoryBlockNode);
        memoryBlock->prev        = allocator->last;
        Assert(memoryBlock->next == 0 && memoryBlock->node == 0);
        if (memoryBlock->prev)
        {
            memoryBlock->prev->next = memoryBlock;
        }
        else
        {
            allocator->first = memoryBlock;
        }
        allocator->last = memoryBlock;
        allocator->baseBlocks++;
        allocator->baseBlockMemory += newSize;
    }

    // Split the newly allocated block if it's larger than the allocation size
    if ((i32)(memoryBlock->size - alignedSize - sizeof(AS_MemoryBlockNode)) > allocator->minBlockSize)
    {
        allocator->numResizes++;
        AS_MemoryBlockNode *newBlock =
            (AS_MemoryBlockNode *)((u8 *)(memoryBlock) + memoryBlock->size + sizeof(AS_MemoryBlockNode));
        newBlock->isBaseBlock = 0;
        newBlock->size        = memoryBlock->size - alignedSize - sizeof(AS_MemoryBlockNode);
        Assert(newBlock->size > allocator->minBlockSize);
        newBlock->prev = memoryBlock;
        newBlock->next = memoryBlock->next;
        newBlock->node = 0;

        if (newBlock->next)
        {
            newBlock->next->prev = newBlock;
        }
        else
        {
            allocator->last = newBlock;
        }

        memoryBlock->size = alignedSize;
        memoryBlock->next = newBlock;

        allocator->freeBlocks++;
        allocator->freeBlockMemory += alignedSize;

        AS_Free(newBlock);
    }
    return memoryBlock;
}

internal void AS_Free(AS_MemoryBlockNode *memoryBlock)
{
    Assert(memoryBlock->node == 0);
    // Merge the next node if it's free and part of the same contiguous chain
    AS_DynamicBlockAllocator *allocator = &as_state->allocator;
    allocator->numFrees++;
    allocator->usedBlocks--;
    allocator->usedBlockMemory -= memoryBlock->size;

    AS_MemoryBlockNode *next = memoryBlock->next;
    if (next != 0 && !next->isBaseBlock && next->node != 0)
    {
        AS_RemoveNode(next->node);
        next->node = 0;
        allocator->freeBlocks--;
        allocator->freeBlockMemory -= next->size;

        memoryBlock->size = memoryBlock->size + sizeof(AS_MemoryBlockNode) + next->size;
        memoryBlock->next = next->next;
        if (next->next)
        {
            next->next->prev = memoryBlock;
        }
        else
        {
            allocator->last = next;
        }
    }

    // Merge the previous node if it's free and part of the same contiguous chain
    AS_MemoryBlockNode *prev = memoryBlock->prev;
    if (prev != 0 && !memoryBlock->isBaseBlock && prev->node != 0)
    {
        AS_RemoveNode(prev->node);
        prev->node = 0;
        allocator->freeBlocks--;
        allocator->freeBlockMemory -= next->size;

        prev->size = prev->size + sizeof(AS_MemoryBlockNode) + memoryBlock->size;
        prev->next = memoryBlock->next;
        if (memoryBlock->next)
        {
            memoryBlock->next->prev = prev;
        }
        else
        {
            allocator->last = prev;
        }
        // Add the three combined blocks to the free b-tree
        AS_AddNode(prev);
        allocator->freeBlocks++;
        allocator->freeBlockMemory += prev->size;
    }
    else
    {
        // Add the two combined blocks to the free b-tree
        AS_AddNode(memoryBlock);
        allocator->freeBlocks++;
        allocator->freeBlockMemory += memoryBlock->size;
    }
}

internal void AS_Free(AS_Asset *asset)
{
    AS_Free(asset->memoryBlock);
    asset->memoryBlock = 0;
}
