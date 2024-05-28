#include "ShaderInterop.h"
#include "ShaderInterop_Mesh.h"

static const uint BINDLESS_TEXTURE_SET = 1;
static const uint BINDLESS_UNIFORM_TEXEL_SET = 2;
static const uint BINDLESS_STORAGE_BUFFER_SET = 3;
static const uint BINDLESS_STORAGE_TEXEL_BUFFER_SET = 4;

#ifdef __spirv__
[[vk::binding(0, BINDLESS_TEXTURE_SET)]] Texture2D bindlessTextures[];

[[vk::binding(0, BINDLESS_UNIFORM_TEXEL_SET)]] Buffer<float> bindlessBuffersFloat[];
[[vk::binding(0, BINDLESS_UNIFORM_TEXEL_SET)]] Buffer<float2> bindlessBuffersFloat2[];
[[vk::binding(0, BINDLESS_UNIFORM_TEXEL_SET)]] Buffer<float4> bindlessBuffersFloat4[];
[[vk::binding(0, BINDLESS_UNIFORM_TEXEL_SET)]] Buffer<uint4> bindlessBuffersUint4[];

[[vk::binding(0, BINDLESS_STORAGE_TEXEL_BUFFER_SET)]] RWBuffer<float> bindlessStorageBuffersFloat[];
[[vk::binding(0, BINDLESS_STORAGE_TEXEL_BUFFER_SET)]] RWBuffer<float2> bindlessStorageBuffersFloat2[];
[[vk::binding(0, BINDLESS_STORAGE_TEXEL_BUFFER_SET)]] RWBuffer<float4> bindlessStorageBuffersFloat4[];

[[vk::binding(0, BINDLESS_STORAGE_BUFFER_SET)]] ByteAddressBuffer bindlessBuffers[];
[[vk::binding(0, BINDLESS_STORAGE_BUFFER_SET)]] RWByteAddressBuffer bindlessStorageBuffers[];
[[vk::binding(0, BINDLESS_STORAGE_BUFFER_SET)]] StructuredBuffer<MeshParams> bindlessMeshParams[];
#else
#error not supported
#endif

SamplerState samplerLinearWrap : register(s50);
SamplerState samplerNearestWrap : register(s51);
SamplerState samplerLinearClamp : register(s52);
SamplerComparisonState samplerShadowMap : register(s53);

float2 GetFloat2(int descriptor, int vertexId)
{
    float2 result = 0;
    if (descriptor >= 0)
    {
        result = bindlessBuffersFloat2[descriptor][vertexId];
    }
    return result;
}

float3 GetFloat3(int descriptor, int vertexId)
{
    float3 result = 0;
    if (descriptor >= 0)
    {
        result = bindlessBuffersFloat4[descriptor][vertexId].xyz;
    }
    return result;
}

float4 GetFloat4(int descriptor, int vertexId)
{
    float4 result = 0;
    if (descriptor >= 0)
    {
        result = bindlessBuffersFloat4[descriptor][vertexId];
    }
    return result;
}

uint4 GetUint4(int descriptor, int vertexId)
{
    uint4 result = 0;
    if (descriptor >= 0)
    {
        result = bindlessBuffersUint4[descriptor][vertexId];
    }
    return result;
}

float3 ApplySRGBCurve(float3 x)
{
    return select(x < 0.0031308, 12.92 * x, 1.055 * pow(x, 1.0 / 2.4) - 0.055);
}
