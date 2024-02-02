#define STB_IMAGE_IMPLEMENTATION
// #define STBI_ONLY_TGA
#include "third_party/stb_image.h"

#if INTERNAL
#include "third_party/assimp/Importer.hpp"
#include "third_party/assimp/scene.h"
#include "third_party/assimp/postprocess.h"
#endif

inline AnimationTransform MakeAnimTform(V3 position, Quat rotation, V3 scale)
{
    AnimationTransform result;
    result.translation = position;
    result.rotation    = rotation;
    result.scale       = scale;
    return result;
}
// TODO: <= or < ?
inline b32 IsEndOfFile(Iter *iter)
{
    b32 result = iter->cursor == iter->end;
    return result;
}

inline u32 GetType(Iter iter)
{
    String8 type = Str8(iter.cursor, 2);
    u32 objType  = OBJ_Invalid;
    if (type == Str8C("v "))
    {
        objType = OBJ_Vertex;
    }
    else if (type == Str8C("vn"))
    {
        objType = OBJ_Normal;
    }
    else if (type == Str8C("vt"))
    {
        objType = OBJ_Texture;
    }
    else if (type == Str8C("f "))
    {
        objType = OBJ_Face;
    }
    return objType;
}

// TODO: is float imprecision solvable?
inline f32 ReadFloat(Iter *iter)
{
    f32 value    = 0;
    i32 exponent = 0;
    u8 c;
    b32 valueSign = (*iter->cursor == '-');
    if (valueSign || *iter->cursor == '+')
    {
        iter->cursor++;
    }
    while (CharIsDigit((c = *iter->cursor++)))
    {
        value = value * 10.0f + (c - '0');
    }
    if (c == '.')
    {
        while (CharIsDigit((c = *iter->cursor++)))
        {
            value = value * 10.0f + (c - '0');
            exponent -= 1;
        }
    }
    if (c == 'e' || c == 'E')
    {
        i32 sign = 1;
        i32 i    = 0;
        c        = *iter->cursor++;
        sign     = c == '+' ? 1 : -1;
        c        = *iter->cursor++;
        while (CharIsDigit(c))
        {
            i = i * 10 + (c - '0');
            c = *iter->cursor++;
        }
        exponent += i * sign;
    }
    while (exponent > 0)
    {
        value *= 10.0f;
        exponent--;
    }
    while (exponent < 0)
    {
        value *= 0.1f;
        exponent++;
    }
    if (valueSign)
    {
        value = -value;
    }
    return value;
}

inline V3I32 ParseFaceVertex(Iter *iter)
{
    V3I32 result;
    for (int i = 0; i < 3; i++)
    {
        i32 value = 0;
        u8 c;
        while (CharIsDigit((c = *iter->cursor)))
        {
            value = value * 10 + (c - '0');
            iter->cursor++;
        }
        result.e[i] = value;
        iter->cursor++;
    }
    return result;
}

inline void SkipToNextLine(Iter *iter)
{
    while (!IsEndOfFile(iter) && *iter->cursor != '\n')
    {
        iter->cursor++;
    }
    iter->cursor++;
}

inline void GetNextWord(Iter *iter)
{
    while (*iter->cursor != ' ' && *iter->cursor != '\t')
    {
        iter->cursor++;
    }
    while (*iter->cursor == ' ' || *iter->cursor == '\t')
    {
        iter->cursor++;
    }
}
internal u32 VertexHash(V3I32 indices)
{
    u32 result = 5381;
    for (i32 i = 0; i < 3; i++)
    {
        result = ((result << 5) + result) + indices.e[i];
    }
    return result;
}
inline u8 GetByte(Iter *iter)
{
    u8 result = 0;
    if (iter->cursor < iter->end)
    {
        result = *iter->cursor++;
    }
    return result;
}

// TODO LATER: chunked linked list while building skeleton, then pack into array?

internal String8 ConsumeLine(Iter *iter)
{
    String8 result;
    result.str = iter->cursor;
    u32 size   = 0;
    while (*iter->cursor++ != '\n')
    {
        size++;
    }
    result.size = size;
    return result;
}

// TODO: maybe memcpy or transpose or cast or something?
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

