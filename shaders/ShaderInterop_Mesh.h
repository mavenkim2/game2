#ifndef SHADERINTEROP_MESH_H
#define SHADERINTEROP_MESH_H

#include "ShaderInterop.h"

#define MODEL_PARAMS_BIND 0
#define SKINNING_BIND     1
#define SHADOW_MAP_BIND   2

struct ModelParams
{
    float4x4 transform;
    float4x4 modelViewMatrix;
    float4x4 modelMatrix;
};

UNIFORM(Ubo, MODEL_PARAMS_BIND)
{
    ModelParams rParams[8];
    float4x4 rLightViewProjectionMatrices[8];
    float4 rCascadeDistances;
    float4 rLightDir;
    float4 rViewPos;
};

struct PushConstant
{
    float4x4 meshTransform;
    int modelIndex;
    int skinningOffset;
    int cascadeNum;

    int vertexPos;
    int vertexNor;
    int vertexTan;
    int vertexUv;
    int vertexBoneId;
    int vertexBoneWeight;

    int albedo;
    int normal;
};

// struct ShadowPushConstant
// {
//     int modelIndex;
//     int skinningOffset;
//     int cascadeNum;
//
//     int vertexPos;
//     int vertexNor;
//     int vertexTan;
//     int vertexUv;
//     int vertexBoneId;
//     int vertexBoneWeight;
// };

#endif