#extension GL_EXT_nonuniform_qualifier : enable

#include "ShaderInterop.h"

layout(set = 1, binding = 0) uniform sampler2D bindlessTextures[];
layout(set = 2, binding = 0) buffer bindlessStorageBuffers_
{
    float v[];
} bindlessStorageBuffers[];

layout(set = 2, binding = 0) buffer bindlessStorageBuffersVec2_
{
    vec2 v[];
} bindlessStorageBuffersVec2[];

layout(set = 2, binding = 0) buffer bindlessStorageBuffersVec3_
{
    vec3 v[];
} bindlessStorageBuffersVec3[];

layout(set = 2, binding = 0) buffer bindlessStorageBuffersVec4_
{
    vec4 v[];
} bindlessStorageBuffersVec4[];

layout(set = 2, binding = 0) buffer bindlessStorageBuffersUVec4_
{
    uvec4 v[];
} bindlessStorageBuffersUVec4[];
