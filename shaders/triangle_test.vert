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
} result;

layout(binding = 0) uniform ModelBufferObject 
{
    mat4 transform;
} ubo;

void main()
{
    gl_Position = ubo.transform * vec4(pos, 1.0); 
    result.uv = uv;
}
