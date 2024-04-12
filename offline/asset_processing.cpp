#include "./asset_processing.h"

#include "../platform_inc.cpp"
#include "../thread_context.cpp"
#include "../keepmovingforward_memory.cpp"
#include "../keepmovingforward_string.cpp"
#include "../job.cpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "../third_party/stb_truetype.h"

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
internal string ProcessAnimation(Arena *arena, CompressedKeyframedAnimation *outAnimation,
                                 aiAnimation *inAnimation);

JOB_CALLBACK(ProcessAnimation);

//////////////////////////////
// File Output
//
internal void WriteModelToFile(InputModel *model, string directory, string filename);
internal void WriteSkeletonToFile(Skeleton *skeleton, string filename);
internal void WriteAnimationToFile(CompressedKeyframedAnimation *animation, string filename);
JOB_CALLBACK(WriteSkeletonToFile);
JOB_CALLBACK(WriteModelToFile);
JOB_CALLBACK(WriteAnimationToFile);

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
// Model Loading
//
internal void *LoadAndWriteModel(void *ptr, Arena *arena)
{
    TempArena scratch = ScratchStart(&arena, 1);
    Data *data        = (Data *)ptr;
    string directory  = data->directory;
    string filename   = data->filename;
    string fullPath   = StrConcat(scratch.arena, directory, filename);

    InputModel model;

    // Load gltf file using Assimp
    Assimp::Importer importer;
    const char *file = (const char *)fullPath.str;
    const aiScene *scene =
        importer.ReadFile(file, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        Printf("Unable to load %S\n", fullPath);
        Assert(!":(");
    }

    filename                    = RemoveFileExtension(filename);
    aiMatrix4x4t<f32> transform = scene->mRootNode->mTransformation;
    model                       = LoadAllMeshes(scratch.arena, scene);

    // Load skeleton
    model.skeleton.parents            = PushArray(scratch.arena, i32, scene->mMeshes[0]->mNumBones);
    model.skeleton.transformsToParent = PushArray(scratch.arena, Mat4, scene->mMeshes[0]->mNumBones);

    MeshNodeInfoArray infoArray;
    infoArray.items = PushArrayNoZero(scratch.arena, MeshNodeInfo, 200);
    infoArray.cap   = 200;
    infoArray.count = 0;
    ProcessNode(scratch.arena, &infoArray, scene->mRootNode);

    // TODO: this is very janky because assimp doesn't have a continuity in count b/t
    // scene nodes, bones, and animation channels. probably need to get far away from assimp
    // asap
    {
        u32 shift = 0;
        {
            MeshNodeInfo *info;
            foreach (&infoArray, info)
            {
                string name    = info->name;
                b32 inSkeleton = false;
                for (u32 i = 0; i < model.skeleton.count; i++)
                {
                    if (name == model.skeleton.names[i])
                    {
                        inSkeleton = true;
                        break;
                    }
                }
                if (inSkeleton)
                {
                    break;
                }
                shift++;
            }
        }
        u32 parentCount    = 0;
        u32 transformCount = 0;
        for (u32 i = 0; i < model.skeleton.count; i++)
        {
            MeshNodeInfo *info = &infoArray.items[i + shift];
            string parentName  = info->parentName;
            i32 parentId       = -1;
            parentId           = FindNodeIndex(&model.skeleton, parentName);

            model.skeleton.parents[parentCount++] = parentId;
            Mat4 parentTransform                  = info->transformToParent;
            if (parentId == -1)
            {
                parentId = FindMeshNodeInfo(&infoArray, parentName);
                while (parentId != -1)
                {
                    info            = &infoArray.items[parentId];
                    parentTransform = info->transformToParent * parentTransform;
                    parentId        = FindMeshNodeInfo(&infoArray, info->parentName);
                }
            }
            model.skeleton.transformsToParent[transformCount++] = parentTransform;
        }
    }

    JS_Counter counter = {};
    // Process all animations and write to file
    JS_Counter animationCounter = {};
    u32 numAnimations           = scene->mNumAnimations;
    AnimationJobData *jobData   = PushArray(scratch.arena, AnimationJobData, numAnimations);
    Printf("Number of animations: %u\n", numAnimations);
    {
        for (u32 i = 0; i < numAnimations; i++)
        {
            jobData[i].outAnimation = PushStruct(scratch.arena, CompressedKeyframedAnimation);
            jobData[i].inAnimation  = scene->mAnimations[i];
            JS_Kick(ProcessAnimation, &jobData[i], 0, Priority_Normal, &animationCounter);
        }
        JS_Join(&animationCounter);
    }

    // AnimationJobWriteData *writeData = PushArray(scratch.arena, AnimationJobWriteData, numAnimations);
    for (u32 i = 0; i < numAnimations; i++)
    {
        AnimationJobWriteData *data = PushStruct(scratch.arena, AnimationJobWriteData);
        data->animation             = jobData[i].outAnimation;
        data->path                  = PushStr8F(scratch.arena, "%S%S.anim", directory, jobData[i].outName);
        Printf("Writing animation to file: %S\n", data->path);
        JS_Kick(WriteAnimationToFile, data, 0, Priority_Normal, &counter);
    }

    // Write skeleton file
    SkeletonJobData skelData = {};
    skelData.skeleton        = &model.skeleton;
    skelData.path            = PushStr8F(scratch.arena, "%S%S.skel", directory, filename);
    Printf("Writing skeleton to file: %S\n", skelData.path);
    JS_Kick(WriteSkeletonToFile, &skelData, 0, Priority_Normal, &counter);

    // Write model file (vertex data & dependencies)
    model.skeleton.filename = StrConcat(scratch.arena, filename, Str8Lit(".skel"));
    ModelJobData modelData  = {};
    modelData.model         = &model;
    modelData.directory     = directory;
    modelData.path          = PushStr8F(scratch.arena, "%S%S.model", directory, filename);
    Printf("Writing model to file: %S\n", modelData.path);
    JS_Kick(WriteModelToFile, &modelData, 0, Priority_Normal, &counter);

    JS_Join(&counter);
    ScratchEnd(scratch);
    return 0;
}

