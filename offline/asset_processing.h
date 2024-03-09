#include "../keepmovingforward_common.h"
#include "../keepmovingforward_math.h"
#include "../keepmovingforward_memory.h"
#include "../keepmovingforward_string.h"
#include "../platform_inc.h"
#include "../thread_context.h"
#include "../job.h"
#include "../third_party/assimp/Importer.hpp"
#include "../third_party/assimp/scene.h"
#include "../third_party/assimp/postprocess.h"

#define MAX_MATRICES_PER_VERTEX 4
#define MAX_BONES               200
#define MAX_FRAMES              200

//////////////////////////////
// Node Info
//
struct AnimationTransform
{
    V3 translation;
    Quat rotation;
    V3 scale;
};

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
// Texture Info
//
enum TextureType
{
    TextureType_Diffuse,
    TextureType_Normal,
    TextureType_Specular,
    TextureType_Height,
    TextureType_Count,
};

//////////////////////////////
// Mesh Info
//
struct MeshVertex
{
    V3 position;
    V3 normal;
    V2 uv;
    V3 tangent;

    u32 boneIds[MAX_MATRICES_PER_VERTEX];
    f32 boneWeights[MAX_MATRICES_PER_VERTEX];
};

struct Material
{
    u32 startIndex;
    u32 onePlusEndIndex;
    string texture[TextureType_Count];
};

struct Model
{
    Array(MeshVertex) vertices;
    Array(u32) indices;

    Skeleton skeleton;

    // One material per mesh, each material can have multiple textures (normal, diffuse, etc.)
    Material materials[4];
    u32 materialCount;

    Mat4 transform;
};

//////////////////////////////
// Animation
//
struct BoneChannel
{
    string name;
    AnimationTransform *transforms;
};

struct KeyframedAnimation
{
    BoneChannel *boneChannels;
    u32 numNodes;

    f32 duration;
    u32 numFrames;
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

struct ModelOutput
{
    Model model;
    KeyframedAnimation *animation;
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
    Model *model;
    string directory;
    string path;
};

struct AnimationJobData
{
    aiAnimation *inAnimation;

    KeyframedAnimation *outAnimation;
    string outName;
};

struct AnimationJobWriteData
{
    KeyframedAnimation *animation;
    string path;
};
