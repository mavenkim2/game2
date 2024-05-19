#version 450
#include "global.glsl"
// layout (location = 0) in vec3 pos;
// layout (location = 1) in vec3 n;
// layout (location = 2) in vec2 uv;
// layout (location = 3) in vec3 tangent;

// layout (location = 4) in uvec4 boneIds;
// layout (location = 5) in vec4 boneWeights;

out VS_OUT
{
    layout(location = 0) out vec2 uv;
    layout(location = 1) out vec4 viewFragPos;
    layout(location = 2) out vec4 worldFragPos;
    layout(location = 3) out mat3 tbn;
} result;

layout(std140, binding = MODEL_PARAMS_BIND) uniform ModelBufferObject
{
    Ubo ubo;
};

layout(binding = SKINNING_BIND) uniform SkinningBuffer
{
    mat4 transforms[1024];
} skinning;

layout(push_constant) uniform PushConstant_
{
    PushConstant push;
};

void main()
{
    vec4 modelSpacePos;
    vec3 pos = GetVec3(push.vertexPos, gl_VertexIndex);
    vec3 n = GetVec3(push.vertexNor, gl_VertexIndex);
    vec2 uv = GetVec2(push.vertexUv, gl_VertexIndex);
    vec3 tangent = GetVec3(push.vertexTan, gl_VertexIndex);
    uvec4 boneIds = bindlessStorageBuffersUVec4[nonuniformEXT(push.vertexBoneId)].v[gl_VertexIndex];
    vec4 boneWeights = GetVec4(push.vertexBoneWeight, gl_VertexIndex);

    mat4 modelToWorldMatrix = ubo.params[push.modelIndex].modelMatrix;
    if (push.skinningOffset != -1)
    {
        mat4 boneTransform = skinning.transforms[push.skinningOffset + boneIds[0]] * boneWeights[0];
        boneTransform += skinning.transforms[push.skinningOffset + boneIds[1]] * boneWeights[1];
        boneTransform += skinning.transforms[push.skinningOffset + boneIds[2]] * boneWeights[2];
        boneTransform += skinning.transforms[push.skinningOffset + boneIds[3]] * boneWeights[3];

        modelSpacePos = boneTransform * vec4(pos, 1.0);
        gl_Position = ubo.params[push.modelIndex].transform * modelSpacePos;
        modelToWorldMatrix = modelToWorldMatrix * boneTransform;
    }
    else
    {
        modelSpacePos = push.meshTransform * vec4(pos, 1.0);
        gl_Position = ubo.params[push.modelIndex].transform * modelSpacePos;
    }

    result.uv = uv;
    result.viewFragPos = ubo.params[push.modelIndex].modelViewMatrix * modelSpacePos;
    result.worldFragPos = modelToWorldMatrix * modelSpacePos;

    vec3 tN = normalize(mat3(modelToWorldMatrix) * n);
    vec3 tT = normalize(mat3(modelToWorldMatrix) * tangent);
    vec3 tB = cross(tN, tT);
    result.tbn = mat3(tT, tB, tN);
}
