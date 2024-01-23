struct OBJIter
{
    u8 *cursor;
    u8 *end;
};

enum OBJLineType
{
    OBJ_Vertex,
    OBJ_Normal,
    OBJ_Texture,
    OBJ_Face,
    OBJ_Invalid,
};

struct ModelVertex
{
    V3 position;
    V3 normal;
    V2 uv;
};

struct Model
{
    ModelVertex *vertices;
    u32 vertexCount;
};
