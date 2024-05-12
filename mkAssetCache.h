#ifndef ASSET_CACHE_H
#define ASSET_CACHE_H

#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "third_party/stb_image.h"
#include <atomic>

#include "mkCrack.h"
#ifdef LSP_INCLUDE
#include "keepmovingforward_common.h"
#include "job.h"
#include "asset.h"
#endif

//////////////////////////////
// File reading
//

struct AssetFileSectionHeader
{
    char tag[4];
    i32 offset;
    i32 size;
};

struct AssetFileBlockHeader
{
    u32 size;
};

struct AS_Asset;

// #if 0
// struct AS_MemoryHeader
// {
//     u8 *buffer;
// };
//
// struct AS_MemoryHeaderNode
// {
//     AS_MemoryHeader header;
//     // Free list
//     AS_MemoryHeaderNode *next;
//     AS_MemoryHeaderNode *prev;
// };
// #endif

//////////////////////////////
// Thread sync
//

struct AS_Thread
{
    OS_Handle handle;
};

// #if 0
// struct AS_Stripe
// {
//     // rw mutex
//     Mutex mutex;
// };
// #endif

//////////////////////////////
// Asset tagging/hashing
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

struct AS_HashMap
{
    i32 *hash;
    i32 *indexChain;
    i32 hashCount;
    i32 indexCount;

    i32 hashMask;
    // i32 lookupMask;
};

//////////////////////////////
// Memory management
//
struct AS_MemoryBlockNode;
struct AS_BTreeNode
{
    AS_BTreeNode *parent;
    AS_BTreeNode *next;
    AS_BTreeNode *prev;
    AS_BTreeNode *first;
    AS_BTreeNode *last;

    AS_MemoryBlockNode *memoryBlock;
    i32 key;
    i32 numChildren;
};

struct AS_BTree
{
    AS_BTreeNode *root;
    AS_BTreeNode *free;
    i32 maxChildren;
};

struct AS_MemoryBlockNode
{
    AS_MemoryBlockNode *next;
    AS_MemoryBlockNode *prev;
    AS_BTreeNode *node;

    // This is the size of the allocation excluding the header
    i32 size;
    b8 isBaseBlock;
};

struct AS_DynamicBlockAllocator
{
    Arena *arena;

    TicketMutex ticketMutex;
    AS_MemoryBlockNode *first;
    AS_MemoryBlockNode *last;
    AS_BTree bTree;

    i32 baseBlockSize;
    i32 minBlockSize;

    // stats
    i32 freeBlocks;
    i32 freeBlockMemory;
    i32 usedBlocks;
    i32 usedBlockMemory;
    i32 baseBlocks;
    i32 baseBlockMemory;
    i32 numAllocs;
    i32 numFrees;
    i32 numResizes;
};

//////////////////////////////
// Global state
//

struct AS_CacheState
{
    Arena *arena;

    // Must be power of 2
    u8 *ringBuffer;
    u64 ringBufferSize;
    u64 volatile readPos;
    u64 volatile writePos;

    TicketMutex mutex;

    // Threads
    AS_Thread *threads;
    u32 threadCount;
    OS_Handle hotloadThread;

    // Semaphores
    OS_Handle writeSemaphore;
    OS_Handle readSemaphore;

    // Stripes
    // #if 0
    //     AS_Stripe *stripes;
    //     i32 numStripes;
    // #endif

    // TODO: should this be an array of pointers?. this would make it so that reallocations could be possible
    // without compromising pointer integrity
    AS_Asset **assets;
    i32 assetCapacity;
    i32 assetCount;
    i32 assetEndOfList;

    // free list, only for assets that have been allocated then deallocated
    i32 *freeAssetList;
    i32 freeAssetCount;

    // inspired by idHashIndex
    AS_HashMap fileHash;

    // Tag Maps for assets
    AS_TagMap tagMap;

    // Block allocator for assets
    // TODO: per thread?
    // #if 0
    //     u32 numBlocks;
    //     u32 blockSize;
    //     u8 *blockBackingBuffer;
    //     AS_MemoryHeaderNode *memoryHeaderNodes;
    //     AS_MemoryHeaderNode freeBlockSentinel;
    // #endif

