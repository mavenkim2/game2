#include "./asset_processing.h"

#include "../mkPlatformInc.cpp"
#include "../mkThreadContext.cpp"
#include "../mkMemory.cpp"
#include "../mkString.cpp"
#include "../mkJob.cpp"
#include "../mkJobsystem.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "../third_party/stb_truetype.h"

#define CGLTF_IMPLEMENTATION
#include "../third_party/cgltf.h"

#include <unordered_map>
#include <atomic>

Engine *engine;
//////////////////////////////
// Globals
//
global i32 skeletonVersionNumber = 1;
global i32 animationFileVersion  = 1;

//////////////////////////////
// Model Loading
//
internal InputModel LoadAllMeshes(Arena *arena, const aiScene *scene);
internal void ProcessMesh(Arena *arena, InputMesh *inputMesh, const aiScene *scene, const aiMesh *mesh);

//////////////////////////////
// Skeleton Loading
//
internal void LoadBones(Arena *arena, AssimpSkeletonAsset *skeleton, aiMesh *mesh, u32 baseVertex);
internal void ProcessNode(Arena *arena, MeshNodeInfoArray *nodeArray, const aiNode *node);

//////////////////////////////
// Animation Loading
//
internal string ProcessAnimation(Arena *arena, CompressedKeyframedAnimation *outAnimation, aiAnimation *inAnimation);

JOB_CALLBACK(ProcessAnimation);

//////////////////////////////
// File Output
//
internal void WriteModelToFile(InputModel *model, string directory, string filename);
internal void WriteSkeletonToFile(Skeleton *skeleton, string filename);
// internal void WriteAnimationToFile(CompressedKeyframedAnimation *animation, string filename);
JOB_CALLBACK(WriteSkeletonToFile);
JOB_CALLBACK(WriteModelToFile);
// JOB_CALLBACK(WriteAnimationToFile);

//////////////////////////////
// Helpers
//
internal Mat4 ConvertAssimpMatrix4x4(aiMatrix4x4t<f32> m);
internal i32 FindNodeIndex(Skeleton *skeleton, string name);
internal i32 FindMeshNodeInfo(MeshNodeInfoArray *array, string name);

inline AnimationTransform MakeAnimTform(V3 position, Quat rotation, V3 scale)
{
    AnimationTransform result;
    result.translation = position;
    result.rotation    = rotation;
    result.scale       = scale;
    return result;
}

//////////////////////////////
// Helpers
//
internal i32 FindNodeIndex(Skeleton *skeleton, string name)
{
    i32 id = -1;
    for (u32 i = 0; i < skeleton->count; i++)
    {
        string *ptrName = &skeleton->names[i];
        if (*ptrName == name)
        {
            id = i;
            break;
        }
    }
    return id;
}

internal Mat4 ConvertAssimpMatrix4x4(aiMatrix4x4t<f32> m)
{
    Mat4 result;
    result.a1 = m.a1;
    result.a2 = m.b1;
    result.a3 = m.c1;
    result.a4 = m.d1;

    result.b1 = m.a2;
    result.b2 = m.b2;
    result.b3 = m.c2;
    result.b4 = m.d2;

    result.c1 = m.a3;
    result.c2 = m.b3;
    result.c3 = m.c3;
    result.c4 = m.d3;

    result.d1 = m.a4;
    result.d2 = m.b4;
    result.d3 = m.c4;
    result.d4 = m.d4;
    return result;
}

internal i32 FindMeshNodeInfo(MeshNodeInfoArray *array, string name)
{
    i32 id = -1;
    MeshNodeInfo *info;
    foreach_index (array, info, i)
    {
        if (info->name == name)
        {
            id = i;
            break;
        }
    }
    return id;
}

//////////////////////////////
// Optimization
//

struct VertData
{
    i32 *triangleIndices;
    i32 remainingTriangles;
    i32 totalTriangles;

    i32 cachePos;
    f32 score;
};

struct LRUCache
{
    LRUCache *next;
    VertData *vert;
    i32 vertIndex;
};

struct TriData
{
    f32 totalScore;
    i32 vertIndices[3];
    b8 added;
};

const f32 lastTriangleScore = 0.75f;
const f32 cacheDecayPower   = 1.5f;
const i32 cacheSize         = 32;
const f32 valenceBoostPower = 0.5f;
const f32 valenceBoostScale = 2.f;

internal f32 CalculateVertexScore(VertData *data)
{
    if (data->remainingTriangles == 0)
    {
        return -1.0f;
    }

    f32 score         = 0.f;
    i32 cachePosition = data->cachePos;
    // Vertex used in the last triangle
    if (cachePosition < 0)
    {
    }
    else
    {
        if (cachePosition < 3)
        {
            score = lastTriangleScore;
        }
        else
        {
            Assert(cachePosition < cacheSize);
            score = 1.f - (cachePosition - 3) * (1.f / (cacheSize - 3));
            score = Powf(score, cacheDecayPower);
        }
    }
    f32 valenceBoost = Powf((f32)data->remainingTriangles, -valenceBoostPower);
    score += valenceBoostScale * valenceBoost;
    return score;
}

