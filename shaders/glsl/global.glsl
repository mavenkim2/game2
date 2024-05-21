#extension GL_EXT_nonuniform_qualifier : enable

#include "ShaderInterop.h"

layout(set = 1, binding = 0) uniform sampler2D bindlessTextures[];
layout(set = 2, binding = 0) buffer bindlessStorageBuffers_
{
    float v[];
} bindlessStorageBuffers[];

// layout(set = 2, binding = 0) buffer bindlessStorageBuffersVec2_
// {
//     vec2 v[];
// } bindlessStorageBuffersVec2[];

// layout(set = 2, binding = 0) buffer bindlessStorageBuffersVec3_
// {
//     vec3 v[];
// } bindlessStorageBuffersVec3[];

layout(set = 2, binding = 0) buffer bindlessStorageBuffersVec4_
{
    vec4 v[];
} bindlessStorageBuffersVec4[];

layout(set = 2, binding = 0) buffer bindlessStorageBuffersUVec4_
{
    uvec4 v[];
} bindlessStorageBuffersUVec4[];

vec2 GetVec2(int vertexBufferPos, int vertexIndex)
{
    vec2 pos;
    pos.x = bindlessStorageBuffers[nonuniformEXT(vertexBufferPos)].v[vertexIndex * 2];
    pos.y = bindlessStorageBuffers[nonuniformEXT(vertexBufferPos)].v[vertexIndex * 2 + 1];
    return pos;
}

vec3 GetVec3(int vertexBufferPos, int vertexIndex)
{
    vec3 pos;
    pos.x = bindlessStorageBuffers[nonuniformEXT(vertexBufferPos)].v[vertexIndex * 3];
    pos.y = bindlessStorageBuffers[nonuniformEXT(vertexBufferPos)].v[vertexIndex * 3 + 1];
    pos.z = bindlessStorageBuffers[nonuniformEXT(vertexBufferPos)].v[vertexIndex * 3 + 2];
    return pos;
}

vec4 GetVec4(int vertexBufferPos, int vertexIndex)
{
    vec4 pos;
    pos.x = bindlessStorageBuffers[nonuniformEXT(vertexBufferPos)].v[vertexIndex * 4];
    pos.y = bindlessStorageBuffers[nonuniformEXT(vertexBufferPos)].v[vertexIndex * 4 + 1];
    pos.z = bindlessStorageBuffers[nonuniformEXT(vertexBufferPos)].v[vertexIndex * 4 + 2];
    pos.w = bindlessStorageBuffers[nonuniformEXT(vertexBufferPos)].v[vertexIndex * 4 + 3];
    return pos;
}
