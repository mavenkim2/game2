#ifndef KEEPMOVINGFORWARD_MATH_H
#include "keepmovingforward_common.h"
#include <math.h>

inline f32 Max(f32 a, f32 b)
{
    f32 result = a > b ? a : b;
    return result;
}

inline f32 Min(f32 a, f32 b)
{
    f32 result = a < b ? a : b;
    return result;
}

inline f32 Abs(f32 a)
{
    f32 result = a > 0 ? a : -a;
    return result;
}

inline f32 Sign(f32 a)
{
    if (a < 0)
    {
        return -1;
    }
    else if (a > 0)
    {
        return 1;
    }
    return 0;
}

inline f32 Sin(f32 a)
{
    f32 result = sinf(a);
    return result;
}

inline f32 Cos(f32 a)
{
    f32 result = cosf(a);
    return result;
}

inline f32 Tan(f32 a)
{
    f32 result = tanf(a);
    return result;
}

inline u32 RoundF32ToU32(f32 value)
{
    u32 result = (u32)roundf(value);
    return result;
}
inline u32 RoundF32ToI32(f32 value)
{
    i32 result = (i32)roundf(value);
    return result;
}

inline u32 FloorF32ToU32(f32 value)
{
    u32 result = (u32)(floorf(value));
    return result;
}

#define Radians(angle) ((PI * angle) / (180))
#define Degrees(angle) ((180 * angle) / (PI))

union Vector2
{
    struct
    {
        f32 x, y;
    };
    f32 e[2];
};
typedef Vector2 V2;

union Vector3
{
    struct
    {
        f32 x, y, z;
    };
    struct
    {
        f32 r, g, b;
    };
    struct
    {
        V2 xy;
        f32 _z;
    };
    struct
    {
        f32 _x;
        V2 yz;
    };
    f32 e[3];
};
typedef Vector3 V3;

union Vector4
{
    struct
    {
        f32 x, y, z, w;
    };
    struct
    {
        f32 r, g, b, a;
    };
    struct
    {
        V2 xy;
        V2 zw;
    };
    struct
    {
        f32 _x;
        V2 yz;
        f32 _w;
    };
    f32 e[4];
};
typedef Vector4 V4;

union Mat3x3
{
    f32 elements[3][3];
    V3 columns[3];
};
typedef Mat3x3 Mat3;

// NOTE: Matrix[COLUMN][ROW]
union Mat4x4
{
    f32 elements[4][4];
    V4 columns[4];
};

typedef Mat4x4 Mat4;

union Rect2
{
    struct
    {
        V2 pos;
        V2 size;
    };
    struct
    {
        f32 x;
        f32 y;
        f32 width;
        f32 height;
    };
};

/*
 * VECTOR2
 */

