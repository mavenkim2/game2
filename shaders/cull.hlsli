Texture2D depthPyramid : register(t0);

struct FrustumCullResults 
{
    float4 aabb;
    float minZ;
    bool isVisible;
    bool crossesNearPlane;
};

FrustumCullResults ProjectBoxAndFrustumCull(float3 bMin, float3 bMax, float4x4 mvp,
                                            float p22, float p23, bool skipFrustumCull)
{
    FrustumCullResults results;
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

    if (skipFrustumCull)
    {
        PLANEMIN(p0, p1);
        PLANEMIN(p2, p3);
        PLANEMIN(p4, p5);
        PLANEMIN(p6, p7);
        visible = !(any(planesMin > 0.f));
    }
    // TODO: don't calculate the aabb on the first pass frustum cull with this frame's transforms
    PROCESS(p0, p1);
    PROCESS(p2, p3);
    PROCESS(p4, p5);
    PROCESS(p6, p7);

    // NOTE: w = -z in view space (zv). Azv + B = z in clip space (zc)
    float maxZ = -maxW * p22 + p23;
    float minZ = -minW * p22 + p23;

// it's because maxW and minW are the negative of the viewspace positions. so the view space positions are negative, 
// but then -z makes them positive. and then they become negative again because p22 is negative. but

    // partially = at least partially
    bool test = maxZ > minZ;//minW > 0;//maxZ > minZ;
    bool isPartiallyInsideNearPlane = maxZ > 0;
    bool isPartiallyOutsideNearPlane = minZ <= 0;
    bool isPartiallyInsideFarPlane = minZ < minW;
    //bool isPartiallyOutsideFarPlane = maxZ >= minW;

    visible = visible && isPartiallyInsideFarPlane && isPartiallyInsideNearPlane;
    
    results.aabb = aabb;
    results.minZ = saturate(minZ / minW);
    results.isVisible = visible;
    results.crossesNearPlane = isPartiallyOutsideNearPlane;

    return results;
}

struct OcclusionResults
{
    bool wasOccluded;
    bool isVisible;
};

OcclusionResults HZBOcclusionTest(FrustumCullResults cullResults, int2 screenSize)
{
    OcclusionResults results;
    results.wasOccluded = false;
    results.isVisible = false;
    if (cullResults.isVisible && !cullResults.crossesNearPlane)
    {
        float4 rect = saturate(cullResults.aabb * 0.5f + 0.5f);
        int4 pixels = (screenSize.xyxy * rect + 0.5f);
        pixels.xy = max(pixels.xy, 0);
        pixels.zw = min(pixels.zw, screenSize.xy - 1);
        bool overlapsPixelCenter = any(pixels.zw >= pixels.xy);
        pixels >>= 1;

        // for n > 1, ceil(log2(n)) = floor(log2(n-1)) + 1
        // floor(log2(n-1)) = firstbithigh(n-1)
        // [pixels.xy, pixels.zw] is inclusive, so difference is n-1
        int2 mipLevel = int2(firstbithigh(pixels.z - pixels.x), firstbithigh(pixels.w - pixels.y));
        int lod = max(max(mipLevel.x, mipLevel.y) - 1, 0);

        lod += any((pixels.zw >> lod) - (pixels.xy >> lod) > 1) ? 1 : 0;
        pixels >>= lod;

        int width, height, levels;
        depthPyramid.GetDimensions(lod, width, height, levels);
        int2 dim = int2(width, height);
        int4 uv = (pixels + 0.5f) / dim.xyxy;

        float depth00 = depthPyramid.SampleLevel(samplerNearestClamp, uv.xy, lod).r;
        float depth01 = depthPyramid.SampleLevel(samplerNearestClamp, uv.zy, lod).r;
        float depth10 = depthPyramid.SampleLevel(samplerNearestClamp, uv.zw, lod).r;
        float depth11 = depthPyramid.SampleLevel(samplerNearestClamp, uv.xw, lod).r;
        float maxDepth = max(max3(depth00, depth01, depth10), depth11);
        results.isVisible = cullResults.minZ < maxDepth;
        results.wasOccluded = !results.isVisible;
    }
    return results;
}
