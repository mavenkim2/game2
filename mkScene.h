#ifndef SCENE_H
#define SCENE_H

#include "mkCrack.h"
#ifdef LSP_INCLUDE
#include "mkList.h"
#include "mkAsset.h"
#include "mkGraphics.h"
#endif

#include <atomic>

namespace scene
{
const i32 NULL_HANDLE = 0;

//////////////////////////////
// Small memory allocator
//

struct SmallMemoryNode
{
    SmallMemoryNode *next;
    i32 id;
};

struct SmallMemoryPool
{
    SmallMemoryNode **freeList;

    const i32 totalSize = kilobytes(4);
    std::atomic<i32> allocatedSize;
    i32 size;
};

struct SmallMemoryAllocator
{
    Mutex mutex;
    Arena *arena;
    // 16, 32, 64, 128
    SmallMemoryPool pools[4];

    void Init();
    void *Alloc(i32 size);
    void Free(void *memory);
};

//////////////////////////////
// Tickets
//

struct MeshTicket
{
    struct MeshCreateRequest *request;
    struct Scene *parentScene;
    struct Mesh *GetMesh();
    ~MeshTicket();
};

struct MaterialTicket
{
    struct MaterialCreateRequest *request;
    struct Scene *parentScene;
    struct MaterialComponent *GetMaterial();
    ~MaterialTicket();
};

struct TransformTicket
{
    struct TransformCreateRequest *request;
    struct Scene *parentScene;
    Mat4 *GetTransform();
    ~TransformTicket();
};

struct SkeletonTicket
{
    struct SkeletonCreateRequest *request;
    struct Scene *parentScene;
    LoadedSkeleton *GetSkeleton();
    ~SkeletonTicket();
};

//////////////////////////////
// Materials
//

// TODO: handles could contain a pointer that directly points to a value, instead of using chunknode/index
struct MaterialComponent
{
    MaterialFlag flags;
    u32 sid;
    AS_Handle textures[TextureType_Count] = {};

    V4 baseColor        = {1, 1, 1, 1};
    f32 metallicFactor  = 0.f;
    f32 roughnessFactor = 1.f;
};

class MaterialManager
{
    friend struct MaterialIter;

private:
    static const i32 numMaterialSlots = 256;
    StaticAssert(IsPow2(numMaterialSlots), MaterialSlotsPow2);
    static const i32 materialSlotMask = numMaterialSlots - 1;

    static const i32 numMaterialsPerChunk = 256;
    StaticAssert(IsPow2(numMaterialsPerChunk), MaterialChunksPow2);
    static const i32 materialChunkMask = numMaterialsPerChunk - 1;

    struct MaterialSlotNode
    {
        MaterialHandle handle;
        u32 id;
        MaterialSlotNode *next;
    };

    struct MaterialSlot
    {
        MaterialSlotNode *first;
        MaterialSlotNode *last;
    };

    struct MaterialChunkNode
    {
        MaterialComponent materials[numMaterialsPerChunk];
        MaterialChunkNode *next;
    };

    struct MaterialFreeNode
    {
        MaterialHandle handle;
        MaterialFreeNode *next;
    };

    // Hash
    MaterialSlot *nameMap;
    MaterialSlotNode *freeSlotNode = 0;

    // Data
    MaterialChunkNode *first = 0;
    MaterialChunkNode *last  = 0;
    u32 totalNumMaterials    = 0;
    u32 materialWritePos     = 0;

    // Freed spots
    MaterialFreeNode *freeMaterialPositions = 0;
    MaterialFreeNode *freeMaterialNodes     = 0;

    // TODO: if in the future it's possible to have multiple scenes/multiple material managers, ensure that the chunk node exists
    // in the linked list and that the global index is less than materialWritePos
    void InsertNameMap(MaterialHandle handle, i32 sid);
    MaterialHandle RemoveFromNameMap(u32 sid);
    void RemoveFromList(MaterialHandle handle);
    MaterialHandle GetHandleFromNameMap(string name);
    MaterialHandle GetFreePosition(MaterialChunkNode **chunkNode, u32 *localIndex);