internal InputModel LoadAllMeshes(Arena *arena, const aiScene *scene)
{
    TempArena scratch    = ScratchStart(&arena, 1);
    u32 totalVertexCount = 0;
    u32 totalFaceCount   = 0;
    InputModel model     = {};
    loopi(0, scene->mNumMeshes)
    {
        aiMesh *mesh = scene->mMeshes[i];
        totalVertexCount += mesh->mNumVertices;
        totalFaceCount += mesh->mNumFaces;
    }

    AssimpSkeletonAsset assetSkeleton = {};
    assetSkeleton.inverseBindPoses    = PushArray(arena, Mat4, scene->mMeshes[0]->mNumBones);
    assetSkeleton.names               = PushArray(arena, string, scene->mMeshes[0]->mNumBones);
    assetSkeleton.vertexBoneInfo      = PushArray(arena, VertexBoneInfo, totalVertexCount);

    model.meshes    = PushArray(arena, InputMesh, scene->mNumMeshes);
    model.numMeshes = scene->mNumMeshes;

    for (i32 i = 0; i < scene->mNumMeshes; i++)
    {
        aiMesh *mesh             = scene->mMeshes[i];
        model.meshes[i].vertices = PushArrayNoZero(arena, MeshVertex, mesh->mNumVertices);
        model.meshes[i].indices  = PushArrayNoZero(arena, u32, mesh->mNumFaces * 3);
        // ProcessMesh(arena, &model, scene, mesh);
        ProcessMesh(arena, &model.meshes[i], scene, mesh);
    }

    u32 baseVertex = 0;
    loopi(0, scene->mNumMeshes)
    {
        aiMesh *mesh = scene->mMeshes[i];
        LoadBones(arena, &assetSkeleton, mesh, baseVertex);
        baseVertex += mesh->mNumVertices;
    }

    u32 vertexOffset = 0;
    for (u32 numMeshes = 0; numMeshes < model.numMeshes; numMeshes++)
    {
        InputMesh *inputMesh = &model.meshes[numMeshes];
        for (u32 vertexCount = 0; vertexCount < inputMesh->vertexCount; vertexCount++)
        {
            VertexBoneInfo *piece = &assetSkeleton.vertexBoneInfo[vertexCount + vertexOffset];
            for (u32 j = 0; j < MAX_MATRICES_PER_VERTEX; j++)
            {
                inputMesh->vertices[vertexCount].boneIds[j]     = piece->pieces[j].boneIndex;
                inputMesh->vertices[vertexCount].boneWeights[j] = piece->pieces[j].boneWeight;
            }
        }
        vertexOffset += inputMesh->vertexCount;
    }

    model.skeleton.count            = assetSkeleton.count;
    model.skeleton.inverseBindPoses = assetSkeleton.inverseBindPoses;
    model.skeleton.names            = assetSkeleton.names;

    ScratchEnd(scratch);
    return model;
}

