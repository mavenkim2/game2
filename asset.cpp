#define STB_IMAGE_IMPLEMENTATION
// #define STBI_ONLY_TGA
#define STBI_ONLY_PNG
#include "third_party/stb_image.h"

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

//
// ANIMATION
//

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
    // TODO: wrap to the first keyframe when looping?
    // bicubic interpolation
    for (u32 boneIndex = 0; boneIndex < player->currentAnimation->numNodes; boneIndex++)
    {
        const AnimationTransform t1 = player->currentAnimation->boneChannels[boneIndex].transforms[frame];
        Assert(!IsZero(t1));
        if (!nextKeyframe)
        {
            transforms[boneIndex] = t1;
        }
        else
        {
            const AnimationTransform t2 = player->currentAnimation->boneChannels[boneIndex].transforms[frame + 1];
            transforms[boneIndex]       = Lerp(t1, t2, fraction);
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
                                   Mat4 *finalTransforms)
{
    Mat4 transformToParent[MAX_BONES];
    Skeleton *skeleton = &model->skeleton;

    i32 previousId = -1;

    loopi(0, skeleton->count)
    {
        const string name = skeleton->names.items[i];
        i32 id            = i;

        i32 animationId = -1;
        for (u32 index = 0; index < skeleton->count; index++)
        {
            if (player->currentAnimation->boneChannels[index].name == name)
            {
                animationId = index;
                break;
            }
        }
        Mat4 lerpedMatrix;
        if (animationId == -1)
        {
            lerpedMatrix = skeleton->transformsToParent.items[id];
        }
        else
        {
            lerpedMatrix = ConvertToMatrix(&transforms[animationId]);
        }
        i32 parentId = skeleton->parents.items[id];
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
        previousId          = id;
        finalTransforms[id] = transformToParent[id] * skeleton->inverseBindPoses.items[id];
    }
}

internal void WriteModelToFile(Model *model, string filename)
{
    StringBuilder builder = {};
    TempArena temp        = ScratchStart(0, 0);
    builder.scratch       = temp;
    Put(&builder, model->vertices.count);
    Put(&builder, model->indices.count);
    PutArray(&builder, model->vertices);
    PutArray(&builder, model->indices);

    b32 success = WriteEntireFile(&builder, filename);
    if (!success)
    {
        Printf("Failed to write file %S\n", filename);
    }
    ScratchEnd(temp);
}

// TODO IMPORTANT: the arrays should point directly to the data from the file, instead of copying
internal void ReadModelFromFile(Arena *arena, Model *model, string filename)
{
    Tokenizer tokenizer;
    tokenizer.input  = ReadEntireFile(filename);
    tokenizer.cursor = tokenizer.input.str;

    u32 vertexCount, indexCount;
    GetPointer(&tokenizer, &vertexCount);
    GetPointer(&tokenizer, &indexCount);
    ArrayInit(arena, model->vertices, MeshVertex, vertexCount);
    ArrayInit(arena, model->indices, u32, indexCount);

    GetArray(&tokenizer, model->vertices, vertexCount);
    GetArray(&tokenizer, model->indices, indexCount);

    Assert(EndOfBuffer(&tokenizer));

    OS_Release(tokenizer.input.str);
}

global u32 skeletonVersionNumber = 1;
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

internal void ReadSkeletonFromFile(Arena *arena, Skeleton *skeleton, string filename)
{
    Tokenizer tokenizer;
    tokenizer.input  = ReadEntireFile(filename);
    tokenizer.cursor = tokenizer.input.str;

    u32 version;
    u32 count;
    GetPointer(&tokenizer, &version);
    GetPointer(&tokenizer, &count);
    skeleton->count = count;
    if (version == 1)
    {
        // TODO: is it weird that these names are a string pointer? who knows
        ArrayInit(arena, skeleton->names, string, count);
        ArrayInit(arena, skeleton->parents, i32, count);
        ArrayInit(arena, skeleton->inverseBindPoses, Mat4, count);
        ArrayInit(arena, skeleton->transformsToParent, Mat4, count);
        loopi(0, count)
        {
            string output            = ReadLine(&tokenizer);
            skeleton->names.items[i] = PushStr8Copy(arena, output);
        }
        skeleton->names.count = count;
        GetArray(&tokenizer, skeleton->parents, count);
        GetArray(&tokenizer, skeleton->inverseBindPoses, count);
        GetArray(&tokenizer, skeleton->transformsToParent, count);

        Assert(EndOfBuffer(&tokenizer));
    }
    OS_Release(tokenizer.input.str);
}

global u32 animationFileVersion = 1;
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

internal void ReadAnimationFile(Arena *arena, KeyframedAnimation *animation, string filename)
{
    Tokenizer tokenizer;
    tokenizer.input  = ReadEntireFile(filename);
    tokenizer.cursor = tokenizer.input.str;

    u32 version;
    GetPointer(&tokenizer, &version);
    GetPointer(&tokenizer, &animation->numNodes);
    GetPointer(&tokenizer, &animation->duration);
    GetPointer(&tokenizer, &animation->numFrames);

    if (version == 1)
    {
        animation->boneChannels = PushArray(arena, BoneChannel, animation->numNodes);
        loopi(0, animation->numNodes)
        {
            BoneChannel *channel = animation->boneChannels + i;
            string output        = ReadLine(&tokenizer);
            channel->name        = PushStr8Copy(arena, output);
            Get(&tokenizer, channel->transforms, sizeof(channel->transforms[0]) * animation->numFrames);
        }
        Assert(EndOfBuffer(&tokenizer));
    }
    OS_Release(tokenizer.input.str);
}

//////////////////////////////
// Texture streaming
//
enum T_LoadStatus
{
    T_LoadStatus_Empty,
    T_LoadStatus_Unloaded,
    T_LoadStatus_Loading,
    T_LoadStatus_Loaded,
};

struct TextureOp
{
    T_LoadStatus status;
    AS_Node *assetNode;

    u8 *buffer;

    TextureOp *next;
    TextureOp *prev;

    u64 pboHandle;
};

struct TextureQueue
{
    TextureOp ops[64];
    u32 numOps;

    // Loaded -> GPU
    u32 finalizePos;
    // Unloaded -> Loading
    u32 loadPos;

    // Add Unloaded Textures to be loaded
    u32 writePos;

    TicketMutex mutex;
};

struct T_State
{
    Arena *arena;
    TextureQueue queue;
};

global T_State *t_state;

internal void T_Init()
{
    Arena *arena   = ArenaAllocDefault();
    t_state        = PushStruct(arena, T_State);
    t_state->arena = arena;

    // Push free texture ops to use
    t_state->queue.numOps = 64;
}

JOB_CALLBACK(LoadTextureCallback)
{
    TextureOp *op = (TextureOp *)data;
    i32 width, height, nChannels;
    u8 *texData = (u8 *)stbi_load_from_memory(op->assetNode->data.str, (i32)op->assetNode->data.size, &width,
                                              &height, &nChannels, 4);
    MemoryCopy(op->buffer, texData, width * height * 4);
    op->assetNode->texture.width  = width;
    op->assetNode->texture.height = height;
    stbi_image_free(texData);

    WriteBarrier();
    op->status = T_LoadStatus_Loaded;

    return 0;
}

// NOTE: if there isn't space it just won't push
internal void PushTextureQueue(AS_Node *node)
{
    TextureQueue *queue = &t_state->queue;
    if (AtomicCompareExchangeU32(&node->status, AS_Status_Queued, AS_Status_Unloaded) == AS_Status_Unloaded)
    {
        TicketMutexScope(&queue->mutex)
        {
            for (;;)
            {
                u32 availableSpots = queue->numOps - (queue->writePos - queue->finalizePos);
                if (availableSpots >= 1)
                {
                    u32 ringIndex = (queue->writePos++ & (queue->numOps - 1));
                    TextureOp *op = queue->ops + ringIndex;
                    op->status    = T_LoadStatus_Unloaded;
                    op->assetNode = node;
                    break;
                }
                _mm_pause();
            }
        }
    }
}

internal void LoadTextureOps()
{
    TextureQueue *queue = &t_state->queue;

    TicketMutexScope(&queue->mutex)
    {
        // Gets the Pixel Buffer Object to map to
        while (queue->loadPos != queue->writePos)
        {
            u32 ringIndex = queue->loadPos & (queue->numOps - 1);
            TextureOp *op = queue->ops + ringIndex;
            Assert(queue->ops[ringIndex].status == T_LoadStatus_Unloaded);
            op->buffer    = 0;
            op->pboHandle = R_AllocateTexture2D(&op->buffer);
            if (op->buffer)
            {
                op->status = T_LoadStatus_Loading;
                queue->loadPos++;
                JS_Kick(LoadTextureCallback, op, 0, Priority_Low);
            }
            else
            {
                break;
            }
        }
        // Submits PBO to OpenGL
        while (queue->finalizePos != queue->loadPos)
        {
            u32 ringIndex = queue->finalizePos & (queue->numOps - 1);
            TextureOp *op = queue->ops + ringIndex;
            if (op->status == T_LoadStatus_Loaded)
            {
                Texture *texture   = &op->assetNode->texture;
                R_TexFormat format = R_TexFormat_Nil;
                switch (texture->type)
                {
                    case TextureType_Diffuse:
                        format = R_TexFormat_SRGB;
                        break;
                    case TextureType_Normal:
                    default:
                        format = R_TexFormat_RGBA8;
                        break;
                }
                R_SubmitTexture2D(&texture->handle, op->pboHandle, texture->width, texture->height, format);
                texture->loaded = true;
                queue->finalizePos++;
                op->status            = T_LoadStatus_Empty;
                op->assetNode->status = AS_Status_Loaded;
            }
            else
            {
                break;
            }
        }
    }
}

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
