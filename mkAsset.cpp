#include "mkCrack.h"
#ifdef LSP_INCLUDE
#include "asset.h"
#include "asset_cache.h"
#include "./render/opengl.cpp"
#include "keepmovingforward.h"
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
    string type = Str8(iter.cursor, 2);
    u32 objType = OBJ_Invalid;
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

internal string ConsumeLine(Iter *iter)
{
    string result;
    result.str = iter->cursor;
    u32 size   = 0;
    while (*iter->cursor++ != '\n')
    {
        size++;
    }
    result.size = size;
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

// TODO: load all the other animations

inline V3 CalculateTangents(const MeshVertex *vertex1, const MeshVertex *vertex2, const MeshVertex *vertex3)
{
    V3 edge1 = vertex2->position - vertex1->position;
    V3 edge2 = vertex3->position - vertex1->position;

    V2 deltaUv1 = vertex2->uv - vertex1->uv;
    V2 deltaUv2 = vertex3->uv - vertex1->uv;

    f32 coef = 1.f / (deltaUv1.u * deltaUv2.v - deltaUv2.u * deltaUv1.v);

    V3 tangent = coef * (deltaUv2.v * edge1 - deltaUv1.v * edge2);

    // DEBUG
    return tangent;
}

internal Mat4 ConvertToMatrix(const AnimationTransform *transform)
{
    Mat4 result = Translate4(transform->translation) * QuatToMatrix(transform->rotation) * Scale(transform->scale);
    return result;
}

//////////////////////////////
// Animation
//
internal b8 LoadAnimation(Arena *arena, AnimationPlayer *player, AS_Handle handle)
{
    player->anim             = handle;
    KeyframedAnimation *anim = GetAnim(handle);
    player->currentAnimation = anim;
    b8 result                = 0;
    if (!IsAnimNil(anim))
    {
        player->duration  = anim->duration;
        player->isLooping = true;
        player->loaded    = true;
        result            = true;
    }
    return result;
}
internal void StartLoopedAnimation(Arena *arena, AnimationPlayer *player, AS_Handle handle)
{
    LoadAnimation(arena, player, handle);
}

internal void PlayCurrentAnimation(Arena *arena, AnimationPlayer *player, f32 dT, AnimationTransform *transforms)
{
    if (!player->loaded)
    {
        LoadAnimation(arena, player, player->anim);
        if (IsAnimNil(player->currentAnimation))
        {
            return;
        }
    }
    KeyframedAnimation *animation = player->currentAnimation;

    for (u32 boneIndex = 0; boneIndex < animation->numNodes; boneIndex++)
    {
        BoneChannel *boneChannel = animation->boneChannels + boneIndex;

        // Move to next key if you've passed it
        // TODO: this seems like just too much overhead
        if (boneChannel->positions[player->currentPositionKey[boneIndex]].time < player->currentTime)
        {
            player->currentPositionKey[boneIndex] =
                (player->currentPositionKey[boneIndex] + 1) % boneChannel->numPositionKeys;
        }
        if (boneChannel->scales[player->currentScaleKey[boneIndex]].time < player->currentTime)
        {
            player->currentScaleKey[boneIndex] =
                (player->currentScaleKey[boneIndex] + 1) % boneChannel->numScalingKeys;
        }
        if (boneChannel->rotations[player->currentRotationKey[boneIndex]].time < player->currentTime)
        {
            player->currentRotationKey[boneIndex] =
                (player->currentRotationKey[boneIndex] + 1) % boneChannel->numRotationKeys;
        }
        AnimationTransform *transform = transforms + boneIndex;
        u32 positionKey               = player->currentPositionKey[boneIndex];
        u32 scaleKey                  = player->currentScaleKey[boneIndex];
        u32 rotationKey               = player->currentRotationKey[boneIndex];

        Assert(boneChannel->numPositionKeys != 0);
        Assert(boneChannel->numScalingKeys != 0);
        Assert(boneChannel->numRotationKeys != 0);

        AnimationPosition position = boneChannel->positions[positionKey];
        AnimationPosition pastPosition =
            boneChannel
                ->positions[(boneChannel->numPositionKeys + positionKey - 1) % boneChannel->numPositionKeys];

        AnimationScale scale = boneChannel->scales[scaleKey];
        AnimationScale pastScale =
            boneChannel->scales[(boneChannel->numScalingKeys + scaleKey - 1) % boneChannel->numScalingKeys];

        AnimationRotation rotation = boneChannel->rotations[rotationKey];
        Quat uncompressedRot;
        uncompressedRot.x = DecompressRotationChannel(rotation.rotation[0]);
        uncompressedRot.y = DecompressRotationChannel(rotation.rotation[1]);
        uncompressedRot.z = DecompressRotationChannel(rotation.rotation[2]);
        uncompressedRot.w = DecompressRotationChannel(rotation.rotation[3]);
        AnimationRotation pastRotation =
            boneChannel
                ->rotations[(boneChannel->numRotationKeys + rotationKey - 1) % boneChannel->numRotationKeys];

        Quat uncompressedPastRot;
        uncompressedPastRot.x = DecompressRotationChannel(pastRotation.rotation[0]);
        uncompressedPastRot.y = DecompressRotationChannel(pastRotation.rotation[1]);
        uncompressedPastRot.z = DecompressRotationChannel(pastRotation.rotation[2]);
        uncompressedPastRot.w = DecompressRotationChannel(pastRotation.rotation[3]);

        f32 fraction = (player->currentTime - pastPosition.time) / (position.time - pastPosition.time);
        if (position.time == pastPosition.time)
        {
            fraction = 0;
        }
        transform->translation = Lerp(pastPosition.position, position.position, fraction);

        fraction = (player->currentTime - pastRotation.time) / (rotation.time - pastRotation.time);
        if (rotation.time == pastRotation.time)
        {
            fraction = 0;
        }
        transform->rotation = Lerp(uncompressedPastRot, uncompressedRot, fraction);

        fraction = (player->currentTime - pastScale.time) / (scale.time - pastScale.time);
        if (scale.time == pastScale.time)
        {
            fraction = 0;
        }
        transform->scale = Lerp(pastScale.scale, scale.scale, fraction);
    }

    player->currentTime += dT;
    if (player->currentTime > player->duration)
    {
        if (player->isLooping)
        {
            player->currentTime -= player->duration;
            for (u32 i = 0; i < animation->numNodes; i++)
            {
                player->currentPositionKey[i] = 0;
                player->currentScaleKey[i]    = 0;
                player->currentRotationKey[i] = 0;
            }
        }
        else
        {
            // TODO: handle this case for non looping animations
        }
    }
}

internal void SkinModelToAnimation(const AnimationPlayer *inPlayer, const AS_Handle inModel,
                                   const AnimationTransform *transforms, Mat4 *outFinalTransforms)
{
    TIMED_FUNCTION();

    TempArena temp           = ScratchStart(0, 0);
    LoadedSkeleton *skeleton = GetSkeletonFromModel(inModel);
    Mat4 *transformToParent  = PushArray(temp.arena, Mat4, skeleton->count);
    i32 previousId           = -1;

    loopi(0, skeleton->count)
    {
        const string name = skeleton->names[i];
        i32 id            = i;

        i32 animationId = -1;
        for (u32 index = 0; index < inPlayer->currentAnimation->numNodes; index++)
        {
            if (inPlayer->currentAnimation->boneChannels[index].name == name)
            {
                animationId = index;
                break;
            }
        }
        Mat4 lerpedMatrix;
        if (animationId == -1)
        {
            lerpedMatrix = skeleton->transformsToParent[id];
        }
        else
        {
            lerpedMatrix = ConvertToMatrix(&transforms[animationId]);
        }
        i32 parentId = skeleton->parents[id];
        if (parentId == -1)
        {
            transformToParent[id] = lerpedMatrix;
        }
        else
        {
            Assert(!IsZero(transformToParent[parentId]));
            transformToParent[id] = transformToParent[parentId] * lerpedMatrix;
        }

        Assert(id > previousId);
        previousId             = id;
        outFinalTransforms[id] = transformToParent[id] * skeleton->inverseBindPoses[id];
    }
    ScratchEnd(temp);
}

internal AnimationTransform operator*(AnimationTransform p, AnimationTransform c)
{
    AnimationTransform result;
    // TODO: is it c.translation * p.scale, then rotation?
    result.translation = p.translation + (Hadamard(p.scale, RotateVector(c.translation, p.rotation)));

    Quat inverse = Conjugate(c.rotation);
    // TODO: i don't know how to do this faster
    Mat4 rotMatrix        = QuatToMatrix(c.rotation);
    Mat4 inverseRotMatrix = QuatToMatrix(Conjugate(c.rotation));
    // this rotmatrix is with respect to the parent? so multiplying by the rotmatrix
    // result.scale    = c.scale * rotMatrix * (p.scale * inverseRotMatrix);
    result.scale    = (inverseRotMatrix * p.scale) * (rotMatrix * c.scale);
    result.rotation = p.rotation * c.rotation;
    return result;
}

internal void SkinModelToBindPose(AS_Handle model, Mat4 *finalTransforms)
{
    TempArena temp           = ScratchStart(0, 0);
    LoadedSkeleton *skeleton = GetSkeletonFromModel(model);
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
        finalTransforms[id] = transformToParent[id] * skeleton->inverseBindPoses[id];
    }
    ScratchEnd(temp);
}