// internal void ProcessMesh(Arena *arena, InputModel *model, const aiScene *scene, const aiMesh *mesh)
internal void ProcessMesh(Arena *arena, InputMesh *inputMesh, const aiScene *scene, const aiMesh *mesh)
{
    // u32 baseVertex = model->vertexCount;
    // u32 baseIndex  = model->indexCount;

    for (u32 i = 0; i < mesh->mNumVertices; i++)
    {
        MeshVertex vertex = {};
        vertex.position.x = mesh->mVertices[i].x;
        vertex.position.y = mesh->mVertices[i].y;
        vertex.position.z = mesh->mVertices[i].z;

        vertex.tangent.x = mesh->mTangents[i].x;
        vertex.tangent.y = mesh->mTangents[i].y;
        vertex.tangent.z = mesh->mTangents[i].z;

        if (mesh->HasNormals())
        {
            vertex.normal.x = mesh->mNormals[i].x;
            vertex.normal.y = mesh->mNormals[i].y;
            vertex.normal.z = mesh->mNormals[i].z;
        }
        if (mesh->mTextureCoords[0])
        {
            vertex.uv.u = mesh->mTextureCoords[0][i].x;
            vertex.uv.y = mesh->mTextureCoords[0][i].y;
        }
        else
        {
            vertex.uv = {0, 0};
        }
        inputMesh->vertices[inputMesh->vertexCount++] = vertex;
    }
    for (u32 i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace *face = &mesh->mFaces[i];
        for (u32 indexindex = 0; indexindex < face->mNumIndices; indexindex++)
        {
            inputMesh->indices[inputMesh->indexCount++] = face->mIndices[indexindex];
        }
    }
    aiMaterial *mMaterial = scene->mMaterials[mesh->mMaterialIndex];
    aiColor4D diffuse;
    aiGetMaterialColor(mMaterial, AI_MATKEY_COLOR_DIFFUSE, &diffuse);
    InputMaterial *material = &inputMesh->material;
    // material->startIndex      = baseIndex;
    // material->onePlusEndIndex = model->indices.count;
    // Diffuse
    for (u32 i = 0; i < mMaterial->GetTextureCount(aiTextureType_DIFFUSE); i++)
    {
        aiString str;
        mMaterial->GetTexture(aiTextureType_DIFFUSE, i, &str);
        string *texturePath = &material->texture[TextureType_Diffuse];
        texturePath->size   = str.length;
        texturePath->str    = PushArray(arena, u8, str.length);
        MemoryCopy(texturePath->str, str.data, str.length);
        *texturePath = PathSkipLastSlash(*texturePath);
    }
    // Specular
    for (u32 i = 0; i < mMaterial->GetTextureCount(aiTextureType_SPECULAR); i++)
    {
        aiString str;
        mMaterial->GetTexture(aiTextureType_SPECULAR, i, &str);
        string *texturePath = &material->texture[TextureType_Specular];
        texturePath->size   = str.length;
        texturePath->str    = PushArray(arena, u8, str.length);
        MemoryCopy(texturePath->str, str.data, str.length);
        *texturePath = PathSkipLastSlash(*texturePath);
    }
    // Normals
    for (u32 i = 0; i < mMaterial->GetTextureCount(aiTextureType_NORMALS); i++)
    {
        aiString str;
        mMaterial->GetTexture(aiTextureType_NORMALS, i, &str);
        string *texturePath = &material->texture[TextureType_Normal];
        texturePath->size   = str.length;
        texturePath->str    = PushArray(arena, u8, str.length);
        MemoryCopy(texturePath->str, str.data, str.length);
        *texturePath = PathSkipLastSlash(*texturePath);
    }
}

