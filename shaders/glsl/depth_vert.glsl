#version 450
#include "global.glsl"

// layout (location = 0) in vec3 pos;
// layout (location = 1) in vec3 n;
// layout (location = 2) in vec2 uv;
// layout (location = 3) in vec3 tangent;
//
// layout (location = 4) in uvec4 boneIds;
// layout (location = 5) in vec4 boneWeights;

layout(binding = MODEL_PARAMS_BIND) uniform ModelBufferObject
{
    Ubo ubo;
};

layout(binding = SKINNING_BIND) uniform SkinningBuffer
{
    mat4 transforms[1024];
} skinning;

layout(push_constant) uniform PushConstant_
{
    ShadowPushConstant push;
};

void main()
{
    vec3 pos = GetVec3(push.vertexPos, gl_VertexIndex);
    uvec4 boneIds = bindlessStorageBuffersUVec4[nonuniformEXT(push.vertexBoneId)].v[gl_VertexIndex];
    vec4 boneWeights = GetVec4(push.vertexBoneWeight, gl_VertexIndex);
    if (push.skinningOffset != -1)
    {
        mat4 boneTransform = skinning.transforms[push.skinningOffset + boneIds[0]] * boneWeights[0];
        boneTransform += skinning.transforms[push.skinningOffset + boneIds[1]] * boneWeights[1];
        boneTransform += skinning.transforms[push.skinningOffset + boneIds[2]] * boneWeights[2];
        boneTransform += skinning.transforms[push.skinningOffset + boneIds[3]] * boneWeights[3];
        vec4 worldSpacePos = ubo.params[push.modelIndex].modelMatrix * boneTransform * vec4(pos, 1.0);
        gl_Position = ubo.lightViewProjectionMatrices[push.cascadeNum] * worldSpacePos;
    }
    else
    {
        gl_Position = ubo.lightViewProjectionMatrices[push.cascadeNum] * ubo.params[push.modelIndex].modelMatrix * vec4(pos, 1.0);
    }
}
