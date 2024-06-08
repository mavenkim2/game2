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
    Assert(pool->allocatedSize + pool->size < pool->totalSize);

    i32 threadIndex           = GetThreadIndex();
    SmallMemoryNode *freeList = pool->freeList[threadIndex];

    if (freeList)
    {
        MutexScope(&mutex)
        {
            u8 *memory        = PushArray(arena, u8, pool->totalSize);
            i32 incrementSize = sizeof(SmallMemoryNode) + pool->size;

            for (u8 *cursor = memory; cursor < memory + pool->totalSize; cursor += incrementSize)
            {
                SmallMemoryNode *node = (SmallMemoryNode *)cursor;
                node->id              = id;
                StackPush(freeList, node);
            }
        }
    }

    SmallMemoryNode *node = freeList;
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
// Tickets
//

Mesh *MeshTicket::GetMesh()
{
    return &request->mesh;
}

MeshTicket::~MeshTicket()
{
    parentScene->componentRequestRing.EndAlloc(request);
}

MaterialComponent *MaterialTicket::GetMaterial()
{
    return &request->material;
}

MaterialTicket::~MaterialTicket()
{
    parentScene->componentRequestRing.EndAlloc(request);
}

Mat4 *TransformTicket::GetTransform()
{
    return &request->transform;
}

TransformTicket::~TransformTicket()
{
    parentScene->componentRequestRing.EndAlloc(request);
}

LoadedSkeleton *SkeletonTicket::GetSkeleton()
{
    return &request->skeleton;
}

SkeletonTicket::~SkeletonTicket()
{
    parentScene->componentRequestRing.EndAlloc(request);
}

//////////////////////////////
// Materials
//
MaterialManager::MaterialManager(Scene *inScene)
{
    parentScene = inScene;
    nameMap     = PushArray(parentScene->arena, MaterialSlot, numMaterialSlots);
}

void MaterialManager::InsertNameMap(MaterialHandle handle, i32 sid)
{
    MaterialSlot *slot         = &nameMap[sid & materialSlotMask];
    MaterialSlotNode *freeNode = freeSlotNode;
    if (freeNode)
    {
        StackPop(freeSlotNode);
    }
    else
    {
        freeNode = PushStruct(parentScene->arena, MaterialSlotNode);
    }

    freeNode->id     = sid;
    freeNode->handle = handle;
    QueuePush(slot->first, slot->last, freeNode);
}