//////////////////////////////
// Skeleton
//
internal void LoadBones(Arena *arena, AssimpSkeletonAsset *skeleton, aiMesh *mesh, u32 baseVertex)
{
    skeleton->count = mesh->mNumBones;
    for (u32 i = 0; i < mesh->mNumBones; i++)
    {
        aiBone *bone = mesh->mBones[i];
        if (baseVertex == 0)
        {
            string name = Str8((u8 *)bone->mName.data, bone->mName.length);
            name        = PushStr8Copy(arena, name);

            skeleton->names[skeleton->nameCount++]                 = name;
            skeleton->inverseBindPoses[skeleton->inverseBPCount++] = ConvertAssimpMatrix4x4(bone->mOffsetMatrix);
        }

        for (u32 j = 0; j < bone->mNumWeights; j++)
        {
            aiVertexWeight *weight = bone->mWeights + j;
            u32 vertexId           = weight->mVertexId + baseVertex;
            f32 boneWeight         = weight->mWeight;
            if (boneWeight)
            {
                VertexBoneInfo *info = &skeleton->vertexBoneInfo[vertexId];
                Assert(info->numMatrices < MAX_MATRICES_PER_VERTEX);
                // NOTE: this doesn't increment the count of the array
                info->pieces[info->numMatrices].boneIndex  = i;
                info->pieces[info->numMatrices].boneWeight = boneWeight;
                info->numMatrices++;
            }
        }
    }
}

internal void ProcessNode(Arena *arena, MeshNodeInfoArray *nodeArray, const aiNode *node)
{
    u32 index                   = nodeArray->count++;
    MeshNodeInfo *nodeInfo      = &nodeArray->items[index];
    string name                 = Str8((u8 *)node->mName.data, node->mName.length);
    name                        = PushStr8Copy(arena, name);
    nodeInfo->name              = name;
    nodeInfo->transformToParent = ConvertAssimpMatrix4x4(node->mTransformation);
    if (node->mParent)
    {
        nodeInfo->hasParent  = true;
        string parentName    = Str8((u8 *)node->mParent->mName.data, node->mParent->mName.length);
        parentName           = PushStr8Copy(arena, parentName);
        nodeInfo->parentName = parentName;
    }
    else
    {
        nodeInfo->hasParent = false;
    }

    Assert(nodeArray->count < nodeArray->cap);

    for (u32 i = 0; i < node->mNumChildren; i++)
    {
        ProcessNode(arena, nodeArray, node->mChildren[i]);
    }
}

//////////////////////////////
// Animation Loading
//
internal string ProcessAnimation(Arena *arena, CompressedKeyframedAnimation *outAnimation,
                                 aiAnimation *inAnimation)

