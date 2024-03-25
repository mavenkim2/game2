#ifndef ASSET_H
#define ASSET_H

#include "crack.h"
#ifdef LSP_INCLUDE
#include "keepmovingforward_common.h"
#include "keepmovingforward_math.h"
#include "keepmovingforward_string.h"
#endif

#define MAX_MATRICES_PER_VERTEX 4

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
    i32 width;
    i32 height;
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

struct AnimationPosition
{
    V3 position;
    f32 time;
};

struct AnimationScale
{
    V3 scale;
    f32 time;
};

struct AnimationRotation
{
    // Quat rotation;
    u16 rotation[4];
    f32 time;
};

struct BoneChannel
{
    string name;
    AnimationPosition *positions;
    AnimationScale *scales;
    AnimationRotation *rotations;

    u32 numPositionKeys;
    u32 numScalingKeys;
    u32 numRotationKeys;
};

struct KeyframedAnimation
{
    BoneChannel *boneChannels;
    u32 numNodes;

    f32 duration;
};

struct AnimationPlayer
{
    AS_Handle anim;
    KeyframedAnimation *currentAnimation;
    f32 currentTime;
    f32 duration;

    u32 currentPositionKey[300];
    u32 currentScaleKey[300];
    u32 currentRotationKey[300];

    // u32 *currentPositionKey;
    // u32 *currentScaleKey;
    // u32 *currentRotationKey;

    b32 isLooping;
    b8 loaded;
};

struct Material
{
    AS_Handle textureHandles[TextureType_Count];
    u32 startIndex;
    u32 onePlusEndIndex;
};

struct LoadedModel
{
    AS_Handle skeleton;
    MeshVertex *vertices;
    u32 *indices;
    Material *materials;

    u32 vertexCount;
    u32 indexCount;
    u32 materialCount;

    R_Handle vertexBuffer;
    R_Handle indexBuffer;

    u32 _pad[1];
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

//////////////////////////////
// Function
//
inline u16 CompressRotationChannel(f32 q)
{
    u16 result = (u16)CompressFloat(q, -1.f, 1.f, 16u);
    return result;
}

inline f32 DecompressRotationChannel(u16 q)
{
    f32 result = DecompressFloat((u32)q, -1.f, 1.f, 16u);
    return result;
}

#endif