    //////////////////////////////
    // Handles
    //
    inline MaterialHandle CreateHandle(MaterialChunkNode *chunkNode, u32 localIndex)
    {
        MaterialHandle handle;
        handle.u64[0] = (u64)chunkNode;
        handle.u32[2] = localIndex;
        return handle;
    }
    inline void UnpackHandle(MaterialHandle handle, MaterialChunkNode **chunkNode, u32 *localIndex)
    {
        *chunkNode  = (MaterialChunkNode *)handle.u64[0];
        *localIndex = handle.u32[2];
    }

public:
    inline b8 IsValidHandle(MaterialHandle handle)
    {
        b8 result = handle.u64[0] != 0;
        return result;
    }
    inline MaterialComponent *GetFromHandle(MaterialHandle handle)
    {
        MaterialComponent *result = 0;
        if (IsValidHandle(handle))
        {
            MaterialChunkNode *chunkNode = (MaterialChunkNode *)handle.u64[0];
            u32 localIndex               = handle.u32[2];
            result                       = &chunkNode->materials[localIndex];
        }
        return result;
    }

    struct Scene *parentScene;

    MaterialManager(Scene *inScene);
    MaterialTicket Create(string name);
    void CreateInternal(struct MaterialCreateRequest *request);
    b32 Remove(string name);
    b32 Remove(u32 sid);
    b32 Remove(MaterialHandle handle);

    MaterialHandle GetHandle(string name);
    MaterialComponent *Get(string name);
    MaterialComponent *Get(Entity entity);

    MaterialIter BeginIter();
    b8 EndIter(MaterialIter *iter);
    void Next(MaterialIter *iter);
    inline MaterialComponent *Get(MaterialIter *iter);
};

struct MaterialIter
{
    MaterialManager::MaterialChunkNode *chunkNode;
    MaterialComponent *material;
    u32 localIndex;
    u32 globalIndex;
};

//////////////////////////////
// Meshes
//
class MeshManager
{
    friend struct MeshIter;

private:
    static const i32 numMeshesPerChunk = 256;
    StaticAssert(IsPow2(numMeshesPerChunk), MeshChunksPow2);
    static const i32 meshChunkMask = numMeshesPerChunk - 1;

    static const i32 numMeshSlots = 256;
    StaticAssert(IsPow2(numMeshSlots), MeshSlotsPow2);
    static const i32 meshSlotMask = numMeshSlots - 1;

    struct MeshFreeNode
    {
        MeshHandle handle;
        MeshFreeNode *next;
    };

    struct MeshSlotNode
    {
        MeshHandle handle;
        Entity entity;
        MeshSlotNode *next;
    };

    struct MeshSlot
    {
        MeshSlotNode *first;
        MeshSlotNode *last;
    };

    struct MeshChunkNode
    {
        Mesh meshes[numMeshesPerChunk];
        Entity entities[numMeshesPerChunk];
        MeshChunkNode *next;
    };

    MeshSlot *meshSlots;
    MeshChunkNode *first = 0;
    MeshChunkNode *last  = 0;
    u32 meshWritePos     = 0;
    u32 totalNumMeshes   = 0;

    MeshFreeNode *freePositions = 0;
    MeshFreeNode *freeNodes     = 0;
    MeshSlotNode *freeSlotNodes = 0;

