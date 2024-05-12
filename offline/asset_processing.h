#include "../mkCommon.h"
#include "../mkMath.h"
#include "../mkMemory.h"
#include "../mkString.h"
#include "../mkList.h"
#include "../mkPlatformInc.h"
#include "../render/mkRenderCore.h"
#include "../render/mkGraphics.h"
#include "../mkThreadContext.h"
#include "../mkShared.h"
#include "../mkJob.h"
#include "../mkAsset.h"
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
    string *names;
    i32 *parents;
    Mat4 *inverseBindPoses;
    Mat4 *transformsToParent;
    u32 count;
};

//////////////////////////////
// Mesh Info
//
struct InputMaterial
{
    string name;
    string texture[TextureType_Count];

    f32 metallicFactor  = 0.f;
    f32 roughnessFactor = 1.f;
    V4 baseColor        = {1, 1, 1, 1};
};

struct InputMesh
{
    MeshVertex *vertices;
    u32 *indices;
    u32 vertexCount;
    u32 indexCount;

    // InputMaterial material;
    string materialName;
    Rect3 bounds;
};

struct InputModel
{
    Skeleton skeleton;
    InputMesh *meshes;
    u32 numMeshes;

    // MeshVertex *vertices;
    // u32 *indices;

    // u32 vertexCount;
    // u32 indexCount;

    // One material per mesh, each material can have multiple textures (normal, diffuse, etc.)
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
    list<AnimationPosition> positions;
    list<AnimationScale> scales;
    list<CompressedAnimationRotation> rotations;
    // AnimationPosition *positions;
    // AnimationScale *scales;
    // CompressedAnimationRotation *rotations;

    // u32 numPositionKeys;
    // u32 numScalingKeys;
    // u32 numRotationKeys;
};

struct CompressedKeyframedAnimation
{
    // CompressedBoneChannel *boneChannels;
    list<CompressedBoneChannel> boneChannels;
    u32 numNodes;

    f32 duration;
};

//////////////////////////////
// Assimp Specific Loading
//

struct AssimpSkeletonAsset
{
    u32 count;
    string *names;
    i32 *parents;
    Mat4 *inverseBindPoses;
    VertexBoneInfo *vertexBoneInfo;

    u32 nameCount;
    u32 parentCount;
    u32 inverseBPCount;
    u32 vertexBoneInfoCount;
    // Array(string) names;
    // Array(i32) parents;
    // Array(Mat4) inverseBindPoses;
    // Array(VertexBoneInfo) vertexBoneInfo;
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

internal void OptimizeMesh(InputMesh *mesh);
internal Rect3 GetMeshBounds(InputMesh *mesh);
internal void SkinModelToBindPose(const InputModel *inModel, Mat4 *outFinalTransforms);
