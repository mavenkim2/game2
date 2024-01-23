// TODO: <= or < ?
inline b32 IsEndOfFile(OBJIter* iter)
{
    b32 result = iter->cursor == iter->end;
    return result;
}

inline u32 GetType(OBJIter iter)
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
inline f32 ReadFloat(OBJIter *iter)
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
    if (valueSign) {
        value = -value;
    }
    return value;
}

inline V3I32 ParseFaceVertex(OBJIter *iter)
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

inline void SkipToNextLine(OBJIter *iter)
{
    while (!IsEndOfFile(iter) && *iter->cursor != '\n')
    {
        iter->cursor++;
    }
    iter->cursor++;
}

inline void GetNextWord(OBJIter *iter)
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

    OBJIter iter;
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