// https://tomforsyth1000.github.io/papers/fast_vert_cache_opt.html
internal void OptimizeMesh(InputMesh::MeshSubset *mesh)
{
    TempArena temp = ScratchStart(0, 0);

    u32 numFaces = (mesh->indexCount) / 3;

    // TODO: this over allocates. maybe have multiple meshes w/ multiple materials?
    u32 numVertices = mesh->vertexCount;

    TriData *triData   = PushArray(temp.arena, TriData, numFaces);
    VertData *vertData = PushArray(temp.arena, VertData, numVertices);

    u32 index = 0;

    // Increment per-vertex triangle count
    for (u32 i = 0; i < numFaces; i++)
    {
        TriData *tri = triData + i;

        for (u32 j = 0; j < 3; j++)
        {
            u32 idx             = mesh->indices[index++];
            tri->vertIndices[j] = idx;
            vertData[idx].remainingTriangles++;
        }
    }

    // Find the score for each vertex, allocate per vertex triangle list
    for (u32 i = 0; i < numVertices; i++)
    {
        VertData *vert = &vertData[i];
        // Assert(vert->remainingTriangles != 0);
        vert->cachePos        = -1;
        vert->triangleIndices = PushArray(temp.arena, i32, vert->remainingTriangles);
        vert->score           = CalculateVertexScore(vert);
    }

    // Add triangles to vertex list
    for (u32 i = 0; i < numFaces; i++)
    {
        TriData *tri = triData + i;
        for (u32 j = 0; j < 3; j++)
        {
            u32 idx                                       = tri->vertIndices[j];
            VertData *vert                                = vertData + idx;
            vert->triangleIndices[vert->totalTriangles++] = i;
            tri->totalScore += vert->score;
        }
    }

    i32 bestTriangle = -1;
    f32 bestScore    = -1.f;
    for (u32 i = 0; i < numFaces; i++)
    {
        triData[i].totalScore = vertData[triData[i].vertIndices[0]].score + vertData[triData[i].vertIndices[1]].score + vertData[triData[i].vertIndices[2]].score;
        if (bestTriangle == -1 || triData[i].totalScore > bestScore)
        {
            bestScore    = triData[i].totalScore;
            bestTriangle = i;
        }
    }

    // Initialize LRU
    // 3 extra temp nodes
    LRUCache *nodes = PushArrayNoZero(temp.arena, LRUCache, cacheSize + 3);

    LRUCache *freeList = 0;
    LRUCache *lruHead  = 0;

    for (i32 i = 0; i < cacheSize + 3; i++)
    {
        StackPush(freeList, nodes + i);
    }

    u32 *drawOrderList = PushArrayNoZero(temp.arena, u32, numFaces);

    for (u32 i = 0; i < numFaces; i++)
    {
        if (bestTriangle == -1)
        {
            bestTriangle = -1;
            bestScore    = -1.f;
            for (u32 j = 0; j < numFaces; j++)
            {
                TriData *tri = triData + j;
                if (!tri->added && tri->totalScore > bestScore)
                {
                    bestScore    = tri->totalScore;
                    bestTriangle = j;
                }
            }
        }
        Assert(bestTriangle != -1);
        drawOrderList[i] = bestTriangle;
        TriData *tri     = triData + bestTriangle;
        Assert(tri->added == 0);
        tri->added = true;

        // For each vertex in the best triangle, update num remaining triangles for the vert, update
        // the per vertex remaining triangle list
        for (u32 j = 0; j < 3; j++)
        {
            i32 vertIndex  = tri->vertIndices[j];
            VertData *vert = vertData + vertIndex;
            for (i32 k = 0; k < vert->totalTriangles; k++)
            {
                if (vert->triangleIndices[k] == bestTriangle)
                {
                    vert->triangleIndices[k] = -1;
                    break;
                }
            }
            Assert(vert->remainingTriangles-- > 0);

            // Move corresponding LRU node to the front
            LRUCache *node = 0;
            // If not in cache, allocate new node
            if (vert->cachePos == -1)
            {
                node = freeList;
                StackPop(freeList);
                node->vert      = vert;
                node->vertIndex = vertIndex;
                StackPush(lruHead, node);
            }
            // Find node in cache
            // TODO: use doubly linked list so vert can directly get its LRU node and then remove
            // it?
            else
            {
                node           = lruHead;
                LRUCache *last = 0;
                while (node->vertIndex != vertIndex)
                {
                    last = node;
                    node = node->next;
                }
                if (node != lruHead)
                {
                    Assert(last);
                    last->next = node->next;
                    StackPush(lruHead, node);
                }
            }
        }
        // Iterate over LRU, update positions in cache,
        LRUCache *node = lruHead;
        LRUCache *last = 0;
        u32 pos        = 0;

        u32 triIndicesToUpdate[256];
        u32 triIndicesToUpdateCount = 0;

        while (node != 0 && pos < cacheSize)
        {
            VertData *vert = node->vert;
            vert->cachePos = pos++;
            for (i32 j = 0; j < vert->totalTriangles; j++)
            {
                u32 triIndex = vert->triangleIndices[j];
                if (triIndex != -1 && !triData[triIndex].added)
                {
                    b8 repeated = 0;
                    for (u32 count = 0; count < triIndicesToUpdateCount; count++)
                    {
                        if (triIndicesToUpdate[count] == triIndex)
                        {
                            repeated = 1;
                            break;
                        }
                    }
                    Assert(triIndicesToUpdateCount < ArrayLength(triIndicesToUpdate));
                    if (!repeated)
                    {
                        triIndicesToUpdate[triIndicesToUpdateCount++] = triIndex;
                    }
                }
            }
            last        = node;
            node        = node->next;
            vert->score = CalculateVertexScore(vert);
        }

        // Remove extra nodes from cache
        last->next = 0;
        while (node != 0)
        {
            node->vert->cachePos = -1;
            node->vert           = 0;
            last                 = node;
            node                 = node->next;
            StackPush(freeList, last);
        }

        // Of the newly added triangles, find the best one to add next
        bestTriangle = -1;
        bestScore    = -1.f;
        for (u32 j = 0; j < triIndicesToUpdateCount; j++)
        {
            u32 triIndex    = triIndicesToUpdate[j];
            tri             = triData + triIndex;
            tri->totalScore = 0;
            for (u32 k = 0; k < 3; k++)
            {
                tri->totalScore += vertData[tri->vertIndices[k]].score;
            }
            if (tri->totalScore > bestScore)
            {
                bestTriangle = triIndex;
                bestScore    = tri->totalScore;
            }
        }
    }

    // Sanity checks
    // {
    //     for (u32 i = 0; i < numVertices; i++)
    //     {
    //         VertData *vert = vertData + i;
    //         Assert(vert->remainingTriangles == 0);
    //     }
    //
    //     u32 *test     = PushArray(temp.arena, u32, mesh->vertexCount);
    //     u32 testCount = 0;
    //     for (u32 i = 0; i < mesh->indexCount; i++)
    //     {
    //         u32 vertexIndex = mesh->indices[i];
    //         if (test[vertexIndex] == 0)
    //         {
    //             testCount++;
    //             test[vertexIndex] = 1;
    //         }
    //     }
    //     Assert(testCount == mesh->vertexCount);
    // }

    // Rewrite indices in the new order
    u32 idxCount = 0;
    for (u32 i = 0; i < numFaces; i++)
    {
        TriData *tri = triData + drawOrderList[i];
        for (u32 j = 0; j < 3; j++)
        {
            mesh->indices[idxCount++] = tri->vertIndices[j];
        }
    }

    // Rearrange the vertices based on the order of the faces
    V3 *newPositions = PushArray(temp.arena, V3, mesh->vertexCount);
    V3 *newNormals   = PushArray(temp.arena, V3, mesh->vertexCount);
    V3 *newTangents  = PushArray(temp.arena, V3, mesh->vertexCount);

    V2 *newUvs = 0;

    if (mesh->uvs)
    {
        newUvs = PushArray(temp.arena, V2, mesh->vertexCount);
    }
    UV4 *newBoneIds    = 0;
    V4 *newBoneWeights = 0;
    if (mesh->boneIds)
    {
        newBoneIds     = PushArray(temp.arena, UV4, mesh->vertexCount);
        newBoneWeights = PushArray(temp.arena, V4, mesh->vertexCount);
    }

    // Mapping from old indices to new indices
    i32 *order = PushArray(temp.arena, i32, mesh->vertexCount);
    for (u32 i = 0; i < mesh->vertexCount; i++)
    {
        order[i] = -1;
    }
    u32 count = 0;
    for (u32 i = 0; i < mesh->indexCount; i++)
    {
        u32 vertexIndex = mesh->indices[i];
        Assert(vertexIndex < mesh->vertexCount);
        if (order[vertexIndex] == -1)
        {
            order[vertexIndex] = count++;

            newPositions[order[vertexIndex]] = mesh->positions[vertexIndex];
            newNormals[order[vertexIndex]]   = mesh->normals[vertexIndex];
            newTangents[order[vertexIndex]]  = mesh->tangents[vertexIndex];

            if (newUvs)
            {
                newUvs[order[vertexIndex]] = mesh->uvs[vertexIndex];
            }
            if (newBoneIds)
            {
                newBoneIds[order[vertexIndex]]     = mesh->boneIds[vertexIndex];
                newBoneWeights[order[vertexIndex]] = mesh->boneWeights[vertexIndex];
            }
        }
        mesh->indices[i] = order[vertexIndex];
    }

    // Assert(count == mesh->vertexCount); NOTE: sometimes models have unused vertices
    for (u32 i = 0; i < mesh->vertexCount; i++)
    {
        mesh->positions[i] = newPositions[i];
    }
    for (u32 i = 0; i < mesh->vertexCount; i++)
    {
        mesh->normals[i] = newNormals[i];
    }
    for (u32 i = 0; i < mesh->vertexCount; i++)
    {
        mesh->tangents[i] = newTangents[i];
    }
    if (newUvs)
    {
        for (u32 i = 0; i < mesh->vertexCount; i++)
        {
            mesh->uvs[i] = newUvs[i];
        }
    }
    if (newBoneIds)
    {
        for (u32 i = 0; i < mesh->vertexCount; i++)
        {
            mesh->boneIds[i] = newBoneIds[i];
        }
        for (u32 i = 0; i < mesh->vertexCount; i++)
        {
            mesh->boneWeights[i] = newBoneWeights[i];
        }
    }

    ScratchEnd(temp);
}

