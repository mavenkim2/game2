#define FLT_EPSILON 1.192092896e-07F

struct ConvexShape
{
    // V2 *points;
    // TODO: actually support all polygons
    V3 *points;
    u32 numPoints;

    // TODO: trying something
    V3 GetSupport(V3 dir);
};

struct Sphere
{
    V3 center;
    f32 radius;

    Sphere(V3 c, f32 r) : center(c), radius(r) {}
    V3 GetSupport(V3 dir);
};

struct GJKState
{
    V3 supportA[4];
    V3 supportB[4];
    V3 simplex[4];
    u32 numPoints;
};
