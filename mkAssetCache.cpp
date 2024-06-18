// NOTE: an asset is anything that is loaded from disk to be used in game.

// TODO: Features of asset system:
// - Hashes the asset filename, stores the data
// - If two requests to load an asset are issued, don't load them twice
// - Hot load assets
// - Later: if there isn't enough space, evict the least used
//
// - LRU for eviction
#include "mkCrack.h"
#ifdef LSP_INCLUDE
#include "mkCommon.h"
#include "render/mkRender.h"
#include "mkAsset.h"
#include "mkAssetCache.h"
#include "offline/asset_processing.h"
#include "mkFont.h"
#include "render/mkGraphics.h"
#include "mkGame.h"
#include "mkScene.h"
#include "mkList.h"
#include "mkString.h"
#include "mkScene.h"
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

global const string skeletonDirectory  = "data/skeletons/";
global const string modelDirectory     = "data/models/";
global const string materialDirectory  = "data/materials/";
global const string textureDirectory   = "data/textures/";
global const string ddsDirectory       = "data/textures/dds/";
global const string animationDirectory = "data/animations/";

global volatile b32 gTerminateThreads;
// const i32 invalidIndex = -1;

internal void AS_Init()
{
    Arena *arena            = ArenaAlloc(megabytes(512));
    AS_CacheState *as_state = PushStruct(arena, AS_CacheState);
    engine->SetAssetCacheState(as_state);
    as_state->arena = arena;

    // as_state->numSlots   = 1024;
    // as_state->assetSlots = PushArray(arena, AS_Slot, as_state->numSlots);

    as_state->assetCapacity = 1024;
    as_state->assets        = PushArray(arena, AS_Asset *, as_state->assetCapacity);
    // Reserve null slot
    as_state->assets[0] = PushStruct(arena, AS_Asset);
    as_state->assetCount++;
    as_state->assetEndOfList++;
    as_state->freeAssetList = PushArray(arena, i32, as_state->assetCapacity);

    // Hash table
    {
        as_state->fileHash.Init(1024, 1024);
    }

    as_state->ringBufferSize = kilobytes(64);
    as_state->ringBuffer     = PushArray(arena, u8, as_state->ringBufferSize);

    as_state->threadCount    = 1; // Min(1, OS_NumProcessors() - 1);
    as_state->readSemaphore  = platform.CreateSemaphore(as_state->threadCount);
    as_state->writeSemaphore = platform.CreateSemaphore(as_state->threadCount);

    AS_InitializeAllocator();

    as_state->threads = PushArray(arena, AS_Thread, as_state->threadCount);
    for (u64 i = 0; i < as_state->threadCount; i++)
    {
        as_state->threads[i].handle = platform.ThreadStart(AS_EntryPoint, (void *)i);
    }

    // Asset tag trees
    as_state->tagMap.maxSlots = AS_TagKey_Count;
    as_state->tagMap.slots    = PushArray(arena, AS_TagSlot, as_state->tagMap.maxSlots);
    for (i32 i = 0; i < AS_TagKey_Count; i++)
    {
        as_state->tagMap.slots[i].maxCount = 256;
        as_state->tagMap.slots[i].nodes    = PushArray(arena, AS_TagNode, as_state->tagMap.slots[i].maxCount);
    }
}

internal void AS_Flush()
{
    gTerminateThreads = 1;
    std::atomic_thread_fence(std::memory_order_release);

    AS_CacheState *as_state = engine->GetAssetCacheState();
    platform.ReleaseSemaphores(as_state->readSemaphore, as_state->threadCount);

    for (u64 i = 0; i < as_state->threadCount; i++)
    {
        platform.ThreadJoin(as_state->threads[i].handle);
    }
}

internal void AS_Restart()
{
    gTerminateThreads       = 0;
    AS_CacheState *as_state = engine->GetAssetCacheState();
    for (u64 i = 0; i < as_state->threadCount; i++)
    {
        as_state->threads[i].handle = platform.ThreadStart(AS_EntryPoint, (void *)i);
    }
}

internal u64 RingRead(u8 *base, u64 ringSize, u64 readPos, void *dest, u64 destSize)
{
    Assert(ringSize >= destSize);

    u64 cursor         = readPos % ringSize;
    u64 firstPartSize  = Min(ringSize - cursor, destSize);
    u64 secondPartSize = destSize - firstPartSize;

    MemoryCopy(dest, base + cursor, firstPartSize);
    MemoryCopy((u8 *)dest + firstPartSize, base, secondPartSize);
    return destSize;
}

internal u64 RingWrite(u8 *base, u64 ringSize, u64 writePos, void *src, u64 srcSize)
{
    Assert(ringSize >= srcSize);

    u64 cursor         = writePos % ringSize;
    u64 firstPartSize  = Min(ringSize - cursor, srcSize);
    u64 secondPartSize = srcSize - firstPartSize;

    MemoryCopy(base + cursor, src, firstPartSize);
    MemoryCopy(base, (u8 *)src + firstPartSize, secondPartSize);

    return srcSize;
}

// TODO: timeout if it takes too long for a file to be loaded?
internal b32 AS_EnqueueFile(string path)
{
    AS_CacheState *as_state = engine->GetAssetCacheState();
    b32 sent                = 0;
    u64 writeSize           = sizeof(path.size) + path.size;
    Assert(writeSize < as_state->ringBufferSize);
    for (;;)
    {
        BeginTicketMutex(&as_state->mutex);

        u64 availableSize = as_state->ringBufferSize - (as_state->writePos - as_state->readPos);
        if (availableSize >= writeSize)
        {
            writeSize = AlignPow2(writeSize, 8);
            sent      = 1;
            as_state->writePos +=
                RingWriteStruct(as_state->ringBuffer, as_state->ringBufferSize, as_state->writePos, &path.size);
            as_state->writePos +=
                RingWrite(as_state->ringBuffer, as_state->ringBufferSize, as_state->writePos, path.str, path.size);
            as_state->writePos = AlignPow2(as_state->writePos, 8);
            EndTicketMutex(&as_state->mutex);
            break;
        }
        EndTicketMutex(&as_state->mutex);
        // Ideally this should never signal
        platform.SignalWait(as_state->writeSemaphore);
    }
    if (sent)
    {
        platform.ReleaseSemaphore(as_state->readSemaphore);
    }
    return sent;
}

internal string AS_DequeueFile(Arena *arena)
{
    AS_CacheState *as_state = engine->GetAssetCacheState();
    string result;
    result.size = 0;
    BeginTicketMutex(&as_state->mutex);

    u64 availableSize = as_state->writePos - as_state->readPos;
    if (availableSize >= sizeof(u64))
    {
        as_state->readPos +=
            RingReadStruct(as_state->ringBuffer, as_state->ringBufferSize, as_state->readPos, &result.size);
        result.str = PushArrayNoZero(arena, u8, result.size + 1);
        as_state->readPos += RingRead(as_state->ringBuffer, as_state->ringBufferSize, as_state->readPos,
                                      result.str, result.size);
        as_state->readPos       = AlignPow2(as_state->readPos, 8);
        result.str[result.size] = 0;
        platform.ReleaseSemaphore(as_state->writeSemaphore);
    }
    EndTicketMutex(&as_state->mutex);

    return result;
}

