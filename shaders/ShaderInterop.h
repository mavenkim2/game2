#ifndef SHADERINTEROP_H
#define SHADERINTEROP_H

#ifdef __cplusplus

using mat4 = Mat4;
using vec4 = V4;
using vec3 = V3;
using vec2 = V2;

#endif

const int MODEL_PARAMS_BIND = 0;
const int SKINNING_BIND     = 1;
const int SHADOW_MAP_BIND   = 2;

struct ModelParams
{
    mat4 transform;
    mat4 modelViewMatrix;
    mat4 modelMatrix;
};

struct Ubo
{
    ModelParams params[8];
    mat4 lightViewProjectionMatrices[8];
    vec4 cascadeDistances;
    vec4 lightDir;
    vec4 viewPos;
};

struct PushConstant
{
    mat4 meshTransform;
    int modelIndex;
    int skinningOffset;

    int vertexPos;
    int vertexNor;
    int vertexTan;
    int vertexUv;
    int vertexBoneId;
    int vertexBoneWeight;

    int albedo;
    int normal;
};

struct ShadowPushConstant
{
    int modelIndex;
    int skinningOffset;
    int cascadeNum;

    int vertexPos;
    int vertexNor;
    int vertexTan;
    int vertexUv;
    int vertexBoneId;
    int vertexBoneWeight;
};

#endif