MaterialHandle MaterialManager::GetHandleFromNameMap(string name)
{
    u32 sid               = GetSID(name);
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

MaterialHandle MaterialManager::GetFreePosition(MaterialChunkNode **chunkNode, u32 *localIndex)
{
    MaterialChunkNode *resultChunkNode;
    u32 resultLocalIndex;

    MaterialFreeNode *freeNode = freeMaterialPositions;
    MaterialHandle handle;
    if (freeNode)
    {
        handle = freeNode->handle;
        StackPop(freeMaterialPositions);
        UnpackHandle(handle, &resultChunkNode, &resultLocalIndex);
        StackPush(freeMaterialNodes, freeNode);
    }
    else
    {
        u32 index        = materialWritePos++;
        resultLocalIndex = index & materialChunkMask;
        resultChunkNode  = last;
        if (resultChunkNode == 0 || resultLocalIndex == 0)
        {
            MaterialChunkNode *newChunkNode = PushStruct(parentScene->arena, MaterialChunkNode);
            QueuePush(first, last, newChunkNode);
            resultChunkNode = newChunkNode;
        }
        handle = CreateHandle(resultChunkNode, resultLocalIndex);
    }
    *chunkNode  = resultChunkNode;
    *localIndex = resultLocalIndex;
    return handle;
}

struct MaterialScope
{
    MaterialCreateRequest *request;
    MaterialManager *manager;
    ~MaterialScope()
    {
        manager->parentScene->componentRequestRing.EndAlloc(request);
    }
};

MaterialTicket MaterialManager::Create(string name)
{
    MaterialCreateRequest *request = (MaterialCreateRequest *)parentScene->componentRequestRing.Alloc(sizeof(MaterialCreateRequest));
    request->type                  = ComponentRequestType_CreateMaterial;
    i32 sid                        = AddSID(name);
    request->sid                   = sid;

    MaterialTicket ticket;
    ticket.parentScene = parentScene;
    ticket.request     = request;
    return ticket;
}

void MaterialManager::CreateInternal(MaterialCreateRequest *request)
{
    MaterialComponent *component = 0;
    Assert(request->type == ComponentRequestType_CreateMaterial);

    u32 sid = request->sid;

    // Get empty position
    MaterialChunkNode *chunkNode = 0;
    u32 localIndex;
    MaterialHandle handle = GetFreePosition(&chunkNode, &localIndex);

    InsertNameMap(handle, sid);

    MaterialComponent *result = &chunkNode->materials[localIndex];
    *result                   = request->material;
    result->flags |= MaterialFlag_Valid;
    result->sid = sid;
    totalNumMaterials++;
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
    //
    MaterialChunkNode *chunkNode;
    u32 localIndex;
    UnpackHandle(handle, &chunkNode, &localIndex);
    // If the last position is freed, decrement material write pos
    if (chunkNode == last && ((materialWritePos & materialChunkMask) == localIndex))
    {
        materialWritePos--;
    }
    else
    {
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

MaterialHandle MaterialManager::GetHandle(string name)
{
    MaterialHandle handle = GetHandleFromNameMap(name);
    return handle;
}

MaterialComponent *MaterialManager::Get(string name)
{
    MaterialHandle handle     = GetHandleFromNameMap(name);
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
        MaterialComponent *material = &iter->chunkNode->materials[iter->localIndex++];
        valid                       = HasFlags(material->flags, MaterialFlag_Valid);
        iter->material              = material;
        iter->globalIndex++;
        if (iter->localIndex == numMaterialsPerChunk)
        {
            Assert(iter->chunkNode);
            iter->localIndex = 0;
            iter->chunkNode  = iter->chunkNode->next;
        }
    } while (!valid);
}

inline MaterialComponent *MaterialManager::Get(MaterialIter *iter)
{
    MaterialComponent *material = iter->material;
    return material;
}

//////////////////////////////
// Meshes
//

MeshManager::MeshManager(Scene *inScene)
{
    parentScene = inScene;
    meshSlots   = PushArray(parentScene->arena, MeshSlot, numMeshSlots);
}

MeshTicket MeshManager::Create(Entity entity)
{
    MeshCreateRequest *request = (MeshCreateRequest *)parentScene->componentRequestRing.Alloc(sizeof(MeshCreateRequest));

    request->type   = ComponentRequestType_CreateMesh;
    request->entity = entity;

    MeshTicket ticket;
    ticket.request     = request;
    ticket.parentScene = parentScene;
    return ticket;
}

void MeshManager::CreateInternal(MeshCreateRequest *request)
{
    u32 entity = request->entity;
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
    *result      = request->mesh;
    result->flags |= MeshFlags_Valid;
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

        if (chunkNode == last && ((meshWritePos & meshChunkMask) == localIndex))
        {
            meshWritePos--;
        }
        else
        {
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
        }

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
        Mesh *mesh = &iter->chunkNode->meshes[iter->localIndex++];
        valid      = mesh->IsValid();
        iter->mesh = mesh;
        iter->globalIndex++;
        if (iter->localIndex == numMeshesPerChunk)
        {
            Assert(iter->chunkNode);
            iter->localIndex = 0;
            iter->chunkNode  = iter->chunkNode->next;
        }
    } while (!valid);
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
TransformManager::TransformManager(Scene *inScene)
{
    parentScene    = inScene;
    transformSlots = PushArray(parentScene->arena, TransformSlot, numTransformSlots);
}

TransformTicket TransformManager::Create(Entity entity)
{
    TransformCreateRequest *request = (TransformCreateRequest *)parentScene->componentRequestRing.Alloc(sizeof(TransformCreateRequest));

    request->type   = ComponentRequestType_CreateTransform;
    request->entity = entity;

    TransformTicket ticket;
    ticket.request     = request;
    ticket.parentScene = parentScene;
    return ticket;
}

TransformHandle TransformManager::CreateInternal(struct TransformCreateRequest *request)
{
    u32 entity = request->entity;
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
            lastChunkNodeIndex++;
        }
        handle = CreateHandle(chunkNode, localIndex, lastChunkNodeIndex);
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
    *result      = request->transform;

    return handle;
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

        if (chunkNode == last && ((transformWritePos & transformChunkMask) == localIndex))
        {
            transformWritePos--;
        }
        else
        {
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
HierarchyManager::HierarchyManager(Scene *inScene)
{
    parentScene    = inScene;
    hierarchySlots = PushArray(parentScene->arena, HierarchySlot, numHierarchyNodeSlots);
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

        if (chunkNode == last && ((hierarchyWritePos & hierarchyNodeChunkMask) == localIndex))
        {
            hierarchyWritePos--;
        }
        else
        {
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
        }
        node->flags &= ~HierarchyFlag_Valid;
    }
    return result;
}

HierarchyIter HierarchyManager::BeginIter()
{
    HierarchyIter iter;
    iter.localIndex  = 0;
    iter.globalIndex = 0;
    iter.chunkNode   = first;
    iter.node        = iter.chunkNode ? &first->components[0] : 0;
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
        HierarchyComponent *node = &iter->chunkNode->components[iter->localIndex++];
        valid                    = HasFlags(node->flags, HierarchyFlag_Valid);
        iter->node               = node;
        iter->globalIndex++;
        if (iter->localIndex == numHierarchyNodesPerChunk)
        {
            Assert(iter->chunkNode);
            iter->localIndex = 0;
            iter->chunkNode  = iter->chunkNode->next;
        }
    } while (!valid);
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
SkeletonManager::SkeletonManager(Scene *inScene)
{
    parentScene = inScene;
}

SkeletonTicket SkeletonManager::Create(Entity entity)
{
    SkeletonCreateRequest *request = (SkeletonCreateRequest *)parentScene->componentRequestRing.Alloc(sizeof(SkeletonCreateRequest));

    request->type   = ComponentRequestType_CreateSkeleton;
    request->entity = entity;

    SkeletonTicket ticket;
    ticket.request     = request;
    ticket.parentScene = parentScene;
    return ticket;
}

void SkeletonManager::CreateInternal(SkeletonCreateRequest *request)
{
    u32 entity = request->entity;
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

    // Add to hash
    SkeletonSlot *slot         = &skeletonSlots[entity & skeletonSlotMask];
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
    slotNode->entity = entity;
    QueuePush(slot->first, slot->last, slotNode);
    totalNumSkeletons++;

    LoadedSkeleton *result = &chunkNode->skeletons[localIndex];
    *result                = request->skeleton;
    chunkNode->flags[localIndex] |= SkeletonFlag_Valid;
}

b32 SkeletonManager::Remove(Entity entity)
{
    b32 result         = 0;
    SkeletonSlot *slot = &skeletonSlots[entity & skeletonSlotMask];

    SkeletonHandle handle  = {};
    SkeletonSlotNode *prev = 0;
    for (SkeletonSlotNode *node = slot->first; node != 0; node = node->next)
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
        totalNumSkeletons--;
        result = 1;
        SkeletonChunkNode *chunkNode;
        u32 localIndex;
        UnpackHandle(handle, &chunkNode, &localIndex);

        if (chunkNode == last && ((skeletonWritePos & skeletonChunkMask) == localIndex))
        {
            skeletonWritePos--;
        }
        else
        {
            SkeletonFreeNode *freeNode = freeNodes;
            if (freeNode)
            {
                StackPop(freeNodes);
            }
            else
            {
                freeNode = PushStruct(parentScene->arena, SkeletonFreeNode);
            }
            freeNode->handle = handle;
            StackPush(freePositions, freeNode);
        }

        chunkNode->flags[localIndex] &= ~SkeletonFlag_Valid;
    }
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
        u32 localIndex           = iter->localIndex++;
        LoadedSkeleton *skeleton = &iter->chunkNode->skeletons[localIndex];
        valid                    = HasFlags(iter->chunkNode->flags[localIndex], SkeletonFlag_Valid);
        iter->skeleton           = skeleton;

        iter->globalIndex++;
        if (iter->localIndex == numSkeletonPerChunk)
        {
            Assert(iter->chunkNode);
            iter->localIndex = 0;
            iter->chunkNode  = iter->chunkNode->next;
        }
    } while (!valid);
}