//////////////////////////////
// Asset Thread Entry Points
//
THREAD_ENTRY_POINT(AS_EntryPoint)
{
    ThreadContextSet(ctx);
    AS_CacheState *as_state = engine->GetAssetCacheState();
    SetThreadName(Str8Lit("[AS] Scanner"));
    for (; !gTerminateThreads;)
    {
        TempArena scratch = ScratchStart(0, 0);
        string path;
        path.size = 0;

        path = AS_DequeueFile(scratch.arena);

        if (path.size != 0)
        {
            OS_Handle handle             = platform.OpenFile(OS_AccessFlag_Read | OS_AccessFlag_ShareRead, path);
            OS_FileAttributes attributes = platform.AttributesFromFile(handle);
            // If the file doesn't exist, abort
            if (attributes.lastModified == 0 && attributes.size == 0)
            {
                continue;
            }
            AS_Asset *asset = 0;

            i32 hash = HashFromString(path);
            Assert(((as_state->assetCapacity) & (as_state->assetCapacity - 1)) == 0);

            // The asset should've been added to the cache already.
            for (i32 i = as_state->fileHash.FirstInHash(hash); i != -1; i = as_state->fileHash.NextInHash(i))
            {
                AS_Asset *nextAsset = as_state->assets[i];
                if (nextAsset->path == path)
                {
                    asset = nextAsset;
                    break;
                }
            }

            if (asset == 0)
            {
                Assert(!"Space for asset not made");
            }

            // Freed/new assets have a "lastModified" timestamp of 0. Assets to be hotloaded have a timestamp != 0.
            if (asset->lastModified != 0)
            {
                u32 loaded = AS_Status_Loaded;
                if (!asset->status.compare_exchange_strong(loaded, AS_Status_Unloaded))
                {
                    continue;
                }

                AS_Free(asset);
                Printf("Asset freed");
            }
            asset->lastModified = attributes.lastModified;
            asset->memoryBlock  = AS_Alloc((i32)attributes.size);

            asset->size = platform.ReadFileHandle(handle, AS_GetMemory(asset));
            platform.CloseFile(handle);

            // Process the raw asset data
            // JS_Kick(AS_LoadAsset, asset, 0, Priority_Low);
            // AS_LoadAsset(asset);
            jobsystem::KickJob(0, [asset](jobsystem::JobArgs args) {
                AS_LoadAsset(asset);
            });
            ScratchEnd(scratch);
        }
        else
        {
            ScratchEnd(scratch);
            platform.SignalWait(as_state->readSemaphore);
        }
    }
}

internal void AS_HotloadEntryPoint(void *p)
{
    AS_CacheState *as_state = engine->GetAssetCacheState();
    SetThreadName("[AS] Hotload");

    for (; !gTerminateThreads;)
    {
        for (i32 i = 0; i < as_state->assetEndOfList; i++)
        {
            AS_Asset *asset = as_state->assets[i];
            if (asset->lastModified)
            {
                // If the asset was modified, its write time changes. Need to hotload.
                OS_FileAttributes attributes = platform.AttributesFromPath(asset->path);
                u64 lastModified             = asset->lastModified;
                if (attributes.lastModified != 0 && attributes.lastModified != lastModified)
                {
                    // Printf("Old last modified: %u\nNew last modified: %u\n\n", node->lastModified,
                    //        attributes.lastModified);
                    AtomicCompareExchangeU64(&asset->lastModified, attributes.lastModified, lastModified);

                    AS_EnqueueFile(asset->path);
                }
            }
        }
        platform.Sleep(100);
    }
}

//////////////////////////////
// Specific asset loading callbacks
//
// TODO: create a pack file so you don't have to check the file extension
// assets should either be loaded directly into main memory, or into a temp storage

// JOB_CALLBACK(AS_LoadAsset)

