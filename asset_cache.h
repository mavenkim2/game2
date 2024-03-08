#ifndef ASSET_CACHE_H
#define ASSET_CACHE_H

#include "crack.h"
#ifdef LSP_INCLUDE
#include "keepmovingforward_common.h"
#include "job.h"
#include "asset.h"
#endif

struct AS_Node;
struct AS_Slot;

struct AS_MemoryHeader
{
    u8 *buffer;
};

struct AS_MemoryHeaderNode
{
    AS_MemoryHeader header;
    // Free list
    AS_MemoryHeaderNode *next;
    AS_MemoryHeaderNode *prev;
};

struct AS_CacheState
{
    Arena *arena;

    // Must be power of 2
    u8 *ringBuffer;
    u64 ringBufferSize;
    u64 readPos;
    u64 writePos;

    TicketMutex mutex;

    // Threads
    OS_Handle *threads;
    u32 threadCount;
    OS_Handle hotloadThread;

    OS_Handle writeSemaphore;
    OS_Handle readSemaphore;

    // Hash table for assets
    u32 numSlots;
    AS_Slot *assetSlots;

    // Block allocator for assets
    u32 numBlocks;
    u32 blockSize;
    u8 *blockBackingBuffer;
    AS_MemoryHeaderNode *memoryHeaderNodes;
    AS_MemoryHeaderNode freeBlockSentinel;

    // Free list
    AS_Node *freeNode;
};

enum AS_Type
{
    AS_Null,
    AS_Mesh,
    AS_Texture,
    AS_Skeleton,
    AS_Model,
    AS_Shader,
    AS_GLTF,
    AS_Count,
};

enum AS_Status
{
    AS_Status_Unloaded,
    AS_Status_Queued,
    AS_Status_Loaded,
};

struct AS_Asset
{
    Arena *arena;
    u64 hash;
    u64 lastModified;
    string path;
    string data;
    AS_Type type;
    AS_Status status;

    union
    {
        LoadedSkeleton skeleton;
        Texture texture;
        LoadedModel model;
    };
};

struct AS_Node
{
    u64 hash;
    u64 lastModified;
    string path;
    u64 size;
    AS_Status status;

    // Memory
    AS_MemoryHeaderNode *memoryBlock;

    // Asset type
    AS_Type type;
    union
    {
        LoadedSkeleton skeleton;
        Texture texture;
        LoadedModel model;
    };

    AS_Node *next;
};

struct AS_Slot
{
    AS_Node *first;
    AS_Node *last;

    Mutex mutex;
};

internal void AS_Init();
internal u64 RingRead(u8 *base, u64 ringSize, u64 readPos, void *dest, u64 destSize);
internal u64 RingWrite(u8 *base, u64 ringSize, u64 writePos, void *src, u64 srcSize);
#define RingReadStruct(base, size, readPos, ptr)   RingRead((base), (size), (readPos), (ptr), sizeof(*(ptr)))
#define RingWriteStruct(base, size, writePos, ptr) RingWrite((base), (size), (writePos), (ptr), sizeof(*(ptr)))

internal b32 AS_EnqueueFile(string path);
internal string AS_DequeueFile(Arena *arena);

internal void AS_EntryPoint(void *p);
internal void AS_HotloadEntryPoint(void *p);
JOB_CALLBACK(AS_LoadAsset);
internal void AS_UnloadAsset(AS_Node *node);

//////////////////////////////
// Handles
//
internal AS_Handle AS_GetAssetHandle(u64 hash);
internal AS_Handle AS_GetAssetHandle(string path);
internal LoadedSkeleton *GetSkeleton(AS_Handle handle);
internal LoadedSkeleton *GetSkeletonFromModel(AS_Handle handle);
internal LoadedModel *GetModel(AS_Handle handle);
internal Texture *GetTexture(AS_Handle handle);
internal R_Handle GetTextureRenderHandle(AS_Handle input);
inline AS_Handle LoadAssetFile(string filename);

//////////////////////////////
// Helpers
//
internal AS_MemoryHeaderNode *AllocateBlock();
internal AS_MemoryHeaderNode *AllocateBlock(AS_MemoryHeaderNode *headerNode);
internal void FreeBlock(AS_MemoryHeaderNode *headerNode);
inline u8 *GetAssetBuffer(AS_Node *node);

#endif
