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
    // std::atomic<i32> allocatedSize;
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
// Materials
//

// TODO: handles could contain a pointer that directly points to a value, instead of using chunknode/index
struct MaterialComponent
{
    AS_Handle textures[TextureType_Count] = {};

    V4 baseColor        = {1, 1, 1, 1};
    f32 metallicFactor  = 0.f;
    f32 roughnessFactor = 1.f;
    MaterialFlag flags;
    u32 sid;

    b32 IsRenderable();
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
    MaterialSlotNode *freeSlotNode;

    // Data
    MaterialChunkNode *first;
    MaterialChunkNode *last;
    u32 totalNumMaterials;
    u32 materialWritePos;
    u32 numChunkNodes;

    // Freed spots
    MaterialFreeNode *freeMaterialPositions;
    MaterialFreeNode *freeMaterialNodes;

    // TODO: if in the future it's possible to have multiple scenes/multiple material managers, ensure that the chunk node exists
    // in the linked list and that the global index is less than materialWritePos
    MaterialHandle RemoveFromNameMap(u32 sid);
    void RemoveFromList(MaterialHandle handle);

    //////////////////////////////
    // Handles
    //
    inline MaterialHandle CreateHandle(MaterialChunkNode *chunkNode, u32 localIndex, u32 chunkNodeIndex)
    {
        MaterialHandle handle;
        handle.u64[0] = (u64)chunkNode;
        handle.u32[2] = localIndex;
        handle.u32[3] = chunkNodeIndex;
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
    inline MaterialHandle GetHandle(u32 sid)
    {
        MaterialSlot *slot    = &nameMap[sid & materialSlotMask];
        MaterialHandle handle = {};
        for (MaterialSlotNode *node = slot->first; node != 0; node = node->next)
        {
            if (node->id == sid)
            {
                handle = node->handle;
                break;
            }
        }
        return handle;
    }
    inline MaterialHandle GetHandle(string name)
    {
        u32 sid               = GetSID(name);
        MaterialHandle handle = GetHandle(sid);
        return handle;
    }

    struct Scene *parentScene;

    void Init(Scene *inScene);
    MaterialComponent *Create(u32 sid);
    MaterialComponent *Create(string name);
    b32 Remove(string name);
    b32 Remove(u32 sid);
    b32 Remove(MaterialHandle handle);

    MaterialComponent *Get(string name);
    MaterialComponent *Get(Entity entity);

    MaterialIter BeginIter();
    b8 EndIter(MaterialIter *iter);
    void Next(MaterialIter *iter);
    inline MaterialComponent *Get(MaterialIter *iter);
    inline Entity GetEntity(MaterialIter *iter);

    inline u32 GetTotal() { return totalNumMaterials; }
    inline u32 GetEndPos() { return materialWritePos; }
    inline u32 GetIndex(MaterialHandle handle)
    {
        u32 chunkNodeIndex = handle.u32[3];
        u32 localIndex     = handle.u32[2];
        return numMaterialsPerChunk * chunkNodeIndex + localIndex;
    }
};

// TODO: consider how to make this iteration multithreaded
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
    MeshChunkNode *first;
    MeshChunkNode *last;
    u32 meshWritePos;
    u32 totalNumMeshes;

    MeshFreeNode *freePositions;
    MeshFreeNode *freeNodes;
    MeshSlotNode *freeSlotNodes;

    //////////////////////////////
    // Handles
    //
    inline MeshHandle CreateHandle(MeshChunkNode *chunkNode, u32 globalIndex)
    {
        MeshHandle handle;
        handle.u64[0] = (u64)chunkNode;
        handle.u32[2] = globalIndex;
        return handle;
    }
    inline void UnpackHandle(MeshHandle handle, MeshChunkNode **chunkNode, u32 *globalIndex)
    {
        *chunkNode   = (MeshChunkNode *)handle.u64[0];
        *globalIndex = handle.u32[2];
    }

public:
    u32 totalNumClusters;
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
            u32 globalIndex          = handle.u32[2];
            result                   = &chunkNode->meshes[globalIndex & meshChunkMask];
        }
        return result;
    }