using namespace graphics;
internal void AS_LoadAsset(AS_Asset *asset)
{
    TempArena temp          = ScratchStart(0, 0);
    AS_CacheState *as_state = engine->GetAssetCacheState();
    u32 unloaded            = AS_Status_Unloaded;
    if (!asset->status.compare_exchange_strong(unloaded, AS_Status_Queued))
    {
        return;
    }
    string extension = GetFileExtension(asset->path);
    if (extension == Str8Lit("model"))
    {
        SceneMergeTicket ticket = gameScene->requestRing.CreateMergeRequest();
        Scene *newScene         = ticket.GetScene();
        string filename         = PathSkipLastSlash(asset->path);

        LoadedModel *model = &asset->model;
        asset->type        = AS_Model;

        Entity rootEntity    = newScene->CreateEntity();
        newScene->rootEntity = rootEntity;

        G_State *g_state = engine->GetGameState();
        u32 index        = g_state->GetIndex(asset->path);
        newScene->CreateTransform(g_state->mTransforms[index], rootEntity);

        u8 *buffer = AS_GetMemory(asset);
        Tokenizer tokenizer;
        tokenizer.input.str  = buffer;
        tokenizer.input.size = asset->size;
        tokenizer.cursor     = tokenizer.input.str;

        GetPointerValue(&tokenizer, &model->numMeshes);

        string modelName        = RemoveFileExtension(filename);
        string materialFilename = PushStr8F(temp.arena, "%S%S.mtr", materialDirectory, modelName);
        string materialData     = platform.ReadEntireFile(temp.arena, materialFilename);

        Tokenizer materialTokenizer;
        materialTokenizer.input.str  = materialData.str;
        materialTokenizer.input.size = materialData.size;
        materialTokenizer.cursor     = materialTokenizer.input.str;

        Assert(Advance(&materialTokenizer, "Num materials: "));
        u32 numMaterials = ReadUint(&materialTokenizer);
        SkipToNextLine(&materialTokenizer);
        SkipToNextLine(&materialTokenizer);

        for (u32 i = 0; i < numMaterials; i++)
        {
            Assert(Advance(&materialTokenizer, "Name: "));
            string line = ReadLine(&materialTokenizer);

            SkipToNextLine(&materialTokenizer);
            MaterialComponent *component = newScene->materials.Create(line);

            // Read the diffuse texture if there is one
            b32 result = Advance(&materialTokenizer, "\tDiffuse: ");
            if (result)
            {
                line           = ReadLine(&materialTokenizer);
                string ddsPath = PushStr8F(temp.arena, "%S%S.dds", ddsDirectory, PathSkipLastSlash(RemoveFileExtension(line)));

                if (platform.FileExists(ddsPath))
                {
                    component->textures[TextureType_Diffuse] = AS_GetAsset(ddsPath);
                }
                else
                {
                    component->textures[TextureType_Diffuse] = AS_GetAsset(StrConcat(temp.arena, textureDirectory, line));
                }
            }
            result = Advance(&materialTokenizer, "\tColor: ");
            if (result)
            {
                component->baseColor.x = ReadFloat(&materialTokenizer);
                component->baseColor.y = ReadFloat(&materialTokenizer);
                component->baseColor.z = ReadFloat(&materialTokenizer);
                component->baseColor.w = ReadFloat(&materialTokenizer);
            }
            result = Advance(&materialTokenizer, "\tNormal: ");
            if (result)
            {
                line                                    = ReadLine(&materialTokenizer);
                component->textures[TextureType_Normal] = AS_GetAsset(StrConcat(temp.arena, textureDirectory, line));
            }
            result = Advance(&materialTokenizer, "\tMR Map: ");
            if (result)
            {
                line                                = ReadLine(&materialTokenizer);
                component->textures[TextureType_MR] = AS_GetAsset(StrConcat(temp.arena, textureDirectory, line));
            }
            result = Advance(&materialTokenizer, "\tMetallic Factor: ");
            if (result)
            {
                component->metallicFactor = ReadFloat(&materialTokenizer);
            }
            result = Advance(&materialTokenizer, "\tRoughness Factor: ");
            if (result)
            {
                component->roughnessFactor = ReadFloat(&materialTokenizer);
            }
            SkipToNextLine(&materialTokenizer); // final closing bracket }
        }
        Assert(EndOfBuffer(&materialTokenizer));

        Mesh **meshes    = PushArray(temp.arena, Mesh *, model->numMeshes);
        Mat4 *transforms = PushArray(temp.arena, Mat4, model->numMeshes);
        Entity *entities = PushArray(temp.arena, Entity, model->numMeshes);
        for (u32 i = 0; i < model->numMeshes; i++)
        {
            Entity meshEntity = newScene->CreateEntity();
            Mesh *mesh        = newScene->meshes.Create(meshEntity);

            meshes[i]   = mesh;
            entities[i] = meshEntity;

            // Get the size
            u32 vertexCount;
            GetPointerValue(&tokenizer, &vertexCount);
            mesh->vertexCount = vertexCount;

            // Get the flags
            MeshFlags flags;
            GetPointerValue(&tokenizer, &flags);

            // Get all of the subsets
            GetPointerValue(&tokenizer, &mesh->numSubsets);

            mesh->subsets = GetTokenCursor(&tokenizer, Mesh::MeshSubset);
            Advance(&tokenizer, sizeof(mesh->subsets[0]) * mesh->numSubsets);
            for (u32 subsetIndex = 0; subsetIndex < mesh->numSubsets; subsetIndex++)
            {
                string materialName;
                GetPointerValue(&tokenizer, &materialName.size);
                if (materialName.size != 0)
                {
                    materialName.str                          = GetTokenCursor(&tokenizer, u8);
                    mesh->subsets[subsetIndex].materialHandle = newScene->materials.GetHandle(materialName);
                    Advance(&tokenizer, (u32)materialName.size);
                }
            }

            mesh->positions = GetTokenCursor(&tokenizer, V3);
            Advance(&tokenizer, sizeof(mesh->positions[0]) * vertexCount);
            mesh->normals = GetTokenCursor(&tokenizer, V3);
            Advance(&tokenizer, sizeof(mesh->normals[0]) * vertexCount);
            mesh->tangents = GetTokenCursor(&tokenizer, V3);
            Advance(&tokenizer, sizeof(mesh->tangents[0]) * vertexCount);
            if (flags & MeshFlags_Uvs)
            {
                mesh->uvs = GetTokenCursor(&tokenizer, V2);
                Advance(&tokenizer, sizeof(mesh->uvs[0]) * vertexCount);
            }
            if (flags & MeshFlags_Skinned)
            {
                mesh->boneIds = GetTokenCursor(&tokenizer, UV4);
                Advance(&tokenizer, sizeof(mesh->boneIds[0]) * vertexCount);

                mesh->boneWeights = GetTokenCursor(&tokenizer, V4);
                Advance(&tokenizer, sizeof(mesh->boneWeights[0]) * vertexCount);
            }
            u32 indexCount;
            GetPointerValue(&tokenizer, &indexCount);
            mesh->indexCount = indexCount;

            mesh->indices = GetTokenCursor(&tokenizer, u32);
            Advance(&tokenizer, sizeof(mesh->indices[0]) * indexCount);

            GetPointerValue(&tokenizer, &mesh->bounds);

            Mat4 transform;
            GetPointerValue(&tokenizer, &transform);
            if (flags & MeshFlags_Skinned)
            {
                transform = MakeMat4(1.f);
            }

            newScene->CreateTransform(transform, meshEntity, rootEntity);
            transforms[i] = transform;
        }

        // Skeleton
        SkeletonHandle skeletonHandle = {};
        {
            string path;
            GetPointerValue(&tokenizer, &path.size);
            if (path.size != 0)
            {
                path.str = GetTokenCursor(&tokenizer, u8);
                Advance(&tokenizer, (u32)path.size);

                // Load the skeleton
                string skeletonName      = path;
                LoadedSkeleton *skeleton = newScene->skeletons.Create(skeletonName);
                skeletonHandle           = newScene->skeletons.GetHandleFromName(skeletonName);
                string skeletonFilename  = PushStr8F(temp.arena, "%S%S.skel", skeletonDirectory, skeletonName);

                AS_Asset *skelAsset = AS_AllocAsset(skeletonFilename, false);
                Printf("Skeleton file name: %S\n", path);

                OS_Handle handle             = platform.OpenFile(OS_AccessFlag_Read | OS_AccessFlag_ShareRead, skeletonFilename);
                OS_FileAttributes attributes = platform.AttributesFromFile(handle);
                // If the file doesn't exist, abort
                if (attributes.lastModified != 0 || attributes.size != 0)
                {
                    skelAsset->lastModified = attributes.lastModified;
                    skelAsset->memoryBlock  = AS_Alloc((i32)attributes.size);

                    skelAsset->size = platform.ReadFileHandle(handle, AS_GetMemory(skelAsset));
                    platform.CloseFile(handle);

                    u8 *skelBuffer = AS_GetMemory(skelAsset);
                    Tokenizer skeletonTokenizer;
                    skeletonTokenizer.input.str  = skelBuffer;
                    skeletonTokenizer.input.size = skelAsset->size;
                    skeletonTokenizer.cursor     = skeletonTokenizer.input.str;

                    u32 version;
                    u32 count;
                    GetPointerValue(&skeletonTokenizer, &version);
                    GetPointerValue(&skeletonTokenizer, &count);
                    skeleton->count = count;

                    if (version == 1)
                    {
                        skeleton->names = GetTokenCursor(&skeletonTokenizer, string);
                        Advance(&skeletonTokenizer, sizeof(skeleton->names[0]) * count);
                        for (u32 i = 0; i < count; i++)
                        {
                            u64 offset             = (u64)skeleton->names[i].str;
                            skeleton->names[i].str = ConvertOffsetToPointer(skeletonTokenizer.input.str, offset);
                            Advance(&skeletonTokenizer, (u32)skeleton->names[i].size);
                        }
                        skeleton->parents = GetTokenCursor(&skeletonTokenizer, i32);
                        Advance(&skeletonTokenizer, sizeof(skeleton->parents[0]) * count);
                        skeleton->inverseBindPoses = GetTokenCursor(&skeletonTokenizer, Mat4);
                        Advance(&skeletonTokenizer, sizeof(skeleton->inverseBindPoses[0]) * count);
                        skeleton->transformsToParent = GetTokenCursor(&skeletonTokenizer, Mat4);
                        Advance(&skeletonTokenizer, sizeof(skeleton->transformsToParent[0]) * count);

                        Assert(EndOfBuffer(&skeletonTokenizer));
                    }
                    skelAsset->type     = AS_Skeleton;
                    skelAsset->skeleton = skeleton;
                    skelAsset->status.store(AS_Status_Loaded);
                }
            }
        }

        Assert(EndOfBuffer(&tokenizer));

        Init(&model->bounds);
        // Load vertices and indices of each mesh to he GPU
        for (u32 i = 0; i < model->numMeshes; i++)
        {
            Mesh *mesh = meshes[i];
            mesh->Init();
            u32 vertexCount = mesh->vertexCount;
            u32 indexCount  = mesh->indexCount;

            if (newScene->skeletons.IsValidHandle(skeletonHandle))
            {
                newScene->skeletons.Link(entities[i], skeletonHandle);
            }
            GPUBufferDesc desc;
            desc.resourceUsage = ResourceUsage::MegaBuffer | ResourceUsage::UniformTexelBuffer | ResourceUsage::IndexBuffer | ResourceUsage::UniformBuffer;
            u64 alignment       = device->GetMinAlignment(&desc);

            Assert(IsPow2(alignment));
            desc.size = AlignPow2(sizeof(mesh->positions[0]) * vertexCount, alignment) + Align(sizeof(mesh->normals[0]) * vertexCount, alignment) + Align(sizeof(mesh->tangents[0]) * vertexCount, alignment);
            if (mesh->uvs)
            {
                desc.size += AlignPow2(sizeof(mesh->uvs[0]) * vertexCount, alignment);
            }
            if (mesh->boneIds)
            {
                desc.size += AlignPow2(sizeof(mesh->boneIds[0]) * vertexCount, alignment);
                desc.size += AlignPow2(sizeof(mesh->boneWeights[0]) * vertexCount, alignment);
            }
            desc.size += AlignPow2(sizeof(mesh->indices[0]) * indexCount, alignment);

            auto initCallback = [&](void *dest) {
                u64 currentOffset = 0;
                u8 *bufferDest    = (u8 *)dest;

                // Load positions
                mesh->vertexPosView.offset = currentOffset;
                mesh->vertexPosView.size   = sizeof(mesh->positions[0]) * vertexCount;
                MemoryCopy(bufferDest + currentOffset, mesh->positions, mesh->vertexPosView.size);
                currentOffset += AlignPow2(mesh->vertexPosView.size, alignment);

                // Load normals
                mesh->vertexNorView.offset = currentOffset;
                mesh->vertexNorView.size   = sizeof(mesh->normals[0]) * vertexCount;
                MemoryCopy(bufferDest + currentOffset, mesh->normals, mesh->vertexNorView.size);
                currentOffset += AlignPow2(mesh->vertexNorView.size, alignment);

                // Load tangents
                mesh->vertexTanView.offset = currentOffset;
                mesh->vertexTanView.size   = sizeof(mesh->tangents[0]) * vertexCount;
                MemoryCopy(bufferDest + currentOffset, mesh->tangents, mesh->vertexTanView.size);
                currentOffset += AlignPow2(mesh->vertexTanView.size, alignment);

                // Load uvs if they exist
                if (mesh->uvs)
                {
                    mesh->vertexUvView.offset = currentOffset;
                    mesh->vertexUvView.size   = sizeof(mesh->uvs[0]) * vertexCount;
                    MemoryCopy(bufferDest + currentOffset, mesh->uvs, mesh->vertexUvView.size);
                    currentOffset += AlignPow2(mesh->vertexUvView.size, alignment);
                }
                if (mesh->boneIds)
                {
                    Assert(mesh->boneWeights);
                    mesh->vertexBoneIdView.offset = currentOffset;
                    mesh->vertexBoneIdView.size   = sizeof(mesh->boneIds[0]) * vertexCount;
                    MemoryCopy(bufferDest + currentOffset, mesh->boneIds, mesh->vertexBoneIdView.size);
                    currentOffset += AlignPow2(mesh->vertexBoneIdView.size, alignment);

                    mesh->vertexBoneWeightView.offset = currentOffset;
                    mesh->vertexBoneWeightView.size   = sizeof(mesh->boneWeights[0]) * vertexCount;
                    MemoryCopy(bufferDest + currentOffset, mesh->boneWeights, mesh->vertexBoneWeightView.size);
                    currentOffset += AlignPow2(mesh->vertexBoneWeightView.size, alignment);
                }

                mesh->indexView.offset = currentOffset;
                mesh->indexView.size   = sizeof(mesh->indices[0]) * mesh->indexCount;
                MemoryCopy(bufferDest + currentOffset, mesh->indices, mesh->indexView.size);
                currentOffset += AlignPow2(mesh->indexView.size, alignment);
            };

            device->CreateBufferCopy(&mesh->buffer, desc, initCallback);
            device->SetName(&mesh->buffer, "Mesh buffer");

            Assert(mesh->positions);
            mesh->vertexPosView.srvIndex      = device->CreateSubresource(&mesh->buffer, ResourceType::SRV, mesh->vertexPosView.offset, mesh->vertexPosView.size, Format::R32G32B32_SFLOAT);
            mesh->vertexPosView.srvDescriptor = device->GetDescriptorIndex(&mesh->buffer, ResourceType::SRV, mesh->vertexPosView.srvIndex);

            Assert(mesh->normals);
            mesh->vertexNorView.srvIndex      = device->CreateSubresource(&mesh->buffer, ResourceType::SRV, mesh->vertexNorView.offset, mesh->vertexNorView.size, Format::R32G32B32_SFLOAT);
            mesh->vertexNorView.srvDescriptor = device->GetDescriptorIndex(&mesh->buffer, ResourceType::SRV, mesh->vertexNorView.srvIndex);

            Assert(mesh->tangents);
            mesh->vertexTanView.srvIndex      = device->CreateSubresource(&mesh->buffer, ResourceType::SRV, mesh->vertexTanView.offset, mesh->vertexTanView.size, Format::R32G32B32_SFLOAT);
            mesh->vertexTanView.srvDescriptor = device->GetDescriptorIndex(&mesh->buffer, ResourceType::SRV, mesh->vertexTanView.srvIndex);

            if (mesh->uvs)
            {
                mesh->vertexUvView.srvIndex      = device->CreateSubresource(&mesh->buffer, ResourceType::SRV, mesh->vertexUvView.offset, mesh->vertexUvView.size, Format::R32G32_SFLOAT);
                mesh->vertexUvView.srvDescriptor = device->GetDescriptorIndex(&mesh->buffer, ResourceType::SRV, mesh->vertexUvView.srvIndex);
            }
            if (mesh->boneIds)
            {
                mesh->vertexBoneIdView.srvIndex      = device->CreateSubresource(&mesh->buffer, ResourceType::SRV, mesh->vertexBoneIdView.offset, mesh->vertexBoneIdView.size, Format::R32G32B32A32_UINT);
                mesh->vertexBoneIdView.srvDescriptor = device->GetDescriptorIndex(&mesh->buffer, ResourceType::SRV, mesh->vertexBoneIdView.srvIndex);

                Assert(mesh->boneWeights);
                mesh->vertexBoneWeightView.srvIndex      = device->CreateSubresource(&mesh->buffer, ResourceType::SRV, mesh->vertexBoneWeightView.offset, mesh->vertexBoneWeightView.size, Format::R32G32B32A32_SFLOAT);
                mesh->vertexBoneWeightView.srvDescriptor = device->GetDescriptorIndex(&mesh->buffer, ResourceType::SRV, mesh->vertexBoneWeightView.srvIndex);
            }

            mesh->indexView.srvIndex      = device->CreateSubresource(&mesh->buffer, ResourceType::SRV, mesh->indexView.offset, mesh->indexView.size);
            mesh->indexView.srvDescriptor = device->GetDescriptorIndex(&mesh->buffer, ResourceType::SRV, mesh->indexView.srvIndex);

            // Create skinning uavs
            if (mesh->boneIds)
            {
                GPUBufferDesc streamDesc;
                streamDesc.resourceUsage = ResourceUsage::MegaBuffer | ResourceUsage::StorageBuffer | ResourceUsage::UniformTexelBuffer;

                alignment = device->GetMinAlignment(&streamDesc);
                Assert(IsPow2(alignment));

                mesh->soPosView.offset = 0;
                mesh->soPosView.size   = sizeof(mesh->positions[0]) * vertexCount;
                streamDesc.size       = AlignPow2(mesh->soPosView.size, alignment);

                mesh->soNorView.offset = streamDesc.size;
                mesh->soNorView.size   = sizeof(mesh->normals[0]) * vertexCount;
                streamDesc.size += AlignPow2(mesh->soNorView.size, alignment);

                mesh->soTanView.offset = streamDesc.size;
                mesh->soTanView.size   = sizeof(mesh->tangents[0]) * vertexCount;
                streamDesc.size += AlignPow2(mesh->soTanView.size, alignment);

                // Load positions
                device->CreateBuffer(&mesh->streamBuffer, streamDesc, 0);
                device->SetName(&mesh->streamBuffer, "Mesh stream buffer");

                mesh->soPosView.srvIndex      = device->CreateSubresource(&mesh->streamBuffer, ResourceType::SRV, mesh->soPosView.offset, mesh->soPosView.size, Format::R32G32B32_SFLOAT, "Streamout pos");
                mesh->soPosView.srvDescriptor = device->GetDescriptorIndex(&mesh->streamBuffer, ResourceType::SRV, mesh->soPosView.srvIndex);
                mesh->soPosView.uavIndex      = device->CreateSubresource(&mesh->streamBuffer, ResourceType::UAV, mesh->soPosView.offset, mesh->soPosView.size);
                mesh->soPosView.uavDescriptor = device->GetDescriptorIndex(&mesh->streamBuffer, ResourceType::UAV, mesh->soPosView.uavIndex);

                mesh->soNorView.srvIndex      = device->CreateSubresource(&mesh->streamBuffer, ResourceType::SRV, mesh->soNorView.offset, mesh->soNorView.size, Format::R32G32B32_SFLOAT);
                mesh->soNorView.srvDescriptor = device->GetDescriptorIndex(&mesh->streamBuffer, ResourceType::SRV, mesh->soNorView.srvIndex);
                mesh->soNorView.uavIndex      = device->CreateSubresource(&mesh->streamBuffer, ResourceType::UAV, mesh->soNorView.offset, mesh->soNorView.size);
                mesh->soNorView.uavDescriptor = device->GetDescriptorIndex(&mesh->streamBuffer, ResourceType::UAV, mesh->soNorView.uavIndex);

                mesh->soTanView.srvIndex      = device->CreateSubresource(&mesh->streamBuffer, ResourceType::SRV, mesh->soTanView.offset, mesh->soTanView.size, Format::R32G32B32_SFLOAT);
                mesh->soTanView.srvDescriptor = device->GetDescriptorIndex(&mesh->streamBuffer, ResourceType::SRV, mesh->soTanView.srvIndex);
                mesh->soTanView.uavIndex      = device->CreateSubresource(&mesh->streamBuffer, ResourceType::UAV, mesh->soTanView.offset, mesh->soTanView.size);
                mesh->soTanView.uavDescriptor = device->GetDescriptorIndex(&mesh->streamBuffer, ResourceType::UAV, mesh->soTanView.uavIndex);

                mesh->posDescriptor = mesh->soPosView.srvDescriptor;
                mesh->norDescriptor = mesh->soNorView.srvDescriptor;
                mesh->tanDescriptor = mesh->soTanView.srvDescriptor;
            }
            else
            {
                mesh->posDescriptor = mesh->vertexPosView.srvDescriptor;
                mesh->norDescriptor = mesh->vertexNorView.srvDescriptor;
                mesh->tanDescriptor = mesh->vertexTanView.srvDescriptor;
            }

            Mat4 transform         = transforms[i];
            Rect3 modelSpaceBounds = Transform(transform, mesh->bounds);
            AddBounds(model->bounds, modelSpaceBounds);
        }
    }
    else if (extension == Str8Lit("anim"))
    {
        asset->type = AS_Anim;
        u8 *buffer  = AS_GetMemory(asset);

        Tokenizer tokenizer;
        tokenizer.input.str  = buffer;
        tokenizer.input.size = asset->size;
        tokenizer.cursor     = tokenizer.input.str;

        GetPointerValue(&tokenizer, &asset->anim.numNodes);
        GetPointerValue(&tokenizer, &asset->anim.duration);

        asset->anim.boneChannels = GetTokenCursor(&tokenizer, BoneChannel);
        for (u32 i = 0; i < asset->anim.numNodes; i++)
        {
            BoneChannel *boneChannel = &asset->anim.boneChannels[i];
            boneChannel->name.str    = (u8 *)ConvertOffsetToPointer(buffer, (u64)boneChannel->name.str);
            boneChannel->positions   = (AnimationPosition *)ConvertOffsetToPointer(buffer, (u64)boneChannel->positions);
            boneChannel->scales      = (AnimationScale *)ConvertOffsetToPointer(buffer, (u64)boneChannel->scales);
            boneChannel->rotations   = (AnimationRotation *)ConvertOffsetToPointer(buffer, (u64)boneChannel->rotations);
        }

        // Advance(&tokenizer, sizeof(BoneChannel) * asset->anim.numNodes);
        // Assert(EndOfBuffer(&tokenizer));

        // asset->anim.boneChannels = (BoneChannel *)AS_Alloc(sizeof(BoneChannel) * asset->anim.numNodes);

        // for (u32 i = 0; i < asset->anim.numNodes; i++)
        // {
        //     BoneChannel *channel = &asset->anim.boneChannels[i];
        //
        //     GetPointerValue(&tokenizer, &channel->name.size);
        //     channel->name.str = GetTokenCursor(&tokenizer, u8);
        //     Advance(&tokenizer, (u32)(sizeof(channel->name.str[0]) * channel->name.size));
        //
        //     GetPointerValue(&tokenizer, &channel->numPositionKeys);
        //     channel->positions = GetTokenCursor(&tokenizer, AnimationPosition);
        //     Advance(&tokenizer, sizeof(channel->positions[0]) * channel->numPositionKeys);
        //
        //     GetPointerValue(&tokenizer, &channel->numScalingKeys);
        //     channel->scales = GetTokenCursor(&tokenizer, AnimationScale);
        //     Advance(&tokenizer, sizeof(channel->scales[0]) * channel->numScalingKeys);
        //
        //     GetPointerValue(&tokenizer, &channel->numRotationKeys);
        //     channel->rotations = GetTokenCursor(&tokenizer, AnimationRotation);
        //     Advance(&tokenizer, sizeof(channel->rotations[0]) * channel->numRotationKeys);
        // }
    }
    else if (extension == Str8Lit("skel"))
    {
        Assert(0);
        //     string skeletonName = RemoveFileExtension(asset->path);
        //
        //     Tokenizer tokenizer;
        //     tokenizer.input.str  = AS_GetMemory(asset);
        //     tokenizer.input.size = asset->size;
        //     tokenizer.cursor     = tokenizer.input.str;
        //
        //     u32 version;
        //     u32 count;
        //     GetPointerValue(&tokenizer, &version);
        //     GetPointerValue(&tokenizer, &count);
        //     skeleton->count = count;
        //
        //     if (version == 1)
        //     {
        //         // NOTE: How this works for future me:
        //         // When written, pointers are converted to offsets in file. Offset + base file address is the new
        //         // pointer location. For now, I am storing the string data right after the offset and size, but
        //         // this could theoretically be moved elsewhere.
        //
        //         skeleton->names = (string *)AS_Alloc(sizeof(string) * skeleton->count);
        //         for (u32 i = 0; i < count; i++)
        //         {
        //             string *boneName = &skeleton->names[i];
        //             boneName->str    = GetPointer(&tokenizer, u8);
        //             GetPointerValue(&tokenizer, &boneName->size);
        //             Advance(&tokenizer, (u32)boneName->size);
        //         }
        //         skeleton->parents = GetTokenCursor(&tokenizer, i32);
        //         Advance(&tokenizer, sizeof(skeleton->parents[0]) * count);
        //         skeleton->inverseBindPoses = GetTokenCursor(&tokenizer, Mat4);
        //         Advance(&tokenizer, sizeof(skeleton->inverseBindPoses[0]) * count);
        //         skeleton->transformsToParent = GetTokenCursor(&tokenizer, Mat4);
        //         Advance(&tokenizer, sizeof(skeleton->transformsToParent[0]) * count);
        //
        //         Assert(EndOfBuffer(&tokenizer));
        //     }
        //     asset->type = AS_Skeleton;
        //     // TODO: having this pointer feels awkward.
        //     asset->skeleton = skeleton;
    }
    else if (extension == Str8Lit("png") || extension == Str8Lit("jpeg"))
    {
        asset->type = AS_Texture;

        Format format   = Format::R8G8B8A8_UNORM;
        Format bcFormat = Format::Null;

        if (FindSubstring(asset->path, Str8Lit("diffuse"), 0, MatchFlag_CaseInsensitive) != asset->path.size ||
            FindSubstring(asset->path, Str8Lit("basecolor"), 0, MatchFlag_CaseInsensitive) != asset->path.size)
        {
            format   = Format::R8G8B8A8_SRGB;
            bcFormat = Format::BC1_RGB_UNORM;
        }
        i32 width, height, nComponents;
        void *texData =
            stbi_load_from_memory(AS_GetMemory(asset), (i32)asset->size, &width, &height, &nComponents, 4);

        Assert(nComponents >= 1);

        TextureDesc desc;
        desc.width        = width;
        desc.height       = height;
        desc.format       = format;
        desc.initialUsage = ResourceUsage::SampledImage;
        desc.textureType  = TextureDesc::TextureType::Texture2D;
        desc.sampler      = TextureDesc::DefaultSampler::Linear;

        device->CreateTexture(&asset->texture, desc, texData);
        device->SetName(&asset->texture, (const char *)asset->path.str);

        stbi_image_free(texData);
    }
    else if (extension == Str8Lit("dds"))
    {
        asset->type = AS_Texture;
        LoadDDS(asset);
    }
    else if (extension == Str8Lit("ttf"))
    {
        asset->type          = AS_Font;
        u8 *buffer           = AS_GetMemory(asset);
        asset->font.fontData = F_InitializeFont(buffer);
        // FreeBlocks(node);
    }
    else
    {
        Assert(!"Asset type not supported");
    }
    asset->status.store(AS_Status_Loaded);
    ScratchEnd(temp);
}

