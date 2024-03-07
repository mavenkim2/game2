#ifndef ASSET_H
#define ASSET_H

#include "crack.h"
#ifdef LSP_INCLUDE
#include "keepmovingforward_common.h"
#include "keepmovingforward_math.h"
#include "keepmovingforward_string.h"
#endif

#define MAX_MATRICES_PER_VERTEX 4
#define MAX_FRAMES              200

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

    u32 boneIds[MAX_MATRICES_PER_VERTEX];
    f32 boneWeights[MAX_MATRICES_PER_VERTEX];
};

// struct BoneInfo
// {
//     string name;
//     u32 boneId;
//     Mat4 convertToBoneSpaceMatrix;
// };

// struct Skeleton
// {
//     u32 count;
//     Array(string) names;
//     Array(i32) parents;
//     Array(Mat4) inverseBindPoses;
//     Array(Mat4) transformsToParent;
// };

struct LoadedSkeleton
{
    u32 count;
    string *names;
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

struct Material
{
    u32 startIndex;
    u32 onePlusEndIndex;
    AS_Handle textureHandles[TextureType_Count];
};

struct LoadedModel
{
    u32 vertexCount;
    u32 indexCount;
    MeshVertex *vertices;
    u32 *indices;

    AS_Handle skeleton;

    u32 materialCount;
    Material *materials;
};

struct Model
{
    AS_Handle loadedModel;
    Mat4 transform;
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

//////////////////////////////
// Function
//
struct AS_Node;
internal void PushTextureQueue(AS_Node *node);
internal void LoadTextureOps();

#endif
