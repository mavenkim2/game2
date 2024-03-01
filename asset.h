#define MAX_MATRICES_PER_VERTEX 4
#define MAX_BONES               200
#define MAX_FRAMES              200

#define MAX_TEXTURES 10

typedef u32 R_Handle;

union AS_Handle
{
    u64 u64[2];
};

enum TextureType
{
    TextureType_Diffuse,
    TextureType_Normal,
    TextureType_Specular,
    TextureType_Height,
    TextureType_Count,
};

struct Texture
{
    R_Handle handle;
    u32 width;
    u32 height;
    TextureType type;
    b32 loaded;
};

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

struct BoneInfo
{
    string name;
    u32 boneId;
    Mat4 convertToBoneSpaceMatrix;
};

struct VertexBoneInfoPiece
{
    u32 boneIndex;
    f32 boneWeight;
};

struct VertexBoneInfo
{
    i32 numMatrices;
    VertexBoneInfoPiece pieces[MAX_MATRICES_PER_VERTEX];
};

struct Skeleton
{
    u32 count;
    Array(string) names;
    Array(i32) parents;
    Array(Mat4) inverseBindPoses;
    Array(Mat4) transformsToParent;
};

struct SkeletonHelp
{
    u32 count;
    i32 *parents;
    Mat4 *inverseBindPoses;
    Mat4 *transformsToParent;
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
    string parentName;
    b32 hasParent;
    Mat4 transformToParent;
};

struct MeshNodeInfoArray
{
    MeshNodeInfo *items;
    u32 count;
    u32 cap;
};

struct BoneChannel
{
    string name;
    AnimationTransform transforms[MAX_FRAMES];
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

struct Model
{
    Array(MeshVertex) vertices;
    Array(u32) indices;

    Skeleton skeleton;

    AS_Handle textures[4];
    u32 numTextures;

    struct
    {
        u32 startIndex;
        u32 onePlusEndIndex;
        R_Handle textureHandles[2];
    } materials[2];

    Mat4 transform;

    // TODO: maybe the model shouldn't own a vbo and ebo. maybe it can have an id or something
    // that the render state can map to a vbo. this could be useful for packing multiple models
    // into one buffer, to reduce the number of draw calls, but doing this now would be premature. 2/12/24
    u32 vbo;
    u32 ebo;
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

struct ModelOutput
{
    Model model;
    KeyframedAnimation *animation;
};

struct AssimpSkeletonAsset
{
    u32 count;
    Array(string) names;
    Array(i32) parents;
    Array(Mat4) inverseBindPoses;
    Array(VertexBoneInfo) vertexBoneInfo;
};
