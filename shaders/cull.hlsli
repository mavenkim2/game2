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

    if (!skipFrustumCull)
    {
        PLANEMIN(p0, p1);
        PLANEMIN(p2, p3);
        PLANEMIN(p4, p5);
        PLANEMIN(p6, p7);
        visible = !(any(planesMin > 0.f));
    }
    // TODO: don't calculate the aabb on the first pass frustum cull with this frame's transforms
    // e.g. if (skipOcclusion)
    PROCESS(p0, p1);
    PROCESS(p2, p3);
    PROCESS(p4, p5);
    PROCESS(p6, p7);

    // NOTE: w = -z in view space. Min(z) = -(Max(-z)) and Max(z) = -(Min(-z))
    float maxZ = -maxW * p22 + p23;
    float minZ = -minW * p22 + p23;

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

bool HZBOcclusionTest(FrustumCullResults cullResults, int2 screenSize, out float4 outUv)
{
    bool occluded = false;
    if (cullResults.isVisible && !cullResults.crossesNearPlane)
    {
        float4 rect = saturate(cullResults.aabb * 0.5f + 0.5f);
        int4 pixels = int4(screenSize.xyxy * rect + 0.5f);
        pixels.xy = max(pixels.xy, 0);
        pixels.zw = min(pixels.zw, screenSize.xy);

        // TODO: deal with case where rectangles don't overlap any pixel centers (and thus aren't rasterized)
        //bool overlapsPixelCenter = any(pixels.zw >= pixels.xy);

        //if (overlapsPixelCenter)
        //{
            // 2 pixels in mip 0 = (2^(k+1)) pixels in mip k
            // in order for n pixels in mip k to be covered by 2 pixels in mip 0: 
            // k = ceil(log2(n)) - 1
            // for n > 1, ceil(log2(n)) - 1 = (floor(log2(n-1)) + 1) - 1 = floor(log2(n-1))
            // floor(log2(n-1)) = firstbithigh(n-1)

            pixels >>= 1;
            int2 mipLevel = int2(firstbithigh(pixels.z - pixels.x - 1), firstbithigh(pixels.w - pixels.y - 1));
            int lod = max(max(mipLevel.x, mipLevel.y), 0);

            lod += any((pixels.zw >> lod) - (pixels.xy >> lod) > 1) ? 1 : 0; // z-x and w-y shouldn't be > 1
            pixels >>= lod;

            float width, height;
            depthPyramid.GetDimensions(width, height);
            float2 texelSize = pow(2, lod) / float2(width, height);
            float4 uv = (pixels + 0.5f) * texelSize.xyxy; // why + .5f?

            outUv = uv;

            float4 depth;
            depth.x = depthPyramid.SampleLevel(samplerNearestClamp, uv.xy, lod).r; // (-, -)
            depth.y = depthPyramid.SampleLevel(samplerNearestClamp, uv.zy, lod).r; // (+, -)
            depth.z = depthPyramid.SampleLevel(samplerNearestClamp, uv.zw, lod).r; // (+, +)
            depth.w = depthPyramid.SampleLevel(samplerNearestClamp, uv.xw, lod).r; // (-, +)

            depth.yz = (pixels.x == pixels.z) ? 1.f : depth.yz;
            depth.zw = (pixels.y == pixels.w) ? 1.f : depth.zw;

            float maxDepth = max(max3(depth.x, depth.y, depth.z), depth.w);
            occluded = cullResults.minZ > maxDepth;
        //}
    }
    return occluded;
}
