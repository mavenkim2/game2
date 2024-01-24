// #define STB_IMAGE_IMPLEMENTATION
// #define STBI_ONLY_TGA
// #include "third_party/stb_image.h"
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

// TODO: # vertices is hardcoded, handle groups?
// Add indices
internal Model DebugLoadModel(Arena *arena, DebugPlatformReadFileFunctionType *PlatformReadFile, const char *filename)
{
    DebugReadFileOutput output = PlatformReadFile(filename);
    Assert(output.fileSize != 0);

    Iter iter;
    iter.cursor = (u8 *)output.contents;
    iter.end = iter.cursor + output.fileSize;

    ModelVertex *vertices = PushArrayNoZero(arena, ModelVertex, 16000);
    u32 count = 0;
    TempArena tempArena = ScratchBegin(arena);

    V3 *positionVertices = PushArrayNoZero(tempArena.arena, V3, 3000);
    u32 positionCount = 0;
    V3 *normalVertices = PushArrayNoZero(tempArena.arena, V3, 3500);
    u32 normalCount = 0;
    V2 *uvVertices = PushArrayNoZero(tempArena.arena, V2, 3000);
    u32 uvCount = 0;

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
                    ModelVertex vertex;
                    vertex.position = positionVertices[indices.x - 1];
                    vertex.uv = uvVertices[indices.y - 1];
                    vertex.normal = normalVertices[indices.z - 1];
                    vertices[count++] = vertex;
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
    model.vertices = vertices;
    model.vertexCount = count;
    return model;
}

#pragma pack(push, 1)
struct TGAHeader
{
    u8 idLength;
    u8 colorMapType;
    u8 imageType;
    u16 colorMapOrigin;
    u16 colorMapLength;
    u8 colorMapEntrySize;
    u16 xOrigin;
    u16 yOrigin;
    u16 width;
    u16 height;
    u8 bitsPerPixel;
    u8 imageDescriptor;
};
#pragma pack(pop)

inline u8 GetByte(Iter *iter)
{
    u8 result = 0;
    if (iter->cursor < iter->end)
    {
        result = *iter->cursor++;
    }
    return result;
}

struct TGAResult
{
    u8 *contents;
    u32 width;
    u32 height;
};
internal TGAResult DebugLoadTGA(Arena *arena, DebugPlatformReadFileFunctionType *PlatformReadFile, const char *filename)
{
    // int width, height, nChannels;
    // u8* data = (u8*)stbi_load(filename, &width, &height, &nChannels, 0);
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
    if (tgaInverted)
    {
        for (u32 j = 0; j * 2 < height; j++)
        {
            i32 index1 = j * width * bytesPerPixel;
            i32 index2 = (height - 1 - j) * width * bytesPerPixel;
            for (i32 i = width * bytesPerPixel; i > 0; --i)
            {
                Swap(u8, result.contents[index1], result.contents[index2]);
                ++index1;
                ++index2;
            }
        }
    }

    // Converts from BGR to RGB
    if (bytesPerPixel >= 3) {
        u8* pixel = result.contents;
        for (u32 i = 0; i < width * height; i++) 
        {
            Swap(u8, pixel[0], pixel[2]);
            pixel += bytesPerPixel;
        }
    }
    return result;
}
