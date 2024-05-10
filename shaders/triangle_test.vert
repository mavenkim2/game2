#version 450
layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 n;
layout (location = 2) in vec2 uv;
layout (location = 3) in vec3 tangent;

layout (location = 4) in uvec4 boneIds;
layout (location = 5) in vec4 boneWeights;

out VS_OUT
{
    layout (location = 0) out vec2 uv;
    layout (location = 1) out vec4 viewFragPos;
    layout (location = 2) out vec4 worldFragPos;
    layout (location = 3) out mat3 tbn;
} result;

struct ModelParams
{
    mat4 transform;
    mat4 modelViewMatrix;
    mat4 modelMatrix;
};

layout(std140, binding = 0) uniform ModelBufferObject 
{
    ModelParams params[8];
    mat4 lightViewProjectionMatrices[8];
    vec4 cascadeDistances;
    vec4 lightDir;
    vec4 viewPos;
} ubo;

layout(binding = 1) uniform SkinningBuffer
{
    mat4 transforms[1024];
} skinning;

layout (push_constant) uniform PushConstant 
{
    int modelIndex;
    int skinningOffset;
} pc;

void main()
{
    // TODO: compute shader for skinning buffers
    mat4 boneTransform = skinning.transforms[pc.skinningOffset + boneIds[0]] * boneWeights[0];
    boneTransform     += skinning.transforms[pc.skinningOffset + boneIds[1]] * boneWeights[1];
    boneTransform     += skinning.transforms[pc.skinningOffset + boneIds[2]] * boneWeights[2];
    boneTransform     += skinning.transforms[pc.skinningOffset + boneIds[3]] * boneWeights[3];

    mat4 modelToWorldMatrix = ubo.params[pc.modelIndex].modelMatrix;
    vec4 modelSpacePos = boneTransform * vec4(pos, 1.0);
    gl_Position = ubo.params[pc.modelIndex].transform * modelSpacePos;
    result.uv = uv;
    result.viewFragPos = ubo.params[pc.modelIndex].modelViewMatrix * modelSpacePos;
    result.worldFragPos = modelToWorldMatrix * modelSpacePos;

    modelToWorldMatrix = modelToWorldMatrix * boneTransform;
    vec3 tN = normalize(mat3(modelToWorldMatrix) * n);
    vec3 tT = normalize(mat3(modelToWorldMatrix) * tangent);
    vec3 tB = cross(tN, tT);
    result.tbn = mat3(tT, tB, tN);
}