public:
    struct Scene *parentScene;
    void Init(Scene *inScene);
    Mesh *Create(Entity entity, u32 *outGlobalIndex = 0);
    b8 Remove(Entity entity);
    Mesh *Get(Entity entity);

    MeshIter BeginIter();
    b8 EndIter(MeshIter *iter);
    void Next(MeshIter *iter);
    inline Mesh *Get(MeshIter *iter);
    inline Entity GetEntity(MeshIter *iter);

    inline u32 GetTotal() { return totalNumMeshes; }
    inline u32 GetEndPos() { return meshWritePos; }
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
    TransformChunkNode *first;
    TransformChunkNode *last;
    u32 transformWritePos;
    u32 totalNumTransforms;
    u32 numChunkNodes;

    TransformSlotNode *freeSlotNodes;
    TransformFreeNode *freePositions;
    TransformFreeNode *freeNodes;

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
        u32 chunkNodeIndex = handle.u32[3];
        u32 localIndex     = handle.u32[2];
        return numTransformsPerChunk * chunkNodeIndex + localIndex;
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
    void Init(Scene *inScene);

    Mat4 *Create(Entity entity);
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
    HierarchyChunkNode *first;
    HierarchyChunkNode *last;
    u32 hierarchyWritePos;
    u32 totalNumHierarchyNodes;

    HierarchySlotNode *freeSlotNodes;
    HierarchyFreeNode *freePositions;
    HierarchyFreeNode *freeNodes;

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
    void Init(Scene *inScene);

    void Create(Entity entity, Entity parent);
    b32 Remove(Entity entity);
    HierarchyHandle GetHandle(Entity entity);
    HierarchyComponent *Get(Entity entity);

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

    static const u32 genMask = 0x7fffffff;

    struct SkeletonChunkNode
    {
        LoadedSkeleton skeletons[numSkeletonPerChunk];
        u32 gen[numSkeletonPerChunk];
        SkeletonChunkNode *next;
    };

    struct SkeletonSlotNode
    {
        SkeletonHandle handle;
        u32 id;
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

    SkeletonSlot *entityMap;
    SkeletonSlot *nameMap;
    SkeletonChunkNode *first;
    SkeletonChunkNode *last;
    u32 skeletonWritePos;
    u32 totalNumSkeletons;

    SkeletonSlotNode *freeSlotNodes;
    SkeletonFreeNode *freePositions;
    SkeletonFreeNode *freeNodes;

    SkeletonHandle Get(SkeletonSlot *map, u32 id);
    void Insert(SkeletonSlot *map, u32 id, SkeletonHandle handle);
    SkeletonHandle Remove(SkeletonSlot *map, u32 id);

    //////////////////////////////
    // Handles
    //

    inline SkeletonHandle CreateHandle(SkeletonChunkNode *chunkNode, u32 localIndex)
    {
        SkeletonHandle handle;
        handle.u64[0] = (u64)chunkNode;
        handle.u32[2] = localIndex;
        handle.u32[3] = chunkNode->gen[localIndex] & genMask; // NOTE: limit of 2 billion rewrites per index
        return handle;
    }
    inline void UnpackHandle(SkeletonHandle handle, SkeletonChunkNode **chunkNode, u32 *localIndex)
    {
        *chunkNode  = (SkeletonChunkNode *)handle.u64[0];
        *localIndex = handle.u32[2];
    }
    inline void UnpackHandle(SkeletonHandle handle, SkeletonChunkNode **chunkNode, u32 *localIndex, u32 *gen)
    {
        *chunkNode  = (SkeletonChunkNode *)handle.u64[0];
        *localIndex = handle.u32[2];
        *gen        = handle.u32[3];
    }
    inline SkeletonHandle IncrementGen(SkeletonHandle handle)
    {
        handle.u32[3]++;
        return handle;
    }

public:
    inline b32 IsValidHandle(SkeletonHandle handle)
    {
        b32 result = 0;
        if (handle.u64[0] != 0)
        {
            SkeletonChunkNode *chunkNode;
            u32 localIndex, gen;
            UnpackHandle(handle, &chunkNode, &localIndex, &gen);
            if (gen == (chunkNode->gen[localIndex] & genMask))
            {
                result = 1;
            }
        }
        return result;
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
    void Init(Scene *inScene);

    LoadedSkeleton *Create(u32 sid);
    LoadedSkeleton *Create(string name);
    void Link(Entity entity, SkeletonHandle handle);
    b32 Remove(string name);
    inline SkeletonHandle GetHandleFromEntity(Entity entity);
    inline SkeletonHandle GetHandleFromSid(u32 sid);
    inline SkeletonHandle GetHandleFromName(string name);
    inline LoadedSkeleton *GetFromEntity(Entity entity);
    inline LoadedSkeleton *GetFromSid(u32 sid);

    // Iter
    SkeletonIter BeginIter();
    b8 EndIter(SkeletonIter *iter);
    void Next(SkeletonIter *iter);
    inline LoadedSkeleton *Get(SkeletonIter *iter);

    inline u32 GetTotal()
    {
        return totalNumSkeletons;
    }
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

enum SceneRequestType
{
    SceneRequestType_Reset,
    SceneRequestType_MergeScene,
};

struct SceneRequest
{
    SceneRequestType type;
    b8 finished = 0;
};

struct SceneMergeTicket
{
    struct SceneMergeRequest *request;
    struct SceneRequestRing *ring;
    b8 initialized = 0;

    ~SceneMergeTicket();
    struct Scene *GetScene();
};

struct SceneRequestRing
{
    std::atomic<u64> writePos = 0;
    u64 readPos               = 0;

    u8 *ringBuffer             = 0;
    static const u32 totalSize = kilobytes(32);
    u32 alignment              = 8;

    void Init(Arena *arena);
    void *Alloc(u64 size);
    void EndAlloc(SceneRequest *req);
    SceneMergeTicket CreateMergeRequest();
    void ProcessRequests(Scene *parent);
};

//////////////////////////////
// Scene
//
struct Scene
{
    Arena *arena;
    std::atomic<u32> entityGen;
    u32 rootEntity;

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
    // Materials
    //
    inline MaterialIter BeginMatIter()
    {
        return materials.BeginIter();
    }
    inline b8 End(MaterialIter *iter)
    {
        b8 result = materials.EndIter(iter);
        return result;
    }
    inline void Next(MaterialIter *iter)
    {
        materials.Next(iter);
    }
    inline MaterialComponent *Get(MaterialIter *iter)
    {
        return materials.Get(iter);
    }

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

    //////////////////////////////
    // Transform
    //
    inline u32 GetIndex(TransformHandle handle)
    {
        return transforms.GetIndex(handle);
    }

    //////////////////////////////
    // Skel
    //
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

    SceneRequestRing requestRing;
    void ProcessRequests();

    void Init(Arena *inArena);
    inline void CreateTransform(Mat4 transform, Entity entity, Entity parent = 0);
    Entity CreateEntity();
    void Merge(Scene *other);
};

//////////////////////////////
// Merge request
//
struct SceneMergeRequest : SceneRequest
{
    Scene mergeScene;
};

} // namespace scene

extern scene::Scene *gameScene;
#endif
