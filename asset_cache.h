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

struct AS_Thread
{
    OS_Handle handle;
    Arena *arena;
};

//////////////////////////////
// Asset tagging
//

enum AS_TagKey
{
    AS_TagKey_UnicodeCodepoint,
    AS_TagKey_Count,
};

struct AS_TagKeyValuePair
{
    AS_Handle assetHandle;
    f32 value;
};

// B tree node
struct AS_TagNode
{
    AS_TagKeyValuePair pairs[4];
    i32 count;
    i16 children[5];
};

struct AS_TagSlot
{
    // Dynamically allocated array of nodes representing a tree
    AS_TagNode *nodes;
    i16 count;
    i16 maxCount;
    // AS_TagNode *root;
    Mutex mutex;
};

struct AS_TagMap
{
    AS_TagSlot *slots;
    u32 maxSlots;
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
    AS_Thread *threads;
    u32 threadCount;
    OS_Handle hotloadThread;

    OS_Handle writeSemaphore;
    OS_Handle readSemaphore;

    // Hash table for assets
    u32 numSlots;
    AS_Slot *assetSlots;

    // Tag Maps for assets
    AS_TagMap tagMap;

    // Block allocator for assets
    // TODO: per thread?
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
    AS_Font,
    AS_Skeleton,
    AS_Anim,
    AS_Model,
    AS_Shader,
    AS_Count,
};

enum AS_Status
{
    AS_Status_Unloaded,
    AS_Status_Queued,
    AS_Status_Loaded,
};

struct Font
{
    F_Data fontData;
};

struct AS_Asset
{
    // Memory
    AS_MemoryHeaderNode *memoryBlock;

    u64 hash;
    u64 lastModified;
    u64 generation;
    string path;
    u64 size;
    AS_Status status;

    // Asset type
    AS_Type type;
    union
    {
        LoadedSkeleton skeleton;
        Texture texture;
        LoadedModel model;
        KeyframedAnimation *anim;
        Font font;
    };
};

struct AS_Node
{
    AS_Node *next;
    AS_Asset asset;
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
internal AS_Handle AS_HandleFromAsset(AS_Node *node);
internal AS_Node *AS_AssetFromHandle(AS_Handle handle);
internal AS_Handle AS_GetAssetHandle(u64 hash);
internal AS_Handle AS_GetAssetHandle(string path);
internal Font *GetFont(AS_Handle handle);
internal LoadedSkeleton *GetSkeleton(AS_Handle handle);
internal LoadedSkeleton *GetSkeletonFromModel(AS_Handle handle);
internal KeyframedAnimation *GetAnim(AS_Handle handle);
internal LoadedModel *GetModel(AS_Handle handle);
internal Texture *GetTexture(AS_Handle handle);
internal R_Handle GetTextureRenderHandle(AS_Handle input);
inline AS_Handle AS_LoadAssetFile(string filename);
inline b8 IsAnimNil(KeyframedAnimation *anim);

//////////////////////////////
// Helpers
//
internal AS_MemoryHeaderNode *AllocateBlock();
internal AS_MemoryHeaderNode *AllocateBlock(AS_MemoryHeaderNode *headerNode);
internal AS_MemoryHeaderNode *AllocateBlocks(u64 size);
internal void FreeBlock(AS_MemoryHeaderNode *headerNode);
internal void FreeBlocks(AS_Node *node);
inline u8 *GetAssetBuffer(AS_Node *node);
inline void EndTemporaryMemory(AS_Node *node);

#endif
