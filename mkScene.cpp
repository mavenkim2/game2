#include "mkCrack.h"
#ifdef LSP_INCLUDE
#include "mkAsset.h"
#include "mkScene.h"
#include "mkMemory.h"
#endif

namespace scene
{
//////////////////////////////
// Small memory allocator
//
void SmallMemoryAllocator::Init()
{
    arena = ArenaAlloc();
    mutex = {};

    i32 start = 16;
    for (u32 i = 0; i < ArrayLength(pools); i++)
    {
        SmallMemoryPool *pool = &pools[i];
        pool->size            = start;
        start *= 2;
        pool->freeList = PushArray(arena, SmallMemoryNode *, platform.NumProcessors());
    }
}

void *SmallMemoryAllocator::Alloc(i32 size)
{
    i32 id = Max(0, GetHighestBit(size) - 3);

    Assert(id < ArrayLength(pools));

    SmallMemoryPool *pool = &pools[id];
    // Assert(pool->allocatedSize + pool->size < pool->totalSize);

    i32 threadIndex = GetThreadIndex();

    if (!pool->freeList[threadIndex])
    {
        MutexScope(&mutex)
        {
            u8 *memory        = PushArray(arena, u8, pool->totalSize);
            i32 incrementSize = sizeof(SmallMemoryNode) + pool->size;

            for (u8 *cursor = memory; cursor < memory + pool->totalSize; cursor += incrementSize)
            {
                SmallMemoryNode *node = (SmallMemoryNode *)cursor;
                node->id              = id;
                StackPush(pool->freeList[threadIndex], node);
            }
        }
    }

    SmallMemoryNode *node = pool->freeList[threadIndex];
    StackPop(pool->freeList[threadIndex]);

    void *result = (node + 1);
    return result;
}

void SmallMemoryAllocator::Free(void *memory)
{
    SmallMemoryNode *node = (SmallMemoryNode *)memory - 1;

    i32 id = node->id;
    Assert(id >= 0 && id < ArrayLength(pools));

    SmallMemoryPool *pool = &pools[id];

    i32 threadIndex = GetThreadIndex();
    StackPush(pool->freeList[threadIndex], node);
}

//////////////////////////////
// Materials
//
void MaterialManager::Init(Scene *inScene)
{
    parentScene           = inScene;
    nameMap               = PushArray(parentScene->arena, MaterialSlot, numMaterialSlots);
    freeSlotNode          = 0;
    first                 = 0;
    last                  = 0;
    totalNumMaterials     = 0;
    materialWritePos      = 0;
    freeMaterialPositions = 0;
    freeMaterialNodes     = 0;
}

MaterialComponent *MaterialManager::Create(u32 sid)
{
    // Get empty position
    MaterialChunkNode *chunkNode = 0;
    u32 localIndex;

    MaterialFreeNode *freeNode = freeMaterialPositions;
    MaterialHandle handle;
    if (freeNode)
    {
        handle = freeNode->handle;
        StackPop(freeMaterialPositions);
        UnpackHandle(handle, &chunkNode, &localIndex);
        StackPush(freeMaterialNodes, freeNode);
    }
    else
    {
        u32 index  = materialWritePos++;
        localIndex = index & materialChunkMask;
        chunkNode  = last;
        if (chunkNode == 0 || localIndex == 0)
        {
            MaterialChunkNode *newChunkNode = PushStruct(parentScene->arena, MaterialChunkNode);
            QueuePush(first, last, newChunkNode);
            chunkNode = newChunkNode;
        }
        handle = CreateHandle(chunkNode, localIndex);
    }

    MaterialSlot *slot         = &nameMap[sid & materialSlotMask];
    MaterialSlotNode *slotNode = freeSlotNode;
    if (slotNode)
    {
        StackPop(freeSlotNode);
    }
    else
    {
        slotNode = PushStruct(parentScene->arena, MaterialSlotNode);
    }

    slotNode->id     = sid;
    slotNode->handle = handle;
    QueuePush(slot->first, slot->last, slotNode);

    MaterialComponent *result = &chunkNode->materials[localIndex];
    result->flags |= MaterialFlag_Valid;
    result->sid = sid;
    totalNumMaterials++;
    return result;
}

MaterialComponent *MaterialManager::Create(string name)
{
    u32 sid                = AddSID(name);
    MaterialComponent *mat = Create(sid);
    return mat;
}

b32 MaterialManager::Remove(string name)
{
    u32 sid    = GetSID(name);
    b32 result = Remove(sid);
    return result;
}

MaterialHandle MaterialManager::RemoveFromNameMap(u32 sid)
{
    MaterialSlot *slot     = &nameMap[sid & materialSlotMask];
    MaterialSlotNode *prev = 0;
    MaterialHandle handle  = {};
    for (MaterialSlotNode *node = slot->first; node != 0; node = node->next)
    {
        if (node->id == sid)
        {
            handle = node->handle;
            if (prev)
            {
                prev->next = node->next;
            }
            else
            {
                slot->first = node->next;
            }
            StackPush(freeSlotNode, node);
            break;
        }
        prev = node;
    }
    return handle;
}

void MaterialManager::RemoveFromList(MaterialHandle handle)
{
    // TODO: gen ids?
    MaterialChunkNode *chunkNode;
    u32 localIndex;
    UnpackHandle(handle, &chunkNode, &localIndex);
    MaterialFreeNode *freeNode = freeMaterialNodes;
    if (freeNode)
    {
        StackPop(freeMaterialNodes);
    }
    else
    {
        freeNode = PushStruct(parentScene->arena, MaterialFreeNode);
    }
    freeNode->handle = handle;
    StackPush(freeMaterialPositions, freeNode);
}

b32 MaterialManager::Remove(u32 sid)
{
    b32 result            = 0;
    MaterialHandle handle = RemoveFromNameMap(sid);
    if (IsValidHandle(handle))
    {
        // Remove from name map
        totalNumMaterials--;
        MaterialComponent *material = GetFromHandle(handle);
        material->flags             = material->flags & ~MaterialFlag_Valid;
        RemoveFromList(handle);
        result = 1;
    }
    return result;
}

b32 MaterialManager::Remove(MaterialHandle handle)
{
    b32 result = 0;
    if (IsValidHandle(handle))
    {
        totalNumMaterials--;
        MaterialComponent *material = GetFromHandle(handle);
        RemoveFromNameMap(material->sid);
        material->flags = material->flags & ~MaterialFlag_Valid;
        RemoveFromList(handle);
        result = 1;
    }
    return result;
}

MaterialComponent *MaterialManager::Get(string name)
{
    MaterialHandle handle     = GetHandle(name);
    MaterialComponent *result = GetFromHandle(handle);
    return result;
}

MaterialIter MaterialManager::BeginIter()
{
    MaterialIter iter;
    iter.localIndex  = 0;
    iter.globalIndex = 0;
    iter.chunkNode   = first;
    iter.material    = iter.chunkNode ? &first->materials[0] : 0;
    return iter;
}

b8 MaterialManager::EndIter(MaterialIter *iter)
{
    b8 result = 0;
    if (iter->globalIndex >= materialWritePos)
    {
        result = 1;
    }
    return result;
}

void MaterialManager::Next(MaterialIter *iter)
{
    b32 valid = 0;
    do
    {
        if (++iter->localIndex == numMaterialsPerChunk)
        {
            iter->localIndex = 0;
            iter->chunkNode  = iter->chunkNode->next;
            Assert(iter->chunkNode);
        }
        MaterialComponent *material = &iter->chunkNode->materials[iter->localIndex];
        valid                       = HasFlags(material->flags, MaterialFlag_Valid);
        iter->material              = material;
        iter->globalIndex++;
    } while (iter->globalIndex < materialWritePos && !valid);
}

inline MaterialComponent *MaterialManager::Get(MaterialIter *iter)
{
    MaterialComponent *material = iter->material;
    return material;
}

//////////////////////////////
// Meshes
//

void MeshManager::Init(Scene *inScene)
{
    parentScene    = inScene;
    meshSlots      = PushArray(parentScene->arena, MeshSlot, numMeshSlots);
    first          = 0;
    last           = 0;
    meshWritePos   = 0;
    totalNumMeshes = 0;
    freePositions  = 0;
    freeNodes      = 0;
    freeSlotNodes  = 0;
}

Mesh *MeshManager::Create(Entity entity)
{
    MeshChunkNode *chunkNode;
    u32 localIndex;

    MeshFreeNode *freeNode = freePositions;
    MeshHandle handle;
    if (freeNode)
    {
        StackPop(freePositions);
        handle = freeNode->handle;
        UnpackHandle(handle, &chunkNode, &localIndex);
        StackPush(freeNodes, freeNode);
    }
    else
    {
        u32 index  = meshWritePos++;
        localIndex = index & meshChunkMask;
        chunkNode  = last;

        if (chunkNode == 0 || localIndex == 0)
        {
            // NOTE: if a struct is initialized w/ an array, it won't have the default values set
            MeshChunkNode *newChunkNode = PushStruct(parentScene->arena, MeshChunkNode);
            QueuePush(first, last, newChunkNode);
            chunkNode = newChunkNode;
        }
        handle = CreateHandle(chunkNode, localIndex);
    }

    chunkNode->entities[localIndex] = entity;

    // Add to hash
    MeshSlot *slot         = &meshSlots[entity & meshSlotMask];
    MeshSlotNode *slotNode = freeSlotNodes;
    if (slotNode)
    {
        StackPop(freeSlotNodes);
    }
    else
    {
        slotNode = PushStruct(parentScene->arena, MeshSlotNode);
    }
    slotNode->handle = handle;
    slotNode->entity = entity;
    QueuePush(slot->first, slot->last, slotNode);
    totalNumMeshes++;

    Mesh *result = &chunkNode->meshes[localIndex];
    result->flags |= MeshFlags_Valid;

    return result;
}

b8 MeshManager::Remove(Entity entity)
{
    b8 result      = 0;
    MeshSlot *slot = &meshSlots[entity & meshSlotMask];

    MeshHandle handle  = {};
    MeshSlotNode *prev = 0;
    for (MeshSlotNode *node = slot->first; node != 0; node = node->next)
    {
        if (node->entity == entity)
        {
            handle = node->handle;
            if (prev)
            {
                prev->next = node->next;
            }
            else
            {
                slot->first = node->next;
            }
            StackPush(freeSlotNodes, node);
            break;
        }
        prev = node;
    }

    if (IsValidHandle(handle))
    {
        totalNumMeshes--;
        result = 1;

        MeshChunkNode *chunkNode;
        u32 localIndex;
        UnpackHandle(handle, &chunkNode, &localIndex);

        MeshFreeNode *freeNode = freeNodes;
        if (freeNode)
        {
            StackPop(freeNodes);
        }
        else
        {
            freeNode = PushStruct(parentScene->arena, MeshFreeNode);
        }
        freeNode->handle = handle;
        StackPush(freePositions, freeNode);

        chunkNode->meshes[localIndex].flags &= ~MeshFlags_Valid;
    }
    return result;
}

Mesh *MeshManager::Get(Entity entity)
{
    MeshSlot *slot    = &meshSlots[entity & meshSlotMask];
    MeshHandle handle = {};
    for (MeshSlotNode *node = slot->first; node != 0; node = node->next)
    {
        if (node->entity == entity)
        {
            handle = node->handle;
            break;
        }
    }

    Mesh *mesh = GetFromHandle(handle);
    return mesh;
}

MeshIter MeshManager::BeginIter()
{
    MeshIter iter;
    iter.localIndex  = 0;
    iter.globalIndex = 0;
    iter.chunkNode   = first;
    iter.mesh        = iter.chunkNode ? &first->meshes[0] : 0;
    return iter;
}

b8 MeshManager::EndIter(MeshIter *iter)
{
    b8 result = 0;
    if (iter->globalIndex >= meshWritePos)
    {
        result = 1;
    }
    return result;
}

void MeshManager::Next(MeshIter *iter)
{
    b32 valid = 0;
    do
    {
        if (++iter->localIndex == numMeshesPerChunk)
        {
            iter->localIndex = 0;
            iter->chunkNode  = iter->chunkNode->next;
            Assert(iter->chunkNode);
        }
        Mesh *mesh = &iter->chunkNode->meshes[iter->localIndex];
        valid      = HasFlags(mesh->flags, MeshFlags_Valid);
        iter->mesh = mesh;
        iter->globalIndex++;
    } while (iter->globalIndex < meshWritePos && !valid);
}

inline Mesh *MeshManager::Get(MeshIter *iter)
{
    Mesh *mesh = iter->mesh;
    return mesh;
}

inline Entity MeshManager::GetEntity(MeshIter *iter)
{
    Entity entity = iter->chunkNode->entities[iter->localIndex];
    return entity;
}

//////////////////////////////
// Transforms
//
void TransformManager::Init(Scene *inScene)
{
    parentScene        = inScene;
    transformSlots     = PushArray(parentScene->arena, TransformSlot, numTransformSlots);
    first              = 0;
    last               = 0;
    transformWritePos  = 1;
    totalNumTransforms = 0;
    numChunkNodes      = 0;
    freeSlotNodes      = 0;
    freePositions      = 0;
    freeNodes          = 0;
}

Mat4 *TransformManager::Create(Entity entity)
{
    TransformChunkNode *chunkNode;
    u32 localIndex;

    TransformFreeNode *freeNode = freePositions;
    TransformHandle handle      = {};
    if (freeNode)
    {
        StackPop(freePositions);
        handle = freeNode->handle;
        UnpackHandle(handle, &chunkNode, &localIndex);
        StackPush(freeNodes, freeNode);
    }
    else
    {
        u32 index  = transformWritePos++;
        localIndex = index & transformChunkMask;
        chunkNode  = last;

        if (chunkNode == 0 || localIndex == 0)
        {
            TransformChunkNode *newChunkNode = PushStruct(parentScene->arena, TransformChunkNode);
            QueuePush(first, last, newChunkNode);
            chunkNode = newChunkNode;
            numChunkNodes++;
        }
        handle = CreateHandle(chunkNode, localIndex, numChunkNodes - 1);
    }

    chunkNode->entities[localIndex] = entity;

    // Add to hash
    TransformSlot *slot         = &transformSlots[entity & transformSlotMask];
    TransformSlotNode *slotNode = freeSlotNodes;
    if (slotNode)
    {
        StackPop(freeSlotNodes);
    }
    else
    {
        slotNode = PushStruct(parentScene->arena, TransformSlotNode);
    }
    slotNode->handle = handle;
    slotNode->entity = entity;
    QueuePush(slot->first, slot->last, slotNode);
    totalNumTransforms++;

    Mat4 *result = &chunkNode->transforms[localIndex];
    return result;
}

b32 TransformManager::Remove(Entity entity)
{
    b32 result          = 0;
    TransformSlot *slot = &transformSlots[entity & transformSlotMask];

    TransformHandle handle  = {};
    TransformSlotNode *prev = 0;
    for (TransformSlotNode *node = slot->first; node != 0; node = node->next)
    {
        if (node->entity == entity)
        {
            handle = node->handle;
            if (prev)
            {
                prev->next = node->next;
            }
            else
            {
                slot->first = node->next;
            }
            StackPush(freeSlotNodes, node);
            break;
        }
        prev = node;
    }

    if (IsValidHandle(handle))
    {
        totalNumTransforms--;
        result = 1;
        TransformChunkNode *chunkNode;
        u32 localIndex;
        UnpackHandle(handle, &chunkNode, &localIndex);

        TransformFreeNode *freeNode = freeNodes;
        if (freeNode)
        {
            StackPop(freeNodes);
        }
        else
        {
            freeNode = PushStruct(parentScene->arena, TransformFreeNode);
        }
        freeNode->handle = handle;
        StackPush(freePositions, freeNode);
    }
    return result;
}

inline TransformHandle TransformManager::GetHandle(Entity entity)
{
    TransformSlot *slot    = &transformSlots[entity & transformSlotMask];
    TransformHandle handle = {};
    for (TransformSlotNode *node = slot->first; node != 0; node = node->next)
    {
        if (node->entity == entity)
        {
            handle = node->handle;
            break;
        }
    }
    return handle;
}

inline u32 TransformManager::GetIndex(Entity entity)
{
    TransformHandle handle = GetHandle(entity);
    u32 index              = GetIndex(handle);
    return index;
}

inline Mat4 *TransformManager::Get(Entity entity)
{
    TransformHandle handle = GetHandle(entity);
    Mat4 *transform        = GetFromHandle(handle);
    return transform;
}

//////////////////////////////
// Hierarchy Component
//
void HierarchyManager::Init(Scene *inScene)
{
    parentScene            = inScene;
    hierarchySlots         = PushArray(parentScene->arena, HierarchySlot, numHierarchyNodeSlots);
    first                  = 0;
    last                   = 0;
    hierarchyWritePos      = 1;
    totalNumHierarchyNodes = 0;
    freeSlotNodes          = 0;
    freePositions          = 0;
    freeNodes              = 0;
}

HierarchyComponent *HierarchyManager::Get(Entity entity)
{
    HierarchyHandle handle     = GetHandle(entity);
    HierarchyComponent *result = GetFromHandle(handle);
    return result;
}

HierarchyHandle HierarchyManager::GetHandle(Entity entity)
{
    HierarchySlot *slot    = &hierarchySlots[entity & hierarchyNodeSlotMask];
    HierarchyHandle handle = {};

    if (entity != 0)
    {
        for (HierarchySlotNode *node = slot->first; node != 0; node = node->next)
        {
            if (node->entity == entity)
            {
                handle = node->handle;
                break;
            }
        }
    }
    return handle;
}

void HierarchyManager::Create(Entity entity, Entity parent)
{
    HierarchyChunkNode *chunkNode;
    HierarchyComponent *node;
    u32 localIndex;

    HierarchyFreeNode *freeNode = freePositions;
    HierarchyHandle handle      = {};
    if (freeNode)
    {
        StackPop(freePositions);
        handle = freeNode->handle;
        UnpackHandle(handle, &node, &chunkNode);
        StackPush(freeNodes, freeNode);

        localIndex = GetLocalIndex(handle);
    }
    else
    {
        u32 index  = hierarchyWritePos++;
        localIndex = index & hierarchyNodeChunkMask;
        chunkNode  = last;

        if (chunkNode == 0 || localIndex == 0)
        {
            HierarchyChunkNode *newChunkNode = PushStruct(parentScene->arena, HierarchyChunkNode);
            QueuePush(first, last, newChunkNode);
            chunkNode = newChunkNode;
        }
        handle = CreateHandle(&chunkNode->components[0], chunkNode);
    }
    chunkNode->entities[localIndex] = entity;

    // Add to hash
    HierarchySlot *slot         = &hierarchySlots[entity & hierarchyNodeSlotMask];
    HierarchySlotNode *slotNode = freeSlotNodes;
    if (slotNode)
    {
        StackPop(freeSlotNodes);
    }
    else
    {
        slotNode = PushStruct(parentScene->arena, HierarchySlotNode);
    }
    slotNode->handle = handle;
    slotNode->entity = entity;
    QueuePush(slot->first, slot->last, slotNode);
    totalNumHierarchyNodes++;

    HierarchyComponent *result = &chunkNode->components[localIndex];
    result->parent             = parent;
    result->flags |= HierarchyFlag_Valid;
}

b32 HierarchyManager::Remove(Entity entity)
{
    b32 result          = 0;
    HierarchySlot *slot = &hierarchySlots[entity & hierarchyNodeSlotMask];

    HierarchyHandle handle  = {};
    HierarchySlotNode *prev = 0;
    for (HierarchySlotNode *node = slot->first; node != 0; node = node->next)
    {
        if (node->entity == entity)
        {
            handle = node->handle;
            if (prev)
            {
                prev->next = node->next;
            }
            else
            {
                slot->first = node->next;
            }
            StackPush(freeSlotNodes, node);
            break;
        }
        prev = node;
    }

    if (IsValidHandle(handle))
    {
        totalNumHierarchyNodes--;
        result = 1;
        HierarchyComponent *node;
        HierarchyChunkNode *chunkNode;
        UnpackHandle(handle, &node, &chunkNode);

        u32 localIndex = GetLocalIndex(handle);

        HierarchyFreeNode *freeNode = freeNodes;
        if (freeNode)
        {
            StackPop(freeNodes);
        }
        else
        {
            freeNode = PushStruct(parentScene->arena, HierarchyFreeNode);
        }
        freeNode->handle = handle;
        StackPush(freePositions, freeNode);
        node->flags &= ~HierarchyFlag_Valid;
    }
    return result;
}

HierarchyIter HierarchyManager::BeginIter()
{
    HierarchyIter iter;
    iter.localIndex  = 1;
    iter.globalIndex = 1;
    iter.chunkNode   = first;
    iter.node        = iter.chunkNode ? &first->components[1] : 0;
    return iter;
}

b8 HierarchyManager::EndIter(HierarchyIter *iter)
{
    b8 result = 0;
    if (iter->globalIndex >= hierarchyWritePos)
    {
        result = 1;
    }
    return result;
}

void HierarchyManager::Next(HierarchyIter *iter)
{
    b32 valid = 0;
    do
    {
        if (++iter->localIndex == numHierarchyNodesPerChunk)
        {
            iter->localIndex = 0;
            iter->chunkNode  = iter->chunkNode->next;
            Assert(iter->chunkNode);
        }
        HierarchyComponent *node = &iter->chunkNode->components[iter->localIndex];
        valid                    = HasFlags(node->flags, HierarchyFlag_Valid);
        iter->node               = node;
        iter->globalIndex++;
    } while (iter->globalIndex < hierarchyWritePos && !valid);
}

inline HierarchyComponent *HierarchyManager::Get(HierarchyIter *iter)
{
    HierarchyComponent *node = iter->node;
    return node;
}

inline Entity HierarchyManager::GetEntity(HierarchyIter *iter)
{
    Entity entity = iter->chunkNode->entities[iter->localIndex];
    return entity;
}

//////////////////////////////
// Skeletons
//
void SkeletonManager::Init(Scene *inScene)
{
    parentScene       = inScene;
    entityMap         = PushArray(parentScene->arena, SkeletonSlot, numSkeletonSlots);
    nameMap           = PushArray(parentScene->arena, SkeletonSlot, numSkeletonSlots);
    first             = 0;
    last              = 0;
    skeletonWritePos  = 0;
    totalNumSkeletons = 0;
    freeSlotNodes     = 0;
    freePositions     = 0;
    freeNodes         = 0;
}

SkeletonHandle SkeletonManager::Get(SkeletonSlot *map, u32 id)
{
    SkeletonSlot *slot    = &map[id & skeletonSlotMask];
    SkeletonHandle handle = {};
    for (SkeletonSlotNode *node = slot->first; node != 0; node = node->next)
    {
        if (node->id == id)
        {
            handle = node->handle;
            break;
        }
    }
    return handle;
}

void SkeletonManager::Insert(SkeletonSlot *map, u32 id, SkeletonHandle handle)
{
    SkeletonSlot *slot         = &map[id & skeletonSlotMask];
    SkeletonSlotNode *slotNode = freeSlotNodes;
    if (slotNode)
    {
        StackPop(freeSlotNodes);
    }
    else
    {
        slotNode = PushStruct(parentScene->arena, SkeletonSlotNode);
    }
    slotNode->handle = handle;
    slotNode->id     = id;
    QueuePush(slot->first, slot->last, slotNode);
}

SkeletonHandle SkeletonManager::Remove(SkeletonSlot *map, u32 id)
{
    SkeletonHandle handle  = {};
    SkeletonSlotNode *prev = 0;
    SkeletonSlot *slot     = &map[id & skeletonSlotMask];
    for (SkeletonSlotNode *node = slot->first; node != 0; node = node->next)
    {
        if (node->id == id)
        {
            handle = node->handle;
            if (prev)
            {
                prev->next = node->next;
            }
            else
            {
                slot->first = node->next;
            }
            StackPush(freeSlotNodes, node);
            break;
        }
        prev = node;
    }
    return handle;
}

LoadedSkeleton *SkeletonManager::Create(u32 sid)
{
    SkeletonChunkNode *chunkNode;
    u32 localIndex;

    SkeletonFreeNode *freeNode = freePositions;
    SkeletonHandle handle      = {};
    if (freeNode)
    {
        StackPop(freePositions);
        handle = freeNode->handle;
        UnpackHandle(handle, &chunkNode, &localIndex);
        StackPush(freeNodes, freeNode);
    }
    else
    {
        u32 index  = skeletonWritePos++;
        localIndex = index & skeletonChunkMask;
        chunkNode  = last;

        if (chunkNode == 0 || localIndex == 0)
        {
            SkeletonChunkNode *newChunkNode = PushStruct(parentScene->arena, SkeletonChunkNode);
            QueuePush(first, last, newChunkNode);
            chunkNode = newChunkNode;
        }
        handle = CreateHandle(chunkNode, localIndex);
    }

    // Add to name hash
    Insert(nameMap, sid, handle);
    totalNumSkeletons++;

    LoadedSkeleton *skeleton = &chunkNode->skeletons[localIndex];
    skeleton->sid            = sid;

    chunkNode->gen[localIndex] |= SkeletonFlag_Valid;
    return skeleton;
}

LoadedSkeleton *SkeletonManager::Create(string name)
{
    u32 sid                = AddSID(name);
    LoadedSkeleton *result = Create(sid);
    return result;
}

void SkeletonManager::Link(Entity entity, SkeletonHandle handle)
{
    Insert(entityMap, entity, handle);
}

b32 SkeletonManager::Remove(string name)
{
    u32 sid               = GetSID(name);
    b32 result            = 0;
    SkeletonHandle handle = Remove(nameMap, sid);

    if (IsValidHandle(handle))
    {
        totalNumSkeletons--;
        result = 1;
        SkeletonChunkNode *chunkNode;
        u32 localIndex;
        UnpackHandle(handle, &chunkNode, &localIndex);

        SkeletonFreeNode *freeNode = freeNodes;
        if (freeNode)
        {
            StackPop(freeNodes);
        }
        else
        {
            freeNode = PushStruct(parentScene->arena, SkeletonFreeNode);
        }
        handle           = IncrementGen(handle);
        freeNode->handle = handle;
        StackPush(freePositions, freeNode);

        chunkNode->gen[localIndex]++;
        chunkNode->gen[localIndex] &= ~SkeletonFlag_Valid;
    }
    return result;
}

// NOTE: removes the entity mapping if the gen id is outdated
inline SkeletonHandle SkeletonManager::GetHandleFromEntity(Entity entity)
{
    SkeletonHandle handle = Get(entityMap, entity);
    if (!IsValidHandle(handle))
    {
        Remove(entityMap, entity);
        handle = {};
    }
    return handle;
}

inline SkeletonHandle SkeletonManager::GetHandleFromSid(u32 sid)
{
    SkeletonHandle handle = Get(nameMap, sid);
    return handle;
}

inline SkeletonHandle SkeletonManager::GetHandleFromName(string name)
{
    u32 sid               = GetSID(name);
    SkeletonHandle handle = Get(nameMap, sid);
    return handle;
}

inline LoadedSkeleton *SkeletonManager::GetFromEntity(Entity entity)
{
    SkeletonHandle handle  = GetHandleFromEntity(entity);
    LoadedSkeleton *result = GetFromHandle(handle);
    return result;
}

inline LoadedSkeleton *SkeletonManager::GetFromSid(u32 sid)
{
    SkeletonHandle handle  = GetHandleFromSid(sid);
    LoadedSkeleton *result = GetFromHandle(handle);
    return result;
}

SkeletonIter SkeletonManager::BeginIter()
{
    SkeletonIter iter;
    iter.localIndex  = 0;
    iter.globalIndex = 0;
    iter.chunkNode   = first;
    iter.skeleton    = iter.chunkNode ? &first->skeletons[0] : 0;
    return iter;
}

b8 SkeletonManager::EndIter(SkeletonIter *iter)
{
    b8 result = 0;
    if (iter->globalIndex >= skeletonWritePos)
    {
        result = 1;
    }
    return result;
}

void SkeletonManager::Next(SkeletonIter *iter)
{
    b32 valid = 0;
    do
    {
        if (++iter->localIndex == numSkeletonPerChunk)
        {
            iter->localIndex = 0;
            iter->chunkNode  = iter->chunkNode->next;
            Assert(iter->chunkNode);
        }
        LoadedSkeleton *skeleton = &iter->chunkNode->skeletons[iter->localIndex];
        valid                    = HasFlags(iter->chunkNode->gen[iter->localIndex], SkeletonFlag_Valid);
        iter->skeleton           = skeleton;
        iter->globalIndex++;
    } while (iter->globalIndex < skeletonWritePos && !valid);
}

inline LoadedSkeleton *SkeletonManager::Get(SkeletonIter *iter)
{
    LoadedSkeleton *skel = iter->skeleton;
    return skel;
}

//////////////////////////////
// Component requests
//
void SceneRequestRing::Init(Arena *arena)
{
    ringBuffer = PushArray(arena, u8, totalSize);
}

void *SceneRequestRing::Alloc(u64 size)
{
    void *result = 0;
    u64 align    = alignment;
    for (;;)
    {
        u64 alignedSize = size;
        alignedSize     = AlignPow2(alignedSize, align);

        u64 loadedWritePos = writePos.load();
        u64 loadedReadPos  = readPos;

        u64 ringWritePos = loadedWritePos & (totalSize - 1);

        u64 remainder = totalSize - ringWritePos;
        if (remainder >= alignedSize)
        {
            remainder = 0;
        }

        u64 totalAllocSize = remainder + alignedSize;

        u64 availableSpace = totalSize - (loadedWritePos - loadedReadPos);
        if (availableSpace >= totalAllocSize)
        {
            if (writePos.compare_exchange_weak(loadedWritePos, loadedWritePos + totalAllocSize))
            {
                u64 offset = loadedWritePos & (totalSize - 1);
                if (remainder != 0)
                {
                    MemoryZero(ringBuffer + offset, remainder);
                    offset = 0;
                }
                result = ringBuffer + offset;
                break;
            }
        }
    }
    return result;
}

void SceneRequestRing::EndAlloc(SceneRequest *req)
{
    req->finished = 1;
    std::atomic_thread_fence(std::memory_order_release);
}

SceneMergeTicket::~SceneMergeTicket()
{
    if (initialized) ring->EndAlloc(request);
}
Scene *SceneMergeTicket::GetScene()
{
    initialized = 1;
    return &request->mergeScene;
}

SceneMergeTicket SceneRequestRing::CreateMergeRequest()
{
    SceneMergeRequest *request = (SceneMergeRequest *)Alloc(sizeof(SceneMergeRequest));
    request->type              = SceneRequestType_MergeScene;
    request->finished          = 0;

    Arena *arena = ArenaAlloc();
    request->mergeScene.Init(arena);

    SceneMergeTicket ticket;
    ticket.request = request;
    ticket.ring    = this;
    return ticket;
}

// NOTE: synchronous
void SceneRequestRing::ProcessRequests(Scene *parent)
{
    u64 loadedWritePos = writePos.load();

    const u32 ringMask = totalSize - 1;
    while (readPos < loadedWritePos)
    {
        u64 ringReadPos       = readPos & ringMask;
        SceneRequest *request = (SceneRequest *)(ringBuffer + ringReadPos);
        u64 advance           = 0;
        if (!request->finished) break;

        switch (request->type)
        {
            case SceneRequestType_Reset:
            {
                advance = totalSize - ringReadPos;
            }
            break;
            case SceneRequestType_MergeScene:
            {
                SceneMergeRequest *mergeReq = (SceneMergeRequest *)request;
                parent->Merge(&mergeReq->mergeScene);
                advance = AlignPow2(sizeof(SceneMergeRequest), (u64)alignment);
            }
            break;
            default: Assert(0);
        }
        readPos += advance;
    }
}

//////////////////////////////
// Scene
//

void Scene::Init(Arena *inArena)
{
    arena = inArena;
    materials.Init(this);
    meshes.Init(this);
    transforms.Init(this);
    hierarchy.Init(this);
    skeletons.Init(this);
    sma.Init();
    requestRing.Init(inArena);
    entityGen = NULL_HANDLE + 1;
}

Entity Scene::CreateEntity()
{
    return entityGen.fetch_add(1);
}

void Scene::Merge(Scene *other)
{
    // Merge materials
    for (MaterialIter iter = other->BeginMatIter(); !other->End(&iter); other->Next(&iter))
    {
        MaterialComponent *mat = other->Get(&iter);
        u32 sid                = mat->sid;

        MaterialComponent *newMat = materials.Create(sid);
        *newMat                   = std::move(*mat);
    }

    // Merge skeletons
    for (SkeletonIter iter = other->BeginSkelIter(); !other->End(&iter); other->Next(&iter))
    {
        LoadedSkeleton *skel = other->Get(&iter);
        u32 sid              = skel->sid;

        LoadedSkeleton *newSkel = skeletons.Create(sid);
        *newSkel                = std::move(*skel);
    }

    // Maps old parent id to new parent id
    TempArena temp = ScratchStart(0, 0);
    u32 *entityMap = PushArrayNoZero(temp.arena, u32, other->entityGen.load());
    entityMap[0]   = 0;
    // Merge hierarchy
    for (HierarchyIter iter = other->BeginHierIter(); !other->End(&iter); other->Next(&iter))
    {
        HierarchyComponent *hier = other->Get(&iter);
        Entity oldEntity         = other->GetEntity(&iter);
        Entity newEntity         = CreateEntity();
        entityMap[oldEntity]     = newEntity;

        Mat4 *transform = other->transforms.Get(oldEntity);
        Assert(transform && hier);

        Mat4 *newTransform = transforms.Create(newEntity);
        *newTransform      = std::move(*transform);

        u32 newParent = entityMap[hier->parent];
        hierarchy.Create(newEntity, newParent);
    }

    // Merge the meshes
    Assert(other->meshes.GetTotal() == other->meshes.GetEndPos());
    for (MeshIter iter = other->BeginMeshIter(); !other->End(&iter); other->Next(&iter))
    {
        Mesh *mesh       = other->Get(&iter);
        Entity oldEntity = other->GetEntity(&iter);

        Entity newEntity = entityMap[oldEntity];
        Mesh *newMesh    = meshes.Create(newEntity);
        *newMesh         = std::move(*mesh); // does this work?

        // Remap materials
        {
            for (u32 subsetIndex = 0; subsetIndex < mesh->numSubsets; subsetIndex++)
            {
                Mesh::MeshSubset *subset = &mesh->subsets[subsetIndex];
                MaterialComponent *mat   = other->materials.GetFromHandle(subset->materialHandle);
                Assert(mat);
                u32 sid               = mat->sid;
                MaterialHandle handle = materials.GetHandle(sid);
                Assert(materials.IsValidHandle(handle));
                newMesh->subsets[subsetIndex].materialHandle = handle;
            }
        }
        // Remap skeletons
        {
            LoadedSkeleton *skel = other->skeletons.GetFromEntity(oldEntity);
            if (skel)
            {
                u32 sid               = skel->sid;
                SkeletonHandle handle = skeletons.GetHandleFromSid(sid);
                skeletons.Link(newEntity, handle);
            }
        }
    }

    ArenaRelease(other->arena);
}

inline void Scene::CreateTransform(Mat4 transform, Entity entity, Entity parent)
{
    Mat4 *t = transforms.Create(entity);
    *t      = transform;

    hierarchy.Create(entity, parent);
}

// TODO: instead of having individual requests, the resources of the model file should probably just be all loaded and joined,
void Scene::ProcessRequests()
{
    requestRing.ProcessRequests(this);
}

} // namespace scene