// NOTE: Maps vertices to bone indices, and the weight of the bone's impact on the vertex
inline void AddVertexBoneData(Skeleton *skeleton, u32 vertexId, u32 boneIndex, f32 boneWeight)
{
    if (boneWeight != 0)
    {
        Assert(vertexId < MAX_VERTEX_COUNT);
        VertexBoneInfo *vertexBoneInfo                        = skeleton->vertexBoneInfo + vertexId;
        vertexBoneInfo->pieces[vertexBoneInfo->numMatrices++] = {boneIndex, boneWeight};
        Assert(vertexBoneInfo->numMatrices <= MAX_MATRICES_PER_VERTEX);
    }
}

internal i32 FindBoneId(Skeleton *skeleton, String8 name)
{
    i32 result = -1;

    for (u32 i = 0; i < skeleton->boneCount; i++)
    {
        if (skeleton->boneInfo[i].name == name)
        {
            result = i;
            Assert(result == (i32)skeleton->boneInfo[i].boneId);
            break;
        }
    }
    return result;
}

inline V3 CalculateTangents(const MeshVertex *vertex1, const MeshVertex *vertex2, const MeshVertex *vertex3)
{
    V3 edge1 = vertex2->position - vertex1->position;
    V3 edge2 = vertex3->position - vertex1->position;

    V2 deltaUv1 = vertex2->uv - vertex1->uv;
    V2 deltaUv2 = vertex3->uv - vertex1->uv;

    f32 coef = 1 / (deltaUv1.u * deltaUv2.v - deltaUv2.u * deltaUv1.v);

    V3 tangent = coef * (deltaUv2.v * edge1 - deltaUv1.v * edge2);

    // DEBUG
    return tangent;
}

internal Mat4 ConvertToMatrix(const AnimationTransform *transform)
{
    Mat4 result =
        Translate4(transform->translation) * QuatToMatrix(transform->rotation) * Scale4(transform->scale);
    return result;
}

internal void LoadBones(Arena *arena, Skeleton *skeleton, aiMesh *mesh, u32 baseVertex)
{
    for (u32 i = 0; i < mesh->mNumBones; i++)
    {
        aiBone *bone = mesh->mBones[i];
        BoneInfo info;
        String8 name                  = Str8((u8 *)bone->mName.data, bone->mName.length);
        name                          = PushStr8Copy(arena, name);
        info.name                     = name;
        info.convertToBoneSpaceMatrix = ConvertAssimpMatrix4x4(bone->mOffsetMatrix);
        u32 boneId                    = skeleton->boneCount;
        info.boneId                   = boneId;
        skeleton->boneInfo[boneId]    = info;
        skeleton->boneCount++;
        Assert(skeleton->boneCount <= mesh->mNumBones);

        for (u32 j = 0; j < bone->mNumWeights; j++)
        {
            aiVertexWeight *weight = bone->mWeights + j;
            u32 vertexId           = weight->mVertexId; // baseVertex + weight->mVertexId;
            f32 boneWeight         = weight->mWeight;

            AddVertexBoneData(skeleton, vertexId, boneId, boneWeight);
        }
    }
}

internal void MapSkeletonVertexInfoToVertices(LoadedMesh *mesh)
{
    Skeleton *skeleton = mesh->skeleton;
    for (u32 i = 0; i < mesh->vertexCount; i++)
    {
        VertexBoneInfo *piece = &skeleton->vertexBoneInfo[i];
        for (u32 j = 0; j < MAX_MATRICES_PER_VERTEX; j++)
        {
            mesh->vertices[i].boneIds[j]     = piece->pieces[j].boneIndex;
            mesh->vertices[i].boneWeights[j] = piece->pieces[j].boneWeight;
        }
    }
}

internal LoadedMesh ProcessMesh(Arena *arena, aiMesh *mesh, const aiScene *scene, u32 baseVertex)
{
    LoadedMesh result;
    std::vector<MeshVertex> vertices;
    std::vector<u32> indices;

    Skeleton *skeleton = PushStruct(arena, Skeleton);

    skeleton->boneInfo       = PushArray(arena, BoneInfo, MAX_BONES);
    skeleton->vertexBoneInfo = PushArray(arena, VertexBoneInfo, MAX_VERTEX_COUNT);
    skeleton->boneCount      = 0;
    // skeleton.vertexBoneInfoCount = 0;

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
        // surely if you have vertex with multiple texture coordinates, it just becomes a different vertex
        // altogeter?
        if (mesh->mTextureCoords[0])
        {
            vertex.uv.u = mesh->mTextureCoords[0][i].x;
            vertex.uv.y = mesh->mTextureCoords[0][i].y;
        }
        else
        {
            vertex.uv = {0, 0};
        }
        vertices.push_back(vertex);
    }
    LoadBones(arena, skeleton, mesh, baseVertex);
    for (u32 i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        for (u32 indexindex = 0; indexindex < face.mNumIndices; indexindex++)
        {
            indices.push_back(face.mIndices[indexindex]);
        }
        Assert(face.mNumIndices == 3);
        MeshVertex *vertex1 = &vertices[face.mIndices[0]];
        MeshVertex *vertex2 = &vertices[face.mIndices[1]];
        MeshVertex *vertex3 = &vertices[face.mIndices[2]];

        // V3 tangent = CalculateTangents(vertex1, vertex2, vertex3);

        // vertex1->tangent += tangent;
        // vertex2->tangent += tangent;
        // vertex3->tangent += tangent;
    }