inline Vector2 operator+(Vector2 a, Vector2 b)
{
    Vector2 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

inline Vector2 &operator+=(Vector2 &a, Vector2 b)
{
    a = a + b;
    return a;
}

inline Vector2 operator-(Vector2 a, Vector2 b)
{
    Vector2 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}

inline Vector2 operator-(Vector2 a)
{
    Vector2 result;
    result.x = -a.x;
    result.y = -a.y;
    return result;
}

inline Vector2 &operator-=(Vector2 &a, Vector2 b)
{
    a = a - b;
    return a;
}
inline Vector2 operator*(Vector2 a, f32 b)
{
    Vector2 result;
    result.x = a.x * b;
    result.y = a.y * b;
    return result;
}

inline Vector2 operator*(f32 b, Vector2 a)
{
    Vector2 result = a * b;
    return result;
}

inline Vector2 operator*=(Vector2 &a, f32 b)
{
    a = a * b;
    return a;
}

inline Vector2 operator/(Vector2 a, f32 b)
{
    Vector2 result = a * (1.f / b);
    return result;
}

inline Vector2 operator/=(Vector2 &a, f32 b)
{
    a = a * (1.f / b);
    return a;
}

inline bool operator==(Vector2 &a, Vector2 b)
{
    bool result = a.x == b.x && a.y == b.y ? true : false;
    return result;
}

inline f32 Dot(Vector2 a, Vector2 b)
{
    f32 result = a.x * b.x + a.y * b.y;
    return result;
}
inline V2 Cross(f32 a, Vector2 b)
{
    V2 result = V2{-a * b.y + a * b.x};
    return result;
}

inline f32 Cross(Vector2 a, Vector2 b)
{
    f32 result = a.x * b.y - a.y * b.x;
    return result;
}

inline f32 SquareRoot(f32 a)
{
    f32 result = sqrtf(a);
    return result;
}

inline f32 Length(Vector2 a)
{
    f32 result = SquareRoot(Dot(a, a));
    return result;
}

inline V2 Normalize(Vector2 a)
{
    f32 length = Length(a);
    V2 result = a * (1.f / length);
    return result;
}

/*
 * VECTOR3
 */

inline Vector3 operator+(Vector3 a, Vector3 b)
{
    Vector3 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    return result;
}

inline Vector3 &operator+=(Vector3 &a, Vector3 b)
{
    a = a + b;
    return a;
}

inline Vector3 operator-(Vector3 a, Vector3 b)
{
    Vector3 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    return result;
}

inline Vector3 operator-(Vector3 a)
{
    Vector3 result;
    result.x = -a.x;
    result.y = -a.y;
    result.z = -a.z;
    return result;
}

inline Vector3 &operator-=(Vector3 &a, Vector3 b)
{
    a = a - b;
    return a;
}
inline Vector3 operator*(Vector3 a, f32 b)
{
    Vector3 result;
    result.x = a.x * b;
    result.y = a.y * b;
    result.z = a.z * b;
    return result;
}

inline Vector3 operator*(f32 b, Vector3 a)
{
    Vector3 result = a * b;
    return result;
}

inline Vector3 operator*=(Vector3 &a, f32 b)
{
    a = a * b;
    return a;
}

inline Vector3 operator/(Vector3 a, f32 b)
{
    Vector3 result = a * (1.f / b);
    return result;
}

inline Vector3 operator/=(Vector3 &a, f32 b)
{
    a = a * (1.f / b);
    return a;
}

inline bool operator==(Vector3 &a, Vector3 b)
{
    bool result = a.x == b.x && a.y == b.y && a.z == b.z ? true : false;
    return result;
}

inline f32 Dot(Vector3 a, Vector3 b)
{
    f32 result = a.x * b.x + a.y * b.y + a.z * b.z;
    return result;
}

inline f32 Length(Vector3 a)
{
    f32 result = SquareRoot(Dot(a, a));
    return result;
}

inline V3 Normalize(Vector3 a)
{
    f32 length = Length(a);
    V3 result = a * (1.f / length);
    return result;
}

/*
 * MATRIX 3X3
 */

inline Mat3 operator*(Mat3x3 a, Mat3x3 b)
{
    Mat3 result;
    for (int j = 0; j < 3; j += 1)
    {
        for (int i = 0; i < 3; i += 1)
        {
            result.elements[i][j] = (a.elements[0][j] * b.elements[i][0] + a.elements[1][j] * b.elements[i][1] +
                                     a.elements[2][j] * b.elements[i][2]);
        }
    }

    return result;
}

/*
 * MATRIX 4X4
 */

inline V4 operator*(Mat4x4 a, V4 b)
{
    V4 result;
    result.x = a.columns[0].x * b.x;
    result.y = a.columns[0].y * b.x;
    result.z = a.columns[0].z * b.x;
    result.w = a.columns[0].w * b.x;

    result.x += a.columns[1].x * b.y;
    result.y += a.columns[1].y * b.y;
    result.z += a.columns[1].z * b.y;
    result.w += a.columns[1].w * b.y;

    result.x += a.columns[2].x * b.z;
    result.y += a.columns[2].y * b.z;
    result.z += a.columns[2].z * b.z;
    result.w += a.columns[2].w * b.z;

    result.x += a.columns[3].x * b.w;
    result.y += a.columns[3].y * b.w;
    result.z += a.columns[3].z * b.w;
    result.w += a.columns[3].w * b.w;
    return result;
}

inline Mat4 operator*(Mat4x4 a, Mat4x4 b)
{
    Mat4 result;
    for (int j = 0; j < 4; j += 1)
    {
        for (int i = 0; i < 4; i += 1)
        {
            result.elements[i][j] = (a.elements[0][j] * b.elements[i][0] + a.elements[1][j] * b.elements[i][1] +
                                     a.elements[2][j] * b.elements[i][2] + a.elements[3][j] * b.elements[i][3]);
        }
    }

    return result;
}

inline Mat4 MakeMat4(f32 a)
{
    Mat4 result = {{
        {a, 0, 0, 0},
        {0, a, 0, 0},
        {0, 0, a, 0},
        {0, 0, 0, a},
    }};
    return result;
}

inline Mat4 Scale4(V3 value)
{
    Mat4 result = MakeMat4(1.f);
    result.elements[0][0] = value.x;
    result.elements[1][1] = value.y;
    result.elements[2][2] = value.z;
    return result;
}

inline Mat4 Rotate4(V3 axis, f32 theta)
{
    Mat4 result = MakeMat4(1.f);
    axis = Normalize(axis);
    f32 sinTheta = Sin(theta);
    f32 cosTheta = Cos(theta);
    f32 cosValue = 1.f - cosTheta;
    result.elements[0][0] = (axis.x * axis.x * cosValue) + cosTheta;
    result.elements[0][1] = (axis.x * axis.y * cosValue) + (axis.z * sinTheta);
    result.elements[0][2] = (axis.x * axis.z * cosValue) - (axis.y * sinTheta);
    result.elements[1][0] = (axis.y * axis.x * cosValue) - (axis.z * sinTheta);
    result.elements[1][1] = (axis.y * axis.y * cosValue) + cosTheta;
    result.elements[1][2] = (axis.y * axis.z * cosValue) + (axis.x * sinTheta);
    result.elements[2][0] = (axis.z * axis.x * cosValue) + (axis.y * sinTheta);
    result.elements[2][1] = (axis.z * axis.y * cosValue) - (axis.x * sinTheta);
    result.elements[2][2] = (axis.z * axis.z * cosValue) + cosTheta;
    return result;
}

inline Mat4 Translate4(V3 value)
{
    Mat4 result = MakeMat4(1.f);

    result.columns[3].x = value.x;
    result.columns[3].y = value.y;
    result.columns[3].z = value.z;
    return result;
}

inline Mat4 Perspective4(f32 fov, f32 aspectRatio, f32 nearZ, f32 farZ)
{
    Mat4 result = {};
    f32 cotangent = 1.f / Tan(fov / 2);
    result.elements[0][0] = cotangent / aspectRatio;
    result.elements[1][1] = cotangent;
    result.elements[2][3] = -1.f;

    result.elements[2][2] = (nearZ + farZ) / (nearZ - farZ);
    result.elements[3][2] = (2.f * nearZ * farZ) / (nearZ - farZ);
    return result;
}

inline Mat4 Orthographic4(f32 left, f32 right, f32 bottom, f32 top, f32 nearZ, f32 farZ)
{
    Mat4 result = {};

    result.elements[0][0] = 2.0f / (right - left);
    result.elements[1][1] = 2.0f / (top - bottom);
    result.elements[2][2] = 2.0f / (nearZ - farZ);
    result.elements[3][3] = 1.0f;

    result.elements[3][0] = (left + right) / (left - right);
    result.elements[3][1] = (bottom + top) / (bottom - top);
    result.elements[3][2] = (nearZ + farZ) / (nearZ - farZ);

    return result;
}

/*
 * RECTANGLE 2
 */

inline Rect2 CreateRectFromCenter(V2 pos, V2 dim)
{
    Rect2 result;
    result.pos = pos - (dim / 2.f);
    result.size = dim;
    return result;
}

inline Rect2 CreateRectFromBottomLeft(V2 pos, V2 dim)
{
    Rect2 result;
    result.pos = pos;
    result.size = dim;
    return result;
}

inline bool IsInRectangle(Rect2 a, V2 pos)
{
    bool result = pos.x >= a.pos.x && pos.x <= a.pos.x + a.size.x && pos.y >= a.pos.y && pos.y <= a.pos.y + a.size.y;
    return result;
}

inline bool Rect2Overlap(const Rect2 rect1, const Rect2 rect2)
{
    bool result = (rect1.x <= rect2.x + rect2.width && rect2.x <= rect1.x + rect1.width &&
                   rect1.y <= rect2.y + rect2.height && rect2.y <= rect1.y + rect1.height);
    return result;
}

inline V2 GetRectCenter(Rect2 a)
{
    V2 result = a.pos + 0.5 * a.size;
    return result;
}

#define KEEPMOVINGFORWARD_MATH_H
#endif
