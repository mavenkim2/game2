#include "crack.h"
#ifdef LSP_INCLUDE
#include "keepmovingforward_common.h"
#include "keepmovingforward_math.h"
#include "physics.h"
#endif

internal ConvexShape MakeSphere(V3 center, f32 radius)
{
    ConvexShape result;
    result.center = center;
    result.radius = radius;
    result.type = Shape_Sphere;
    result.numPoints = 0;
    return result;
}

V3 ConvexShape::GetSupport(V3 dir)
{
    switch (type)
    {
        case Shape_Sphere:
        {
            f32 length = Length(dir);
            return length > 0.0f ? center + radius / length * dir : center;
        }
        default:
        {
            Assert(numPoints > 0);
            V3 bestPoint  = points[0];
            f32 bestValue = Dot(points[0], dir);
            loopi(1, numPoints)
            {
                f32 tempValue = Dot(points[i], dir);
                if (tempValue > bestValue)
                {
                    bestPoint = points[i];
                    bestValue = tempValue;
                }
            }
            return bestPoint;
        }
    }
}

internal V3 GetClosestPointOnLine(GJKState *state)
{
    Assert(state->numPoints == 2);
    V3 a = state->simplex[1];
    V3 b = state->simplex[0];

    V3 ab = b - a;
    V3 ao = -a;

    V3 abperp = Cross(Cross(ab, ao), ab);
    return abperp;
}

internal V3 GetClosestPointOnTriangle(GJKState *state)
{
    Assert(state->numPoints == 3);
    V3 a = state->simplex[2];
    V3 b = state->simplex[1];
    V3 c = state->simplex[0];

    V3 ab  = b - a;
    V3 ac  = c - a;
    V3 ao  = -a;
    V3 abc = Cross(ab, ac);

    V3 dir = {};
    if (Dot(Cross(abc, ac), ao) > 0)
    {
        // Case 1: Direction perp to AC is direction to origin
        if (Dot(ac, ao) > 0) dir = Cross(Cross(ac, ao), ac);
        else
        {
            // Case 2: Direction perp to AB is direction to origin
            if (Dot(ab, ao) > 0) dir = Cross(Cross(ab, ao), ab);
            else
            {
                // Case 3: Point a is closest point to origin. I don't think this case is reachable, because
                // a should always be across the origin or else GJK stops.
                dir = ao;
                Assert(!"I don't think this case is reachable, but I guess I'm wrong.");
            }
        }
    }
    else
    {
        if (Dot(Cross(ab, abc), ao) > 0)
        {
            // Case 2
            if (Dot(ab, ao) > 0) dir = Cross(Cross(ab, ao), ab);
            else
            {
                // Case 3
                dir = ao;
                Assert(!"I don't think this case is reachable, but I guess I'm wrong.");
            }
        }
        else
        {
            if (Dot(abc, ao))
            {
                dir = abc;
            }
            else
            {
                dir = -abc;
            }
        }
    }
    return dir;
}

internal V3 TriangleCheck(V3 originDir, V3 ab, V3 ac, V3 abc, b32 normalsFacingOut)
{
    V3 dir;
    if (Dot(Cross(abc, ac), originDir) > 0)
    {
        dir = Cross(Cross(ac, originDir), ac);
    }
    else if (Dot(Cross(ab, abc), originDir) > 0)
    {
        dir = Cross(Cross(ab, originDir), ab);
    }
    else
    {
        dir = normalsFacingOut ? abc : -abc;
    }
    return dir;
}