#if 1
    aiVector3D result1 = mesh->mTangents[0];
    // aiVector3D result2 = mesh->mTangents[1];
    // aiVector3D result3 = mesh->mTangents[2];

    V3 r = Normalize(vertices[0].tangent);
    V3 n = vertices[0].normal;
    Printf("%f %f %f\n%f %f %f\n%f %f %f\n\n", result1.x, result1.y, result1.z, r.x, r.y, r.z, n.x, n.y, n.z);

#endif
    result.vertices    = vertices;
    result.indices     = indices;
    result.vertexCount = (u32)vertices.size();
    result.indexCount  = (u32)indices.size();
    result.skeleton    = skeleton;

    MapSkeletonVertexInfoToVertices(&result);
    // TODO: Load materials
    //  aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
    return result;
}

internal void ProcessNode(Arena *arena, LoadedModel *model, aiNode *node, const aiScene *scene,
                          MeshNodeInfoArray *nodeArray)
{
    u32 index                   = nodeArray->count++;
    MeshNodeInfo *nodeInfo      = &nodeArray->info[index];
    String8 name                = Str8((u8 *)node->mName.data, node->mName.length);
    name                        = PushStr8Copy(arena, name);
    nodeInfo->name              = name;
    nodeInfo->transformToParent = ConvertAssimpMatrix4x4(node->mTransformation);
    if (node->mParent)
    {
        String8 parentName   = Str8((u8 *)node->mParent->mName.data, node->mParent->mName.length);
        parentName           = PushStr8Copy(arena, parentName);
        nodeInfo->parentName = parentName;
        nodeInfo->hasParent  = true;
    }
    else
    {
        nodeInfo->hasParent = false;
    }

    // u32 parentId = -1;
    // if (node->mParent) {
    //     String8 parentName = Str8((u8*)node->mParent->mName.data, node->mParent->mName.length);
    //     nodeInfo->parentId = FindBone(parentName);
    // }

    Assert(nodeArray->count < nodeArray->cap);
    // String8 parentName = Str8((u8*)node->mParent->mName.data, node->mParent->mName.data);

    for (u32 i = 0; i < node->mNumMeshes; i++)
    {
        u32 baseVertex = 0;
        aiMesh *mesh   = scene->mMeshes[node->mMeshes[i]];
        if (mesh)
        {
            model->meshes.push_back(ProcessMesh(arena, mesh, scene, baseVertex));
            baseVertex += model->meshes[i].vertexCount;
        }
    }
    for (u32 i = 0; i < node->mNumChildren; i++)
    {
        ProcessNode(arena, model, node->mChildren[i], scene, nodeArray);
    }
}

internal Model ConvertModel(Arena *arena, LoadedModel *model)
{
    Model result;
    u32 meshCount    = (u32)model->meshes.size();
    result.meshCount = meshCount;

    Mesh *meshes = PushArray(arena, Mesh, meshCount);
    for (u32 i = 0; i < meshCount; i++)
    {
        LoadedMesh *loadedMesh = &model->meshes[i];
        u32 vertexCount        = loadedMesh->vertexCount;
        u32 indexCount         = loadedMesh->indexCount;

        MeshVertex *vertices = PushArray(arena, MeshVertex, vertexCount);
        u32 *indices         = PushArray(arena, u32, indexCount);

        std::copy(loadedMesh->vertices.begin(), loadedMesh->vertices.end(), vertices);
        std::copy(loadedMesh->indices.begin(), loadedMesh->indices.end(), indices);

        meshes[i].vertices    = vertices;
        meshes[i].indices     = indices;
        meshes[i].vertexCount = vertexCount;
        meshes[i].indexCount  = indexCount;
        meshes[i].skeleton    = loadedMesh->skeleton;
    }
    result.meshes = meshes;
    return result;
}