//////////////////////////////
// DDS loading
//

internal void LoadDDS(AS_Asset *asset)
{
    Format format = Format::Null;

    u8 *memory    = AS_GetMemory(asset);
    DDSFile *file = (DDSFile *)memory;
    Assert(file->magic == MakeFourCC('D', 'D', 'S', ' '));

    b8 usesDXT10Header = false;
    if ((file->header.format.flags & PixelFormatFlagBits_FourCC) && (file->header.format.fourCC == MakeFourCC('D', 'X', '1', '0')))
    {
        usesDXT10Header = true;
    }

    // TODO: support all dds header types
    Assert(!usesDXT10Header);
    Assert(file->header.mipMapCount == 1);

    u64 offset = sizeof(DDSFile) + (usesDXT10Header ? sizeof(DDSHeaderDXT10) : 0);

    // Find the format
    if (file->header.format.flags & PixelFormatFlagBits_FourCC)
    {
        if (file->header.format.fourCC == MakeFourCC('D', 'X', 'T', '1'))
        {
            format = Format::BC1_RGB_UNORM;
        }
        else
        {
            Assert(0);
        }
    }
    else
    {
        Assert(0);
    }

    u8 *data = memory + offset;

    TextureDesc bcDesc;
    bcDesc.width        = file->header.width;
    bcDesc.height       = file->header.height;
    bcDesc.depth        = file->header.depth;
    bcDesc.format       = format;
    bcDesc.initialUsage = ResourceUsage::SampledImage;
    device->CreateTexture(&asset->texture, bcDesc, data);
    device->SetName(&asset->texture, (const char *)asset->path.str);
}