internal V3 GetClosestPointOnTetrahedron(GJKState *state, u32 *set)
{
    // Point a is the point most recently added to the simplex
    V3 a = state->simplex[3];
    V3 b = state->simplex[2];
    V3 c = state->simplex[1];
    V3 d = state->simplex[0];

    // direction origin
    V3 originDir = -a;

    // sanity check
    // V3 bc = c - b;
    // V3 bd = d - b;

    V3 ab = b - a;
    V3 ac = c - a;
    V3 ad = d - a;

    // Basically, what the following does is see whether the origin is outside or inside with respect to one of the
    // 3 planes we're checking. The following 3 normals either ALL point inside or ALL point outside of the
    // tetrahedron. We use the dot product to find the direction of each normal compared to a side that isn't on
    // the plane (e.g ab for plane acd). If these are in the same direction, then the normals are pointing INWARDS.
    // Otherwise, they're pointing OUTWARDS. If the normals are pointing inwards, then if dotting with the origin
    // direction is positive, then the origin is inside. Otherwise, if the normals are pointing outwards, and
    // dotting with the origin direction is negative. then the origin is inside as wellis inside. Otherwise, if the
    // normals are pointing outwards, and dotting with the origin direction is negative. then the origin is inside
    // as well.
    V3 adb = Cross(ad, ab);
    V3 acd = Cross(ac, ad);
    V3 abc = Cross(ab, ac);

    f32 adbCompareO = Dot(originDir, adb);
    f32 acdCompareO = Dot(originDir, acd);
    f32 abcCompareO = Dot(originDir, abc);

    f32 normalDir1 = Dot(ac, adb);
    f32 normalDir2 = Dot(ab, acd);
    f32 normalDir3 = Dot(ad, abc);

    b32 normalsFacingOut;
    if (normalDir1 < 0 && normalDir2 < 0 && normalDir3 < 0)
    {
        normalsFacingOut = 1;
    }
    else if (normalDir1 > 0 && normalDir2 > 0 && normalDir3 > 0)
    {
        normalsFacingOut = 0;
    }
    else
    {
        Assert(!"Invalid?");
    }

    // TODO: compress
    // Check if origin is outside the face
    u32 mask = 0;
    if (normalsFacingOut)
    {
        mask = mask | (abcCompareO >= -FLT_EPSILON ? 0x001 : 0);
        mask = mask | (acdCompareO >= -FLT_EPSILON ? 0x010 : 0);
        mask = mask | (adbCompareO >= -FLT_EPSILON ? 0x100 : 0);
    }
    else
    {
        mask = mask | (abcCompareO <= FLT_EPSILON ? 0x001 : 0);
        mask = mask | (acdCompareO <= FLT_EPSILON ? 0x010 : 0);
        mask = mask | (adbCompareO <= FLT_EPSILON ? 0x100 : 0);
    }

    u32 closestSet = 0xf;
    V3 dir         = {};

    // Check face abc, since origin is outside of it
    if (mask & 0x001)
    {
        dir        = TriangleCheck(originDir, ab, ac, abc, normalsFacingOut);
        closestSet = 0b0111;
    }
    else if (mask & 0x010)
    {
        dir        = TriangleCheck(originDir, ac, ad, acd, normalsFacingOut);
        closestSet = 0b1101;
    }
    else if (mask & 0x100)
    {
        dir        = TriangleCheck(originDir, ad, ab, adb, normalsFacingOut);
        closestSet = 01011;
    }
    *set = closestSet;
    return dir;
}

internal b32 Intersects(ConvexShape *a, ConvexShape *b, V3 dir)
{
    GJKState state;
    state.numPoints = 0;
    for (;;)
    {
        V3 supportA = a->GetSupport(dir);
        V3 supportB = b->GetSupport(-dir);
        V3 p        = supportA - supportB;

        if (Dot(p, dir) < 0.f)
        {
            return false;
        }
        state.simplex[state.numPoints] = p;
        state.supportA[state.numPoints] = supportA;
        state.numPoints++;
        switch (state.numPoints)
        {
            case 1:
                dir = -state.simplex[0];
                break;
            case 2:
                dir = GetClosestPointOnLine(&state);
                break;
            case 3:
                dir = GetClosestPointOnTriangle(&state);
                break;
            case 4:
            {
                u32 set = 0;
                dir     = GetClosestPointOnTetrahedron(&state, &set);
                if (set == 0xf)
                {
                    return true;
                }
                else
                {
                    u32 numPoints   = state.numPoints;
                    state.numPoints = 0;
                    // Removes points no longer on simplex
                    loopi(0, numPoints)
                    {
                        if ((1 << i) & set)
                        {
                            state.simplex[state.numPoints++] = state.simplex[i];
                        }
                    }
                }
                break;
            }
            default:
                Assert(!"Invalid GJK state");
                return false;
        }
        // If direction vector is 0.
        if (dir.x == 0 && dir.y == 0 && dir.z == 0)
        {
            return true;
        }
    }
}