// internal Model AddP

// TODO: load all the other animations
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
        String8 name        = Str8((u8 *)channel->mNodeName.data, channel->mNodeName.length);
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
            // Assert(!IsZero(position));
            Assert(!IsZero(rotation));
            AnimationTransform transform = MakeAnimTform(position, rotation, scale);
            boneChannel.transforms[j]    = transform;
            // boneChannel.transforms[j]= (f32)channel->mPositionKeys[positionIndex].mTime;
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

    // for (u32 keyFrameIndex = 0; keyFrameIndex < animationChannel->numFrames; keyFrameIndex++)
    // {
    //     for (u32 boneIndex = 0; boneIndex < animation->mNumChannels; boneIndex++)
    //     {
    //         animationChannel->keyframes[keyFrameIndex].transforms[boneIndex] =
    //             boneChannels[boneIndex].transforms[keyFrameIndex];
    //         animationChannel->nodeInfo[boneIndex].name = boneChannels[boneIndex].name;
    //     }
    // }
}

/*
 * ASSIMP
 */
// NOTE IMPORTANT: WE DO NOT CARE ABOUT THIS CODE AT ALL. EVENTUALLY WE WILL LOAD DIFFERENT FILE TYPES
// (GLTF, FBX, OBJ, BMP, TGA, WAV, ETC), AND JUST BAKE ALL THE ASSETS INTO ONE MEGA FILE THAT WE CAN EASILY
// READ/ACCESS/COMPRESS.
struct ModelOutput
{
    Model model;
    KeyframedAnimation *animation;
    MeshNodeInfoArray *infoArray;
};

internal ModelOutput AssimpDebugLoadModel(Arena *arena, String8 filename)
{
    LoadedModel loadedModel = {};

    ModelOutput result = {};

    Assimp::Importer importer;
    const char *file = (const char *)filename.str;
    const aiScene *scene =
        importer.ReadFile(file, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        return result;
    }

    // NOTE LEAK: we're probably leaking memory when processing assimp nodes
    aiMatrix4x4t<f32> transform = scene->mRootNode->mTransformation;
    Mat4 globalTransform        = ConvertAssimpMatrix4x4(transform);

    result.infoArray             = PushStruct(arena, MeshNodeInfoArray);
    MeshNodeInfoArray *infoArray = result.infoArray;
    infoArray->info              = PushArrayNoZero(arena, MeshNodeInfo, 200);
    infoArray->cap               = 200;
    infoArray->count             = 0;
    ProcessNode(arena, &loadedModel, scene->mRootNode, scene, infoArray);

    result.model                        = ConvertModel(arena, &loadedModel);
    result.model.globalInverseTransform = Inverse(globalTransform);
    result.animation                    = PushStruct(arena, KeyframedAnimation);
    ProcessAnimations(arena, scene, result.animation);

    result.infoArray = infoArray;

    return result;
}
/*
 * END ASSIMP
 */

// TODO: is this too object oriented??
internal void StartLoopedAnimation(AnimationPlayer *player, KeyframedAnimation *animation)
{
    player->currentAnimation = animation;
    player->duration         = animation->duration;
    player->numFrames        = animation->numFrames;
    player->isLooping        = true;
}

internal void PlayCurrentAnimation(AnimationPlayer *player, f32 dT, AnimationTransform *transforms)
{
    u32 frame = (u32)((player->numFrames - 1) * (player->currentTime / player->duration));
    frame     = Clamp(frame, 0, player->numFrames - 1);

    // this gets how far into the current frame the animation is, in order to lerp
    f32 fraction;
    if (player->numFrames > 1)
    {
        f32 timePerSample = player->duration * (1.f / (f32)(player->numFrames - 1));
        f32 timeBase      = player->duration * (frame / (f32)(player->numFrames - 1));
        fraction          = (player->currentTime - timeBase) / (timePerSample);
    }
    else
    {
        fraction = 0.f;
    }
    fraction = Clamp(fraction, 0.f, 1.f);

    b32 nextKeyframe = (frame + 1 < player->numFrames);
    // Keyframe *currentKeyframe = player->currentAnimation->keyframes + frame;
    // Keyframe *nextKeyframe = 0;
    // if (frame + 1 < player->numFrames)
    // {
    //     nextKeyframe = player->currentAnimation->keyframes + frame + 1;
    // }
    for (u32 boneIndex = 0; boneIndex < player->currentAnimation->numNodes; boneIndex++)
    {
        const AnimationTransform t1 = player->currentAnimation->boneChannels[boneIndex].transforms[frame];
        // ->transforms[frame]; // currentKeyframe->transforms[boneIndex];
        Assert(!IsZero(t1)); // boneIndex >
        if (!nextKeyframe)
        {
            transforms[boneIndex] = t1;
        }
        else
        {
            const AnimationTransform t2 = player->currentAnimation->boneChannels[boneIndex].transforms[frame + 1];
            // ->transforms[frame + 1]; // nextKeyframe->transforms[boneIndex];
            transforms[boneIndex] = Lerp(t1, t2, fraction);
        }
    }

    player->currentTime += dT;
    if (player->currentTime > player->duration)
    {
        if (player->isLooping)
        {
            player->currentTime -= player->duration;
        }
        else
        {
            // TODO: handle this case for non looping animations
        }
    }
}