internal void AS_UnloadAsset(AS_Asset *asset)
{
    switch (asset->type)
    {
        case AS_Null: break;
        case AS_Mesh: break;
        case AS_Texture:
        {
            // R_DeleteTexture2D(node->texture.handle);
            break;
        }
        case AS_Skeleton: break;
        case AS_Model: break;
        default: Assert(!"Invalid asset type");
    }
}

//////////////////////////////
// Handles
//
internal AS_Asset *AS_GetAssetFromHandle(AS_Handle handle)
{
    AS_CacheState *as_state = engine->GetAssetCacheState();
    // Assert(sizeof(handle) <= 16);

    AS_Asset *result = 0;
    i32 index        = handle.i32[0];

    result = as_state->assets[index];
    if (result->generation != handle.i32[1] || result->status.load() != AS_Status_Loaded)
    {
        result = 0;
    }
    return result;
}

internal AS_Asset *AS_AllocAsset(const string inPath, b8 queueFile)
{
    AS_CacheState *as_state = engine->GetAssetCacheState();
    BeginMutex(&as_state->lock);

    AS_Asset *asset = 0;
    i32 hash        = HashFromString(inPath);

    as_state->assetCount++;
    // If there is an asset on the free list, use that

    if (as_state->freeAssetCount != 0)
    {
        asset = as_state->assets[as_state->freeAssetList[--as_state->freeAssetCount]];
        asset->generation += 1;
        Assert(asset->memoryBlock == 0);
    }
    else
    {
        i32 assetId               = as_state->assetEndOfList++;
        as_state->assets[assetId] = PushStruct(as_state->arena, AS_Asset);
        // NOTE: all strings will have a fixed backing buffer of 256 bytes.
        asset           = as_state->assets[assetId];
        asset->path.str = PushArray(as_state->arena, u8, MAX_OS_PATH);
        StringCopy(&asset->path, inPath);
        asset->id = assetId;
    }
    as_state->fileHash.AddInHash(hash, asset->id);

    EndMutex(&as_state->lock);

    if (queueFile) AS_EnqueueFile(inPath);

    return asset;
}