    //////////////////////////////
    // Handles
    //
    inline MeshHandle CreateHandle(MeshChunkNode *chunkNode, u32 localIndex)
    {
        MeshHandle handle;
        handle.u64[0] = (u64)chunkNode;
        handle.u32[2] = localIndex;
        return handle;
    }
    inline void UnpackHandle(MeshHandle handle, MeshChunkNode **chunkNode, u32 *localIndex)
    {
        *chunkNode  = (MeshChunkNode *)handle.u64[0];
        *localIndex = handle.u32[2];
    }

public:
    inline b8 IsValidHandle(MeshHandle handle)
    {
        return handle.u64[0] != 0;
    }
    inline Mesh *GetFromHandle(MeshHandle handle)
    {
        Mesh *result = 0;
        if (IsValidHandle(handle))
        {
            MeshChunkNode *chunkNode = (MeshChunkNode *)handle.u64[0];
            u32 localIndex           = handle.u32[2];
            result                   = &chunkNode->meshes[localIndex];
        }
        return result;
    }

public:
    struct Scene *parentScene;
    MeshManager(Scene *inScene);
    MeshTicket Create(Entity entity);
    void CreateInternal(struct MeshCreateRequest *request);
    b8 Remove(Entity entity);
    Mesh *Get(Entity entity);

    MeshIter BeginIter();
    b8 EndIter(MeshIter *iter);
    void Next(MeshIter *iter);
    inline Mesh *Get(MeshIter *iter);
    inline Entity GetEntity(MeshIter *iter);

    inline u32 GetTotal() { return totalNumMeshes; }
};

struct MeshIter
{
    MeshManager::MeshChunkNode *chunkNode;
    Mesh *mesh;
    u32 localIndex;
    u32 globalIndex;
};

//////////////////////////////
// Transforms
//

class TransformManager
{
private:
    static const u32 numTransformsPerChunk = 256;
    StaticAssert(IsPow2(numTransformsPerChunk), TransformChunksPow2);
    static const i32 transformChunkMask = numTransformsPerChunk - 1;

    static const u32 numTransformSlots = 256;
    StaticAssert(IsPow2(numTransformSlots), TransformSlotsPow2);
    static const i32 transformSlotMask = numTransformSlots - 1;

    struct TransformChunkNode
    {
        Mat4 transforms[numTransformsPerChunk];
        Entity entities[numTransformsPerChunk];
        TransformChunkNode *next;
    };

    struct TransformSlotNode
    {
        TransformHandle handle;
        Entity entity;
        TransformSlotNode *next;
    };

    struct TransformSlot
    {
        TransformSlotNode *first;
        TransformSlotNode *last;
    };

    struct TransformFreeNode
    {
        TransformHandle handle;
        TransformFreeNode *next;
    };

    TransformSlot *transformSlots;
    TransformChunkNode *first = 0;
    TransformChunkNode *last  = 0;
    u32 transformWritePos     = 1;
    u32 totalNumTransforms    = 0;
    u32 lastChunkNodeIndex    = 0;

    TransformSlotNode *freeSlotNodes = 0;
    TransformFreeNode *freePositions = 0;
    TransformFreeNode *freeNodes     = 0;

    //////////////////////////////
    // Handles
    //
    inline TransformHandle CreateHandle(TransformChunkNode *chunkNode, u32 localIndex, u32 chunkNodeIndex)
    {
        TransformHandle handle;
        handle.u64[0] = (u64)chunkNode;
        handle.u32[2] = localIndex;
        handle.u32[3] = chunkNodeIndex;
        return handle;
    }
    inline void UnpackHandle(TransformHandle handle, TransformChunkNode **chunkNode, u32 *localIndex)
    {
        *chunkNode  = (TransformChunkNode *)handle.u64[0];
        *localIndex = handle.u32[2];
    }

public:
    inline b32 IsValidHandle(TransformHandle handle)
    {
        return handle.u64[0] != 0;
    }
    inline u32 GetIndex(TransformHandle handle)
    {
        u32 chunkNodeNum = handle.u32[3];
        u32 localIndex   = handle.u32[2];
        return numTransformsPerChunk * chunkNodeNum + localIndex;
    }
    inline Mat4 *GetFromHandle(TransformHandle handle)
    {
        Mat4 *transform = 0;
        if (IsValidHandle(handle))
        {
            TransformChunkNode *chunkNode = (TransformChunkNode *)handle.u64[0];
            u32 localIndex                = handle.u32[2];
            transform                     = &chunkNode->transforms[localIndex];
        }
        return transform;
    }

public:
    struct Scene *parentScene;
    TransformManager(Scene *inScene);