internal void SkinModelToAnimation(AnimationPlayer *player, Model *model, const AnimationTransform *transforms,
                                   MeshNodeInfoArray *infoArray, Mat4 **output)
{
    u32 baseBone = 0;
    for (u32 meshCount = 0; meshCount < model->meshCount; meshCount++)
    {
        Mesh *currentMesh = model->meshes;
        SkinMeshToAnimation(player, currentMesh, transforms, infoArray, model->globalInverseTransform,
                            *output + baseBone);
        baseBone += currentMesh->skeleton->boneCount;
    }
}

internal Mat4 FindNodeMatrix(MeshNodeInfoArray *infoArray, String8 name)
{
    Mat4 result = {};
    for (u32 i = 0; i < infoArray->count; i++)
    {
        MeshNodeInfo *info = infoArray->info + i;
        if (info->name == name)
        {
            result = info->transformToParent;
            break;
        }
    }
    return result;
}

internal MeshNodeInfo* FindNode(MeshNodeInfoArray *infoArray, String8 name)
{
    MeshNodeInfo *node = 0;
    for (u32 i = 0; i < infoArray->count; i++)
    {
        MeshNodeInfo *info = infoArray->info + i;
        if (info->name == name)
        {
            node = info;
            break;
        }
    }
    return node;
}

// internal i32 FindNode(MeshNodeInfoArray *infoArray, String8 name)
// {
//     i32 result = -1;
//     for (u32 i = 0; i < infoArray->count; i++)
//     {
//         MeshNodeInfo *info = infoArray->info + i;
//         if (info->name == name)
//         {
//             result = (i32)i;
//             break;
//         }
//     }
//     return result;
// }