//////////////////////////////
// Bounds
//

// internal Rect3 GetMeshBounds(InputMesh *mesh)
// {
//     Rect3 rect;
//     Init(&rect);
//     for (u32 i = 0; i < mesh->vertexCount; i++)
//     {
//         MeshVertex *vertex = &mesh->vertices[i];
//         if (vertex->position.x < rect.minP.x)
//         {
//             rect.minP.x = vertex->position.x;
//         }
//         if (vertex->position.x > rect.maxP.x)
//         {
//             rect.maxP.x = vertex->position.x;
//         }
//         if (vertex->position.y < rect.minP.y)
//         {
//             rect.minP.y = vertex->position.y;
//         }
//         if (vertex->position.y > rect.maxP.y)
//         {
//             rect.maxP.y = vertex->position.y;
//         }
//         if (vertex->position.z < rect.minP.z)
//         {
//             rect.minP.z = vertex->position.z;
//         }
//         if (vertex->position.z > rect.maxP.z)
//         {
//             rect.maxP.z = vertex->position.z;
//         }
//     }
//     return rect;
// }

internal void SkinModelToBindPose(const InputModel *inModel, Mat4 *outFinalTransforms)
{
    TempArena temp           = ScratchStart(0, 0);
    const Skeleton *skeleton = &inModel->skeleton;
    Mat4 *transformToParent  = PushArray(temp.arena, Mat4, skeleton->count);
    i32 previousId           = -1;

    for (i32 id = 0; id < (i32)skeleton->count; id++)
    {
        Mat4 parentTransform = skeleton->transformsToParent[id];
        i32 parentId         = skeleton->parents[id];
        if (parentId == -1)
        {
            transformToParent[id] = parentTransform;
        }
        else
        {
            Assert(!IsZero(transformToParent[parentId]));
            transformToParent[id] = transformToParent[parentId] * parentTransform;
        }

        Assert(id > previousId);
        previousId = id;
        // TODO: there's probably a way of representing this in non matrix form somehow
        outFinalTransforms[id] = transformToParent[id] * skeleton->inverseBindPoses[id];
    }
    ScratchEnd(temp);
}

struct LoadState
{
    std::unordered_map<cgltf_mesh *, i32> meshMap;
    list<Mat4> transforms;
    i32 count = 0;
};

