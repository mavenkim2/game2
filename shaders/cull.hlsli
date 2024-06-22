#ifndef PASS
#define PASS 0 
#endif

#define FIRST_PASS 1
#define SECOND_PASS 2

#if PASS == SECOND_PASS
static const bool isSecondPass = true;
#else
static const bool isSecondPass = false;
#endif

Texture2D depthPyramid : register(t0);

struct FrustumCullResults 
{
    float4 aabb;
    float minZ;
    bool isVisible;
};

FrustumCullResults ProjectBoxAndFrustumCull(float3 bMin, float3 bMax, float4x4 mvp,
                                            float p22, float p23, bool skipFrustumCull)
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
    float minW = INFINITE_FLOAT;
    float maxW = -INFINITE_FLOAT;
    float4 aabb;
    aabb.xy = float2(1.f, 1.f);
    aabb.zw = float2(-1.f, -1.f);

#define PLANEMIN(a, b) planesMin = min3(planesMin, float4(a.xy, -a.xy) - a.w, float4(b.xy, -b.xy) - b.w)
#define PROCESS(a, b) \
{ \
    float2 pa = a.xy/a.w; \
    float2 pb = b.xy/b.w; \
    minW = min3(minW, a.w, b.w); \
    maxW = max3(maxW, a.w, b.w); \
    aabb.xy = min3(aabb.xy, pa, pb); \
    aabb.zw = max3(aabb.zw, pa, pb); \
}
    
    float4 p0 = mul(mvp, float4(bMin.x, bMin.y, bMin.z, 1.0));
    float4 p1 = p0 + sZ;

    float4 p2 = p0 + sX;
    float4 p3 = p1 + sX;

    float4 p4 = p2 + sY;
    float4 p5 = p3 + sY;

    float4 p6 = p4 - sX;
    float4 p7 = p5 - sX;

    bool visible = true;

    //if (skipFrustumCull)
    //{
        PLANEMIN(p0, p1);
        PLANEMIN(p2, p3);
        PLANEMIN(p4, p5);
        PLANEMIN(p6, p7);
        visible = !(any(planesMin > 0.f));
    //}
    if (!skipFrustumCull)
    {
        PROCESS(p0, p1);
        PROCESS(p2, p3);
        PROCESS(p4, p5);
        PROCESS(p6, p7);
    }

    // NOTE: w = -z in view space (zv). Azv + B = z in clip space (zc)
    float maxZ = -maxW * p22 + p23;
    float minZ = -minW * p22 + p23;

// it's because maxW and minW are the negative of the viewspace positions. so the view space positions are negative, 
// but then -z makes them positive. and then they become negative again because p22 is negative. but

    // partially = at least partially
    bool test = maxZ > minZ;//minW > 0;//maxZ > minZ;
    bool isPartiallyInsideNearPlane = maxZ > 0;
    //bool isPartiallyOutsideNearPlane = minZ <= 0;
    bool isPartiallyInsideFarPlane = minZ < minW;
    //bool isPartiallyOutsideFarPlane = maxZ >= minW;

    visible = visible && isPartiallyInsideFarPlane && isPartiallyInsideNearPlane;
    
    FrustumCullResults results;
    results.aabb = aabb;
    results.minZ = saturate(minZ / minW);
    results.isVisible = visible;

    return results;
}

#if 0
bool IsVisible(float4 rect)
{
    rect = rect * 0.5f + 0.5f;
    int2 mipLevel = firstbithigh(rect.zw - rect.xy);

}
#endif

