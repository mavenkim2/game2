#ifndef SHADERINTEROP_MESH_H
#define SHADERINTEROP_MESH_H

#include "ShaderInterop.h"

#define CASCADE_PARAMS_BIND 0
#define SHADOW_MAP_BIND     1

struct MeshParams
{
    float4x4 transform;
    float4x4 modelViewMatrix;
    float4x4 modelMatrix;
};

UNIFORM(CascadeParams, CASCADE_PARAMS_BIND)
{
    float4x4 rLightViewProjectionMatrices[8];
    float4 rCascadeDistances;
    float4 rLightDir;
    float4 rViewPos;
};

struct PushConstant
{
    // float4x4 meshTransform;
    int meshIndex;
    int meshParamsBuffer;

    int cascadeNum;

    int vertexPos;
    int vertexNor;
    int vertexTan;
    int vertexUv;

    int albedo;
    int normal;
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