{
    outAnimation->boneChannels          = PushArray(arena, CompressedBoneChannel, inAnimation->mNumChannels);
    outAnimation->numNodes              = inAnimation->mNumChannels;
    outAnimation->duration              = (f32)inAnimation->mDuration / (f32)inAnimation->mTicksPerSecond;
    CompressedBoneChannel *boneChannels = outAnimation->boneChannels;
    f32 startTime                       = FLT_MAX;
    b8 change                           = 0;
    for (u32 i = 0; i < inAnimation->mNumChannels; i++)
    {
        CompressedBoneChannel *boneChannel = &boneChannels[i];
        aiNodeAnim *channel                = inAnimation->mChannels[i];
        string name;
        name.str  = PushArray(arena, u8, channel->mNodeName.length);
        name.size = channel->mNodeName.length;
        MemoryCopy(name.str, channel->mNodeName.data, name.size);
        name = PushStr8Copy(arena, name);

        boneChannel->name      = name;
        boneChannel->positions = PushArray(arena, AnimationPosition, channel->mNumPositionKeys);
        boneChannel->scales    = PushArray(arena, AnimationScale, channel->mNumScalingKeys);
        boneChannel->rotations = PushArray(arena, CompressedAnimationRotation, channel->mNumRotationKeys);

        boneChannel->numPositionKeys = 1;
        boneChannel->numScalingKeys  = 1;
        boneChannel->numRotationKeys = channel->mNumRotationKeys;

        f64 minTimeTest = Min(Min(channel->mPositionKeys[0].mTime, channel->mScalingKeys[0].mTime),
                              channel->mRotationKeys[0].mTime);
        f32 time        = (f32)minTimeTest / (f32)(inAnimation->mTicksPerSecond);
        if (time < startTime)
        {
            if (!(change && time == 0))
            {
                startTime = time;
                change++;
            }
        }

        V3 firstPosition = MakeV3(channel->mPositionKeys[0].mValue.x, channel->mPositionKeys[0].mValue.y,
                                  channel->mPositionKeys[0].mValue.z);
        f32 epsilon      = 0.000001;

        for (u32 j = 0; j < channel->mNumPositionKeys; j++)
        {
            aiVector3t<f32> aiPosition  = channel->mPositionKeys[j].mValue;
            AnimationPosition *position = boneChannel->positions + j;
            position->position          = MakeV3(aiPosition.x, aiPosition.y, aiPosition.z);
            if (!AlmostEqual(position->position, firstPosition, epsilon))
            {
                boneChannel->numPositionKeys = channel->mNumPositionKeys;
            }
            position->time = ((f32)channel->mPositionKeys[j].mTime / (f32)inAnimation->mTicksPerSecond - time);
        }

        V3 firstScale = MakeV3(channel->mScalingKeys[0].mValue.x, channel->mScalingKeys[0].mValue.y,
                               channel->mScalingKeys[0].mValue.z);
        for (u32 j = 0; j < channel->mNumScalingKeys; j++)
        {
            aiVector3t<f32> aiScale = channel->mScalingKeys[j].mValue;
            AnimationScale *scale   = boneChannel->scales + j;
            scale->scale            = MakeV3(aiScale.x, aiScale.y, aiScale.z);
            if (!AlmostEqual(scale->scale, firstScale, epsilon))
            {
                boneChannel->numScalingKeys = channel->mNumScalingKeys;
            }
            scale->time = ((f32)channel->mScalingKeys[j].mTime / (f32)inAnimation->mTicksPerSecond - time);
        }
        for (u32 j = 0; j < channel->mNumRotationKeys; j++)
        {
            aiQuaterniont<f32> aiQuat             = channel->mRotationKeys[j].mValue;
            CompressedAnimationRotation *rotation = boneChannel->rotations + j;
            Quat r                                = Normalize(MakeQuat(aiQuat.x, aiQuat.y, aiQuat.z, aiQuat.w));
            rotation->rotation[0]                 = CompressRotationChannel(r.x);
            rotation->rotation[1]                 = CompressRotationChannel(r.y);
            rotation->rotation[2]                 = CompressRotationChannel(r.z);
            rotation->rotation[3]                 = CompressRotationChannel(r.w);
            rotation->time = ((f32)channel->mRotationKeys[j].mTime / (f32)inAnimation->mTicksPerSecond - time);
        }
    }
    outAnimation->duration -= startTime == FLT_MAX ? 0 : startTime;

    // string animationName = {(u8 *)inAnimation->mName.data, inAnimation->mName.length};
    string animationName = Str8((u8 *)inAnimation->mName.data, inAnimation->mName.length);
    animationName        = PushStr8Copy(arena, animationName);
    return animationName;
}

JOB_CALLBACK(ProcessAnimation)
{
    AnimationJobData *jobData = (AnimationJobData *)data;
    string name               = ProcessAnimation(arena, jobData->outAnimation, jobData->inAnimation);
    jobData->outName          = name;
    return 0;
}