internal void AS_FreeAsset(AS_Handle handle)
{
    AS_CacheState *as_state = engine->GetAssetCacheState();
    BeginMutex(&as_state->lock);
    AS_Asset *asset = AS_GetAssetFromHandle(handle);
    if (asset)
    {
        as_state->assetCount--;
        AS_Free(asset);
        asset->lastModified = 0;
        as_state->assetCount--;

        i32 id = as_state->freeAssetCount++;
        Assert(id >= 0);
        as_state->freeAssetList[id] = asset->id;
    }
    EndMutex(&as_state->lock);
}

internal AS_Handle AS_GetAsset(const string inPath, const b32 inLoadIfNotFound)
{
    AS_CacheState *as_state = engine->GetAssetCacheState();
    AS_Handle result        = {};
    result.i32[0]           = -1;

    i32 hash = HashFromString(inPath);
    BeginMutex(&as_state->lock);
    for (i32 i = as_state->fileHash.FirstInHash(hash); i != -1; i = as_state->fileHash.NextInHash(i))
    {
        if (as_state->assets[i]->path == inPath)
        {
            result.i32[0] = i;
            result.i32[1] = as_state->assets[i]->generation;
            break;
        }
    }
    EndMutex(&as_state->lock);

    // Unloaded
    if (result.i32[0] == -1 && inLoadIfNotFound)
    {
        // TODO: growth strategy?
        Assert(as_state->assetCount < as_state->assetCapacity);
        AS_Asset *asset = AS_AllocAsset(inPath);
        result.i32[0]   = asset->id;
        result.i32[1]   = asset->generation;
    }
    return result;
}
internal Font *GetFont(AS_Handle handle)
{
    AS_Asset *asset = AS_GetAssetFromHandle(handle);
    Font *result    = &fontNil;
    if (asset)
    {
        Assert(asset->type == AS_Font);
        result = &asset->font;
    }
    return result;
}

internal LoadedSkeleton *GetSkeleton(AS_Handle handle)
{
    AS_Asset *asset        = AS_GetAssetFromHandle(handle);
    LoadedSkeleton *result = &skeletonNil;
    if (asset)
    {
        Assert(asset->type == AS_Skeleton);
        result = asset->skeleton;
    }
    return result;
}

internal Texture *GetTexture(AS_Handle handle)
{
    AS_Asset *asset = AS_GetAssetFromHandle(handle);
    Texture *result = 0;
    if (asset)
    {
        Assert(asset->type == AS_Texture);
        result = &asset->texture;
    }
    return result;
}

internal LoadedModel *GetModel(AS_Handle handle)
{
    AS_Asset *asset     = AS_GetAssetFromHandle(handle);
    LoadedModel *result = &modelNil;
    if (asset)
    {
        Assert(asset->type == AS_Model);
        result = &asset->model;
    }
    return result;
}
// internal LoadedSkeleton *GetSkeletonFromModel(AS_Handle handle)
// {
//     LoadedModel *model     = GetModel(handle);
//     LoadedSkeleton *result = model == &modelNil ? &skeletonNil : GetSkeleton(model->skeleton);
//     return result;
// }
internal KeyframedAnimation *GetAnim(AS_Handle handle)
{
    AS_Asset *asset            = AS_GetAssetFromHandle(handle);
    KeyframedAnimation *result = &animNil;
    if (asset)
    {
        Assert(asset->type == AS_Anim);
        result = &asset->anim;
    }
    return result;
}

inline b32 IsModelHandleNil(AS_Handle handle)
{
    LoadedModel *model = GetModel(handle);
    b32 result         = (model == 0 || model == &modelNil);
    return result;
}
inline b8 IsAnimNil(KeyframedAnimation *anim)
{
    b8 result = (anim == 0 || anim == &animNil);
    return result;
}

//////////////////////////////
// B-tree memory allocation
//

// How this allocator works:
//
// Allocation
//
// 1. Every time an asset is allocated, a size is passed in and a buffer is returned. A b-tree is used to
// keep track of previous allocations. The smallest memory block with an allocation size greater than the
// requested memory size is retrieved. If such a memory block exists, then it is used for the next steps.
// Otherwise, a new chunk of memory (w/ a size defined at startup) is used. This new chunk of memory is a "base
// block."
//
// 2. Once a new block is obtained, the memory block is split if the remaining size in the memory block is
// greater than the block size. A new block is created from these leftovers, and added to the b-tree.
//
// Free
//
// 1. When freeing a block, the next and previous nodes in the allocation chain are tested. If they are part of
// the same chain (i.e they share a base block) and are freed, then they are combined with the block. This
// block is then added to the b-tree.

