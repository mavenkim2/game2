#include "mkCrack.h"
#ifdef LSP_INCLUDE
#include "mkAsset.h"
#include "mkScene.h"
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
// Materials
//
MaterialManager::MaterialManager(Scene *inScene)
{
    parentScene = inScene;
    nameMap     = PushArray(parentScene->arena, MaterialSlot, numMaterialSlots);
    entityMap   = PushArray(parentScene->arena, MaterialSlot, numMaterialSlots);
}

inline b8 MaterialManager::IsValidHandle(MaterialHandle handle)
{
    b8 result = handle.u64[0] != 0;
    return result;
}

inline MaterialComponent *MaterialManager::GetFromHandle(MaterialHandle handle)
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

void MaterialManager::InsertNameMap(MaterialHandle handle, i32 sid)
{
    MaterialSlot *slot         = &nameMap[sid & (numMaterialSlots - 1)];
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
    MaterialSlot *slot    = &nameMap[sid & (numMaterialSlots - 1)];
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

void MaterialManager::InsertEntityMap(MaterialHandle handle, Entity entity)
{
    MaterialSlot *slot         = &entityMap[entity & (numMaterialSlots - 1)];
    MaterialSlotNode *freeNode = freeSlotNode;
    if (freeNode)
    {
        StackPop(freeSlotNode);
    }
    else
    {
        freeNode = PushStruct(parentScene->arena, MaterialSlotNode);
    }

    freeNode->id     = entity;
    freeNode->handle = handle;
    QueuePush(slot->first, slot->last, freeNode);
}

MaterialHandle MaterialManager::GetHandleFromEntityMap(Entity entity)
{
    MaterialSlot *slot    = &entityMap[entity & (numMaterialSlots - 1)];
    MaterialHandle handle = {};
    for (MaterialSlotNode *node = slot->first; node != 0; node = node->next)
    {
        if (node->id == entity)
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
    MaterialHandle handle      = {};
    if (freeNode)
    {
        handle = freeNode->handle;
        StackPop(freeMaterialPositions);
        resultChunkNode  = (MaterialChunkNode *)handle.u64[0];
        resultLocalIndex = handle.u32[2];
        StackPush(freeMaterialNodes, freeNode);
    }
    else
    {
        u32 index        = materialWritePos++;
        resultLocalIndex = index & (numMaterialsPerChunk - 1);
        resultChunkNode  = last;
        if (resultChunkNode == 0 || resultLocalIndex == 0)
        {
            MaterialChunkNode *newChunkNode = PushStruct(parentScene->arena, MaterialChunkNode);
            QueuePush(first, last, newChunkNode);
            resultChunkNode = newChunkNode;
        }
        handle.u64[0] = (u64)resultChunkNode;
        handle.u32[2] = resultLocalIndex;
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

MaterialComponent *MaterialManager::Create(string name, Entity entity)
{
    MaterialCreateRequest *request = (MaterialCreateRequest *)parentScene->componentRequestRing.Alloc(sizeof(MaterialCreateRequest));
    request->type                  = ComponentRequestType_CreateMaterial;
    i32 sid                        = AddSID(name);
    request->entity                = entity;
    request->sid                   = sid;
    return &request->material;
}

void MaterialManager::CreateInternal(MaterialCreateRequest *request)
{
    MaterialComponent *component = 0;
    Assert(request->type == ComponentRequestType_CreateMaterial);

    u32 entity = request->entity;
    u32 sid    = request->sid;

    // Get empty position
    MaterialChunkNode *chunkNode = 0;
    u32 localIndex;
    MaterialHandle handle = GetFreePosition(&chunkNode, &localIndex);

    InsertNameMap(handle, sid);
    if (entity != 0)
    {
        InsertEntityMap(handle, entity);
        chunkNode->entities[localIndex] = entity;
    }

    MaterialComponent *result = &chunkNode->materials[localIndex];
    *result                   = request->material;
    result->flags |= MaterialFlag_Valid;
    result->sid = sid;
    totalNumMaterials++;
}

MaterialComponent *MaterialManager::Link(Entity entity, string name)
{
    MaterialComponent *result = 0;
    MaterialHandle handle     = GetHandle(name);

    if (IsValidHandle(handle))
    {
        MaterialChunkNode *chunkNode    = (MaterialChunkNode *)handle.u64[0];
        u32 localIndex                  = handle.u32[2];
        chunkNode->entities[localIndex] = entity;

        InsertEntityMap(handle, entity);
        result = &chunkNode->materials[localIndex];
    }
    return result;
}

b32 MaterialManager::Remove(Entity entity)
{
    b32 result = 0;

    // Remove from entity map
    MaterialSlot *slot     = &entityMap[entity & (numMaterialSlots - 1)];
    MaterialHandle handle  = {};
    MaterialSlotNode *prev = 0;
    for (MaterialSlotNode *node = slot->first; node != 0; node = node->next)
    {
        if (node->id == entity)
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

    if (IsValidHandle(handle))
    {
        // Remove from name map
        totalNumMaterials--;
        MaterialComponent *material = GetFromHandle(handle);
        material->flags             = material->flags & ~MaterialFlag_Valid;
        u32 sid                     = material->sid;
        slot                        = &nameMap[sid & (numMaterialSlots - 1)];
        prev                        = 0;
        for (MaterialSlotNode *node = slot->first; node != 0; node = node->next)
        {
            if (node->id == sid)
            {
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

        // Remove from chunked linked list
        // TODO: gen ids?
        MaterialChunkNode *chunkNode = (MaterialChunkNode *)handle.u64[0];
        u32 localIndex               = handle.u32[2];
        // If the last position is freed, decrement material write pos
        if (chunkNode == last && ((materialWritePos & (numMaterialsPerChunk - 1)) == localIndex))
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

MaterialComponent *MaterialManager::Get(Entity entity)
{
    MaterialHandle handle     = GetHandleFromEntityMap(entity);
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

Mesh *MeshManager::Create(Entity entity)
{
    MeshCreateRequest *request = (MeshCreateRequest *)parentScene->componentRequestRing.Alloc(sizeof(MeshCreateRequest));
    request->type              = ComponentRequestType_CreateMesh;
    request->entity            = entity;
    return &request->mesh;
}

inline b8 MeshManager::IsValidHandle(MeshHandle handle)
{
    return handle.u64[0] != 0;
}

void MeshManager::CreateInternal(MeshCreateRequest *request)
{
    u32 entity = request->entity;
    MeshChunkNode *chunkNode;
    u32 localIndex;

    MeshFreeNode *freeNode = freeMeshPositions;
    if (freeNode)
    {
        StackPop(freeMeshPositions);
        MeshHandle handle = freeNode->handle;
        chunkNode         = (MeshChunkNode *)handle.u64[0];
        localIndex        = handle.u32[2];
        StackPush(freeMeshNodes, freeNode);
    }
    else
    {
        u32 index  = meshWritePos++;
        localIndex = index & (meshesPerChunk - 1);
        chunkNode  = first;

        if (chunkNode == 0 || localIndex == 0)
        {
            MeshChunkNode *newChunkNode = PushStruct(parentScene->arena, MeshChunkNode);
            QueuePush(first, last, newChunkNode);
            chunkNode = newChunkNode;
        }
    }

    // TODO: gen id?
    MeshHandle handle;
    handle.u64[0] = (u64)chunkNode;
    handle.u32[2] = localIndex;

    chunkNode->entities[localIndex] = entity;

    // Add to hash
    MeshSlot *slot         = &meshSlots[entity & (numMeshSlots - 1)];
    MeshSlotNode *slotNode = freeMeshSlotNode;
    if (slotNode)
    {
        StackPop(freeMeshSlotNode);
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
    MeshSlot *slot = &meshSlots[entity & numMeshSlots];

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
            StackPush(freeMeshSlotNode, node);
            break;
        }
        prev = node;
    }

    if (IsValidHandle(handle))
    {
        totalNumMeshes--;
        result                   = 1;
        MeshChunkNode *chunkNode = (MeshChunkNode *)handle.u64[0];
        u32 localIndex           = handle.u32[2];

        if (chunkNode == last && ((meshWritePos & (meshesPerChunk - 1)) == localIndex))
        {
            meshWritePos--;
        }
        else
        {
            MeshFreeNode *freeNode = freeMeshNodes;
            if (freeNode)
            {
                StackPop(freeMeshNodes);
            }
            else
            {
                freeNode = PushStruct(parentScene->arena, MeshFreeNode);
            }
            freeNode->handle = handle;
            StackPush(freeMeshPositions, freeNode);
        }
    }
    return result;
}

Mesh *MeshManager::Get(Entity entity)
{
    MeshSlot *slot    = &meshSlots[entity & numMeshSlots];
    MeshHandle handle = {};
    for (MeshSlotNode *node = slot->first; node != 0; node = node->next)
    {
        if (node->entity == entity)
        {
            handle = node->handle;
            break;
        }
    }

    Mesh *mesh = 0;
    if (IsValidHandle(handle))
    {
        MeshChunkNode *chunkNode = (MeshChunkNode *)handle.u64[0];
        u32 localIndex           = handle.u32[2];
        mesh                     = &chunkNode->meshes[localIndex];
    }
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
        if (iter->localIndex == numMaterialsPerChunk)
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

//////////////////////////////
// Component request ring
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

Scene::Scene() : materials(this), meshes(this)
{
    sma.Init();
}

Entity Scene::CreateEntity()
{
    static std::atomic<u32> entityGen = NULL_HANDLE + 1;
    return entityGen.fetch_add(1);
}

i32 Scene::CreateTransform(Mat4 transform, i32 parent)
{
    i32 transformIndex = -1;

    Assert(transformCount.load() < ArrayLength(transforms));

    transformIndex             = transformCount.fetch_add(1);
    transforms[transformIndex] = transform;

    i32 hierarchyIndex                       = hierarchyWritePos.fetch_add(1);
    hierarchy[hierarchyIndex].transformIndex = transformIndex;
    hierarchy[hierarchyIndex].parentId       = parent;

    return transformIndex;
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
            break;
            default: Assert(0);
        }
        componentRequestRing.readPos += advance;
    }
}

} // namespace scene
