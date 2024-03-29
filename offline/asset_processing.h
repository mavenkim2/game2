#include "../keepmovingforward_common.h"
#include "../keepmovingforward_math.h"
#include "../keepmovingforward_memory.h"
#include "../keepmovingforward_string.h"
#include "../platform_inc.h"
#include "../thread_context.h"
#include "../job.h"
#include "../render/render_core.h"
#include "../asset.h"
#include "../third_party/assimp/Importer.hpp"
#include "../third_party/assimp/scene.h"
#include "../third_party/assimp/postprocess.h"

#define MAX_MATRICES_PER_VERTEX 4
#define MAX_BONES               200
#define MAX_FRAMES              200

//////////////////////////////
// Skeleton/ Node Info
//
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
    string filename;
    u32 count;
    Array(string) names;
    Array(i32) parents;
    Array(Mat4) inverseBindPoses;
    Array(Mat4) transformsToParent;
};

//////////////////////////////
// Mesh Info
//
struct InputMaterial
{
    u32 startIndex;
    u32 onePlusEndIndex;
    string texture[TextureType_Count];
};

struct InputModel
{
    Array(MeshVertex) vertices;
    Array(u32) indices;

    Skeleton skeleton;

    // One material per mesh, each material can have multiple textures (normal, diffuse, etc.)
    InputMaterial materials[4];
    u32 materialCount;

    Mat4 transform;
};

//////////////////////////////
// Animation
//
// struct AnimationPosition
// {
//     V3 position;
//     f32 time;
// };
//
// struct AnimationScale
// {
//     V3 scale;
//     f32 time;
// };

struct CompressedAnimationRotation
{
    u16 rotation[4];
    // Quat rotation;
    f32 time;
};

struct CompressedBoneChannel
{
    string name;
    AnimationPosition *positions;
    AnimationScale *scales;
    CompressedAnimationRotation *rotations;

    u32 numPositionKeys;
    u32 numScalingKeys;
    u32 numRotationKeys;
};

struct CompressedKeyframedAnimation
{
    CompressedBoneChannel *boneChannels;
    u32 numNodes;

    f32 duration;
};

//////////////////////////////
// Assimp Specific Loading
//

struct AssimpSkeletonAsset
{
    u32 count;
    Array(string) names;
    Array(i32) parents;
    Array(Mat4) inverseBindPoses;
    Array(VertexBoneInfo) vertexBoneInfo;
};

struct BoneInfo
{
    string name;
    u32 boneId;
    Mat4 convertToBoneSpaceMatrix;
};

struct Data
{
    string directory;
    string filename;
};

//////////////////////////////
// Job Data
//
struct SkeletonJobData
{
    Skeleton *skeleton;
    string path;
};

struct ModelJobData
{
    InputModel *model;
    string directory;
    string path;
};

struct AnimationJobData
{
    aiAnimation *inAnimation;

    CompressedKeyframedAnimation *outAnimation;
    string outName;
};

struct AnimationJobWriteData
{
    CompressedKeyframedAnimation *animation;
    string path;
};
