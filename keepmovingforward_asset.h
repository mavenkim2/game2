#include <vector>
#define MAX_MATRICES_PER_VERTEX 4
#define MAX_BONES 500
#define MAX_VERTEX_COUNT 20000
#define MAX_FRAMES 200

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

enum TextureType
{
    TextureType_Diffuse,
    TextureType_Specular,
    TextureType_Normal,
    TextureType_Height
};

struct Texture
{
    b32 loaded;
    u8 *contents;
    u32 width;
    u32 height;

    u32 id;
    u32 type;
};
// struct Texture {
//     u32 id;
//     u32 type;
// };

/* struct BoneInfo
{
    String8 name;
    Mat4 convertToBoneSpaceMatrix; // TODO: I don't think this is the right name

    BoneInfo *nextInHash;
    BoneInfo *parent;
};

struct BoneInfoTable
{
    BoneInfo *slots;
    u32 count;
}; */

struct BoneInfo
{
    String8 name;
    Mat4 convertToBoneSpaceMatrix; // TODO: I don't think this is the right name
};

struct VertexBoneInfoPiece
{
    u32 boneIndex;
    f32 boneWeight;
};

struct VertexBoneInfo
{
    i32 numMatrices; // is this necesary? maybe just hardcode 4
    VertexBoneInfoPiece pieces[MAX_MATRICES_PER_VERTEX];
};

struct Skeleton
{
    BoneInfo *boneInfo;
    u32 boneCount;

    VertexBoneInfo *vertexBoneInfo;
};

struct AnimationTransform
{
    V3 translation;
    Quat rotation;
    V3 scale;
};

inline AnimationTransform Lerp(AnimationTransform t1, AnimationTransform t2, f32 t)
{
    AnimationTransform result;
    result.translation = Lerp(t1.translation, t2.translation, t);
    // NEIGHBORHOOD
    if (Dot(t1.rotation, t2.rotation) < 0)
    {
        result.rotation = Nlerp(t1.rotation, -t2.rotation, t);
    }
    else
    {
        result.rotation = Nlerp(t1.rotation, t2.rotation, t);
    }
    result.scale = Lerp(t1.scale, t2.scale, t);
    return result;
}

struct MeshNodeInfo
{
    String8 name;
    u32 parentId;
    Mat4 transformToParent;
};

struct MeshNodeInfoArray
{
    MeshNodeInfo* info;
    u32 count;
    u32 cap;
};

struct AnimationTransformData {
    f32 time;
    AnimationTransform transform;
};

struct BoneChannel {
    String8 name;
    AnimationTransformData transforms[MAX_FRAMES];
};

struct AnimationChannel
{
    BoneChannel boneChannels[MAX_BONES];
    f32 duration;
    u32 numFrames;
    // AnimationTransform transformStates[MAX_FRAMES];

    b32 isActive;
};

struct AnimationPlayer
{
    f32 currentTime;
    f32 currentDuration;
};

struct LoadedMesh
{
    std::vector<MeshVertex> vertices;
    std::vector<u32> indices;
    Skeleton *skeleton;
    // int VertexToSkeletonInfoMap[];
    // std::vector<Texture> textures;

    u32 vertexCount;
    u32 indexCount;
};
struct LoadedModel
{
    std::vector<LoadedMesh> meshes;
};

struct Mesh
{
    MeshVertex *vertices;
    u32 vertexCount;

    u32 *indices;
    u32 indexCount;

    Skeleton *skeleton;
};

struct Model
{
    Mesh *meshes;
    u32 meshCount;

    AnimationChannel* animationChannel;
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
