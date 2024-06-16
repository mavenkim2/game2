#ifndef SHADERINTEROP_MESH_H
#define SHADERINTEROP_MESH_H

#include "ShaderInterop.h"

#define CASCADE_PARAMS_BIND 0
#define SHADOW_MAP_BIND     1

#define BATCH_SIZE 256
#define SKINNING_GROUP_SIZE 64

struct MeshParams
{
    float4x4 transform;
    float4x4 modelViewMatrix;
    float4x4 modelMatrix;
    float3 minP;
    float _pad0;
    float3 maxP;
    float _pad1;
};

struct MeshGeometry
{
    int vertexPos;
    int vertexNor;
    int vertexTan;
    int vertexUv;
    int vertexInd;
};

struct MeshBatch
{
    // uint baseVertex;
    uint drawID;
    uint indexOffset;
    uint indexCount;

    uint firstBatch;
    uint meshIndex;

    uint outputIndexOffset;
    // uint materialIndex;
};

struct ShaderMeshSubset
{
    uint meshIndex;
    uint materialIndex;
};

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

struct PushConstant
{
    int meshParamsDescriptor;
    int subsetDescriptor;
    int materialDescriptor;
    int geometryDescriptor;

    int cascadeNum;
};

struct TriangleCullPushConstant
{
    int meshBatchDescriptor;
    int meshGeometryDescriptor;
    int meshParamsDescriptor;
};

// struct PushConstant
// {
//     // float4x4 meshTransform;
//     int meshIndex;
//     int meshParamsBuffer;
//
//     int cascadeNum;
//
//     int vertexPos;
//     int vertexNor;
//     int vertexTan;
//     int vertexUv;
//
//     int albedo;
//     int normal;
// };

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
