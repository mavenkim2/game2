#ifndef SHADERINTEROP_MESH_H
#define SHADERINTEROP_MESH_H

#include "ShaderInterop.h"

#define CASCADE_PARAMS_BIND 0
#define SHADOW_MAP_BIND     1

#define SKINNING_GROUP_SIZE 64
#define CLUSTER_SIZE        256

STRUCT(MeshParams)
{
    // float4x4 transform;
    // float4x4 modelViewMatrix;
    float4x4 localToWorld;
    float4x4 prevLocalToWorld;
    float3 minP;
    uint clusterOffset;
    float3 maxP;
    uint clusterCount;
};

STRUCT(GPUView)
{
    float4x4 worldToClip;
    float4x4 prevWorldToClip;
    float p22;
    float p23;
};

struct MeshGeometry
{
    int vertexPos;
    int vertexNor;
    int vertexTan;
    int vertexUv;
    int vertexInd;
};

struct MeshChunk
{
    uint numClusters;
    uint clusterOffset;
};

struct MeshCluster
{
    float3 minP;
    uint meshIndex;
    float3 maxP;
    uint indexOffset;
    uint indexCount;
    uint materialIndex;
    uint2 _pad;
};

// an instance in a shader uses only one material, but in the cpu app code it can have multiple materials, so
// multiple instances are spawned

struct ShaderMaterial
{
    int albedo;
    int normal;
};

struct DrawIndexedIndirectCommand
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    uint vertexOffset;
    uint firstInstance;
};

#ifdef __cplusplus
StaticAssert(sizeof(DrawIndexedIndirectCommand) == 20, IndirectStructSize);
#endif

UNIFORM(CascadeParams, CASCADE_PARAMS_BIND)
{
    float4x4 rLightViewProjectionMatrices[8];
    float4 rCascadeDistances;
    float4 rLightDir;
    float4 rViewPos;
};

//////////////////////////////
// Push constants
//
struct PushConstant
{
    float4x4 worldToClip;

    int meshParamsDescriptor;
    int meshClusterDescriptor;
    int materialDescriptor;
    int geometryDescriptor;
    int cascadeNum;
};

struct SkinningPushConstants
{
    int vertexPos;
    int vertexNor;
    int vertexTan;
    int vertexBoneId;
    int vertexBoneWeight;

    int soPos;
    int soNor;
    int soTan;

    int skinningOffset;
    int skinningBuffer;
};

#endif
