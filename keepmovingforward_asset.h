#include <vector>
#define MAX_MATRICES_PER_VERTEX 4
#define MAX_BONES 200
#define MAX_VERTEX_COUNT 100000
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
    V3 tangent;
    // V3 bitangent;

    u32 boneIds[MAX_MATRICES_PER_VERTEX];
    f32 boneWeights[MAX_MATRICES_PER_VERTEX];
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

// struct BoneInfo
// {
//     string name;
//     Mat4 convertToBoneSpaceMatrix; // TODO: I don't think this is the right name
//
//     BoneInfo *nextInHash;
// };
//
// struct BoneInfoTable
// {
//     BoneInfo *slots;
//     u32 count;
// };

struct BoneInfo
{
    string name;
    u32 boneId;
    Mat4 convertToBoneSpaceMatrix; // TODO: I don't think this is the right name
};

struct VertexBoneInfoPiece
{
    u32 boneIndex;
    // string boneName;
    f32 boneWeight;
};

struct VertexBoneInfo
{
    i32 numMatrices; // is this necesary? maybe just hardcode 4
    VertexBoneInfoPiece pieces[MAX_MATRICES_PER_VERTEX];
};

struct Skeleton
{
    // BoneInfoTable boneTable;
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
    string name;
    // u32 parentId;
    b32 hasParent;
    string parentName;
    Mat4 transformToParent;
};

struct MeshNodeInfoArray
{
    MeshNodeInfo *info;
    u32 count;
    u32 cap;
};

// struct AnimationData {
//     AnimationTransform transform;
//     f32 time;
// };
// struct BoneChannel {
//     string name; 
//     AnimationData data[MAX_FRAMES];
// };

struct BoneChannel
{
    string name;
    AnimationTransform transforms[MAX_FRAMES];
};
struct Keyframe
{
    AnimationTransform transforms[MAX_BONES];
};

struct KeyframedAnimation
{
    BoneChannel *boneChannels;
    u32 numNodes;

    f32 duration;
    u32 numFrames;

    // b32 isActive;
};

struct AnimationPlayer
{
    KeyframedAnimation *currentAnimation;
    f32 currentTime;
    f32 duration;
    u32 numFrames;

    b32 isLooping;
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

    Mat4 globalInverseTransform;
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

internal void SkinMeshToAnimation(AnimationPlayer *player, Mesh *mesh, const AnimationTransform *transforms,
                                  MeshNodeInfoArray *infoArray, Mat4 globalInverseTransform, Mat4 *finalTransforms);
                                  // u32 vertexCount);