inline LoadedSkeleton *SkeletonManager::Get(SkeletonIter *iter)
{
    LoadedSkeleton *skel = iter->skeleton;
    return skel;
}

inline SkeletonHandle SkeletonManager::GetHandle(Entity entity)
{
    SkeletonSlot *slot    = &skeletonSlots[entity & skeletonSlotMask];
    SkeletonHandle handle = {};
    for (SkeletonSlotNode *node = slot->first; node != 0; node = node->next)
    {
        if (node->entity == entity)
        {
            handle = node->handle;
            break;
        }
    }
    return handle;
}

inline LoadedSkeleton *SkeletonManager::Get(Entity entity)
{
    SkeletonHandle handle  = GetHandle(entity);
    LoadedSkeleton *result = GetFromHandle(handle);
    return result;
}

//////////////////////////////
// Component requests
//
void *ComponentRequestRing::Alloc(u64 size)
{
    void *result = 0;
    u64 align    = alignment;
    for (;;)
    {
        u64 alignedSize = size + sizeof(u64);
        alignedSize     = AlignPow2(alignedSize, align);

        u64 loadedCommitWritePos = commitWritePos.load();
        u64 loadedReadPos        = readPos;

        u64 ringCommitWritePos = loadedCommitWritePos & (totalSize - 1);

        u64 remainder = totalSize - ringCommitWritePos;
        if (remainder >= alignedSize)
        {
            remainder = 0;
        }

        u64 totalAllocSize = remainder + alignedSize;

        u64 availableSpace = totalSize - (loadedCommitWritePos - loadedReadPos);
        if (availableSpace >= totalAllocSize)
        {
            if (commitWritePos.compare_exchange_weak(loadedCommitWritePos, loadedCommitWritePos + totalAllocSize))
            {
                u64 offset = loadedCommitWritePos & (totalSize - 1);
                if (remainder != 0)
                {
                    MemoryZero(ringBuffer + offset, remainder);
                    offset = 0;
                }
                u64 *tagSpot = (u64 *)(ringBuffer + offset);
                *tagSpot     = writeTag.fetch_add(1);
                result       = ringBuffer + offset + sizeof(u64);
                break;
            }
        }
    }
    return result;
}