// NOTE: fix this. some nodes aren't meshes :)
internal void LoadNode(cgltf_node *node, LoadState *state)
{
    if (node->mesh)
    {
        auto it = state->meshMap.find(node->mesh);
        if (it == state->meshMap.end())
        {
            state->meshMap[node->mesh] = state->count++;
            Mat4 transform;
            cgltf_node_transform_world(node, transform.elements[0]);
            state->transforms.push_back(transform);
            Assert(state->count == state->transforms.size());
        }
    }
    for (u32 i = 0; i < node->children_count; i++)
    {
        LoadNode(node->children[i], state);
    }
}

internal LoadState LoadNodes(cgltf_data *data)
{
    LoadState state;

    for (u32 i = 0; i < data->nodes_count; i++)
    {
        LoadNode(&data->nodes[i], &state);
    }
    return state;
}

PlatformApi platform;
// Model processing entry point
int main(int argc, char *argv[])
{
    platform = GetPlatform();

    ThreadContext tctx = {};
    ThreadContextInitialize(&tctx, 1);
    SetThreadName(Str8Lit("[Main Thread]"));

    Engine engineLocal;
    engine = &engineLocal;

    OS_Init();
    jobsystem::InitializeJobsystem();

    // JS_Init();
    // TODO: these are the steps
    // Load model using assimp, get the per vertex info, all of the animation data, etc.
    // Write out using the file format
    // could recursively go through every directory, loading all gltfs and writing them to the same
    // directory

    TempArena scratch = ScratchStart(0, 0);
    // Max asset size
    Arena *arena = ArenaAlloc(megabytes(4));
    // JS_Counter counter = {};

    string directories[1024];
    u32 size = 0;
    // TODO: Hardcoded
    string cwd          = StrConcat(scratch.arena, OS_GetCurrentWorkingDirectory(), Str8Lit("\\data"));
    directories[size++] = cwd;
    while (size != 0)
    {
        string directoryPath = directories[--size];
        OS_FileIter fileIter = OS_DirectoryIterStart(directoryPath, OS_FileIterFlag_SkipHiddenFiles);
        directoryPath        = StrConcat(scratch.arena, directoryPath, Str8Lit("\\"));
        for (OS_FileProperties props = {}; OS_DirectoryIterNext(scratch.arena, &fileIter, &props);)
        {
            if (!(props.isDirectory))
            {
                if (MatchString(GetFileExtension(props.name), Str8Lit("gltf"), MatchFlag_CaseInsensitive | MatchFlag_RightSideSloppy))
                {
                    string fullPath            = StrConcat(scratch.arena, directoryPath, props.name);
                    jobsystem::Counter counter = {};

                    string folderName = directoryPath;
                    folderName.size -= 1;
                    folderName = PathSkipLastSlash(folderName);

                    cgltf_options options = {};
                    cgltf_data *data      = 0;
                    cgltf_result result   = cgltf_parse_file(&options, (const char *)fullPath.str, &data);
                    Assert(result == cgltf_result_success);

                    result = cgltf_load_buffers(&options, data, (const char *)fullPath.str);
                    Assert(result == cgltf_result_success);

                    LoadState state = LoadNodes(data);

                    // Get all of the materials
                    jobsystem::KickJob(&counter, [data, folderName](jobsystem::JobArgs args) {
                        TempArena temp           = ScratchStart(0, 0);
                        InputMaterial *materials = PushArray(temp.arena, InputMaterial, data->materials_count);
                        for (size_t i = 0; i < data->materials_count; i++)
                        {
                            InputMaterial &material      = materials[i];
                            cgltf_material &gltfMaterial = data->materials[i];

                            material.name = Str8C(gltfMaterial.name);

                            auto FindUri = [&material](cgltf_texture_view &view, TextureType type) {
                                if (view.texture)
                                {
                                    if (view.texture->image->buffer_view)
                                    {
                                        Assert(0);
                                    }
                                    material.texture[type] = PathSkipLastSlash(Str8C(view.texture->image->uri));
                                }
                            };

                            if (gltfMaterial.has_pbr_metallic_roughness)
                            {
                                FindUri(gltfMaterial.pbr_metallic_roughness.base_color_texture, TextureType_Diffuse);
                                FindUri(gltfMaterial.pbr_metallic_roughness.metallic_roughness_texture, TextureType_MR);

                                material.metallicFactor  = gltfMaterial.pbr_metallic_roughness.metallic_factor;
                                material.roughnessFactor = gltfMaterial.pbr_metallic_roughness.roughness_factor;
                                for (i32 i = 0; i < 4; i++)
                                {
                                    material.baseColor[i] = gltfMaterial.pbr_metallic_roughness.base_color_factor[i];
                                }
                            }
                            else if (gltfMaterial.has_pbr_specular_glossiness)
                            {
                                FindUri(gltfMaterial.pbr_specular_glossiness.diffuse_texture, TextureType_Diffuse);
                                material.roughnessFactor = 1 - gltfMaterial.pbr_specular_glossiness.glossiness_factor;
                                for (i32 i = 0; i < 4; i++)
                                {
                                    material.baseColor[i] = gltfMaterial.pbr_specular_glossiness.diffuse_factor[i];
                                }
                            }

                            FindUri(gltfMaterial.normal_texture, TextureType_Normal);
                        }

                        // Build material file
                        StringBuilder builder = {};
                        builder.arena         = temp.arena;
                        PutLine(&builder, 0, "Num materials: %u\n", (u32)data->materials_count);
                        for (u32 i = 0; i < data->materials_count; i++)
                        {
                            InputMaterial &material = materials[i];
                            PutLine(&builder, 0, "Name: %S", material.name);
                            PutLine(&builder, 0, "{");

                            // Diffuse
                            if (material.texture[TextureType_Diffuse].size != 0)
                            {
                                PutLine(&builder, 1, "Diffuse: %S", material.texture[TextureType_Diffuse]);
                            }
                            // Base color rgba
                            if (material.baseColor != MakeV4(1))
                            {
                                PutLine(&builder, 1, "Color: %f %f %f %f", material.baseColor.r, material.baseColor.g, material.baseColor.b, material.baseColor.a);
                            }
                            // Normal
                            if (material.texture[TextureType_Normal].size != 0)
                            {
                                PutLine(&builder, 1, "Normal: %S", material.texture[TextureType_Normal]);
                            }
                            // Metallic roughness
                            if (material.texture[TextureType_MR].size != 0)
                            {
                                PutLine(&builder, 1, "MR Map: %S", material.texture[TextureType_MR]);
                            }
                            if (material.metallicFactor != 1.f)
                            {
                                PutLine(&builder, 1, "Metallic Factor: %f", material.metallicFactor);
                            }
                            if (material.roughnessFactor != 1.f)
                            {
                                PutLine(&builder, 1, "Roughness Factor: %f", material.roughnessFactor);
                            }
                            PutLine(&builder, 0, "}");
                        }

                        // Write to disk
                        string materialFilename = PushStr8F(temp.arena, "data\\materials\\%S.mtr", folderName);

                        b32 result = WriteEntireFile(&builder, materialFilename);
                        if (!result)
                        {
                            Printf("Unable to print material file: %S\n", materialFilename);
                            Assert(0);
                        }

                        ScratchEnd(temp);
                    });

                    // Write all of the models
                    TempArena temp = ScratchStart(&scratch.arena, 1);
                    InputModel model;
                    // Find total number of meshes
                    InputMesh *meshes = PushArrayNoZero(temp.arena, InputMesh, data->meshes_count);
                    model.meshes      = meshes;
                    model.numMeshes   = data->meshes_count;

                    // TODO: find a way to have per thread allocations that don't last for only one function scope?
                    // u32 numThreads = jobsystem::GetNumThreads();
                    // Arena **arenas = PushArrayNoZero(temp.arena, Arena **, numThreads);
                    Arena *arenas[16];
                    for (u32 i = 0; i < ArrayLength(arenas); i++)
                    {
                        arenas[i] = ArenaAlloc();
                    }

                    jobsystem::KickJobs(
                        &counter, data->meshes_count, 8, [&state, meshes, data, &arenas](jobsystem::JobArgs args) {
                            // Get the vertex/index attribute data
                            //
                            Arena *arena          = arenas[args.threadId];
                            cgltf_mesh *cgltfMesh = &data->meshes[args.jobId];
                            InputMesh *mesh       = &meshes[args.jobId];

                            mesh->subsets = PushArray(arena, InputMesh::MeshSubset, cgltfMesh->primitives_count);

                            mesh->transform        = state.transforms[state.meshMap[cgltfMesh]];
                            mesh->totalVertexCount = 0;
                            mesh->totalIndexCount  = 0;
                            mesh->flags            = 0;
                            mesh->totalSubsets     = cgltfMesh->primitives_count;

                            for (size_t primitiveIndex = 0; primitiveIndex < cgltfMesh->primitives_count; primitiveIndex++)
                            {
                                cgltf_primitive *primitive = &cgltfMesh->primitives[primitiveIndex];

                                InputMesh::MeshSubset *subset = &mesh->subsets[primitiveIndex];
                                if (primitive->material->name)
                                {
                                    subset->materialName = primitive->material->name;
                                }
                                else
                                {
                                    subset->materialName.size = 0;
                                }

                                subset->indexCount = primitive->indices->count;
                                mesh->totalIndexCount += subset->indexCount;
                                subset->indices = PushArrayNoZero(arena, u32, subset->indexCount);
                                for (size_t indexIndex = 0; indexIndex < primitive->indices->count; indexIndex++)
                                {
                                    subset->indices[indexIndex] = cgltf_accessor_read_index(primitive->indices, indexIndex);
                                }

                                // Get the attributes
                                u32 vertexCount = 0;
                                for (size_t attribIndex = 0; attribIndex < primitive->attributes_count; attribIndex++)
                                {
                                    cgltf_attribute *attribute = &primitive->attributes[attribIndex];
                                    if (Str8C(attribute->name) == Str8Lit("POSITION"))
                                    {
                                        vertexCount         = attribute->data->count;
                                        subset->vertexCount = vertexCount;
                                        mesh->totalVertexCount += vertexCount;
                                        subset->positions = PushArrayNoZero(arena, V3, vertexCount);

                                        for (u32 i = 0; i < attribute->data->count; i++)
                                        {
                                            cgltf_accessor_read_float(attribute->data, i, subset->positions[i].elements, 3);
                                        }

                                        Assert(attribute->data->has_max && attribute->data->has_min);
                                        V3 min;
                                        min.x             = attribute->data->min[0];
                                        min.y             = attribute->data->min[1];
                                        min.z             = attribute->data->min[2];
                                        mesh->bounds.minP = min;

                                        V3 max;
                                        max.x             = attribute->data->max[0];
                                        max.y             = attribute->data->max[1];
                                        max.z             = attribute->data->max[2];
                                        mesh->bounds.maxP = max;
                                    }
                                    else if (Str8C(attribute->name) == Str8Lit("NORMAL"))
                                    {
                                        subset->normals = PushArrayNoZero(arena, V3, vertexCount);
                                        for (u32 i = 0; i < attribute->data->count; i++)
                                        {
                                            cgltf_accessor_read_float(attribute->data, i, subset->normals[i].elements, 3);
                                        }
                                    }
                                    else if (Str8C(attribute->name) == Str8Lit("TANGENT"))
                                    {
                                        subset->tangents = PushArrayNoZero(arena, V3, vertexCount);
                                        for (u32 i = 0; i < attribute->data->count; i++)
                                        {
                                            V4 tangent;
                                            cgltf_accessor_read_float(attribute->data, i, tangent.elements, 4);
                                            subset->tangents[i] = tangent.xyz;
                                        }
                                    }
                                    else if (Str8C(attribute->name) == Str8Lit("TEXCOORD_0"))
                                    {
                                        mesh->flags |= MeshFlags_Uvs;
                                        subset->uvs = PushArrayNoZero(arena, V2, vertexCount);
                                        for (u32 i = 0; i < attribute->data->count; i++)
                                        {
                                            cgltf_accessor_read_float(attribute->data, i, subset->uvs[i].elements, 2);
                                        }
                                    }
                                    else if (Str8C(attribute->name) == Str8Lit("JOINTS_0"))
                                    {
                                        vertexCount = attribute->data->count;
                                        mesh->flags |= MeshFlags_Skinned;
                                        subset->boneIds = PushArrayNoZero(arena, UV4, vertexCount);
                                        for (u32 i = 0; i < attribute->data->count; i++)
                                        {
                                            cgltf_accessor_read_uint(attribute->data, i, subset->boneIds[i].elements, 4);
                                        }
                                    }
                                    else if (Str8C(attribute->name) == Str8Lit("WEIGHTS_0"))
                                    {
                                        subset->boneWeights = PushArrayNoZero(arena, V4, vertexCount);
                                        for (u32 i = 0; i < attribute->data->count; i++)
                                        {
                                            cgltf_accessor_read_float(attribute->data, i, subset->boneWeights[i].elements, 4);
                                        }
                                    }
                                    else if (Str8C(attribute->name) == Str8Lit("COLOR_0"))
                                    {
                                        // TODO
                                    }
                                }

                                Assert(subset->positions);
                                Assert(subset->normals);
                                // TODO: I'm going to have to generate these, either using mikkt or manually
                                Assert(subset->tangents);
                                OptimizeMesh(subset);
                            }
                            // ScratchEnd(temp);
                        },
                        jobsystem::Priority::High);

                    // Write the whole model to file
                    jobsystem::WaitJobs(&counter);

                    jobsystem::KickJob(&counter, [&model, &temp, data, folderName](jobsystem::JobArgs args) {
                        StringBuilder builder = {};
                        builder.arena         = temp.arena;
                        Put(&builder, model.numMeshes);
                        for (u32 meshIndex = 0; meshIndex < model.numMeshes; meshIndex++)
                        {
                            InputMesh *mesh = &model.meshes[meshIndex];

                            Put(&builder, mesh->totalVertexCount);
                            Put(&builder, mesh->flags);
                            Put(&builder, mesh->totalSubsets);

                            u32 indexOffset = 0;
                            for (u32 subsetIndex = 0; subsetIndex < mesh->totalSubsets; subsetIndex++)
                            {
                                InputMesh::MeshSubset *subset = &mesh->subsets[subsetIndex];
                                PutPointerValue(&builder, &indexOffset);
                                PutPointerValue(&builder, &subset->indexCount);
                                PutU64(&builder, subset->materialName.size);
                                if (subset->materialName.size != 0)
                                {
                                    Put(&builder, subset->materialName);
                                }

                                indexOffset += subset->indexCount;
                            }

                            // Positions
                            for (u32 subsetIndex = 0; subsetIndex < mesh->totalSubsets; subsetIndex++)
                            {
                                InputMesh::MeshSubset *subset = &mesh->subsets[subsetIndex];
                                Assert(subset->positions);
                                Put(&builder, subset->positions, sizeof(subset->positions[0]) * subset->vertexCount);
                            }

                            // Normals
                            for (u32 subsetIndex = 0; subsetIndex < mesh->totalSubsets; subsetIndex++)
                            {
                                InputMesh::MeshSubset *subset = &mesh->subsets[subsetIndex];
                                Assert(subset->normals);
                                Put(&builder, subset->normals, sizeof(subset->normals[0]) * subset->vertexCount);
                            }

                            // Tangents
                            for (u32 subsetIndex = 0; subsetIndex < mesh->totalSubsets; subsetIndex++)
                            {
                                InputMesh::MeshSubset *subset = &mesh->subsets[subsetIndex];
                                Assert(subset->tangents);
                                Put(&builder, subset->tangents, sizeof(subset->tangents[0]) * subset->vertexCount);
                            }

                            // Uvs. (what if some subsets have uvs and some don't?)
                            if (mesh->flags & MeshFlags_Uvs)
                            {
                                for (u32 subsetIndex = 0; subsetIndex < mesh->totalSubsets; subsetIndex++)
                                {
                                    InputMesh::MeshSubset *subset = &mesh->subsets[subsetIndex];
                                    Assert(subset->uvs);
                                    Put(&builder, subset->uvs, sizeof(subset->uvs[0]) * subset->vertexCount);
                                }
                            }

                            // Skinning data
                            if (mesh->flags & MeshFlags_Skinned)
                            {
                                for (u32 subsetIndex = 0; subsetIndex < mesh->totalSubsets; subsetIndex++)
                                {
                                    InputMesh::MeshSubset *subset = &mesh->subsets[subsetIndex];
                                    Assert(subset->boneIds && subset->boneWeights);
                                    Put(&builder, subset->boneIds, sizeof(subset->boneIds[0]) * subset->vertexCount);
                                    Put(&builder, subset->boneWeights, sizeof(subset->boneWeights[0]) * subset->vertexCount);
                                }
                            }

                            // Finally indices
                            Put(&builder, mesh->totalIndexCount);
                            for (u32 subsetIndex = 0; subsetIndex < mesh->totalSubsets; subsetIndex++)
                            {
                                InputMesh::MeshSubset *subset = &mesh->subsets[subsetIndex];
                                Assert(subset->indices);
                                Put(&builder, subset->indices, sizeof(subset->indices[0]) * subset->indexCount);
                            }

                            PutStruct(&builder, mesh->bounds);
                            PutStruct(&builder, mesh->transform);
                        }

                        if (data->skins_count != 0)
                        {
                            Assert(data->skins_count == 1);
                            PutU64(&builder, folderName.size);
                            Put(&builder, folderName);
                        }
                        else
                        {
                            PutU64(&builder, 0);
                        }
                        string modelFilename = PushStr8F(temp.arena, "data\\models\\%S.model", folderName);
                        b32 success          = WriteEntireFile(&builder, modelFilename);
                        if (!success)
                        {
                            Printf("Failed to write file %S\n", modelFilename);
                            Assert(0);
                        }

                        ScratchEnd(temp);
                    });

                    // Get any skeleton data
                    jobsystem::KickJob(&counter, [data, folderName](jobsystem::JobArgs args) {
                        TempArena temp = ScratchStart(0, 0);
                        for (size_t i = 0; i < data->skins_count; i++)
                        {
                            Assert(data->skins_count == 1);
                            cgltf_skin &skin   = data->skins[i];
                            Skeleton &skeleton = *PushStruct(temp.arena, Skeleton);
                            skeleton.count     = skin.joints_count;

                            std::unordered_map<cgltf_node *, i32> nodeToIndex;
                            skeleton.inverseBindPoses   = PushArrayNoZero(temp.arena, Mat4, skeleton.count);
                            skeleton.names              = PushArrayNoZero(temp.arena, string, skeleton.count);
                            skeleton.parents            = PushArrayNoZero(temp.arena, i32, skeleton.count);
                            skeleton.transformsToParent = PushArrayNoZero(temp.arena, Mat4, skeleton.count);

                            for (size_t jointIndex = 0; jointIndex < skeleton.count; jointIndex++)
                            {
                                cgltf_node *joint = skin.joints[jointIndex];
                                cgltf_accessor_read_float(skin.inverse_bind_matrices, jointIndex, skeleton.inverseBindPoses[jointIndex].elements[0], 16);
                                if (jointIndex == 0)
                                {
                                    cgltf_node_transform_world(joint, skeleton.transformsToParent[jointIndex].elements[0]);
                                }
                                else
                                {
                                    cgltf_node_transform_local(joint, skeleton.transformsToParent[jointIndex].elements[0]);
                                }
                                skeleton.names[jointIndex] = Str8C(joint->name);

                                auto it              = nodeToIndex.find(joint->parent);
                                i32 parentJointIndex = -1;
                                Assert(it == nodeToIndex.end() ? jointIndex == 0 : 1); // only the root node should have no parent
                                if (it != nodeToIndex.end())
                                {
                                    parentJointIndex = it->second;
                                }
                                skeleton.parents[jointIndex] = parentJointIndex;
                                nodeToIndex[joint]           = jointIndex;
                            }

                            // Write the skeleton to file
                            StringBuilder builder = {};
                            builder.arena         = temp.arena;
                            Put(&builder, skeletonVersionNumber);
                            Put(&builder, skeleton.count);
                            Printf("Num bones: %u\n", skeleton.count);

                            for (u32 i = 0; i < skeleton.count; i++)
                            {
                                string *name = &skeleton.names[i];
                                u64 offset   = PutPointer(&builder, 8);
                                PutPointerValue(&builder, &name->size);
                                Assert(builder.totalSize == offset);
                                Put(&builder, *name);
                            }

                            PutArray(&builder, skeleton.parents, skeleton.count);
                            PutArray(&builder, skeleton.inverseBindPoses, skeleton.count);
                            PutArray(&builder, skeleton.transformsToParent, skeleton.count);

                            string skeletonFilename = PushStr8F(temp.arena, "data\\skeletons\\%S.skel", folderName);
                            b32 success             = WriteEntireFile(&builder, skeletonFilename);
                            if (!success)
                            {
                                Printf("Failed to write file %S\n", skeletonFilename);
                                Assert(!"Failed");
                            }
                        }
                        ScratchEnd(temp);
                    });

                    // Get any animations
                    jobsystem::KickJob(&counter, [data](jobsystem::JobArgs args) {
                        TempArena temp                           = ScratchStart(0, 0);
                        CompressedKeyframedAnimation *animations = PushArrayNoZero(temp.arena, CompressedKeyframedAnimation, data->animations_count);
                        for (size_t i = 0; i < data->animations_count; i++)
                        {
                            CompressedKeyframedAnimation *animation = &animations[i];
                            cgltf_animation &anim                   = data->animations[i];
                            animation->boneChannels                 = PushArrayNoZero(temp.arena, CompressedBoneChannel, anim.channels_count);
                            u32 channelCount                        = 0;
                            std::unordered_map<cgltf_node *, i32> animationNodeIndexMap;

                            const f32 uninitialized = -10000000.f;
                            const f32 epsilon       = 0.000001f;
                            V3 firstPosition;
                            firstPosition.x       = uninitialized;
                            b32 addPositionToList = 0;

                            V3 firstScale;
                            firstScale.x       = uninitialized;
                            b32 addScaleToList = 0;

                            V4 firstRotation;
                            firstRotation.x       = uninitialized;
                            b32 addRotationToList = 0;

                            f32 minTime = FLT_MAX;
                            f32 maxTime = -FLT_MAX;

                            for (size_t channelIndex = 0; channelIndex < anim.channels_count; channelIndex++)
                            {
                                cgltf_animation_channel &channel = anim.channels[channelIndex];
                                // i32 jointIndex                   =
                                // nodeToIndex.find(channel.target_node)->second;
                                auto it                = animationNodeIndexMap.find(channel.target_node);
                                i32 animationNodeIndex = -1;
                                if (it == animationNodeIndexMap.end())
                                {
                                    animationNodeIndexMap[channel.target_node] = channelCount;
                                    animationNodeIndex                         = channelCount;
                                    CompressedBoneChannel &boneChannel         = animation->boneChannels[channelCount];
                                    boneChannel.name                           = Str8C(channel.target_node->name);
                                    boneChannel.positions                      = PushArrayNoZero(temp.arena, AnimationPosition, anim.channels_count);
                                    boneChannel.scales                         = PushArrayNoZero(temp.arena, AnimationScale, anim.channels_count);
                                    boneChannel.rotations                      = PushArrayNoZero(temp.arena, CompressedAnimationRotation, anim.channels_count);
                                    boneChannel.numPositionKeys                = 0;
                                    boneChannel.numScalingKeys                 = 0;
                                    boneChannel.numRotationKeys                = 0;

                                    channelCount++;
                                }
                                else
                                {
                                    animationNodeIndex = it->second;
                                }
                                CompressedBoneChannel &boneChannel = animation->boneChannels[animationNodeIndex];

                                f32 time;
                                Assert(cgltf_accessor_read_float(channel.sampler->input, 0, &time, 1));
                                minTime = Min(minTime, channel.sampler->input->min[0]);
                                maxTime = Max(maxTime, channel.sampler->input->max[0]);
                                // NOTE: subtracts the minimum time from the sample time, because
                                // sometimes the start time is really really late. also checks to
                                // see if all of the samples for position/scale/rotation are all
                                // roughly equal, and if so just store one copy otherwise store all
                                // of the copies
                                switch (channel.target_path)
                                {
                                    case cgltf_animation_path_type_translation:
                                    {
                                        V3 position;
                                        Assert(cgltf_accessor_read_float(channel.sampler->output, 0, position.elements, 3));
                                        if (firstPosition.x == uninitialized)
                                        {
                                            firstPosition                                               = position;
                                            boneChannel.positions[boneChannel.numPositionKeys].time     = time - channel.sampler->input->min[0];
                                            boneChannel.positions[boneChannel.numPositionKeys].position = position;
                                            boneChannel.numPositionKeys++;
                                        }
                                        else if (addPositionToList || !AlmostEqual(position, firstPosition, epsilon))
                                        {
                                            addPositionToList                                           = 1;
                                            boneChannel.positions[boneChannel.numPositionKeys].time     = time - channel.sampler->input->min[0];
                                            boneChannel.positions[boneChannel.numPositionKeys].position = position;
                                            boneChannel.numPositionKeys++;
                                        }
                                    }
                                    break;
                                    case cgltf_animation_path_type_rotation:
                                    {
                                        V4 rotation;
                                        Assert(cgltf_accessor_read_float(channel.sampler->output, 0, rotation.elements, 4));
                                        if (firstRotation.x == uninitialized)
                                        {
                                            firstRotation                                           = rotation;
                                            boneChannel.rotations[boneChannel.numRotationKeys].time = time - channel.sampler->input->min[0];
                                            for (u32 rotIndex = 0; rotIndex < 4; rotIndex++)
                                            {
                                                boneChannel.rotations[boneChannel.numRotationKeys].rotation[rotIndex] =
                                                    CompressRotationChannel(rotation[rotIndex]);
                                            }
                                            boneChannel.numRotationKeys++;
                                        }
                                        else if (addRotationToList || !AlmostEqual(rotation, firstRotation, epsilon))
                                        {
                                            addRotationToList                                       = 1;
                                            boneChannel.rotations[boneChannel.numRotationKeys].time = time - channel.sampler->input->min[0];
                                            for (u32 rotIndex = 0; rotIndex < 4; rotIndex++)
                                            {
                                                boneChannel.rotations[boneChannel.numRotationKeys].rotation[rotIndex] =
                                                    CompressRotationChannel(rotation[rotIndex]);
                                            }
                                            boneChannel.numRotationKeys++;
                                        }
                                    }
                                    break;
                                    case cgltf_animation_path_type_scale:
                                    {
                                        V3 scale;
                                        Assert(cgltf_accessor_read_float(channel.sampler->output, 0, scale.elements, 3));
                                        if (firstScale.x == uninitialized)
                                        {
                                            firstScale                                           = scale;
                                            boneChannel.scales[boneChannel.numScalingKeys].time  = time - channel.sampler->input->min[0];
                                            boneChannel.scales[boneChannel.numScalingKeys].scale = scale;
                                            boneChannel.numScalingKeys++;
                                        }
                                        else if (addScaleToList || !AlmostEqual(scale, firstScale, epsilon))
                                        {
                                            addScaleToList                                       = 1;
                                            boneChannel.scales[boneChannel.numScalingKeys].time  = time - channel.sampler->input->min[0];
                                            boneChannel.scales[boneChannel.numScalingKeys].scale = scale;
                                            boneChannel.numScalingKeys++;
                                        }
                                    }
                                    break;
                                }
                            }
                            animation->duration = maxTime - minTime;

                            // Write animation to file
                            StringBuilder builder = {};
                            builder.arena         = temp.arena;
                            u64 numNodes          = animationNodeIndexMap.size();

                            animation->numNodes = numNodes;

                            Put(&builder, (u32)numNodes);
                            Put(&builder, animation->duration);
                            for (u32 i = 0; i < numNodes; i++)
                            {
                                CompressedBoneChannel *boneChannel = &animation->boneChannels[i];

                                PutPointerValue(&builder, &boneChannel->name.size);
                                Put(&builder, boneChannel->name);
                                Put(&builder, boneChannel->numPositionKeys);
                                AppendArray(&builder, boneChannel->positions, boneChannel->numPositionKeys);
                                Put(&builder, boneChannel->numScalingKeys);
                                AppendArray(&builder, boneChannel->scales, boneChannel->numScalingKeys);
                                Put(&builder, boneChannel->numRotationKeys);
                                AppendArray(&builder, boneChannel->rotations, boneChannel->numRotationKeys);
                            }

                            string animationFilename = PushStr8F(temp.arena, "data\\animations\\%S.anim", Str8C(anim.name));
                            b32 success              = WriteEntireFile(&builder, animationFilename);
                            if (!success)
                            {
                                Printf("Failed to write file %S\n", animationFilename);
                                Assert(0);
                            }
                        }
                        ScratchEnd(temp);
                    });

                    jobsystem::WaitJobs(&counter);
                    // Free
                    cgltf_free(data);
                    for (u32 i = 0; i < ArrayLength(arenas); i++)
                    {
                        ArenaRelease(arenas[i]);
                    }
                }
            }
            else
            {
                string directory = StrConcat(scratch.arena, directoryPath, props.name);
                Printf("Adding directory: %S\n", directory);
                directories[size++] = directory;
            }
        }
        OS_DirectoryIterEnd(&fileIter);
    }
    ScratchEnd(scratch);
}
