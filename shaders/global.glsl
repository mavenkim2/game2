#version 460 core
#define f32 float
#define u32 int unsigned
#define V2 vec2
#define V3 vec3
#define V4 vec4
#define Mat4 mat4

#define PI 3.14159265
const int MAX_CASCADES = 4;
const int MAX_BONES = 200;

// TODO: these should be passed in
#define MAX_TEXTURES_PER_MATERIAL 4
#define MAX_MESHES 32

// NOTE: this is the max size.(64 KB)
#define MAX_SKINNING_MATRICES 1024

struct MeshPerDrawParams 
{
    Mat4 mModelToWorldMatrix;
    uvec2 mIndex[MAX_TEXTURES_PER_MATERIAL];
    unsigned int mSlice[MAX_TEXTURES_PER_MATERIAL];
    int mJointOffset;
    int mIsPBR;

    int _pad[2];
};

layout (std140, binding = 2) uniform globalUniforms
{
    Mat4 viewPerspectiveMatrix;
    Mat4 viewMatrix;
    V4 viewPosition;
    // TODO: light struct
    V4 lightDir;
    V4 cascadeDistances;
};

layout (std430, binding = 3) readonly buffer perDrawUniforms
{
    MeshPerDrawParams rMeshParams[MAX_MESHES];
};

layout (std140, binding = 4) uniform skinningMatrices
{
    Mat4 rBoneTransforms[MAX_SKINNING_MATRICES];
};
