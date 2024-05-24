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
    HashIndex fileHash;

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

    Mutex lock;
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

internal void AS_Init();
internal u64 RingRead(u8 *base, u64 ringSize, u64 readPos, void *dest, u64 destSize);
internal u64 RingWrite(u8 *base, u64 ringSize, u64 writePos, void *src, u64 srcSize);
#define RingReadStruct(base, size, readPos, ptr)   RingRead((base), (size), (readPos), (ptr), sizeof(*(ptr)))
#define RingWriteStruct(base, size, writePos, ptr) RingWrite((base), (size), (writePos), (ptr), sizeof(*(ptr)))

internal b32 AS_EnqueueFile(string path);
internal string AS_DequeueFile(Arena *arena);

THREAD_ENTRY_POINT(AS_EntryPoint);
internal void AS_HotloadEntryPoint(void *p);
// JOB_CALLBACK(AS_LoadAsset);
internal void AS_LoadAsset(AS_Asset *asset);
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
internal AS_Handle AS_GetAsset(const string inPath, const b32 inLoadIfNotFound = 1);
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
// B tree
//

// Main functions
internal AS_MemoryBlockNode *AS_Alloc(i32 size);
internal void AS_Free(AS_Asset *asset);
internal void AS_Free(void **ptr);
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

//////////////////////////////
// DDS
//

#pragma pack(push, 1)
enum PixelFormatFlagBits
{
    PixelFormatFlagBits_AlphaPixels = 0x1,
    PixelFormatFLagBits_Alpha       = 0x2,
    PixelFormatFlagBits_FourCC      = 0x4,
    PixelFormatFlagBits_RGB         = 0x40,
    PixelFormatFlagBits_YUV         = 0x200,
    PixelFormatFlagBits_Luminance   = 0x20000,
};

struct PixelFormat
{
    u32 size;
    u32 flags;
    u32 fourCC;
    u32 rgbBitCount;
    u32 rBitMask;
    u32 gBitMask;
    u32 bBitMask;
    u32 aBitMask;
};

enum HeaderFlagBits
{
    HeaderFlagBits_Caps   = 0x00000001,
    HeaderFlagBits_Height = 0x00000002,
    HeaderFlagBits_Width  = 0x00000004,
    HeaderFlagBits_Pitch  = 0x00000008,

    HeaderFlagBits_PixelFormat = 0x00001000,
    HeaderFlagBits_Mipmap      = 0x00020000,
    HeaderFlagBits_LinearSize  = 0x00080000,
    HeaderFlagBits_Depth       = 0x00800000,
};

struct DDSHeader
{
    u32 size;
    u32 flags;
    u32 height;
    u32 width;
    u32 pitchOrLinearSize;
    u32 depth;
    u32 mipMapCount;
    u32 reserved1[11];
    PixelFormat format;
    u32 caps;
    u32 caps2;
    u32 caps3;
    u32 caps4;
    u32 reserved2;
};

