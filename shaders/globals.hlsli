#include "ShaderInterop.h"
#include "ShaderInterop_Mesh.h"

#define INFINITE_FLOAT 1.#INF

static const uint BINDLESS_TEXTURE_SET = 1;
static const uint BINDLESS_UNIFORM_TEXEL_SET = 2;
static const uint BINDLESS_STORAGE_BUFFER_SET = 3;
static const uint BINDLESS_STORAGE_TEXEL_BUFFER_SET = 4;

#ifdef __spirv__
[[vk::binding(0, BINDLESS_TEXTURE_SET)]] Texture2D bindlessTextures[];

[[vk::binding(0, BINDLESS_UNIFORM_TEXEL_SET)]] Buffer<uint> bindlessBuffersUint[];
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
[[vk::binding(0, BINDLESS_STORAGE_BUFFER_SET)]] StructuredBuffer<MeshGeometry> bindlessMeshGeometry[];
[[vk::binding(0, BINDLESS_STORAGE_BUFFER_SET)]] StructuredBuffer<MeshCluster> bindlessMeshClusters[];
[[vk::binding(0, BINDLESS_STORAGE_BUFFER_SET)]] StructuredBuffer<ShaderMaterial> bindlessMaterials[];
#else
#error not supported
#endif

SamplerState samplerLinearWrap : register(s50);
SamplerState samplerNearestWrap : register(s51);
SamplerState samplerLinearClamp : register(s52);
SamplerState samplerNearestClamp : register(s53);
SamplerComparisonState samplerShadowMap : register(s54);

uint GetUint(int descriptor, int indexIndex)
{
    uint result = 0;
    if (descriptor >= 0)
    {
        result = bindlessBuffers[descriptor].Load<uint>(indexIndex * sizeof(uint));
    }
    return result;
}

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

float3 RotateQuat(float3 v, float4 q)
{
	return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

// 2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere. Michael Mara, Morgan McGuire. 2013
// the center needs to be in view space
bool ProjectSphere(float3 center, float radius, float nearZ, float p00, float p11, out float4 aabb)
{
    if (center.z - radius <= nearZ) // if the sphere partially intersects the near plane, disregard
        return false;

    if (center.z < radius + nearZ) return false;

    float3 centerTimesR = center * radius;
    float centerZRSquared = center.z * center.z - radius * radius;

    float vx = sqrt(center.x * center.x + centerZRSquared);
    float minx = (vx * center.x - centerTimesR.z) / (vx * center.z + centerTimesR.x); // perspective divide
    float maxx = (vx * center.x + centerTimesR.z) / (vx * center.z - centerTimesR.x);

    float vy = sqrt(center.y * center.y + centerZRSquared);
    float miny = (vy * center.y - centerTimesR.z) / (vy * center.z + centerTimesR.y);
    float maxy = (vy * center.y + centerTimesR.z) / (vy * center.z - centerTimesR.y);

    aabb = float4(minx * p00, miny * p11, maxx * p00, maxy * p11) * 0.5f + 0.5f;
    return true;
}

float min3(float a, float b, float c)
{
    return min(min(a, b), c);
}

float2 min3(float2 a, float2 b, float2 c)
{
    return min(min(a, b), c);
}

float max3(float a, float b, float c)
{
    return max(max(a, b), c);
}

float2 max3(float2 a, float2 b, float2 c)
{
    return max(max(a, b), c);
}

float4 min3(float4 a, float4 b, float4 c)
{
    return min(min(a, b), c);
}

////////////////////////////// 
// Wave Intrinsics
//

#define WAVE_INTRINSICS 1
#ifdef WAVE_INTRINSICS
uint WaveGetActiveLaneIndexLast()
{
    uint2 activeMask = WaveActiveBallot(true).xy;
    return firstbithigh(activeMask.y ? activeMask.y : activeMask.x) + (activeMask.y ? 32 : 0);
}
#define WaveReadLaneLast(x) WaveReadLaneAt(x, WaveGetActiveLaneIndexLast())
// Value is constant for each thread in a wave
#define WaveInterlockedAddScalar(dest, value, outputIndex) \
{ \
    uint __numToAdd__ = WaveActiveCountBits(true) * value; \
    outputIndex = 0; \
    if (WaveIsFirstLane()) \
    { \
        InterlockedAdd(dest, __numToAdd__, outputIndex); \
    } \
    outputIndex = WaveReadLaneFirst(outputIndex) + WavePrefixCountBits(true) * value; \
}

#define WaveInterlockedAddScalarTest(dest, test, value, outputIndex) \
{ \
    uint __numToAdd__ = WaveActiveCountBits(test) * value; \
    outputIndex = 0; \
    if (WaveIsFirstLane() && __numToAdd__ > 0) \
    { \
        InterlockedAdd(dest, __numToAdd__, outputIndex); \
    } \
    outputIndex = WaveReadLaneFirst(outputIndex) + WavePrefixCountBits(test) * value; \
}

#define WaveInterlockedAddScalarTestNoOutput(dest, test, value) \
{ \
    uint __numToAdd__ = WaveActiveCountBits(test) * value; \
    if (WaveIsFirstLane() && __numToAdd__ > 0) \
    { \
        InterlockedAdd(dest, __numToAdd__); \
    } \
}

#define WaveInterlockedAddScalarInGroupsTest(dest, destGroups, numPerGroup, test, value, outputIndex) \
{ \
    uint __numToAdd__ = WaveActiveCountBits(test) * value; \
    outputIndex = 0; \
    if (WaveIsFirstLane() && __numToAdd__ > 0) \
    { \
        InterlockedAdd(dest, __numToAdd__, outputIndex); \
        InterlockedMax(destGroups, (outputIndex + __numToAdd__ + numPerGroup - 1)/numPerGroup); \
    } \
    outputIndex = WaveReadLaneFirst(outputIndex) + WavePrefixCountBits(test) * value; \
}

#define WaveInterlockedAdd(dest, value, outputIndex) \
{ \
    uint __localIndex__ = WavePrefixSum(value); \
    uint __numToAdd__ = WaveReadLaneLast(__localIndex__ + value); \
    outputIndex = 0; \
    if (WaveIsFirstLane()) \
    { \
        InterlockedAdd(dest, __numToAdd__, outputIndex); \
    } \
    outputIndex = WaveReadLaneFirst(outputIndex) + __localIndex__; \
}

#else
#define WaveInterlockedAddScalar(dest, value, outputIndex) InterlockedAdd(dest, value, outputIndex)
#define WaveInterlockedAddScalarTest(dest, test, value, outputIndex) \
{ \
    if (test) \
    { \
        InterlockedAdd(dest, value, outputIndex); \
    } \
}
#define WaveInterlockedAdd(dest, value, outputIndex) InterlockedAdd(dest, value, outputIndex)
#endif
