#include "../keepmovingforward_common.h"
#include "../keepmovingforward_math.h"
#include "../keepmovingforward_memory.h"
#include "../keepmovingforward_string.h"
#include "../platform_inc.h"
#include "../thread_context.h"
#include "../job.h"
#include "./asset_processing.h"

#include "third_party/assimp/Importer.hpp"
#include "third_party/assimp/scene.h"
#include "third_party/assimp/postprocess.h"

#include "../platform_inc.cpp"
#include "../thread_context.cpp"
#include "../keepmovingforward_memory.cpp"
#include "../keepmovingforward_string.cpp"
#include "../job.cpp"

//////////////////////////////
// Globals
//
global i32 skeletonVersionNumber = 1;
global i32 animationFileVersion  = 1;

//////////////////////////////
// Model Loading
//
internal ModelOutput AssimpDebugLoadModel(Arena *arena, string filename);
internal Model LoadAllMeshes(Arena *arena, const aiScene *scene);
internal void ProcessMesh(Arena *arena, Model *model, const aiScene *scene, const aiMesh *mesh);

//////////////////////////////
// Skeleton Loading
//
internal void LoadBones(Arena *arena, AssimpSkeletonAsset *skeleton, aiMesh *mesh, u32 baseVertex);
internal void ProcessNode(Arena *arena, MeshNodeInfoArray *nodeArray, const aiNode *node);