    TransformTicket Create(Entity entity);
    TransformHandle CreateInternal(struct TransformCreateRequest *request);
    b32 Remove(Entity entity);
    inline Mat4 *Get(Entity entity);
    inline TransformHandle GetHandle(Entity entity);
    inline u32 GetIndex(Entity entity);

    inline u32 GetEndPos() { return transformWritePos; }
};

//////////////////////////////
// Hierarchy Component
//

struct HierarchyComponent
{
    HierarchyFlag flags;
    Entity parent;
};

class HierarchyManager
{
    friend struct HierarchyIter;

private:
    static const u32 numHierarchyNodesPerChunk = 256;
    StaticAssert(IsPow2(numHierarchyNodesPerChunk), HierarchyChunksPow2);
    static const i32 hierarchyNodeChunkMask = numHierarchyNodesPerChunk - 1;

    static const u32 numHierarchyNodeSlots = 256;
    StaticAssert(IsPow2(numHierarchyNodeSlots), HierarchySlotsPow2);
    static const i32 hierarchyNodeSlotMask = numHierarchyNodeSlots - 1;

    struct HierarchyChunkNode
    {
        HierarchyComponent components[numHierarchyNodesPerChunk];
        Entity entities[numHierarchyNodesPerChunk];
        HierarchyChunkNode *next;
    };

    struct HierarchySlotNode
    {
        HierarchyHandle handle;
        Entity entity;
        HierarchySlotNode *next;
    };

    struct HierarchySlot
    {
        HierarchySlotNode *first;
        HierarchySlotNode *last;
    };

    struct HierarchyFreeNode
    {
        HierarchyHandle handle;
        HierarchyFreeNode *next;
    };

    HierarchySlot *hierarchySlots;
    HierarchyChunkNode *first  = 0;
    HierarchyChunkNode *last   = 0;
    u32 hierarchyWritePos      = 0;
    u32 totalNumHierarchyNodes = 0;

    HierarchySlotNode *freeSlotNodes = 0;
    HierarchyFreeNode *freePositions = 0;
    HierarchyFreeNode *freeNodes     = 0;

    //////////////////////////////
    // Handle
    //
    inline HierarchyHandle CreateHandle(HierarchyComponent *node, HierarchyChunkNode *chunkNode)
    {
        HierarchyHandle handle;
        handle.u64[0] = (u64)node;
        handle.u64[1] = (u64)chunkNode;
        return handle;
    }
    inline void UnpackHandle(HierarchyHandle handle, HierarchyComponent **node, HierarchyChunkNode **chunkNode)
    {
        *node      = (HierarchyComponent *)handle.u64[0];
        *chunkNode = (HierarchyChunkNode *)handle.u64[1];
    }

public:
    inline b32 IsValidHandle(HierarchyHandle handle)
    {
        return handle.u64[0] != 0;
    }
    inline u8 GetLocalIndex(HierarchyHandle handle)
    {
        u64 localIndex = (handle.u64[0] - handle.u64[1]) / sizeof(HierarchyComponent);
        Assert(localIndex >= 0 && localIndex < numHierarchyNodesPerChunk);
        return (u8)localIndex;
    }
    inline HierarchyComponent *GetFromHandle(HierarchyHandle handle)
    {
        HierarchyComponent *component = (HierarchyComponent *)handle.u64[0];
        return component;
    }

public:
    struct Scene *parentScene;
    HierarchyManager(Scene *inScene);

    void Create(Entity entity, Entity parent);
    b32 Remove(Entity entity);
    HierarchyHandle GetHandle(Entity entity);