//////////////////////////////
// Convert
//
internal void WriteModelToFile(InputModel *model, string directory, string filename)
{
    StringBuilder builder = {};
    TempArena temp        = ScratchStart(0, 0);
    builder.arena         = temp.arena;

    // Get sizes and offsets for each section
    u64 totalOffset = 0;
    u64 totalSize   = 0;

    // Write the header
    // Put(&builder, Str8Lit("MAIN"));
    // Put(&builder, Str8Lit("TEMP"));
    // Put(&builder, Str8Lit("DBG "));

    /*
     * Types of sections:
     *
     * have currently
     * - GPU : sent to vram
     * - MAIN: allocated in main memory to be directly used by the asset cache
     *      - note if the data is sent to main memory & some transmutation is performed on it, but
     *      said transmutation doesn't change the size
     * - (maybe this is actually TEMP) DEP : dependency related information (e.g animations, materials)
     *
     * maybe need later
     * - TEMP: used only in loading then discarded
     * - DBG : used for debugging
     *
     * Each section is represented by a 4 char tag put in the file. Directly after the tag is a 64-bit
     * uint that says how big the section is (so the OS knows how much to read and such).
     */

    // Gpu memory
    // AssetFileSectionHeader gpu;
    {
        // gpu.tag = "GPU ";
        // gpu.size = sizeof(model->vertices.count) + model->vertices.count * sizeof(model->vertices[0]) +
        //            sizeof(model->indices.count) + model->indices.count * sizeof(model->indices[0]);
        // gpu.numBlocks = 2;

        // PutStruct(&builder, gpu);

        // AssetFileBlockHeader block;
        // block.count = model->vertices.count;
        // block.size  = (u32)(model->vertices.count * sizeof(model->vertices[0]));
        Put(&builder, model->numMeshes);
        for (u32 i = 0; i < model->numMeshes; i++)
        {
            InputMesh *mesh = &model->meshes[i];
            OptimizeMesh(mesh);

            Put(&builder, mesh->vertexCount);
            Put(&builder, mesh->vertices, sizeof(mesh->vertices[0]) * mesh->vertexCount);

            Put(&builder, mesh->indexCount);
            Put(&builder, mesh->indices, sizeof(mesh->indices[0]) * mesh->indexCount);

            Rect3 bounds = GetMeshBounds(mesh);
            PutStruct(&builder, bounds);

            // TODO: also bounds here

            Printf("Num vertices: %u\n", mesh->vertexCount);
            Printf("Num indices: %u\n", mesh->indexCount);

            for (u32 j = 0; j < TextureType_Count; j++)
            {
                if (mesh->material.texture[j].size != 0)
                {
                    string output = StrConcat(temp.arena, directory, mesh->material.texture[j]);
                    Printf("Writing texture filename: %S\n", output);
                    // Place the pointer to the string data
                    PutU64(&builder, output.size);
                    Put(&builder, output);
                }
                else
                {
                    PutU64(&builder, 0);
                }
            }
        }

        string output = StrConcat(temp.arena, directory, model->skeleton.filename);
        PutU64(&builder, output.size);
        Put(&builder, output);
    }

    b32 success = WriteEntireFile(&builder, filename);
    if (!success)
    {
        Printf("Failed to write file %S\n", filename);
    }
    ScratchEnd(temp);
}

JOB_CALLBACK(WriteModelToFile)
{
    ModelJobData *modelData = (ModelJobData *)data;
    WriteModelToFile(modelData->model, modelData->directory, modelData->path);
    return 0;
}

