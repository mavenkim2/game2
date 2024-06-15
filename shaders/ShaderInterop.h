#ifndef SHADERINTEROP_H
#define SHADERINTEROP_H

#define GLUE(a, b) a##b

#ifdef __cplusplus

using mat4 = Mat4;
using vec4 = V4;
using vec3 = V3;
using vec2 = V2;

using float4x4 = Mat4;
using float4   = V4;
using float3   = V3;
using float2   = V2;
using uint4    = UV4;
using uint     = u32;

// #define GET_UNIFORM_BIND_SLOT(name) __UNIFORM_BIND_SLOT__##name##__
#define UNIFORM(name, slot) \
    struct alignas(16) name

// static const int GET_UNIFORM_BIND_SLOT(name) = slot; \

#else
#define GET_UNIFORM_BIND_SLOT(name)
#define UNIFORM(name, slot) cbuffer name : register(GLUE(b, slot))
#define SLOT(type, slot)    GLUE(type, slot)
#endif

#endif
