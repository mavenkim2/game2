#include "ShaderInterop.h"
#include "ShaderInterop_Mesh.h"

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
SamplerComparisonState samplerShadowMap : register(s53);
SamplerState samplerNearestClamp : register(s54);

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

float4 min3(float4 a, float4 b, float4 c)
{
    return min(min(a, b), c);
}

bool ProjectBoxAndFrustumCull(float3 bMin, float3 bMax, float4x4 mvp, float nearZ, float farZ, 
                              uint isSecondPass, float p22, float p23, out float4 aabb, out float minZ)
{
    // precise box bounds
    // if the frustum check fails, then the screen space aabb isn't calculated
    // use the distributive property of matrices: if B = D + C, AB = AD + AC
    float4 sX = mul(mvp, float4(bMax.x - bMin.x, 0, 0, 0));
    float4 sY = mul(mvp, float4(0, bMax.y - bMin.y, 0, 0));
    float4 sZ = mul(mvp, float4(0, 0, bMax.z - bMin.z, 0));

    float4 planesMin = 1.f;
    // If the min of p.x - p.w > 0, then all points are past the right clip plane.
    // If the min of -p.x - p.w > 0, then -p.x > p.w -> p.x < -p.w for all points.
#define PLANEMIN(a, b) planesMin = min3(planesMin, float4(a.xy, -a.xy) - a.w, float4(b.xy, -b.xy) - b.w)
    
    float4 p0 = mul(mvp, float4(bMin.x, bMin.y, bMin.z, 1.0));
    float4 p1 = p0 + sZ;
    PLANEMIN(p0, p1);

    float4 p2 = p0 + sX;
    float4 p3 = p1 + sX;
    PLANEMIN(p2, p3);

    float4 p4 = p2 + sY;
    float4 p5 = p3 + sY;
    PLANEMIN(p4, p5);

    float4 p6 = p4 - sX;
    float4 p7 = p5 - sX;
    PLANEMIN(p6, p7);

    // frustum culling
    bool visible = !(any(planesMin > 0.f));

    minZ = 0;
    aabb = 0;
    return visible;
#if 0
    if (visible && isSecondPass)
    {
        // view space z
        float minW = min(min(min(p0.w, p1.w), min(p2.w, p3.w)), min(min(p4.w, p5.w), min(p6.w, p7.w)));
        if (minW < nearZ) 
            return false;
        aabb.xy = min(
                    min(min(p0.xy / p0.w, p1.xy / p1.w), min(p2.xy / p2.w, p3.xy / p3.w)),
                    min(min(p4.xy / p4.w, p5.xy / p5.w), min(p6.xy / p6.w, p7.xy / p7.w)));
        aabb.zw = max(
                    max(max(p0.xy / p0.w, p1.xy / p1.w), max(p2.xy / p2.w, p3.xy / p3.w)),
                    max(max(p4.xy / p4.w, p5.xy / p5.w), max(p6.xy / p6.w, p7.xy / p7.w)));

        minZ = minW * p22 + p23; // [2][2] and [2][3] in perspective projection matrix
        aabb = aabb * 0.5 + 0.5;
    }
    return visible;
#endif
}