internal void WriteSkeletonToFile(Skeleton *skeleton, string filename)
{
    StringBuilder builder = {};
    TempArena temp        = ScratchStart(0, 0);
    builder.arena         = temp.arena;
    Put(&builder, skeletonVersionNumber);
    Put(&builder, skeleton->count);
    Printf("Num bones: %u\n", skeleton->count);

    for (u32 i = 0; i < skeleton->count; i++)
    {
        string *name = &skeleton->names[i];
        u64 offset   = PutPointer(&builder, 8);
        PutPointerValue(&builder, &name->size);
        Assert(builder.totalSize == offset);
        Put(&builder, *name);
    }

    PutArray(&builder, skeleton->parents, skeleton->count);
    PutArray(&builder, skeleton->inverseBindPoses, skeleton->count);
    PutArray(&builder, skeleton->transformsToParent, skeleton->count);

    b32 success = WriteEntireFile(&builder, filename);
    if (!success)
    {
        Printf("Failed to write file %S\n", filename);
        Assert(!"Failed");
    }
    ScratchEnd(temp);
}

JOB_CALLBACK(WriteSkeletonToFile)
{
    SkeletonJobData *jobData = (SkeletonJobData *)data;
    WriteSkeletonToFile(jobData->skeleton, jobData->path);
    return 0;
}

internal void WriteAnimationToFile(CompressedKeyframedAnimation *animation, string filename)
{
    StringBuilder builder = {};
    TempArena temp        = ScratchStart(0, 0);
    builder.arena         = temp.arena;

    u64 animationWrite   = PutPointerValue(&builder, animation);
    u64 boneChannelWrite = AppendArray(&builder, animation->boneChannels, animation->numNodes);

    u64 *stringDataWrites = PushArray(builder.arena, u64, animation->numNodes);
    u64 *positionWrites   = PushArray(builder.arena, u64, animation->numNodes);
    u64 *scalingWrites    = PushArray(builder.arena, u64, animation->numNodes);
    u64 *rotationWrites   = PushArray(builder.arena, u64, animation->numNodes);

    for (u32 i = 0; i < animation->numNodes; i++)
    {
        CompressedBoneChannel *boneChannel = animation->boneChannels + i;

        stringDataWrites[i] = Put(&builder, animation->boneChannels[i].name);
        positionWrites[i]   = AppendArray(&builder, boneChannel->positions, boneChannel->numPositionKeys);
        scalingWrites[i]    = AppendArray(&builder, boneChannel->scales, boneChannel->numScalingKeys);
        rotationWrites[i]   = AppendArray(&builder, boneChannel->rotations, boneChannel->numRotationKeys);
    }

    string result = CombineBuilderNodes(&builder);
    ConvertPointerToOffset(result.str, animationWrite + Offset(CompressedKeyframedAnimation, boneChannels),
                           boneChannelWrite);
    for (u32 i = 0; i < animation->numNodes; i++)
    {
        ConvertPointerToOffset(result.str,
                               boneChannelWrite + i * sizeof(CompressedBoneChannel) +
                                   Offset(CompressedBoneChannel, name) + Offset(string, str),
                               stringDataWrites[i]);
        ConvertPointerToOffset(result.str,
                               boneChannelWrite + i * sizeof(CompressedBoneChannel) +
                                   Offset(CompressedBoneChannel, positions),
                               positionWrites[i]);
        ConvertPointerToOffset(result.str,
                               boneChannelWrite + i * sizeof(CompressedBoneChannel) +
                                   Offset(CompressedBoneChannel, scales),
                               scalingWrites[i]);
        ConvertPointerToOffset(result.str,
                               boneChannelWrite + i * sizeof(CompressedBoneChannel) +
                                   Offset(CompressedBoneChannel, rotations),
                               rotationWrites[i]);
    }

    b32 success = WriteFile(filename, result.str, (u32)result.size);
    if (!success)
    {
        Printf("Failed to write file %S\n", filename);
    }
    ScratchEnd(temp);
}

JOB_CALLBACK(WriteAnimationToFile)
{
    AnimationJobWriteData *jobData = (AnimationJobWriteData *)data;
    WriteAnimationToFile(jobData->animation, jobData->path);
    return 0;
}