    AS_DynamicBlockAllocator allocator;

    FakeLock fakeLock;
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

// enum AS_MemoryType
// {
//     AS_MemoryType_StaticCPU,
//     AS_MemoryType_StaticGPU,
// };

struct AS_Asset
{
    // Memory
    u64 size;

    u64 lastModified;
    string path;
    std::atomic<u32> status;

    // TODO: intrusive. may be bad? idk
    i32 id;
    i32 generation;

    // Asset type
    AS_MemoryBlockNode *memoryBlock;
    AS_Type type;
    union
    {
        LoadedSkeleton skeleton;
        graphics::Texture texture;
        LoadedModel model;
        KeyframedAnimation anim;
        Font font;
    };
};

// struct AS_Node
// {
//     AS_Node *next;
//     AS_Asset asset;
// };
//
// struct AS_Slot
// {
//     AS_Node *first;
//     AS_Node *last;
//
//     Mutex mutex;
// };

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
internal void AS_UnloadAsset(AS_Asset *asset);

//////////////////////////////
// Handles
//
global readonly LoadedSkeleton skeletonNil;
global readonly LoadedModel modelNil;
global readonly graphics::Texture textureNil;
global readonly KeyframedAnimation animNil;
global readonly Font fontNil;

// #if 0
// internal AS_Handle AS_HandleFromAsset(AS_Asset *asset);
// internal AS_Node *AS_AssetFromHandle(AS_Handle handle);
// #endif

internal Font *GetFont(AS_Handle handle);
internal AS_Asset *AS_GetAssetFromHandle(AS_Handle handle);
internal AS_Handle AS_GetAsset(const string inPath);
internal LoadedSkeleton *GetSkeleton(AS_Handle handle);
internal LoadedSkeleton *GetSkeletonFromModel(AS_Handle handle);
internal KeyframedAnimation *GetAnim(AS_Handle handle);
internal LoadedModel *GetModel(AS_Handle handle);
internal graphics::Texture *GetTexture(AS_Handle handle);
// internal R_Handle GetTextureRenderHandle(AS_Handle input);
inline AS_Handle AS_LoadAssetFile(string filename);
inline b8 IsAnimNil(KeyframedAnimation *anim);
inline b8 IsFontNil(Font *font)
{
    b8 result = (font == 0 || font == &fontNil);
    return result;
}

//////////////////////////////
// Helpers
//
// #if 0
// internal AS_MemoryHeaderNode *AllocateBlock();
// internal AS_MemoryHeaderNode *AllocateBlock(AS_MemoryHeaderNode *headerNode);
// internal AS_MemoryHeaderNode *AllocateBlocks(u64 size);
// internal void FreeBlock(AS_MemoryHeaderNode *headerNode);
// internal void FreeBlocks(AS_Asset *asset);
// inline u8 *GetAssetBuffer(AS_Asset *asset);
// inline void EndTemporaryMemory(AS_Asset *asset);
// #endif

//////////////////////////////
// Asset hash maps (using indices)
//

internal i32 AS_FirstInHash(i32 hash);
internal i32 AS_NextInHash(i32 index);
internal i32 AS_FirstInHash(string path);
internal void AS_AddInHash(i32 key, i32 index);
internal void AS_RemoveFromHash(i32 key, i32 index);

//////////////////////////////
// B tree
//

// Main functions
internal AS_MemoryBlockNode *AS_Alloc(i32 size);
internal void AS_Free(AS_Asset *asset);
internal u8 *AS_GetMemory(AS_Asset *asset);

// Helpers
internal void AS_InitializeAllocator();
internal void AS_Free(AS_MemoryBlockNode *memoryBlock);
internal AS_BTreeNode *AS_AllocNode();
internal AS_BTreeNode *AS_AddNode(AS_MemoryBlockNode *memNode);
internal void AS_RemoveNode(AS_BTreeNode *node);
internal void AS_SplitNode(AS_BTreeNode *node);
internal AS_BTreeNode *AS_MergeNodes(AS_BTreeNode *node1, AS_BTreeNode *node2);
internal AS_BTreeNode *AS_FindMemoryBlock(i32 size);
internal u8 *AS_GetMemory(AS_MemoryBlockNode *node);

#endif
