#define FLT_EPSILON 1.192092896e-07F

enum ShapeType
{
    Shape_Sphere,
    Shape_Box,
};
// NOTE: trying out a mega struct of shapes
struct ConvexShape
{
    ShapeType type;

    V3 *points;
    u32 numPoints;

    V3 center;
    f32 radius;

    // AABB
    V3 min;
    V3 max;
    V3 GetSupport(V3 dir);
};

struct GJKState
{
    V3 supportA[4];
    V3 supportB[4];
    V3 simplex[4];
    u32 numPoints;
};
