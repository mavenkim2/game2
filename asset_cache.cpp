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
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "third_party/stb_image.h"

//////////////////////////////
// Globals
//
global AS_CacheState *as_state = 0;
global readonly LoadedSkeleton skeletonNil;
global readonly LoadedModel modelNil;
global readonly Texture textureNil;
global readonly KeyframedAnimation animNil;

internal void AS_Init()
{
    Arena *arena    = ArenaAlloc(megabytes(512));
    as_state        = PushStruct(arena, AS_CacheState);
    as_state->arena = arena;

    as_state->numSlots   = 1024;
    as_state->assetSlots = PushArray(arena, AS_Slot, as_state->numSlots);

    as_state->ringBufferSize = kilobytes(64);
    as_state->ringBuffer     = PushArray(arena, u8, as_state->ringBufferSize);

    as_state->threadCount    = Min(2, OS_NumProcessors() - 1);
    as_state->readSemaphore  = OS_CreateSemaphore(as_state->threadCount);
    as_state->writeSemaphore = OS_CreateSemaphore(as_state->threadCount);

    as_state->threads = PushArray(arena, AS_Thread, as_state->threadCount);
    for (u64 i = 0; i < as_state->threadCount; i++)
    {
        as_state->threads[i].handle = OS_ThreadStart(AS_EntryPoint, (void *)i);
        as_state->threads[i].arena  = ArenaAlloc();
    }
    as_state->hotloadThread = OS_ThreadStart(AS_HotloadEntryPoint, 0);

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

        u64 hash      = HashFromString(path);
        AS_Slot *slot = &as_state->assetSlots[(hash & (as_state->numSlots - 1))];
        AS_Node *n    = 0;
        // Find node with matching filename
        BeginRMutex(&slot->mutex);
        for (AS_Node *node = slot->first; node != 0; node = node->next)
        {
            if (node->hash == hash)
            {
                n = node;
                break;
            }
        }
        EndRMutex(&slot->mutex);

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
                n->path = PushStr8Copy(as_state->arena, path);
            }

            BeginWMutex(&slot->mutex);
            QueuePush(slot->first, slot->last, n);
            EndWMutex(&slot->mutex);

            n->hash = hash;
        }
        // If node does exist, then it needs to be hot reloaded.
        else
        {
            // Don't hot load if the texture is loading.
            if (AtomicCompareExchangeU32(&n->status, AS_Status_Unloaded, AS_Status_Loaded) != AS_Status_Loaded)
            {
                continue;
            }
            // Puts on free list
            FreeBlocks(n);
        }

        // Update the node
        n->lastModified = attributes.lastModified;

        // Allocate memory
        n->memoryBlock = AllocateBlocks(attributes.size);

        n->size = OS_ReadEntireFile(handle, GetAssetBuffer(n));
        Assert(n->size == attributes.size);
        OS_CloseFile(handle);

        // TODO IMPORTANT: virtual protect the memory so it can only be read

        // Process the raw asset data
        JS_Kick(AS_LoadAsset, n, 0, Priority_Low);
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
        for (u32 i = 0; i < as_state->numSlots; i++)
        {
            AS_Slot *slot = as_state->assetSlots + i;
            MutexScope(&slot->mutex)
            {
                for (AS_Node *node = slot->first; node != 0; node = node->next)
                {
                    // If the asset was modified, its write time changes. Need to hotload.
                    OS_FileAttributes attributes = OS_AttributesFromPath(node->path);
                    if (attributes.lastModified != 0 && attributes.lastModified != node->lastModified)
                    {
                        // Printf("Old last modified: %u\nNew last modified: %u\n\n", node->lastModified,
                        //        attributes.lastModified);
                        node->lastModified = attributes.lastModified;
                        AS_EnqueueFile(node->path);
                    }
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
    AS_Node *node = (AS_Node *)data;
    if (AtomicCompareExchangeU32(&node->status, AS_Status_Queued, AS_Status_Unloaded) != AS_Status_Unloaded)
    {
        return 0;
    }
    string extension = GetFileExtension(node->path);
    if (extension == Str8Lit("model"))
    {
        string directory = Str8PathChopPastLastSlash(node->path);

        LoadedModel *model = &node->model;
        node->type         = AS_Model;

        Tokenizer tokenizer;
        tokenizer.input.str  = GetAssetBuffer(node);
        tokenizer.input.size = node->size;
        tokenizer.cursor     = tokenizer.input.str;

        GetPointerValue(&tokenizer, &model->vertexCount);
        GetPointerValue(&tokenizer, &model->indexCount);

        // TODO: if the data is freed, this instantly goes bye bye. use a handle.
        // what I'm thinking is that the memory itself is wrapped in a structure that is pointed to by a handle,
        // instead of just pointing to the asset node which contains information you don't really need?
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
                    model->materials[i].textureHandles[j] = LoadAssetFile(path);
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
            AS_EnqueueFile(path);
            model->skeleton = AS_GetAssetHandle(path);
        }

        Assert(EndOfBuffer(&tokenizer));
        WriteBarrier();
        node->status = AS_Status_Loaded;
    }
    else if (extension == Str8Lit("anim"))
    {
        node->type = AS_Anim;
        u8 *buffer = GetAssetBuffer(node);

        // NOTE: crazy town incoming
        KeyframedAnimation **animation = &node->anim;
        *animation                     = (KeyframedAnimation *)buffer;
        KeyframedAnimation *a          = node->anim;

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
        node->status = AS_Status_Loaded;
    }
    else if (extension == Str8Lit("skel"))
    {
        LoadedSkeleton skeleton;
        Tokenizer tokenizer;
        tokenizer.input.str  = GetAssetBuffer(node);
        tokenizer.input.size = node->size;
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
            // pointer location. For now, I am storing the string data right after the offset and size, but this
            // could theoretically be moved elsewhere.
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
        node->type     = AS_Skeleton;
        node->skeleton = skeleton;
        WriteBarrier();
        node->status = AS_Status_Loaded;
    }
    else if (extension == Str8Lit("png"))
    {
        node->type = AS_Texture;
        if (FindSubstring(node->path, Str8Lit("diffuse"), 0, MatchFlag_CaseInsensitive) != node->path.size)
        {
            node->texture.type = TextureType_Diffuse;
        }
        else if (FindSubstring(node->path, Str8Lit("normal"), 0, MatchFlag_CaseInsensitive) != node->path.size)
        {
            node->texture.type = TextureType_Normal;
        }
        R_TexFormat format = R_TexFormat_RGBA8;
        switch (node->texture.type)
        {
            case TextureType_Diffuse: format = R_TexFormat_SRGB; break;
            case TextureType_Normal:
            default: format = R_TexFormat_RGBA8; break;
        }
        AS_Handle handle = AS_HandleFromAsset(node);
        i32 width, height, nComponents;
        u8 *texData =
            stbi_load_from_memory(GetAssetBuffer(node), (i32)node->size, &width, &height, &nComponents, 4);
        node->texture.width  = width;
        node->texture.height = height;

        node->texture.handle = R_AllocateTexture2D(texData, width, height, format);
        node->status         = AS_Status_Loaded;
    }
    else if (extension == Str8Lit("ttf"))
    {
        // u8 *buffer = GetAssetBuffer(node);
        // stbtt_fontinfo font;
        //
        // i32 width, height, xOffset, yOffset;
        // stbtt_InitFont(&font, buffer, stbtt_GetFontOffsetForIndex(buffer, 0));
        // u8 *bitmap = stbtt_GetCodepointBitmap(&font, 0, stbtt_ScaleForPixelHeight(&font, s), c, &w, &h, 0, 0);
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

internal AS_Handle AS_HandleFromAsset(AS_Node *node)
{
    AS_Handle result = {};
    result.u64[0]    = (u64)(node);
    result.u64[1]    = node->generation;
    return result;
}

global readonly AS_Node as_nodeNil;
internal AS_Node *AS_AssetFromHandle(AS_Handle handle)
{
    AS_Node *node = (AS_Node *)(handle.u64[0]);
    if (node == 0 || node->generation != handle.u64[1])
    {
        node = &as_nodeNil;
    }
    return node;
}

internal void AS_UnloadAsset(AS_Node *node)
{
    switch (node->type)
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

internal AS_Handle AS_GetAssetHandle(string path)
{
    u64 hash         = HashFromString(path);
    AS_Handle result = {hash, 0};

    return result;
}

// TODO IMPORTANT: get the nodes directly from the handle instead of having to do this dance every time
// also need to do model loading similar to how texture loading is done
#define AS_NodeFoundBit 0xf000000000000000
internal AS_Node *AS_GetNodeFromHandle(AS_Handle handle)
{
    AS_Node *result = 0;
    Assert(sizeof(handle) <= 16);
    if (handle.u64[2] & AS_NodeFoundBit)
    {
        result = (AS_Node *)handle.u64[0];
        if (handle.u64[2] != result->generation)
        {
            result = 0;
        }
    }
    else
    {
        u64 hash      = handle.u64[0];
        u32 slotIndex = hash & (as_state->numSlots - 1);
        AS_Slot *slot = as_state->assetSlots + slotIndex;

        // TODO: can this be lockless?
        BeginRMutex(&slot->mutex);
        for (AS_Node *node = slot->first; node != 0; node = node->next)
        {
            if (node->hash == hash)
            {
                result = node;
            }
        }
        EndRMutex(&slot->mutex);
        if (result && result->status != AS_Status_Loaded)
        {
            result = 0;
        }
    }

    return result;
}

internal LoadedSkeleton *GetSkeleton(AS_Handle handle)
{
    LoadedSkeleton *result = &skeletonNil;
    AS_Node *node          = AS_GetNodeFromHandle(handle);
    if (node)
    {
        Assert(node->type == AS_Skeleton);
        result = &node->skeleton;
    }

    return result;
}

internal Texture *GetTexture(AS_Handle handle)
{
    Texture *result = &textureNil;
    AS_Node *node   = AS_GetNodeFromHandle(handle);
    if (node)
    {
        Assert(node->type == AS_Texture);
        result = &node->texture;
    }
    return result;
}

internal LoadedModel *GetModel(AS_Handle handle)
{
    LoadedModel *result = &modelNil;
    AS_Node *node       = AS_GetNodeFromHandle(handle);
    if (node)
    {
        Assert(node->type == AS_Model);
        result = &node->model;
    }
    return result;
}
internal LoadedSkeleton *GetSkeletonFromModel(AS_Handle handle)
{
    LoadedModel *model     = GetModel(handle);
    LoadedSkeleton *result = &skeletonNil;
    AS_Node *node          = AS_GetNodeFromHandle(handle);
    if (node == 0)
    {
        result = &skeletonNil;
    }
    else
    {
        Assert(node->type == AS_Model);
        model  = &node->model;
        result = GetSkeleton(model->skeleton);
    }

    return result;
}
internal KeyframedAnimation *GetAnim(AS_Handle handle)
{
    KeyframedAnimation *result = &animNil;
    AS_Node *node              = AS_GetNodeFromHandle(handle);
    if (node)
    {
        Assert(node->type == AS_Anim);
        result = node->anim;
    }
    return result;
}

internal R_Handle GetTextureRenderHandle(AS_Handle input)
{
    Texture *texture = GetTexture(input);
    R_Handle handle  = texture->handle;
    return handle;
}

inline AS_Handle LoadAssetFile(string filename)
{
    AS_EnqueueFile(filename);
    AS_Handle result = AS_GetAssetHandle(filename);
    return result;
}

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
internal AS_MemoryHeaderNode *AllocateBlock()
{
    AS_MemoryHeaderNode *sentinel   = &as_state->freeBlockSentinel;
    AS_MemoryHeaderNode *headerNode = sentinel->next;
    headerNode->next->prev          = headerNode->prev;
    headerNode->prev->next          = headerNode->next;
    headerNode->next = headerNode->prev = 0;
    return headerNode;
}
// Removes the input element from the free list (for >2MB block allocation)
internal AS_MemoryHeaderNode *AllocateBlock(AS_MemoryHeaderNode *headerNode)
{
    headerNode->next->prev = headerNode->prev;
    headerNode->prev->next = headerNode->next;
    headerNode->next = headerNode->prev = 0;
    return headerNode;
}
// Adds to free list
internal void FreeBlock(AS_MemoryHeaderNode *headerNode)
{
    AS_MemoryHeaderNode *sentinel = &as_state->freeBlockSentinel;
    headerNode->next              = sentinel;
    headerNode->prev              = sentinel->prev;
    headerNode->next->prev        = headerNode;
    headerNode->prev->next        = headerNode;
}

internal void FreeBlocks(AS_Node *n)
{
    TicketMutexScope(&as_state->mutex)
    {
        i32 numBlocks             = (i32)((n->size / as_state->blockSize) + (n->size % as_state->blockSize != 0));
        AS_MemoryHeaderNode *node = n->memoryBlock;

        Assert(node + numBlocks <= as_state->memoryHeaderNodes + as_state->numBlocks);
        for (i32 i = 0; i < numBlocks; i++)
        {
            FreeBlock(node + i);
        }
        n->memoryBlock = 0;
    }
}

internal AS_MemoryHeaderNode *AllocateBlocks(u64 size)
{
    AS_MemoryHeaderNode *result = 0;
    TicketMutexScope(&as_state->mutex)
    {
        AS_MemoryHeaderNode *sentinel = &as_state->freeBlockSentinel;
        Assert(sentinel->next != 0);
        // Assert(attributes.size <= 2 * as_state->blockSize);
        // If block is small enough, just get off free list
        if (size <= as_state->blockSize)
        {
            AS_MemoryHeaderNode *block = sentinel->next;
            result                     = AllocateBlock();
        }
        // Otherwise
        else
        {
            u32 s = SafeTruncateU64(size);
            // Find a set of consecutive blocks that fits the needed size
            i32 numBlocks  = (i32)((s / as_state->blockSize) + (s % as_state->blockSize != 0));
            i32 startIndex = -1;
            i32 count      = 0;
            for (i32 i = 0; i < (i32)as_state->numBlocks - (i32)numBlocks; i++)
            {
                AS_MemoryHeaderNode *n = &as_state->memoryHeaderNodes[i];
                b32 isFree             = (n->next != 0);
                b32 isChaining         = (startIndex != -1 && count != 0);
                // Start a chain of contiguous blocks
                if (isFree && !isChaining)
                {
                    startIndex = i;
                    count      = 1;
                }
                // If there aren't enough contiguous blocks, restart
                else if (!isFree && isChaining)
                {
                    startIndex = -1;
                    count      = 0;
                }
                // If block is free and part of an ongoing chain, increment and continue
                else if (isFree && isChaining)
                {
                    count += 1;
                }
                if (count == numBlocks)
                {
                    Assert(as_state->memoryHeaderNodes[startIndex].next != 0);
                    Assert(&as_state->memoryHeaderNodes[startIndex] + count <=
                           as_state->memoryHeaderNodes + as_state->numBlocks);
                    for (i32 j = startIndex; j < startIndex + count; j++)
                    {
                        AllocateBlock(&as_state->memoryHeaderNodes[j]);
                    }
                    result = &as_state->memoryHeaderNodes[startIndex];
                    break;
                }
            }
        }
    }
    return result;
}

inline u8 *GetAssetBuffer(AS_Node *node)
{
    u8 *result = 0;
    if (node->memoryBlock)
    {
        result = node->memoryBlock->header.buffer;
    }
    return result;
}

inline void EndTemporaryMemory(AS_Node *node)
{
    FreeBlocks(node);
}