//////////////////////////////
// Helpers
//
internal i32 FindNodeIndex(Skeleton *skeleton, string name)
{
    i32 parentId = -1;
    for (u32 i = 0; i < skeleton->count; i++)
    {
        string *ptrName = &skeleton->names[i];
        if (*ptrName == name)
        {
            parentId = i;
            break;
        }
    }
    return parentId;
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
internal void OptimizeMesh(InputMesh *mesh)
{
    TempArena temp = ScratchStart(0, 0);

    InputMaterial *material = &mesh->material;
    u32 numFaces            = (mesh->indexCount) / 3;

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
        triData[i].totalScore = vertData[triData[i].vertIndices[0]].score +
                                vertData[triData[i].vertIndices[1]].score +
                                vertData[triData[i].vertIndices[2]].score;
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

        // For each vertex in the best triangle, update num remaining triangles for the vert, update the per
        // vertex remaining triangle list
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
            // TODO: use doubly linked list so vert can directly get its LRU node and then remove it?
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
    // for (u32 i = 0; i < numVertices; i++)
    // {
    //     VertData *vert = vertData + i;
    //     Assert(vert->remainingTriangles == 0);
    // }

    // Rewrite indices in the new order
    u32 indexCount = 0;
    for (u32 i = 0; i < numFaces; i++)
    {
        TriData *tri = triData + drawOrderList[i];
        for (u32 j = 0; j < 3; j++)
        {
            mesh->indices[indexCount++] = tri->vertIndices[j];
        }
    }

    // Rearrange the vertices based on the order of the faces
    MeshVertex *newVertices = PushArrayNoZero(temp.arena, MeshVertex, mesh->vertexCount);
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
            order[vertexIndex]              = count++;
            newVertices[order[vertexIndex]] = mesh->vertices[vertexIndex];
        }
        mesh->indices[i] = order[vertexIndex];
    }
    Assert(count == mesh->vertexCount);
    for (u32 i = 0; i < count; i++)
    {
        mesh->vertices[i] = newVertices[i];
    }

    ScratchEnd(temp);
}

//////////////////////////////
// Bounds
//

internal Rect3 GetMeshBounds(InputMesh *mesh)
{
    Rect3 rect;
    Init(&rect);
    for (u32 i = 0; i < mesh->vertexCount; i++)
    {
        MeshVertex *vertex = &mesh->vertices[i];
        if (vertex->position.x < rect.minP.x)
        {
            rect.minP.x = vertex->position.x;
        }
        if (vertex->position.x > rect.maxP.x)
        {
            rect.maxP.x = vertex->position.x;
        }
        if (vertex->position.y < rect.minP.y)
        {
            rect.minP.y = vertex->position.y;
        }
        if (vertex->position.y > rect.maxP.y)
        {
            rect.maxP.y = vertex->position.y;
        }
        if (vertex->position.z < rect.minP.z)
        {
            rect.minP.z = vertex->position.z;
        }
        if (vertex->position.z > rect.maxP.z)
        {
            rect.maxP.z = vertex->position.z;
        }
    }
    return rect;
}

// Model processing entry point
int main(int argc, char *argv[])
{
    ThreadContext tctx = {};
    ThreadContextInitialize(&tctx, 1);
    SetThreadName(Str8Lit("[Main Thread]"));

    OS_Init();

    JS_Init();
    // TODO: these are the steps
    // Load model using assimp, get the per vertex info, all of the animation data, etc.
    // Write out using the file format
    // could recursively go through every directory, loading all gltfs and writing them to the same directory

    TempArena scratch = ScratchStart(0, 0);
    // Max asset size
    Arena *arena       = ArenaAlloc(megabytes(4));
    JS_Counter counter = {};

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
                // TODO: support other file formats (fbx, obj, etc.) and make async
                if (MatchString(GetFileExtension(props.name), Str8Lit("gltf"),
                                MatchFlag_CaseInsensitive | MatchFlag_RightSideSloppy))
                {
                    string path = StrConcat(scratch.arena, directoryPath, props.name);
                    Printf("Loading file: %S\n", path);

                    Data *data      = PushStruct(scratch.arena, Data);
                    data->directory = directoryPath;
                    data->filename  = props.name;
                    JS_Kick(LoadAndWriteModel, data, 0, Priority_High, &counter);
                    // NOTE: this asset loading library just does not play well with mutltithreading
                    JS_Join(&counter);
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