// heavily inspired by: https://github.com/id-Software/DOOM-3-BFG/blob/master/neo/idlib/containers/BTree.h
// and https://github.com/id-Software/DOOM-3-BFG/blob/1caba1979589971b5ed44e315d9ead30b278d8b4/neo/idlib/Heap.h
internal AS_BTreeNode *AS_AllocNode()
{
    AS_CacheState *as_state = engine->GetAssetCacheState();
    AS_BTree *tree          = &as_state->allocator.bTree;
    AS_BTreeNode *newNode   = tree->free;

    if (newNode == 0)
    {
        newNode = PushStruct(as_state->allocator.arena, AS_BTreeNode);
    }
    else
    {
        StackPop(tree->free);
        newNode->memoryBlock = 0;
        newNode->key         = 0;
        newNode->numChildren = 0;
        newNode->parent      = 0;
        newNode->next        = 0;
        newNode->prev        = 0;
        newNode->first       = 0;
        newNode->last        = 0;
    }
    return newNode;
}

internal void AS_FreeNode(AS_BTreeNode *node)
{
    AS_CacheState *as_state = engine->GetAssetCacheState();
    StackPush(as_state->allocator.bTree.free, node);
}

// NOTE: All children of a node have a key <= to that of their parent. Only the leaves contain objects.
// This simplifies the control flow complexity.
internal AS_BTreeNode *AS_AddNode(AS_MemoryBlockNode *memNode)
{
    AS_CacheState *as_state = engine->GetAssetCacheState();
    AS_BTree *tree          = &as_state->allocator.bTree;
    AS_BTreeNode *newNode   = 0;

    // Assert(memNode->size < as_state->allocator.baseBlockSize);

    // If the root is null, allocate
    if (tree->root == 0)
    {
        tree->root = AS_AllocNode();
    }
    AS_BTreeNode *root = tree->root;

    // If the root is full, add another level (shift level of all current nodes down by 1, so that
    // memory blocks are found only in leaf nodes)
    if (root->numChildren > tree->maxChildren)
    {
        newNode              = AS_AllocNode();
        newNode->first       = root;
        newNode->last        = root;
        newNode->key         = root->key;
        newNode->numChildren = 1;
        root->parent         = newNode;
        AS_SplitNode(root);
        tree->root = newNode;
    }

    newNode              = AS_AllocNode();
    newNode->memoryBlock = memNode;
    i32 key              = memNode->size;
    newNode->key         = key;

    // How this works:
    // 1. the keys of the children of a node are <= to the key of their parent
    // Iterate down to leaf node

    AS_BTreeNode *child = 0;
    for (AS_BTreeNode *node = tree->root; node->first != 0; node = child)
    {
        if (node->key < key)
        {
            node->key = key;
        }
        for (child = node->first; child->next != 0; child = child->next)
        {
            if (child->key >= key)
            {
                break;
            }
        }
        if (child->memoryBlock)
        {
            if (child->key >= key)
            {
                // Add node before child
                newNode->prev = child->prev;
                newNode->next = child;
                if (newNode->prev == 0)
                {
                    node->first = newNode;
                }
                else
                {
                    newNode->prev->next = newNode;
                }
                newNode->next->prev = newNode;
            }
            else
            {
                // Add node after child
                newNode->prev = child;
                newNode->next = child->next;
                if (newNode->next == 0)
                {
                    node->last = newNode;
                }
                else
                {
                    newNode->next->prev = newNode;
                }
                newNode->prev->next = newNode;
            }
            newNode->parent = node;
            node->numChildren++;

#if 0

            // TODO: not sure if this is correct. need some way of visualizing this tree
            if (node != root && node->numChildren > tree->maxChildren)
            {
                AS_SplitNode(node);
                if (node->prev->key >= node->prev->last->key)
                {
                    node->prev->key = node->prev->last->key;
                }
            }
#endif

            return newNode;
        }
#if 1
        if (child->numChildren > tree->maxChildren)
        {
            AS_SplitNode(child);
            if (child->prev->key >= key)
            {
                child = child->prev;
            }
        }
#endif
    }

    Assert(root->numChildren == 0);
    newNode->parent = root;
    root->first     = newNode;
    root->last      = newNode;
    root->numChildren++;
    return newNode;
}

internal void AS_RemoveNode(AS_BTreeNode *node)
{
    AS_CacheState *as_state = engine->GetAssetCacheState();
    Assert(node->memoryBlock != 0);
    // Unlink from parent
    if (node->prev)
    {
        node->prev->next = node->next;
    }
    else
    {
        node->parent->first = node->next;
    }

    if (node->next)
    {
        node->next->prev = node->prev;
    }
    else
    {
        node->parent->last = node->prev;
    }
    node->parent->numChildren--;

    AS_BTreeNode *parent = 0;
    AS_BTree *tree       = &as_state->allocator.bTree;
    for (parent = node->parent; parent != tree->root && parent->numChildren < (tree->maxChildren + 1) / 2;
         parent = parent->parent)
    {
        if (parent->next)
        {
            parent = AS_MergeNodes(parent, parent->next);
        }
        else if (parent->prev)
        {
            parent = AS_MergeNodes(parent->prev, parent);
        }
        if (parent->key > parent->last->key)
        {
            parent->key = parent->last->key;
        }
        if (parent->numChildren > tree->maxChildren)
        {
            // Splitting the node will make the parent's parent have at least 2 children
            AS_SplitNode(parent);
            break;
        }
    }

    for (; parent != 0 && parent->last != 0; parent = parent->parent)
    {
        if (parent->key > parent->last->key)
        {
            parent->key = parent->last->key;
        }
    }
    AS_FreeNode(node);
    if (tree->root->numChildren == 1 && tree->root->memoryBlock == 0)
    {
        AS_BTreeNode *oldRoot     = tree->root;
        tree->root->first->parent = 0;
        tree->root                = tree->root->first;
        AS_FreeNode(oldRoot);
    }
}

internal void AS_SplitNode(AS_BTreeNode *node)
{
    AS_CacheState *as_state = engine->GetAssetCacheState();
    AS_BTreeNode *newNode   = AS_AllocNode();
    newNode->parent         = node->parent;
    AS_BTreeNode *child     = node->first;

    Assert(node->parent->numChildren < as_state->allocator.bTree.maxChildren);

    for (i32 i = 0; i < node->numChildren; i += 2)
    {
        child->parent = newNode;
        child         = child->next;
    }

    // All nodes before the current child node are now children of the newnode.
    newNode->key         = child->prev->key;
    newNode->numChildren = (node->numChildren + 1) / 2;
    newNode->next        = node;
    newNode->prev        = node->prev;
    newNode->first       = node->first;
    newNode->last        = child->prev;

    child->prev->next = 0;
    child->prev       = 0;

    if (node->prev)
    {
        node->prev->next = newNode;
    }
    else
    {
        node->parent->first = newNode;
    }
    node->prev  = newNode;
    node->first = child;
    node->numChildren -= newNode->numChildren;
    Assert(node->numChildren <= as_state->allocator.bTree.maxChildren);

    node->parent->numChildren++;
}

internal AS_BTreeNode *AS_MergeNodes(AS_BTreeNode *node1, AS_BTreeNode *node2)
{
    AS_CacheState *as_state = engine->GetAssetCacheState();
    Assert(node1->parent == node2->parent);
    Assert(node1->next == node2 && node2->prev == node1);
    Assert(node1->memoryBlock == 0 && node2->memoryBlock == 0);
    Assert(node1->numChildren <= (as_state->allocator.bTree.maxChildren + 1) / 2 &&
           node2->numChildren <= (as_state->allocator.bTree.maxChildren + 1) / 2);

    AS_BTreeNode *child = 0;
    for (child = node1->first; child->next != 0; child = child->next)
    {
        child->parent = node2;
    }

    child->parent     = node2;
    child->next       = node2->first;
    child->next->prev = child;
    node2->first      = node1->first;
    node2->prev       = node1->prev;
    node2->parent->numChildren--;
    node2->numChildren += node1->numChildren;

    if (node1->prev)
    {
        node1->prev->next = node2;
    }
    else
    {
        node1->parent->first = node2;
    }

    AS_FreeNode(node1);
    return node2;
}