void ComponentRequestRing::EndAlloc(void *mem)
{
    for (;;)
    {
        u64 *tag = (u64 *)mem - 1;
        if (*tag == readTag.load())
        {
            readTag.fetch_add(1);
            writePos.exchange(commitWritePos.load());
        }
    }
}

//////////////////////////////
// Scene
//

Scene::Scene() : materials(this), meshes(this), transforms(this), hierarchy(this), skeletons(this)
{
    sma.Init();
}

Entity Scene::CreateEntity()
{
    static std::atomic<u32> entityGen = NULL_HANDLE + 1;
    return entityGen.fetch_add(1);
}

void Scene::CreateTransform(Mat4 transform, i32 entity, i32 parent)
{
    TransformCreateRequest *request = (TransformCreateRequest *)componentRequestRing.Alloc(sizeof(TransformCreateRequest));
    request->type                   = ComponentRequestType_CreateTransform;
    request->transform              = transform;
    request->entity                 = entity;
    request->parent                 = parent;
}

void Scene::ProcessRequests()
{
    u64 writePos = componentRequestRing.writePos.load();

    const u32 ringTotalSize = componentRequestRing.totalSize;
    const u32 ringMask      = ringTotalSize - 1;
    const u64 alignment     = componentRequestRing.alignment;
    while (componentRequestRing.readPos < writePos)
    {
        u64 ringReadPos           = componentRequestRing.readPos & ringMask;
        ComponentRequest *request = (ComponentRequest *)(componentRequestRing.ringBuffer + ringReadPos + sizeof(u64));

        u64 advance = 0;
        switch (request->type)
        {
            case ComponentRequestType_Null:
            {
                advance = ringTotalSize - ringReadPos;
                Assert(((componentRequestRing.readPos + advance) & ringMask) == 0);
            }
            break;
            case ComponentRequestType_CreateMaterial:
            {
                MaterialCreateRequest *materialCreateRequest = (MaterialCreateRequest *)request;
                materials.CreateInternal(materialCreateRequest);
                advance = AlignPow2(sizeof(*materialCreateRequest), alignment);
            }
            break;
            case ComponentRequestType_CreateMesh:
            {
                MeshCreateRequest *meshCreateRequest = (MeshCreateRequest *)request;
                meshes.CreateInternal(meshCreateRequest);
                advance = AlignPow2(sizeof(*meshCreateRequest), alignment);
            }
            case ComponentRequestType_CreateTransform:
            {
                TransformCreateRequest *transformCreateRequest = (TransformCreateRequest *)request;
                TransformHandle handle                         = transforms.CreateInternal(transformCreateRequest);

                u32 entity       = transformCreateRequest->entity;
                u32 parentEntity = transformCreateRequest->parent;

                hierarchy.Create(entity, parentEntity);

                advance = AlignPow2(sizeof(*transformCreateRequest), alignment);
            }
            break;
            default: Assert(0);
        }
        componentRequestRing.readPos += advance;
    }
}

} // namespace scene
