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