internal AS_BTreeNode *AS_FindMemoryBlock(i32 size)
{
    AS_CacheState *as_state = engine->GetAssetCacheState();
    AS_BTreeNode *root      = as_state->allocator.bTree.root;

    AS_BTreeNode *result = 0;
    if (root != 0)
    {
        for (AS_BTreeNode *node = root->first; node != 0; node = node->first)
        {
            while (node->next != 0)
            {
                if (node->key >= size)
                {
                    break;
                }
                node = node->next;
            }
            if (node->memoryBlock)
            {
                if (node->key >= size)
                {
                    result = node;
                }
                break;
            }
        }
    }
    return result;
}

internal void AS_InitializeAllocator()
{
    AS_CacheState *as_state               = engine->GetAssetCacheState();
    Arena *arena                          = ArenaAlloc(megabytes(128));
    as_state->allocator.arena             = arena;
    as_state->allocator.bTree.root        = PushStruct(arena, AS_BTreeNode);
    as_state->allocator.bTree.maxChildren = 4;
    as_state->allocator.baseBlockSize     = megabytes(2);
    as_state->allocator.minBlockSize      = kilobytes(4);
}

internal u8 *AS_GetMemory(AS_MemoryBlockNode *node)
{
    u8 *result = (u8 *)(node) + sizeof(AS_MemoryBlockNode);
    return result;
}

internal AS_MemoryBlockNode *AS_GetMemoryBlock(u8 *memory)
{
    AS_MemoryBlockNode *result = (AS_MemoryBlockNode *)memory - 1;
    return result;
}

internal u8 *AS_GetMemory(AS_Asset *asset)
{
    return AS_GetMemory(asset->memoryBlock);
}

internal AS_MemoryBlockNode *AS_Alloc(i32 size)
{
    AS_CacheState *as_state = engine->GetAssetCacheState();
    BeginTicketMutex(&as_state->allocator.ticketMutex);
    i32 alignedSize                     = AlignPow2(size, 16);
    AS_DynamicBlockAllocator *allocator = &as_state->allocator;
    allocator->usedBlocks++;
    allocator->usedBlockMemory += alignedSize;
    allocator->numAllocs++;
    AS_BTreeNode *node = AS_FindMemoryBlock(size);

    AS_MemoryBlockNode *memoryBlock = 0;
    // Use an existing memory block for the allocation. Split it to prevent excessive leaks.
    if (node)
    {
        memoryBlock = node->memoryBlock;
        AS_RemoveNode(node);
        memoryBlock->node = 0;
        allocator->freeBlocks--;
        allocator->freeBlockMemory -= memoryBlock->size;
    }
    // Create an entirely separate block.
    else
    {
        i32 newSize              = Max(allocator->baseBlockSize, alignedSize + (i32)sizeof(AS_MemoryBlockNode));
        memoryBlock              = (AS_MemoryBlockNode *)PushArray(allocator->arena, u8, newSize);
        memoryBlock->isBaseBlock = true;
        memoryBlock->size        = newSize - (i32)sizeof(AS_MemoryBlockNode);
        memoryBlock->next        = 0;
        memoryBlock->prev        = allocator->last;
        Assert(memoryBlock->next == 0 && memoryBlock->node == 0);
        if (memoryBlock->prev)
        {
            memoryBlock->prev->next = memoryBlock;
        }
        else
        {
            allocator->first = memoryBlock;
        }
        allocator->last   = memoryBlock;
        memoryBlock->node = 0;
        allocator->baseBlocks++;
        allocator->baseBlockMemory += newSize;
    }

    // Split the newly allocated block if it's larger than the allocation size
    if ((i32)(memoryBlock->size - alignedSize - sizeof(AS_MemoryBlockNode)) > allocator->minBlockSize)
    {
        allocator->numResizes++;
        AS_MemoryBlockNode *newBlock =
            (AS_MemoryBlockNode *)((u8 *)(memoryBlock) + alignedSize + (i32)sizeof(AS_MemoryBlockNode));
        newBlock->isBaseBlock = 0;
        newBlock->size        = memoryBlock->size - alignedSize - (i32)sizeof(AS_MemoryBlockNode);
        Assert(newBlock->size > allocator->minBlockSize);
        newBlock->prev = memoryBlock;
        newBlock->next = memoryBlock->next;
        newBlock->node = 0;

        if (newBlock->next)
        {
            newBlock->next->prev = newBlock;
        }
        else
        {
            allocator->last = newBlock;
        }

        memoryBlock->size = alignedSize;
        memoryBlock->next = newBlock;

        allocator->freeBlocks++;
        allocator->freeBlockMemory += alignedSize;

        EndTicketMutex(&as_state->allocator.ticketMutex);
        AS_Free(newBlock);
        BeginTicketMutex(&as_state->allocator.ticketMutex);
    }
    EndTicketMutex(&as_state->allocator.ticketMutex);
    return memoryBlock;
}

internal void AS_Free(AS_MemoryBlockNode *memoryBlock)
{
    AS_CacheState *as_state = engine->GetAssetCacheState();
    BeginTicketMutex(&as_state->allocator.ticketMutex);
    Assert(memoryBlock->node == 0);
    Assert(memoryBlock->isBaseBlock == 0 || memoryBlock->isBaseBlock == 1);
    // Merge the next node if it's free and part of the same contiguous chain
    AS_DynamicBlockAllocator *allocator = &as_state->allocator;
    allocator->numFrees++;
    allocator->usedBlocks--;
    allocator->usedBlockMemory -= memoryBlock->size;

    AS_MemoryBlockNode *next = memoryBlock->next;
    if (next != 0 && !next->isBaseBlock && next->node != 0)
    {
        Assert(next->node->key == next->size);
        AS_RemoveNode(next->node);
        next->node = 0;
        allocator->freeBlocks--;
        allocator->freeBlockMemory -= next->size;

        memoryBlock->size = memoryBlock->size + sizeof(AS_MemoryBlockNode) + next->size;
        memoryBlock->next = next->next;
        if (next->next)
        {
            next->next->prev = memoryBlock;
        }
        else
        {
            allocator->last = next;
        }
    }

    // Merge the previous node if it's free and part of the same contiguous chain
    AS_MemoryBlockNode *prev = memoryBlock->prev;
    if (prev != 0 && !memoryBlock->isBaseBlock && prev->node != 0)
    {
        AS_RemoveNode(prev->node);
        prev->node = 0;
        allocator->freeBlocks--;
        allocator->freeBlockMemory -= next->size;

        prev->size = prev->size + sizeof(AS_MemoryBlockNode) + memoryBlock->size;
        prev->next = memoryBlock->next;
        if (memoryBlock->next)
        {
            memoryBlock->next->prev = prev;
        }
        else
        {
            allocator->last = prev;
        }
        // Add the three combined blocks to the free b-tree
        AS_AddNode(prev);
        allocator->freeBlocks++;
        allocator->freeBlockMemory += prev->size;
    }
    else
    {
        // Add the two combined blocks to the free b-tree
        AS_AddNode(memoryBlock);
        allocator->freeBlocks++;
        allocator->freeBlockMemory += memoryBlock->size;
    }
    EndTicketMutex(&as_state->allocator.ticketMutex);
}

internal void AS_Free(AS_Asset *asset)
{
    AS_Free(asset->memoryBlock);
    asset->memoryBlock = 0;
}

internal void AS_Free(void **ptr)
{
    AS_MemoryBlockNode *node = (AS_MemoryBlockNode *)(*ptr) - 1;
    AS_Free(node);
    *ptr = 0;
}
