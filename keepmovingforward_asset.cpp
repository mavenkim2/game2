#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_TGA
#include "third_party/stb_image.h"

#if INTERNAL
#include "third_party/assimp/Importer.hpp"
#include "third_party/assimp/scene.h"
#include "third_party/assimp/postprocess.h"
#endif

// TODO: <= or < ?
inline b32 IsEndOfFile(Iter *iter)
{
    b32 result = iter->cursor == iter->end;
    return result;
}

inline u32 GetType(Iter iter)
{
    String8 type = Str8(iter.cursor, 2);
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
    f32 value = 0;
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
        i32 i = 0;
        c = *iter->cursor++;
        sign = c == '+' ? 1 : -1;
        c = *iter->cursor++;
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

// TODO: # vertices is hardcoded, handle groups?
// Add indices
#if 0
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

internal String8 ConsumeLine(Iter *iter)
{
    String8 result;
    result.str = iter->cursor;
    u32 size = 0;
    while (*iter->cursor++ != '\n')
    {
        size++;
    }
    result.size = size;
    return result;
}

internal LoadedMesh ProcessMesh(aiMesh *mesh, const aiScene *scene)
{
    LoadedMesh result;
    std::vector<MeshVertex> vertices;
    std::vector<u32> indices;

    for (u32 i = 0; i < mesh->mNumVertices; i++)
    {
        MeshVertex vertex;
        vertex.position.x = mesh->mVertices[i].x;
        vertex.position.y = mesh->mVertices[i].y;
        vertex.position.z = mesh->mVertices[i].z;

        if (mesh->HasNormals())
        {
            vertex.normal.x = mesh->mNormals[i].x;
            vertex.normal.y = mesh->mNormals[i].y;
            vertex.normal.z = mesh->mNormals[i].z;
        }
        // surely if you have vertex with multiple texture coordinates, it just becomes a different vertex altogeter?
        if (mesh->mTextureCoords[0])
        {
            vertex.uv.u = mesh->mTextureCoords[0][i].x;
            vertex.uv.y = mesh->mTextureCoords[0][i].y;
            // TODO: load tangents/bitangents maybe
        }
        else {
            vertex.uv = {0, 0};
        }
        vertices.push_back(vertex);
        
    }
    for (u32 i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (u32 indexindex = 0; indexindex < face.mNumIndices; indexindex++) {
            indices.push_back(face.mIndices[indexindex]);
        }
    }
    result.vertices = vertices;
    result.indices = indices;
    result.vertexCount = (u32)vertices.size();
    result.indexCount = (u32)indices.size();
    
    //TODO: Load materials
    // aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
    return result;
}

internal void ProcessNode(LoadedModel *model, aiNode *node, const aiScene *scene)
{
    for (u32 i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
        model->meshes.push_back(ProcessMesh(mesh, scene));
    }
    for (u32 i = 0; i < node->mNumChildren; i++)
    {
        ProcessNode(model, node->mChildren[i], scene);
    }
}

internal Model ConvertModel(Arena* arena, LoadedModel* model) {
    Model result = {};
    u32 meshCount = (u32)model->meshes.size();
    result.meshCount = meshCount;
    Mesh* meshes = PushArray(arena, Mesh, meshCount);
    for (u32 i = 0; i < meshCount; i++) {
        u32 vertexCount = model->meshes[i].vertexCount;
        u32 indexCount = model->meshes[i].indexCount;

        MeshVertex* vertices = PushArray(arena, MeshVertex, vertexCount);
        u32* indices = PushArray(arena, u32, indexCount);

        std::copy(model->meshes[i].vertices.begin(), model->meshes[i].vertices.end(), vertices);
        std::copy(model->meshes[i].indices.begin(), model->meshes[i].indices.end(), indices);

        meshes[i].vertices = vertices;
        meshes[i].indices = indices;
        meshes[i].vertexCount = vertexCount;
        meshes[i].indexCount = indexCount;
    }
    result.meshes = meshes;
    return result;
}

// ANIMATION: 
// if you have a parent node, multiple by the inverse of the parent offset matrix, then your offset matrix
struct Skeleton {};
struct AnimationChannel {};
struct Animation {
};
// internal Mat4 ConvertAssimpMatrix4x4(aiMatrix4x4t a) {
//     Mat4 result;
//     result.
// }
// internal Skeleton
/*
 * ASSIMP
 */
// NOTE IMPORTANT: WE DO NOT CARE ABOUT THIS CODE AT ALL. EVENTUALLY WE WILL LOAD DIFFERENT FILE TYPES
// (GLTF, FBX, OBJ, BMP, TGA, WAV, ETC), AND JUST BAKE ALL THE ASSETS INTO ONE MEGA FILE THAT WE CAN EASILY
// READ/ACCESS/COMPRESS.
internal Model AssimpDebugLoadModel(Arena* arena, String8 filename)
{
    LoadedModel loadedModel = {};
    Model model = {};
    Assimp::Importer importer;
    const char *file = (const char *)filename.str;
    const aiScene *scene = importer.ReadFile(file, aiProcess_Triangulate | aiProcess_FlipUVs);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        return model;
    }

    aiMatrix4x4t<f32> globalTransform = scene->mRootNode->mTransformation;
    ProcessNode(&loadedModel, scene->mRootNode, scene);

    model = ConvertModel(arena, &loadedModel);
    return model;
}
/*
 * END ASSIMP
 */

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