internal void SkinMeshToAnimation(AnimationPlayer *player, Mesh *mesh, const AnimationTransform *transforms,
                                  MeshNodeInfoArray *infoArray, Mat4 globalInverseTransform, Mat4 *finalTransforms)
{
    VertexBoneInfo *vertexBoneInfo = mesh->skeleton->vertexBoneInfo;
    BoneInfo *boneInfo             = mesh->skeleton->boneInfo;

    Mat4 transformToParent[MAX_BONES];
    Skeleton *skeleton = mesh->skeleton;

    i32 previousId = -1;
    // Mat4 parentTransform = Identity();

    for (u32 i = 0; i < infoArray->count; i++)
    {
        MeshNodeInfo *node = infoArray->info + i;
        i32 id             = FindBoneId(skeleton, node->name);

        if (id == -1)
        {
            // parentTransform = parentTransform * FindNodeMatrix(infoArray, node->name);
            continue;
        }

        i32 animationId = -1;
        // WHY OH WHY GOD
        for (u32 index = 0; index < MAX_BONES; index++)
        {
            if (player->currentAnimation->boneChannels[index].name == node->name)
            {
                animationId = index;
                break;
            }
        }
        // Mat4 lerpedMatrix = node->transformToParent;
        Mat4 lerpedMatrix;
        if (animationId == -1)
        {
            lerpedMatrix = node->transformToParent;
        }
        else
        {
            lerpedMatrix = ConvertToMatrix(&transforms[animationId]);
#if 0
            const AnimationTransform *transform = &transforms[animationId];
            char printBuffer[256];
            stbsp_snprintf(printBuffer, sizeof(printBuffer), "%f %f %f \n%f %f %f %f\n%f %f %f %S\n\n",
                           transform->translation.x, transform->translation.y, transform->translation.z,
                           transform->rotation.x, transform->rotation.y, transform->rotation.z,
                           transform->rotation.w, transform->scale.x, transform->scale.y, transform->scale.z, 
                           node->name);
            OutputDebugStringA(printBuffer);
#endif
#if 0
            // char printBuffer[256];
            stbsp_snprintf(printBuffer, sizeof(printBuffer),
                           "%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n\n", lerpedMatrix.a1,
                           lerpedMatrix.b1, lerpedMatrix.c1, lerpedMatrix.d1, lerpedMatrix.a2, lerpedMatrix.b2,
                           lerpedMatrix.c2, lerpedMatrix.d2, lerpedMatrix.a3, lerpedMatrix.b3, lerpedMatrix.c3,
                           lerpedMatrix.d3, lerpedMatrix.a4, lerpedMatrix.b4, lerpedMatrix.c4, lerpedMatrix.d4);
            OutputDebugStringA(printBuffer);
#endif
        }
        i32 parentId = FindBoneId(skeleton, node->parentName);

        // TODO: store parent index and node index
        // THE PARENT DOESN'T EXIST IN THE SKELETON, BUT IN THE MESH NODE INFO IT HAS A TRANSFORM
        if (parentId == -1)
        {
            Mat4 parentTransform    = Identity();
            String8 parentName = node->parentName;
            while (node->hasParent && parentName.size)
            {
                parentTransform = parentTransform * FindNodeMatrix(infoArray, parentName);

                node       = FindNode(infoArray, parentName);
                parentName = node->parentName;
            }
            // parentTransform       = parentTransform * FindNodeMatrix(infoArray, node->name);
            transformToParent[id] = parentTransform * node->transformToParent;
        }
        // THE PARENT DOES EXIST IN THE SKELETON
        // NOTE IMPORTANT: the animation data in assimp replaces the transformation matrix to the parent. sigh
        else
        {
            Assert(!IsZero(transformToParent[parentId]));
            transformToParent[id] = transformToParent[parentId] * lerpedMatrix;
        }

        Assert(id > previousId);
        previousId = id;
        finalTransforms[id] =
            globalInverseTransform * transformToParent[id] * boneInfo[id].convertToBoneSpaceMatrix;
    }
}

internal void WriteAnimation(KeyframedAnimation *animation) {}
#if 0
// TODO: i really don't like using function pointers for platform layer stuff
// NOTE: array of nodes, each with a parent index and a name
// you know i think I'm just goin to use assimp to load the file once, write it in an easier format to use,
// and then use that format
internal void DebugLoadGLTF(Arena *arena, DebugPlatformReadFileFunctionType *PlatformReadFile,
                            DebugPlatformFreeFileFunctionType *PlatformFreeFile, const char *gltfFilename,
                            const char *binaryFilename)
{
    // Model result;

    // READ THE ENTIRE FILE
    // DebugReadFileOutput gltfOutput = PlatformReadFile(gltfFilename);
    // Assert(gltfOutput.fileSize != 0);
    //
    // DebugReadFileOutput binaryOutput = PlatformReadFile(binaryFilename);
    // Assert(binaryOutput.fileSize != 0);
    //
    // Iter iter;
    // iter.cursor = (u8 *)gltfOutput.contents;
    // iter.end = (u8 *)gltfOutput.contents + gltfOutput.fileSize;
    //
    // // NOTE: gltf is json format. things can be nested, unnested etc etc
    // // right now we're just hard coding the order because we're only loading one gltf file
    // // READ ACCESSORS
    // {
    //     // TODO: carriage returns???
    //     String8 bracket = ConsumeLine(&iter);
    //     String8 accessorsStart = ConsumeLine(&iter);
    //     String8 accessor = SkipWhitespace(accessorsStart);
    //     if (StartsWith(accessor, Str8C("\"accessors\"")))
    //     {
    //         // loop:
    //         // never mind just get the entire element string
    //         //  check for closing ], if not
    //         //  get element, load accessor
    //         //      stop on }
    //         OutputDebugStringA("All good");
    //     }
    //     else
    //     {
    //         OutputDebugStringA("Not all good");
    //         return;
    //     }
    // }
    // // return result;
    // PlatformFreeFile(gltfOutput.contents);
}