    HierarchyIter BeginIter();
    b8 EndIter(HierarchyIter *iter);
    void Next(HierarchyIter *iter);
    inline HierarchyComponent *Get(HierarchyIter *iter);
    inline Entity GetEntity(HierarchyIter *iter);
    inline u32 GetTotal() { return totalNumHierarchyNodes; }
};

struct HierarchyIter
{
    HierarchyManager::HierarchyChunkNode *chunkNode;
    HierarchyComponent *node;
    u32 localIndex;
    u32 globalIndex;
};

//////////////////////////////
// Skeleton
//

class SkeletonManager
{
    friend struct SkeletonIter;

private:
    static const u32 numSkeletonPerChunk = 256;
    StaticAssert(IsPow2(numSkeletonPerChunk), SkeletonChunksPow2);
    static const i32 skeletonChunkMask = numSkeletonPerChunk - 1;

    static const u32 numSkeletonSlots = 256;
    StaticAssert(IsPow2(numSkeletonSlots), SkeletonSlotsPow2);
    static const i32 skeletonSlotMask = numSkeletonSlots - 1;

    struct SkeletonChunkNode
    {
        LoadedSkeleton skeletons[numSkeletonPerChunk];
        SkeletonFlag flags[numSkeletonPerChunk];
        SkeletonChunkNode *next;
    };

    struct SkeletonSlotNode
    {
        SkeletonHandle handle;
        Entity entity;
        SkeletonSlotNode *next;
    };

    struct SkeletonSlot
    {
        SkeletonSlotNode *first;
        SkeletonSlotNode *last;
    };

    struct SkeletonFreeNode
    {
        SkeletonHandle handle;
        SkeletonFreeNode *next;
    };

    SkeletonSlot *skeletonSlots;
    SkeletonChunkNode *first = 0;
    SkeletonChunkNode *last  = 0;
    u32 skeletonWritePos     = 0;
    u32 totalNumSkeletons    = 0;

    SkeletonSlotNode *freeSlotNodes = 0;
    SkeletonFreeNode *freePositions = 0;
    SkeletonFreeNode *freeNodes     = 0;

    //////////////////////////////
    // Handles
    //

    inline SkeletonHandle CreateHandle(SkeletonChunkNode *chunkNode, u32 localIndex)
    {
        SkeletonHandle handle;
        handle.u64[0] = (u64)chunkNode;
        handle.u32[2] = localIndex;
        return handle;
    }
    inline void UnpackHandle(SkeletonHandle handle, SkeletonChunkNode **chunkNode, u32 *localIndex)
    {
        *chunkNode  = (SkeletonChunkNode *)handle.u64[0];
        *localIndex = handle.u32[2];
    }

public:
    inline b32 IsValidHandle(SkeletonHandle handle)
    {
        return handle.u64[0] != 0;
    }
    inline LoadedSkeleton *GetFromHandle(SkeletonHandle handle)
    {
        LoadedSkeleton *skel = 0;
        if (IsValidHandle(handle))
        {
            SkeletonChunkNode *chunkNode = (SkeletonChunkNode *)handle.u64[0];
            u32 localIndex               = handle.u32[2];
            skel                         = &chunkNode->skeletons[localIndex];
        }
        return skel;
    }

public:
    struct Scene *parentScene;
    SkeletonManager(Scene *inScene);

    SkeletonTicket Create(Entity entity);
    void CreateInternal(SkeletonCreateRequest *request);
    b32 Remove(Entity entity);
    LoadedSkeleton *Get(Entity entity);
    inline SkeletonHandle GetHandle(Entity entity);

    // Iter
    SkeletonIter BeginIter();
    b8 EndIter(SkeletonIter *iter);
    void Next(SkeletonIter *iter);
    inline LoadedSkeleton *Get(SkeletonIter *iter);

