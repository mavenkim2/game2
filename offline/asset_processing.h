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
#include "../mkAsset.h"
// #include "../third_party/assimp/Importer.hpp"
// #include "../third_party/assimp/scene.h"
// #include "../third_party/assimp/postprocess.h"

#define MAX_MATRICES_PER_VERTEX 4
#define MAX_BONES               200
#define MAX_FRAMES              200

//////////////////////////////
// Skeleton/ Node Info
//
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

    f32 metallicFactor  = 1.f;
    f32 roughnessFactor = 1.f;
    V4 baseColor        = {1, 1, 1, 1};
};

struct InputMesh
{
    struct MeshSubset
    {
        V3 *positions;
        V3 *normals;
        V2 *uvs;
        V3 *tangents;
        UV4 *boneIds;
        V4 *boneWeights;
        u32 *indices;

        u32 vertexCount;
        u32 indexCount;
        string materialName;
    };
    MeshSubset *subsets;

    Mat4 transform;
    u32 totalVertexCount;
    u32 totalIndexCount;
    u32 totalSubsets;
    Rect3 bounds;
    MeshFlags flags;
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

struct CompressedAnimationRotation
{
    u16 rotation[4];
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
// Job Data
//
internal void OptimizeMesh(InputMesh *mesh);
internal Rect3 GetMeshBounds(InputMesh *mesh);
internal void SkinModelToBindPose(const InputModel *inModel, Mat4 *outFinalTransforms);