internal Heightmap CreateHeightmap(string filename)
{
    TempArena temp  = ScratchStart(0, 0);
    const f32 scale = 64.f / 256.f;
    const f32 shift = -30.f;

    i32 width, height, nComponents;
    u8 *buffer = stbi_load((char *)filename.str, &width, &height, &nComponents, 0);

    // TODO: chunk this
    f32 *altitudes = PushArray(temp.arena, f32, height * width);

    // f32 *altitudes = PushArray(temp.arena, f32, height * width);

    u32 count      = 0;
    u32 totalCount = width * height;
    i32 stride     = width * nComponents;
    for (i32 h = 0; h < height; h++)
    {
        for (i32 w = 0; w < width; w++)
        {
            u8 *data           = buffer + (h * stride + w * nComponents);
            f32 z              = (f32)(data[0] * scale + shift);
            altitudes[count++] = z;
            // vertices[count++] = MakeV3(-width / 2.f + w, height / 2.f - h, z);
        }
    }

    u32 *indices   = PushArray(temp.arena, u32, (height - 1) * (6 + (width - 2) * 2));
    u32 indexCount = 0;
    for (i32 h = 0; h < height - 1; h++)
    {
        // Connect triangle strips with degenerate triangles
        u32 index1            = (width * (h + 1));
        u32 index2            = (width * h);
        u32 index3            = (width * (h + 1)) + 1;
        u32 index4            = (width * h) + 1;
        indices[indexCount++] = index1;
        indices[indexCount++] = index1;
        indices[indexCount++] = index2;
        indices[indexCount++] = index3;
        indices[indexCount++] = index4;
        for (i32 w = 2; w < width; w++)
        {
            indices[indexCount++] = w + width * (h + 1);
            indices[indexCount++] = w + width * h;
        }
        indices[indexCount++] = width - 1 + (width)*h;
    }

    RenderState *state = engine->GetRenderState();
    VC_Handle vertexHandle =
        state->vertexCache.VC_AllocateBuffer(BufferType_Vertex, BufferUsage_Static, altitudes, sizeof(altitudes[0]), count);

    VC_Handle indexHandle =
        state->vertexCache.VC_AllocateBuffer(BufferType_Index, BufferUsage_Static, indices, sizeof(indices[0]), indexCount);

    Heightmap result;
    result.vertexHandle = vertexHandle;
    result.indexHandle  = indexHandle;
    result.width        = width;
    result.height       = height;

    stbi_image_free(buffer);
    ScratchEnd(temp);
    return result;
}

#if 0
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
    //     string bracket = ConsumeLine(&iter);
    //     string accessorsStart = ConsumeLine(&iter);
    //     string accessor = SkipWhitespace(accessorsStart);
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