enum DXGI_Format
{
    DXGI_FORMAT_UNKNOWN                    = 0,
    DXGI_FORMAT_R32G32B32A32_TYPELESS      = 1,
    DXGI_FORMAT_R32G32B32A32_FLOAT         = 2,
    DXGI_FORMAT_R32G32B32A32_UINT          = 3,
    DXGI_FORMAT_R32G32B32A32_SINT          = 4,
    DXGI_FORMAT_R32G32B32_TYPELESS         = 5,
    DXGI_FORMAT_R32G32B32_FLOAT            = 6,
    DXGI_FORMAT_R32G32B32_UINT             = 7,
    DXGI_FORMAT_R32G32B32_SINT             = 8,
    DXGI_FORMAT_R16G16B16A16_TYPELESS      = 9,
    DXGI_FORMAT_R16G16B16A16_FLOAT         = 10,
    DXGI_FORMAT_R16G16B16A16_UNORM         = 11,
    DXGI_FORMAT_R16G16B16A16_UINT          = 12,
    DXGI_FORMAT_R16G16B16A16_SNORM         = 13,
    DXGI_FORMAT_R16G16B16A16_SINT          = 14,
    DXGI_FORMAT_R32G32_TYPELESS            = 15,
    DXGI_FORMAT_R32G32_FLOAT               = 16,
    DXGI_FORMAT_R32G32_UINT                = 17,
    DXGI_FORMAT_R32G32_SINT                = 18,
    DXGI_FORMAT_R32G8X24_TYPELESS          = 19,
    DXGI_FORMAT_D32_FLOAT_S8X24_UINT       = 20,
    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS   = 21,
    DXGI_FORMAT_X32_TYPELESS_G8X24_UINT    = 22,
    DXGI_FORMAT_R10G10B10A2_TYPELESS       = 23,
    DXGI_FORMAT_R10G10B10A2_UNORM          = 24,
    DXGI_FORMAT_R10G10B10A2_UINT           = 25,
    DXGI_FORMAT_R11G11B10_FLOAT            = 26,
    DXGI_FORMAT_R8G8B8A8_TYPELESS          = 27,
    DXGI_FORMAT_R8G8B8A8_UNORM             = 28,
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB        = 29,
    DXGI_FORMAT_R8G8B8A8_UINT              = 30,
    DXGI_FORMAT_R8G8B8A8_SNORM             = 31,
    DXGI_FORMAT_R8G8B8A8_SINT              = 32,
    DXGI_FORMAT_R16G16_TYPELESS            = 33,
    DXGI_FORMAT_R16G16_FLOAT               = 34,
    DXGI_FORMAT_R16G16_UNORM               = 35,
    DXGI_FORMAT_R16G16_UINT                = 36,
    DXGI_FORMAT_R16G16_SNORM               = 37,
    DXGI_FORMAT_R16G16_SINT                = 38,
    DXGI_FORMAT_R32_TYPELESS               = 39,
    DXGI_FORMAT_D32_FLOAT                  = 40,
    DXGI_FORMAT_R32_FLOAT                  = 41,
    DXGI_FORMAT_R32_UINT                   = 42,
    DXGI_FORMAT_R32_SINT                   = 43,
    DXGI_FORMAT_R24G8_TYPELESS             = 44,
    DXGI_FORMAT_D24_UNORM_S8_UINT          = 45,
    DXGI_FORMAT_R24_UNORM_X8_TYPELESS      = 46,
    DXGI_FORMAT_X24_TYPELESS_G8_UINT       = 47,
    DXGI_FORMAT_R8G8_TYPELESS              = 48,
    DXGI_FORMAT_R8G8_UNORM                 = 49,
    DXGI_FORMAT_R8G8_UINT                  = 50,
    DXGI_FORMAT_R8G8_SNORM                 = 51,
    DXGI_FORMAT_R8G8_SINT                  = 52,
    DXGI_FORMAT_R16_TYPELESS               = 53,
    DXGI_FORMAT_R16_FLOAT                  = 54,
    DXGI_FORMAT_D16_UNORM                  = 55,
    DXGI_FORMAT_R16_UNORM                  = 56,
    DXGI_FORMAT_R16_UINT                   = 57,
    DXGI_FORMAT_R16_SNORM                  = 58,
    DXGI_FORMAT_R16_SINT                   = 59,
    DXGI_FORMAT_R8_TYPELESS                = 60,
    DXGI_FORMAT_R8_UNORM                   = 61,
    DXGI_FORMAT_R8_UINT                    = 62,
    DXGI_FORMAT_R8_SNORM                   = 63,
    DXGI_FORMAT_R8_SINT                    = 64,
    DXGI_FORMAT_A8_UNORM                   = 65,
    DXGI_FORMAT_R1_UNORM                   = 66,
    DXGI_FORMAT_R9G9B9E5_SHAREDEXP         = 67,
    DXGI_FORMAT_R8G8_B8G8_UNORM            = 68,
    DXGI_FORMAT_G8R8_G8B8_UNORM            = 69,
    DXGI_FORMAT_BC1_TYPELESS               = 70,
    DXGI_FORMAT_BC1_UNORM                  = 71,
    DXGI_FORMAT_BC1_UNORM_SRGB             = 72,
    DXGI_FORMAT_BC2_TYPELESS               = 73,
    DXGI_FORMAT_BC2_UNORM                  = 74,
    DXGI_FORMAT_BC2_UNORM_SRGB             = 75,
    DXGI_FORMAT_BC3_TYPELESS               = 76,
    DXGI_FORMAT_BC3_UNORM                  = 77,
    DXGI_FORMAT_BC3_UNORM_SRGB             = 78,
    DXGI_FORMAT_BC4_TYPELESS               = 79,
    DXGI_FORMAT_BC4_UNORM                  = 80,
    DXGI_FORMAT_BC4_SNORM                  = 81,
    DXGI_FORMAT_BC5_TYPELESS               = 82,
    DXGI_FORMAT_BC5_UNORM                  = 83,
    DXGI_FORMAT_BC5_SNORM                  = 84,
    DXGI_FORMAT_B5G6R5_UNORM               = 85,
    DXGI_FORMAT_B5G5R5A1_UNORM             = 86,
    DXGI_FORMAT_B8G8R8A8_UNORM             = 87,
    DXGI_FORMAT_B8G8R8X8_UNORM             = 88,
    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM = 89,
    DXGI_FORMAT_B8G8R8A8_TYPELESS          = 90,
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB        = 91,
    DXGI_FORMAT_B8G8R8X8_TYPELESS          = 92,
    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB        = 93,
    DXGI_FORMAT_BC6H_TYPELESS              = 94,
    DXGI_FORMAT_BC6H_UF16                  = 95,
    DXGI_FORMAT_BC6H_SF16                  = 96,
    DXGI_FORMAT_BC7_TYPELESS               = 97,
    DXGI_FORMAT_BC7_UNORM                  = 98,
    DXGI_FORMAT_BC7_UNORM_SRGB             = 99,
    DXGI_FORMAT_AYUV                       = 100,
    DXGI_FORMAT_Y410                       = 101,
    DXGI_FORMAT_Y416                       = 102,
    DXGI_FORMAT_NV12                       = 103,
    DXGI_FORMAT_P010                       = 104,
    DXGI_FORMAT_P016                       = 105,
    DXGI_FORMAT_420_OPAQUE                 = 106,
    DXGI_FORMAT_YUY2                       = 107,
    DXGI_FORMAT_Y210                       = 108,
    DXGI_FORMAT_Y216                       = 109,
    DXGI_FORMAT_NV11                       = 110,
    DXGI_FORMAT_AI44                       = 111,
    DXGI_FORMAT_IA44                       = 112,
    DXGI_FORMAT_P8                         = 113,
    DXGI_FORMAT_A8P8                       = 114,
    DXGI_FORMAT_B4G4R4A4_UNORM             = 115,
    DXGI_FORMAT_P208                       = 130,
    DXGI_FORMAT_V208                       = 131,
    DXGI_FORMAT_V408                       = 132,
    DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE,
    DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE,
    DXGI_FORMAT_FORCE_UINT = 0xffffffff
};

enum ResourceDimension
{
    ResourceDimension_Unknown   = 0,
    ResourceDimension_Buffer    = 1,
    ResourceDimension_Texture1D = 2,
    ResourceDimension_Texture2D = 3,
    ResourceDimension_Texture3D = 4
};

struct DDSHeaderDXT10
{
    DXGI_Format dxgiFormat;
    ResourceDimension resourceDimension;
    u32 miscFlag;
    u32 arraySize;
    u32 miscFlags2;
};

struct DDSFile
{
    u32 magic;
    DDSHeader header;
};
#pragma pack(pop)

#define MakeFourCC(a, b, c, d) (((d) << 24) | ((c) << 16) | ((b) << 8) | ((a) << 0))

// TODO: mips
internal void WriteImageToDDS(graphics::Texture *input, string name);
internal void LoadDDS(AS_Asset *asset);

#endif
