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
internal void ProcessMesh(Arena *arena, InputModel *model, const aiScene *scene, const aiMesh *mesh);

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
        for (u32 i = 0; i < model.skeleton.names.count; i++)
        {
            MeshNodeInfo *info = &infoArray.items[i + shift];
            string parentName  = info->parentName;
            i32 parentId       = -1;
            parentId           = FindNodeIndex(&model.skeleton, parentName);
            ArrayPush(&model.skeleton.parents, parentId);
            Mat4 parentTransform = info->transformToParent;
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
            ArrayPush(&model.skeleton.transformsToParent, parentTransform);
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

    model.vertices = PushArray(arena, MeshVertex, totalVertexCount);
    model.indices  = PushArray(arena, u32, totalFaceCount * 3);

    AssimpSkeletonAsset assetSkeleton = {};
    assetSkeleton.inverseBindPoses    = PushArray(arena, Mat4, scene->mMeshes[0]->mNumBones);
    assetSkeleton.names               = PushArray(arena, string, scene->mMeshes[0]->mNumBones);
    assetSkeleton.vertexBneInfo       = PushArray(arena, VertexBoneInfo, totalVertexCount);

    loopi(0, scene->mNumMeshes)
    {
        aiMesh *mesh = scene->mMeshes[i];
        ProcessMesh(arena, &model, scene, mesh);
    }

    u32 baseVertex = 0;
    loopi(0, scene->mNumMeshes)
    {
        aiMesh *mesh = scene->mMeshes[i];
        LoadBones(arena, &assetSkeleton, mesh, baseVertex);
        baseVertex += mesh->mNumVertices;
    }

    loopi(0, totalVertexCount)
    {
        VertexBoneInfo *piece = &assetSkeleton.vertexBoneInfo.items[i];
        loopj(0, MAX_MATRICES_PER_VERTEX)
        {
            model.vertices.items[i].boneIds[j]     = piece->pieces[j].boneIndex;
            model.vertices.items[i].boneWeights[j] = piece->pieces[j].boneWeight;
        }
    }

    model.skeleton.count            = assetSkeleton.count;
    model.skeleton.inverseBindPoses = assetSkeleton.inverseBindPoses;
    model.skeleton.names            = assetSkeleton.names;

    ScratchEnd(scratch);
    return model;
}

internal void ProcessMesh(Arena *arena, InputModel *model, const aiScene *scene, const aiMesh *mesh)
{
    u32 baseVertex = model->vertexCount;
    u32 baseIndex  = model->indexCount;

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
        model->vertices[model->vertexCount++] = vertex;
    }
    for (u32 i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace *face = &mesh->mFaces[i];
        for (u32 indexindex = 0; indexindex < face->mNumIndices; indexindex++)
        {
            model->indices[model->indexCount++] = face->mIndices[indexindex] + baseVertex;
        }
    }
    aiMaterial *mMaterial = scene->mMaterials[mesh->mMaterialIndex];
    aiColor4D diffuse;
    aiGetMaterialColor(mMaterial, AI_MATKEY_COLOR_DIFFUSE, &diffuse);
    InputMaterial *material   = &model->materials[model->materialCount++];
    material->startIndex      = baseIndex;
    material->onePlusEndIndex = model->indices.count;
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
            skeleton->inverseBindPoses[skeleton->inverseBPCount++] = ConvertAssimpMatrix4x4(bone->mOffsetMatrix));
        }

        for (u32 j = 0; j < bone->mNumWeights; j++)
        {
            aiVertexWeight *weight = bone->mWeights + j;
            u32 vertexId           = weight->mVertexId + baseVertex;
            f32 boneWeight         = weight->mWeight;
            if (boneWeight)
            {
                VertexBoneInfo *info = &skeleton->vertexBoneInfo.items[vertexId];
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
    Printf("Change: %i\n", change);

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
    OptimizeModel(model);

    // Gpu memory
    AssetFileSectionHeader gpu;
    {
        gpu.tag = "GPU ";
        // gpu.size = sizeof(model->vertices.count) + model->vertices.count * sizeof(model->vertices[0]) +
        //            sizeof(model->indices.count) + model->indices.count * sizeof(model->indices[0]);
        gpu.numBlocks = 2;

        PutStruct(&builder, gpu);

        AssetFileBlockHeader block;
        block.count = model->vertices.count;
        block.size  = (u32)(model->vertices.count * sizeof(model->vertices[0]));
        PutStruct(&builder, block);
        PutArray(&builder, model->vertices);

        block.count = model->indices.count;
        block.size  = model->indices.count * sizeof(model->indices[0]);
        PutStruct(&builder, block);
        PutArray(&builder, model->indices);

        Printf("Num vertices: %u\n", model->vertices.count);
        Printf("Num indices: %u\n", model->indices.count);
    }

    AssetFileSectionHeader temp;
    {
        temp.tag       = "TEMP";
        temp.numBlocks = 1;
        PutStruct(&builder, temp);

        // Add materials
        Put(&builder, model->materialCount);
        for (u32 i = 0; i < model->materialCount; i++)
        {
            Put(&builder, model->materials[i].startIndex);
            Put(&builder, model->materials[i].onePlusEndIndex);
            for (u32 j = 0; j < TextureType_Count; j++)
            {
                Put(&builder, Str8Lit("marker"));
                if (model->materials[i].texture[j].size != 0)
                {
                    string output = StrConcat(temp.arena, directory, model->materials[i].texture[j]);
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

        // Add skeleton dependency
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

    string *name;
    foreach (&skeleton->names, name)
    {
        u64 offset = PutPointer(&builder, 8);
        PutPointerValue(&builder, &name->size);
        Assert(builder.totalSize == offset);
        Put(&builder, *name);
    }
    PutArray(&builder, skeleton->parents);
    PutArray(&builder, skeleton->inverseBindPoses);
    PutArray(&builder, skeleton->transformsToParent);

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
    string *ptrName;
    foreach_index (&skeleton->names, ptrName, i)
    {
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

// Model processing entry point
int main(int argc, char *argv[])
{
    OS_Init();

    ThreadContext tctx = {};
    ThreadContextInitialize(&tctx, 1);
    SetThreadName(Str8Lit("[Main Thread]"));

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