internal TGAResult DebugLoadTGA(Arena *arena, DebugPlatformReadFileFunctionType *PlatformReadFile,
                                DebugPlatformFreeFileFunctionType *PlatformFreeFile, const char *filename)
{
    // int width, height, nChannels;
    // stbi_set_flip_vertically_on_load(true);
    // u8 *data = (u8 *)stbi_load(filename, &width, &height, &nChannels, 0);
    // TGAResult result;
    // result.width = (u32)width;
    // result.height = (u32)height;
    // result.contents = data;
    // return result;
    TGAResult result;
    DebugReadFileOutput output = PlatformReadFile(filename);
    Assert(output.fileSize != 0);
    TGAHeader *header = (TGAHeader *)output.contents;
    b32 isRle = 0;
    u32 width = (u32)header->width;
    u32 height = (u32)header->height;
    result.width = width;
    result.height = height;
    Assert(header->imageType != 1 && header->imageType != 9);
    // Image type = 9, 10, or 11 is RLE
    if (header->imageType > 8)
    {
        isRle = 1;
    }
    Iter inputIter;
    inputIter.cursor = (u8 *)output.contents + sizeof(TGAHeader) + header->idLength;
    inputIter.end = inputIter.cursor + output.fileSize;

    i32 tgaInverted = header->imageDescriptor;
    tgaInverted = 1 - ((tgaInverted >> 5) & 1);
    i32 rleCount = 0;
    b32 repeating = 0;
    b32 readNextPixel = 0;
    i32 bytesPerPixel = header->bitsPerPixel >> 3;

    result.contents = PushArrayNoZero(arena, u8, width * height * bytesPerPixel);
    u8 rawData[4] = {};

    for (u32 i = 0; i < width * height; i++)
    {
        // Read RLE byte:
        // If the high-order bit is set to 1, it's a run-length packet type
        // The next 7 bits represent the count + 1. A 0 pixel count means that 1 pixel
        // is encoded by the packet.
        if (isRle)
        {
            if (rleCount == 0)
            {
                i32 rleCmd = GetByte(&inputIter);
                rleCount = 1 + (rleCmd & 0x7F);
                repeating = rleCmd >> 7;
                readNextPixel = 1;
            }
            else if (!repeating)
            {
                readNextPixel = 1;
            }
        }
        else
        {
            readNextPixel = 1;
        }
        if (readNextPixel)
        {
            for (i32 b = 0; b < bytesPerPixel; b++)
            {
                rawData[b] = GetByte(&inputIter);
            }
            readNextPixel = 0;
        }
        for (i32 j = 0; j < bytesPerPixel; j++)
        {
            result.contents[i * bytesPerPixel + j] = rawData[j];
        }
        rleCount--;
    }
    // if (tgaInverted)
    // {
    //     for (u32 j = 0; j * 2 < height; j++)
    //     {
    //         i32 index1 = j * width * bytesPerPixel;
    //         i32 index2 = (height - 1 - j) * width * bytesPerPixel;
    //         for (i32 i = width * bytesPerPixel; i > 0; --i)
    //         {
    //             Swap(u8, result.contents[index1], result.contents[index2]);
    //             ++index1;
    //             ++index2;
    //         }
    //     }
    // }

    // Converts from BGR to RGB
    if (bytesPerPixel >= 3)
    {
        u8 *pixel = result.contents;
        for (u32 i = 0; i < width * height; i++)
        {
            Swap(u8, pixel[0], pixel[2]);
            pixel += bytesPerPixel;
        }
    }
    PlatformFreeFile(output.contents);
    return result;
}

