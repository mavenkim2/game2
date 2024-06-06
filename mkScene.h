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
// Materials
//
struct MaterialComponent
{
    MaterialFlag flags;
    u32 sid;
    AS_Handle textures[TextureType_Count] = {};

    V4 baseColor        = {1, 1, 1, 1};
    f32 metallicFactor  = 0.f;
    f32 roughnessFactor = 1.f;
};

global const i32 numMaterialSlots     = 512;
global const i32 numMaterialsPerChunk = 512;

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
    Entity entities[numMaterialsPerChunk];
    MaterialChunkNode *next;
    u32 numMaterials;
};

struct MaterialFreeNode
{
    MaterialHandle handle;
    MaterialFreeNode *next;
};

struct MaterialIter
{
    MaterialChunkNode *chunkNode;
    MaterialComponent *material;
    u32 localIndex;
    u32 globalIndex;
};

class MaterialManager
{
private:
    // Hash
    MaterialSlot *nameMap;
    MaterialSlot *entityMap;
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
    inline b8 IsValidHandle(MaterialHandle handle);
    void InsertNameMap(MaterialHandle handle, i32 sid);
    MaterialHandle GetHandleFromNameMap(string name);
    void InsertEntityMap(MaterialHandle handle, Entity entity);
    MaterialHandle GetHandleFromEntityMap(Entity entity);
    MaterialHandle GetFreePosition(MaterialChunkNode **chunkNode, u32 *localIndex);

public:
    struct Scene *parentScene;
    MaterialManager(Scene *inScene);
    MaterialComponent *Create(string name, Entity entity = 0);
    void CreateInternal(struct MaterialCreateRequest *request);
    MaterialComponent *Link(Entity entity, string name);
    // TODO: remove by name/sid?
    b32 Remove(Entity entity);

    inline MaterialComponent *GetFromHandle(MaterialHandle handle);
    MaterialHandle GetHandle(string name);
    MaterialComponent *Get(string name);
    MaterialComponent *Get(Entity entity);

    MaterialIter BeginIter();
    b8 EndIter(MaterialIter *iter);
    void Next(MaterialIter *iter);
    inline MaterialComponent *Get(MaterialIter *iter);
};

//////////////////////////////
// Meshes
//
global const i32 meshesPerChunk = 256;
global const i32 numMeshSlots   = 256;
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
    Mesh meshes[meshesPerChunk];
    Entity entities[meshesPerChunk];
    MeshChunkNode *next;
    u32 count;
};

struct MeshIter
{
    MeshChunkNode *chunkNode;
    Mesh *mesh;
    u32 localIndex;
    u32 globalIndex;
};

class MeshManager
{
private:
    MeshSlot *meshSlots;
    MeshChunkNode *first = 0;
    MeshChunkNode *last  = 0;
    u32 meshWritePos     = 0;
    u32 totalNumMeshes   = 0;

    MeshFreeNode *freeMeshPositions = 0;
    MeshFreeNode *freeMeshNodes     = 0;
    MeshSlotNode *freeMeshSlotNode  = 0;

public:
    struct Scene *parentScene;
    MeshManager(Scene *inScene);
    Mesh *Create(Entity entity);
    inline b8 IsValidHandle(MeshHandle handle);
    void CreateInternal(struct MeshCreateRequest *request);
    b8 Remove(Entity entity);
    Mesh *Get(Entity entity);

    MeshIter BeginIter();
    b8 EndIter(MeshIter *iter);
    void Next(MeshIter *iter);
    inline Mesh *Get(MeshIter *iter);
};

//////////////////////////////
// Hierarchy
//
struct HierarchyComponent
{
    i32 parentId = -1;
    i32 transformIndex;
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
};

struct ComponentRequest
{
    ComponentRequestType type;
};

struct MaterialCreateRequest : ComponentRequest
{
    MaterialComponent material;
    Entity entity;
    u32 sid;
};

struct MeshCreateRequest : ComponentRequest
{
    Mesh mesh;
    Entity entity;
};

//////////////////////////////
// Scene
//
struct Scene
{
    // TicketMutex arenaMutex = {};
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

    // TicketMutex transformMutex = {};
    Mat4 transforms[256];
    std::atomic<u32> transformCount = 0;

    // TicketMutex aabbMutex = {};
    Rect3 aabbs[256];
    u32 aabbCount = 0;

    //////////////////////////////
    // Component requests
    //

    ComponentRequestRing componentRequestRing;
    void ProcessRequests();

    //////////////////////////////
    // Hierarchy
    //
    std::atomic<u32> hierarchyWritePos;
    HierarchyComponent hierarchy[256] = {};

    Scene();
    Entity CreateEntity();
    i32 CreateTransform(Mat4 transform, i32 parent = -1);
};

} // namespace scene

global scene::Scene gameScene;
#endif