//////////////////////////////
// Animation Loading
//
internal void ProcessAnimations(Arena *arena, const aiScene *scene, KeyframedAnimation *animationChannel);
internal void AssimpLoadAnimation(Arena *arena, string filename, KeyframedAnimation *animation);

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
internal ModelOutput AssimpDebugLoadModel(Arena *arena, string filename)
{
    ModelOutput result = {};
    Model model;

    Assimp::Importer importer;
    const char *file = (const char *)filename.str;
    const aiScene *scene =
        importer.ReadFile(file, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        return result;
    }

    aiMatrix4x4t<f32> transform = scene->mRootNode->mTransformation;
    model                       = LoadAllMeshes(arena, scene);

    // WriteModelToFile(&model, filename);

    ArrayInit(arena, model.skeleton.parents, i32, scene->mMeshes[0]->mNumBones);
    ArrayInit(arena, model.skeleton.transformsToParent, Mat4, scene->mMeshes[0]->mNumBones);

    TempArena scratch = ScratchStart(0, 0);

    MeshNodeInfoArray infoArray;
    infoArray.items = PushArrayNoZero(scratch.arena, MeshNodeInfo, 200);
    infoArray.cap   = 200;
    infoArray.count = 0;
    ProcessNode(arena, &infoArray, scene->mRootNode);

    // TODO: this is very janky because assimp doesn't have a continuity in count b/t
    // scene nodes, bones, and animation channels. probably need to get far away from assimp
    // asap
    u32 shift = 0;
    {
        MeshNodeInfo *info;
        foreach (&infoArray, info)
        {
            string name    = info->name;
            b32 inSkeleton = false;
            for (u32 i = 0; i < model.skeleton.names.count; i++)
            {
                if (name == model.skeleton.names.items[i])
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

    result.model     = model;
    result.animation = PushStruct(arena, KeyframedAnimation);
    ProcessAnimations(arena, scene, result.animation);

    ScratchEnd(scratch);

    return result;
}

internal Model LoadAllMeshes(Arena *arena, const aiScene *scene)
{
    TempArena scratch    = ScratchStart(0, 0);
    u32 totalVertexCount = 0;
    u32 totalFaceCount   = 0;
    Model model          = {};
    loopi(0, scene->mNumMeshes)
    {
        aiMesh *mesh = scene->mMeshes[i];
        totalVertexCount += mesh->mNumVertices;
        totalFaceCount += mesh->mNumFaces;
    }

    ArrayInit(arena, model.vertices, MeshVertex, totalVertexCount);
    ArrayInit(arena, model.indices, u32, totalFaceCount * 3);

    AssimpSkeletonAsset assetSkeleton = {};
    ArrayInit(arena, assetSkeleton.inverseBindPoses, Mat4, scene->mMeshes[0]->mNumBones);
    ArrayInit(arena, assetSkeleton.names, string, scene->mMeshes[0]->mNumBones);
    ArrayInit(scratch.arena, assetSkeleton.vertexBoneInfo, VertexBoneInfo, scene->mMeshes[0]->mNumBones);

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

    model.skeleton.count                  = assetSkeleton.count;
    model.skeleton.inverseBindPoses.items = assetSkeleton.inverseBindPoses.items;
    model.skeleton.inverseBindPoses.count = assetSkeleton.inverseBindPoses.count;
    model.skeleton.inverseBindPoses.cap   = assetSkeleton.inverseBindPoses.cap;
    model.skeleton.names.items            = assetSkeleton.names.items;
    model.skeleton.names.count            = assetSkeleton.names.count;
    model.skeleton.names.cap              = assetSkeleton.names.cap;

    ScratchEnd(scratch);
    return model;
}

internal void ProcessMesh(Arena *arena, Model *model, const aiScene *scene, const aiMesh *mesh)
{
    u32 baseVertex = model->vertices.count;
    u32 baseIndex  = model->indices.count;

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
        ArrayPush(&model->vertices, vertex);
    }
    for (u32 i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace *face = &mesh->mFaces[i];
        for (u32 indexindex = 0; indexindex < face->mNumIndices; indexindex++)
        {
            ArrayPush(&model->indices, face->mIndices[indexindex] + baseVertex);
        }
    }
    aiMaterial *mMaterial = scene->mMaterials[mesh->mMaterialIndex];
    aiColor4D diffuse;
    aiGetMaterialColor(mMaterial, AI_MATKEY_COLOR_DIFFUSE, &diffuse);
    Material *material        = &model->materials[model->materialCount++];
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

            ArrayPush(&skeleton->names, name);
            ArrayPush(&skeleton->inverseBindPoses, ConvertAssimpMatrix4x4(bone->mOffsetMatrix));
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
internal void ProcessAnimations(Arena *arena, const aiScene *scene, KeyframedAnimation *animationChannel)
{

    // for (u32 i = 0; i < scene->mNumAnimations; i++) {
    //     aiAnimation* animation = scene->mAnimations[i]
    // }
    aiAnimation *animation      = scene->mAnimations[0];
    animationChannel->duration  = (f32)animation->mDuration / (f32)animation->mTicksPerSecond;
    animationChannel->numFrames = 0;
    animationChannel->numNodes  = 0;
    BoneChannel *boneChannels   = PushArray(arena, BoneChannel, animation->mNumChannels);
    u32 boneCount               = 0;
    for (u32 i = 0; i < animation->mNumChannels; i++)
    {
        BoneChannel boneChannel;
        aiNodeAnim *channel = animation->mChannels[i];
        string name         = Str8((u8 *)channel->mNodeName.data, channel->mNodeName.length);
        name                = PushStr8Copy(arena, name);

        boneChannel.name = name;

        u32 iterateLength =
            Max(channel->mNumPositionKeys, Max(channel->mNumRotationKeys, channel->mNumScalingKeys));
        if (animationChannel->numFrames == 0)
        {
            animationChannel->numFrames = iterateLength;
        }
        u32 positionIndex = 0;
        u32 scaleIndex    = 0;
        u32 rotationIndex = 0;
        for (u32 j = 0; j < iterateLength; j++)
        {
            aiVector3t<f32> aiPosition = channel->mPositionKeys[positionIndex].mValue;
            V3 position                = MakeV3(aiPosition.x, aiPosition.y, aiPosition.z);
            aiQuaterniont<f32> aiQuat  = channel->mRotationKeys[rotationIndex].mValue;
            Quat rotation              = MakeQuat(aiQuat.x, aiQuat.y, aiQuat.z, aiQuat.w);
            aiVector3t<f32> aiScale    = channel->mScalingKeys[scaleIndex].mValue;
            V3 scale                   = MakeV3(aiScale.x, aiScale.y, aiScale.z);

            Assert(!IsZero(scale));
            Assert(!IsZero(rotation));
            AnimationTransform transform = MakeAnimTform(position, rotation, scale);
            boneChannel.transforms[j]    = transform;
            positionIndex++;
            rotationIndex++;
            scaleIndex++;

            if (positionIndex == channel->mNumPositionKeys)
            {
                positionIndex--;
            }
            if (rotationIndex == channel->mNumRotationKeys)
            {
                rotationIndex--;
            }
            if (scaleIndex == channel->mNumScalingKeys)
            {
                scaleIndex--;
            }
            // NOTE: cruft because converting from mTime to key frames

            f64 positionTime = channel->mPositionKeys[positionIndex].mTime;
            f64 rotationTime = channel->mRotationKeys[rotationIndex].mTime;
            f64 scaleTime    = channel->mScalingKeys[scaleIndex].mTime;

            if (channel->mNumPositionKeys < channel->mNumScalingKeys &&
                channel->mNumPositionKeys < channel->mNumRotationKeys &&
                (positionTime > rotationTime || positionTime > scaleTime))
            {
                positionIndex--;
            }
            if (channel->mNumRotationKeys < channel->mNumScalingKeys &&
                channel->mNumRotationKeys < channel->mNumPositionKeys &&
                (rotationTime > positionTime || rotationTime > scaleTime))
            {
                rotationIndex--;
            }
            if (channel->mNumScalingKeys < channel->mNumRotationKeys &&
                channel->mNumScalingKeys < channel->mNumPositionKeys &&
                (scaleTime > rotationTime || scaleTime > positionTime))
            {
                scaleIndex--;
            }
        }
        boneChannels[boneCount++] = boneChannel;
    }
    animationChannel->boneChannels = boneChannels;
    animationChannel->numNodes     = boneCount;
}

internal void AssimpLoadAnimation(Arena *arena, string filename, KeyframedAnimation *animation)
{
    Assimp::Importer importer;
    const char *file = (const char *)filename.str;
    const aiScene *scene =
        importer.ReadFile(file, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        return;
    }

    ProcessAnimations(arena, scene, animation);
}

//////////////////////////////
// Convert
//
internal void WriteModelToFile(Model *model, string filename)
{
    StringBuilder builder = {};
    TempArena temp        = ScratchStart(0, 0);
    builder.scratch       = temp;
    Put(&builder, model->vertices.count);
    Put(&builder, model->indices.count);
    PutArray(&builder, model->vertices);
    PutArray(&builder, model->indices);

    // Add texture filenames
    // for (u32 i = 0; i < model->materialCount; i++)
    // {
    //     for (u32 j = 0; j < TextureType_Count; j++)
    //     {
    //         if (model->materials[i].texture[j].size != 0)
    //         {
    //             Put(&builder, model->materials[i].texture[j].size);
    //             Put(&buililer, model->materials[i].texture[j]);
    //         }
    //     }
    // }

    b32 success = WriteEntireFile(&builder, filename);
    if (!success)
    {
        Printf("Failed to write file %S\n", filename);
    }
    ScratchEnd(temp);
}

internal void WriteSkeletonToFile(Skeleton *skeleton, string filename)
{
    StringBuilder builder = {};
    TempArena temp        = ScratchStart(0, 0);
    builder.scratch       = temp;
    Put(&builder, skeletonVersionNumber);
    Put(&builder, skeleton->count);

    // TODO: maybe get rid of names entirely for bones
    string *name;
    foreach (&skeleton->names, name)
    {
        string output = PushStr8F(temp.arena, "%S\n", *name);
        Put(&builder, output);
    }
    PutArray(&builder, skeleton->parents);
    PutArray(&builder, skeleton->inverseBindPoses);
    PutArray(&builder, skeleton->transformsToParent);

    b32 success = WriteEntireFile(&builder, filename);
    if (!success)
    {
        Printf("Failed to write file %S\n", filename);
    }
    ScratchEnd(temp);
}

internal void WriteAnimationToFile(KeyframedAnimation *animation, string filename)
{
    StringBuilder builder = {};
    TempArena temp        = ScratchStart(0, 0);
    builder.scratch       = temp;

    Put(&builder, animationFileVersion);
    Put(&builder, animation->numNodes);
    PutPointer(&builder, &animation->duration);
    Put(&builder, animation->numFrames);

    loopi(0, animation->numNodes)
    {
        BoneChannel *channel = animation->boneChannels + i;
        string output        = PushStr8F(temp.arena, "%S\n", channel->name);
        Put(&builder, output);
        Put(&builder, channel->transforms, sizeof(channel->transforms[0]) * animation->numFrames);
    }

    b32 success = WriteEntireFile(&builder, filename);
    if (!success)
    {
        Printf("Failed to write file %S\n", filename);
    }
    ScratchEnd(temp);
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
                // TODO: support other file formats (fbx, obj, etc.)
                if (MatchString(GetFileExtension(props.name), Str8Lit("gltf"),
                                MatchFlag_CaseInsensitive | MatchFlag_RightSideSloppy))
                {
                    string path = StrConcat(scratch.arena, directoryPath, props.name);
                    Printf("Loading file: %S\n", path);
                }
                int yippi_kai_yay = 0;
            }
            else
            {
                string directory = StrConcat(scratch.arena, directoryPath, props.name);
                Printf("Adding directory: %S\n", directory);
                directories[size++] = directory;
                int boohoo          = true;
            }
        }
        OS_DirectoryIterEnd(&fileIter);
    }
    ScratchEnd(scratch);
}