    inline u32 GetTotal() { return totalNumSkeletons; }
};

struct SkeletonIter
{
    SkeletonManager::SkeletonChunkNode *chunkNode;
    LoadedSkeleton *skeleton;
    u32 localIndex;
    u32 globalIndex;
};

//////////////////////////////
// Component requests
//

struct ComponentRequestRing
{
    std::atomic<u64> writePos;
    std::atomic<u64> commitWritePos;
    u64 readPos;
    std::atomic<u64> writeTag;
    std::atomic<u64> readTag;

    u8 *ringBuffer;
    static const u32 totalSize = kilobytes(16);
    u32 alignment              = 8;

    void *Alloc(u64 size);
    void EndAlloc(void *mem);
};

enum ComponentRequestType
{
    ComponentRequestType_Null,
    ComponentRequestType_CreateMaterial,
    ComponentRequestType_CreateMesh,
    ComponentRequestType_CreateTransform,
    ComponentRequestType_CreateSkeleton,
};

struct ComponentRequest
{
    ComponentRequestType type;
};

struct MaterialCreateRequest : ComponentRequest
{
    MaterialComponent material;
    u32 sid;
};

struct MeshCreateRequest : ComponentRequest
{
    Mesh mesh;
    Entity entity;
};

struct TransformCreateRequest : ComponentRequest
{
    Mat4 transform;
    Entity entity;
    Entity parent;
};

struct SkeletonCreateRequest : ComponentRequest
{
    LoadedSkeleton skeleton;
    Entity entity;
};

//////////////////////////////
// Scene
//
struct Scene
{
    Arena *arena;

    SmallMemoryAllocator sma;

    void *Alloc(i32 size)
    {
        return sma.Alloc(size);
    }
    void Free(void *memory)
    {
        sma.Free(memory);
    }

    MaterialManager materials;
    MeshManager meshes;
    TransformManager transforms;
    HierarchyManager hierarchy;
    SkeletonManager skeletons;

    //////////////////////////////
    // Meshes
    //
    inline MeshIter BeginMeshIter()
    {
        return meshes.BeginIter();
    }
    inline b8 End(MeshIter *iter)
    {
        b8 result = meshes.EndIter(iter);
        return result;
    }
    inline void Next(MeshIter *iter)
    {
        meshes.Next(iter);
    }
    inline Mesh *Get(MeshIter *iter)
    {
        return meshes.Get(iter);
    }
    inline Entity GetEntity(MeshIter *iter)
    {
        return meshes.GetEntity(iter);
    }

    //////////////////////////////
    // Hierarchy
    //
    inline HierarchyIter BeginHierIter()
    {
        return hierarchy.BeginIter();
    }
    inline b32 End(HierarchyIter *iter)
    {
        b8 result = hierarchy.EndIter(iter);
        return result;
    }
    inline void Next(HierarchyIter *iter)
    {
        hierarchy.Next(iter);
    }
    inline HierarchyComponent *Get(HierarchyIter *iter)
    {
        return hierarchy.Get(iter);
    }
    inline Entity GetEntity(HierarchyIter *iter)
    {
        return hierarchy.GetEntity(iter);
    }

    inline u32 GetIndex(TransformHandle handle)
    {
        return transforms.GetIndex(handle);
    }

    inline SkeletonIter BeginSkelIter()
    {
        return skeletons.BeginIter();
    }
    inline b32 End(SkeletonIter *iter)
    {
        b8 result = skeletons.EndIter(iter);
        return result;
    }
    inline void Next(SkeletonIter *iter)
    {
        skeletons.Next(iter);
    }
    inline LoadedSkeleton *Get(SkeletonIter *iter)
    {
        return skeletons.Get(iter);
    }

    Rect3 aabbs[256];
    u32 aabbCount = 0;

    //////////////////////////////
    // Component requests
    //

    ComponentRequestRing componentRequestRing;
    void ProcessRequests();

    Scene();
    Entity CreateEntity();
    void CreateTransform(Mat4 transform, i32 entity, i32 parent = 0);
};

} // namespace scene

global scene::Scene gameScene;
#endif