// TODO: # vertices is hardcoded, handle groups?
// Add indices
internal Model DebugLoadOBJModel(Arena *arena, DebugPlatformReadFileFunctionType *PlatformReadFile,
                                 DebugPlatformFreeFileFunctionType *PlatformFreeFile, const char *filename)
{
    DebugReadFileOutput output = PlatformReadFile(filename);
    Assert(output.fileSize != 0);

    Iter iter;
    iter.cursor = (u8 *)output.contents;
    iter.end = iter.cursor + output.fileSize;

    const i32 MAX_MODEL_VERTEX_COUNT = 4000;
    const i32 MAX_MODEL_INDEX_COUNT = 16000;
    const i32 MAX_POSITION_VERTEX_COUNT = 3000;
    const i32 MAX_NORMAL_VERTEX_COUNT = 3500;
    const i32 MAX_UV_VERTEX_COUNT = 3300;

    MeshVertex *modelVertices = PushArrayNoZero(arena, MeshVertex, MAX_MODEL_VERTEX_COUNT);
    u32 vertexCount = 0;

    u16 *modelIndices = PushArrayNoZero(arena, u16, MAX_MODEL_INDEX_COUNT);
    u32 indexCount = 0;

    TempArena tempArena = ScratchBegin(arena);
    V3 *positionVertices = PushArrayNoZero(tempArena.arena, V3, MAX_POSITION_VERTEX_COUNT);
    u32 positionCount = 0;
    V3 *normalVertices = PushArrayNoZero(tempArena.arena, V3, MAX_NORMAL_VERTEX_COUNT);
    u32 normalCount = 0;
    V2 *uvVertices = PushArrayNoZero(tempArena.arena, V2, MAX_UV_VERTEX_COUNT);
    u32 uvCount = 0;

    u32 length = MAX_MODEL_INDEX_COUNT / 2 - 7;
    FaceVertex *vertexHash = PushArray(tempArena.arena, FaceVertex, length);
    // FaceVertex vertexHash[MAX_MODEL_INDEX_COUNT / 2-7] = {};
    // std::unordered_map<V3I32, u32> vertexHash;

    while (!IsEndOfFile(&iter))
    {
        u32 type = GetType(iter);
        switch (type)
        {
            case OBJ_Vertex:
            {
                GetNextWord(&iter);
                V3 pos;
                pos.x = ReadFloat(&iter);
                pos.y = ReadFloat(&iter);
                pos.z = ReadFloat(&iter);
                positionVertices[positionCount++] = pos;
                Assert(positionCount < MAX_POSITION_VERTEX_COUNT);
                break;
            }
            case OBJ_Normal:
            {
                GetNextWord(&iter);
                V3 normal;
                normal.x = ReadFloat(&iter);
                normal.y = ReadFloat(&iter);
                normal.z = ReadFloat(&iter);
                normalVertices[normalCount++] = normal;
                Assert(normalCount < MAX_NORMAL_VERTEX_COUNT);
                break;
            }
            case OBJ_Texture:
            {
                GetNextWord(&iter);
                V2 uv;
                uv.x = ReadFloat(&iter);
                uv.y = ReadFloat(&iter);
                SkipToNextLine(&iter);
                uvVertices[uvCount++] = uv;
                Assert(uvCount < MAX_UV_VERTEX_COUNT);
                break;
            }
            case OBJ_Face:
            {
                GetNextWord(&iter);
                // TODO: hardcoded triangles
                for (int i = 0; i < 3; i++)
                {
                    // POSITION, TEXTURE, NORMAL
                    V3I32 indices = ParseFaceVertex(&iter);
                    u32 hash = VertexHash(indices) % length;
                    Assert(hash < length);
                    FaceVertex *vertex = vertexHash + hash;
                    do
                    {
                        // If already in hash table, then reuse the vertex
                        if (vertex->indices == indices)
                        {
                            modelIndices[indexCount++] = vertex->index;
                            break;
                        }
                        if (vertex->indices != V3I32{0, 0, 0} && vertex->nextInHash == 0)
                        {
                            vertex->nextInHash = PushStruct(tempArena.arena, FaceVertex);
                            vertex = vertex->nextInHash;
                        }
                        if (vertex->indices == V3I32{0, 0, 0})
                        {
                            vertex->indices = indices;
                            vertex->index = (u16)vertexCount;
                            vertex->nextInHash = 0;
                            modelIndices[indexCount++] = (u16)vertexCount;
                            Assert(vertexCount == (u16)vertexCount);

                            MeshVertex newMeshVertex;
                            newMeshVertex.position = positionVertices[indices.x - 1];
                            newMeshVertex.uv = uvVertices[indices.y - 1];
                            newMeshVertex.normal = normalVertices[indices.z - 1];
                            modelVertices[vertexCount++] = newMeshVertex;
                            Assert(vertexCount < MAX_MODEL_VERTEX_COUNT);
                            Assert(indexCount < MAX_MODEL_INDEX_COUNT);
                            break;
                        }
                        vertex = vertex->nextInHash;

                    } while (vertex);
                }
                break;
            }
            default:
            {
                SkipToNextLine(&iter);
                break;
            }
        }
    }
    ScratchEnd(tempArena);
    Model model;
    model.vertices = modelVertices;
    model.vertexCount = vertexCount;
    model.indices = modelIndices;
    model.indexCount = indexCount;
    PlatformFreeFile(output.contents);
    return model;
}
#endif
