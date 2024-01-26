#include <vector>
struct Iter
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

struct MeshVertex
{
    V3 position;
    V3 normal;
    V2 uv;
};

enum TextureType {
    TextureType_Diffuse,
    TextureType_Specular,
    TextureType_Normal,
    TextureType_Height
};

struct Texture {
    b32 loaded;
    u8* contents;
    u32 width;
    u32 height;

    u32 id;
    u32 type;
};
// struct Texture {
//     u32 id;
//     u32 type;
// };

struct LoadedMesh {
    std::vector<MeshVertex> vertices;
    std::vector<u32> indices;
    // std::vector<Texture> textures;

    u32 vertexCount;
    u32 indexCount;
};
struct LoadedModel {
    std::vector<LoadedMesh> meshes; 
};

struct Mesh {
    MeshVertex *vertices;
    u32 vertexCount;

    u32 *indices;
    u32 indexCount;
};

struct Model {
    Mesh* meshes;
    u32 meshCount;
};

// NOTE: Temporary hash
struct FaceVertex
{
    V3I32 indices;
    u16 index;

    FaceVertex *nextInHash;
};

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

struct TGAResult
{
    u8 *contents;
    u32 width;
    u32 height;
};
