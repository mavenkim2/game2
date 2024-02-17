#ifndef KEEPMOVINGFORWARD_MATH_H
#include <math.h>

inline u32 Max(u32 a, u32 b)
{
    u32 result = a > b ? a : b;
    return result;
}

inline u32 Min(u32 a, u32 b)
{
    u32 result = a < b ? a : b;
    return result;
}

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

inline f64 Max(f64 a, f64 b)
{
    f64 result = a > b ? a : b;
    return result;
}

inline f64 Min(f64 a, f64 b)
{
    f64 result = a < b ? a : b;
    return result;
}

inline u32 Clamp(u32 a, u32 low, u32 high)
{
    u32 result = Max(low, Min(a, high));
    return result;
}
inline f64 Clamp(f64 a, f64 low, f64 high)
{
    f64 result = Max(low, Min(a, high));
    return result;
}

inline f32 Clamp(f32 a, f32 low, f32 high)
{
    f32 result = Max(low, Min(a, high));
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

inline i32 Pow(i32 a, i32 b)
{
    i32 result = 1;
    for (int i = 0; i < b; i++)
    {
        result *= a;
    }
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

union V2
{
    struct
    {
        f32 x, y;
    };
    struct
    {
        f32 u, v;
    };
    f32 e[2];
};

union V2I32
{
    struct
    {
        i32 x, y;
    };
    struct
    {
        i32 u, v;
    };
    i32 elements[2];
};

union V3
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
    f32 elements[3];
};

union V3I32
{
    struct
    {
        i32 x, y, z;
    };
    struct
    {
        i32 r, g, b;
    };
    struct
    {
        V2I32 xy;
        i32 _z;
    };
    struct
    {
        i32 _x;
        V2I32 yz;
    };
    i32 e[3];
};

union V4
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
    struct
    {
        V3 xyz;
        f32 __w;
    };
    struct
    {
        V3 rgb;
        f32 __a;
    };
    f32 elements[4];
};

union V4I32
{
    struct
    {
        i32 x, y, z, w;
    };
    struct
    {
        i32 r, g, b, a;
    };
    struct
    {
        V2I32 xy;
        V2I32 zw;
    };
    struct
    {
        i32 _x;
        V2I32 yz;
        i32 _w;
    };
    struct
    {
        V3I32 xyz;
        i32 __w;
    };
    struct
    {
        V3I32 rgb;
        i32 __a;
    };
    i32 elements[4];
};

union Mat3
{
    f32 elements[3][3];
    V3 columns[3];
};

// NOTE: Matrix[COLUMN][ROW]
union Mat4
{
    f32 elements[4][4];
    V4 columns[4];
    struct
    {
        f32 a1, a2, a3, a4;
        f32 b1, b2, b3, b4;
        f32 c1, c2, c3, c4;
        f32 d1, d2, d3, d4;
    };
};

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

union Rect3
{
    struct
    {
        V3 pos;
        V3 size;
    };
    struct
    {
        f32 x;
        f32 y;
        f32 z;
        f32 xSize;
        f32 ySize;
        f32 zSize;
    };
};

union Quat
{
    V4 xyzw;
    struct
    {
        V3 xyz;
        f32 w;
    };
    struct
    {
        f32 x;
        V3 yzw;
    };
    struct
    {
        f32 __x;
        f32 y;
        f32 z;
        f32 __w;
    };
    f32 elements[4];
};
/*
 * VECTOR 2
 */

inline V2 operator+(V2 a, V2 b)
{
    V2 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

inline V2 &operator+=(V2 &a, V2 b)
{
    a = a + b;
    return a;
}

inline V2 operator-(V2 a, V2 b)
{
    V2 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}

inline V2 operator-(V2 a)
{
    V2 result;
    result.x = -a.x;
    result.y = -a.y;
    return result;
}

inline V2 &operator-=(V2 &a, V2 b)
{
    a = a - b;
    return a;
}
inline V2 operator*(V2 a, f32 b)
{
    V2 result;
    result.x = a.x * b;
    result.y = a.y * b;
    return result;
}

inline V2 operator*(f32 b, V2 a)
{
    V2 result = a * b;
    return result;
}

inline V2 operator*=(V2 &a, f32 b)
{
    a = a * b;
    return a;
}

inline V2 operator/(V2 a, f32 b)
{
    V2 result = a * (1.f / b);
    return result;
}

inline V2 operator/=(V2 &a, f32 b)
{
    a = a * (1.f / b);
    return a;
}

inline b32 operator==(V2 &a, V2 b)
{
    b32 result = a.x == b.x && a.y == b.y ? true : false;
    return result;
}

inline f32 Dot(V2 a, V2 b)
{
    f32 result = a.x * b.x + a.y * b.y;
    return result;
}
inline V2 Cross(f32 a, V2 b)
{
    V2 result = V2{-a * b.y + a * b.x};
    return result;
}

inline f32 Cross(V2 a, V2 b)
{
    f32 result = a.x * b.y - a.y * b.x;
    return result;
}

inline f32 SquareRoot(f32 a)
{
    f32 result = sqrtf(a);
    return result;
}

inline f64 SquareRoot(f64 a)
{
    f64 result = sqrt(a);
    return result;
}

inline f32 Length(V2 a)
{
    f32 result = SquareRoot(Dot(a, a));
    return result;
}

inline V2 Normalize(V2 a)
{
    f32 length = Length(a);
    V2 result  = a * (1.f / length);
    return result;
}

/*
 * VECTOR 3
 */

inline V3 MakeV3(f32 x, f32 y, f32 z)
{
    V3 result;
    result.x = x;
    result.y = y;
    result.z = z;
    return result;
}

inline V3 MakeV3(V2 xy, float z)
{
    V3 result;
    result.xy = xy;
    result.z  = z;
    return result;
}
inline V3 operator+(V3 a, V3 b)
{
    V3 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    return result;
}

inline V3 &operator+=(V3 &a, V3 b)
{
    a = a + b;
    return a;
}

inline V3 operator-(V3 a, V3 b)
{
    V3 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    return result;
}

inline V3 operator-(V3 a)
{
    V3 result;
    result.x = -a.x;
    result.y = -a.y;
    result.z = -a.z;
    return result;
}

inline V3 &operator-=(V3 &a, V3 b)
{
    a = a - b;
    return a;
}
inline V3 operator*(V3 a, f32 b)
{
    V3 result;
    result.x = a.x * b;
    result.y = a.y * b;
    result.z = a.z * b;
    return result;
}

inline V3 operator*(f32 b, V3 a)
{
    V3 result = a * b;
    return result;
}

inline V3 operator*=(V3 &a, f32 b)
{
    a = a * b;
    return a;
}

inline V3 operator/(V3 a, f32 b)
{
    V3 result = a * (1.f / b);
    return result;
}

inline V3 operator/=(V3 &a, f32 b)
{
    a = a * (1.f / b);
    return a;
}

inline b32 operator==(V3 &a, V3 b)
{
    b32 result = a.x == b.x && a.y == b.y && a.z == b.z ? true : false;
    return result;
}

inline f32 Dot(V3 a, V3 b)
{
    f32 result = a.x * b.x + a.y * b.y + a.z * b.z;
    return result;
}

inline V3 Cross(V3 a, V3 b)
{
    V3 result;
    result.x = a.y * b.z - a.z * b.y;
    result.y = a.z * b.x - a.x * b.z;
    result.z = a.x * b.y - a.y * b.x;
    return result;
}

inline f32 Length(V3 a)
{
    f32 result = SquareRoot(Dot(a, a));
    return result;
}

inline V3 Normalize(V3 a)
{
    f32 length = Length(a);
    V3 result  = a * (1.f / length);
    return result;
}

inline V3 NormalizeOrZero(V3 a)
{
    V3 result  = {};
    f32 length = Length(a);
    if (length == 0)
    {
        return result;
    }
    result = a * (1.f / length);
    return result;
}

inline V3 Hadamard(V3 a, V3 b)
{
    V3 result;
    result.x = a.x * b.x;
    result.y = a.y * b.y;
    result.z = a.z * b.z;
    return result;
}

inline b32 operator==(V3I32 &a, V3I32 b)
{
    b32 result = a.x == b.x && a.y == b.y && a.z == b.z ? true : false;
    return result;
}

inline b32 operator!=(V3I32 &a, V3I32 b)
{
    b32 result = a.x != b.x || a.y != b.y || a.z != b.z ? true : false;
    return result;
}

inline V3 Lerp(V3 a, V3 b, f32 t)
{
    V3 result = (1 - t) * a + t * b;
    return result;
}

/*
 * VECTOR 4
 */
inline V4 MakeV4(V3 xyz, float w)
{
    V4 result;
    result.xyz = xyz;
    result.w   = w;
    return result;
}

inline V4 Hadamard(V4 a, V4 b)
{
    V4 result;
    result.x = a.x * b.x;
    result.y = a.y * b.y;
    result.z = a.z * b.z;
    result.w = a.w * b.w;
    return result;
}

inline V4 operator+(V4 a, V4 b)
{
    V4 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    result.w = a.w + b.w;
    return result;
}

inline V4 &operator+=(V4 &a, V4 b)
{
    a = a + b;
    return a;
}

inline V4 operator-(V4 a, V4 b)
{
    V4 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    result.w = a.w - b.w;
    return result;
}

inline V4 operator-(V4 a)
{
    V4 result;
    result.x = -a.x;
    result.y = -a.y;
    result.z = -a.z;
    result.w = -a.w;
    return result;
}

inline V4 &operator-=(V4 &a, V4 b)
{
    a = a - b;
    return a;
}
inline V4 operator*(V4 a, f32 b)
{
    V4 result;
    result.x = a.x * b;
    result.y = a.y * b;
    result.z = a.z * b;
    result.w = a.w * b;
    return result;
}

inline V4 operator*(f32 b, V4 a)
{
    V4 result = a * b;
    return result;
}

inline V4 operator*=(V4 &a, f32 b)
{
    a = a * b;
    return a;
}

inline V4 operator/(V4 a, f32 b)
{
    V4 result = a * (1.f / b);
    return result;
}

inline V4 operator/=(V4 &a, f32 b)
{
    a = a * (1.f / b);
    return a;
}

inline b32 operator==(V4 &a, V4 b)
{
    b32 result = a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w ? true : false;
    return result;
}

inline f32 Dot(V4 a, V4 b)
{
    f32 result = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    return result;
}

inline f32 Length(V4 a)
{
    f32 result = SquareRoot(Dot(a, a));
    return result;
}

/*
 * QUATERNIONS!!!
 */

inline Quat QuatFromAxisAngle(V3 axis, f32 theta)
{
    Quat result;
    V3 axisN   = Normalize(axis);
    result.xyz = axisN / Sin(theta / 2.f);
    result.w   = Cos(theta / 2.f);
    return result;
}

inline Quat operator+(Quat a, Quat b)
{
    Quat result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    result.w = a.w + b.w;
    return result;
}

inline Quat operator-(Quat q)
{
    Quat result;
    result.x = -q.x;
    result.y = -q.y;
    result.z = -q.z;
    result.w = -q.w;

    return result;
}

inline Quat operator-(Quat a, Quat b)
{
    Quat result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    result.w = a.w - b.w;
    return result;
}

inline Quat operator*(Quat a, Quat b)
{
    Quat result;
    result.x = a.w * b.x + a.x * b.w + a.y * b.z + a.z * -b.y;
    result.y = a.w * b.y + a.x * -b.z + a.y * b.w + a.z * b.x;
    result.z = a.w * b.z + a.x * b.y + a.y * -b.x + a.z * b.w;
    result.w = a.w * b.w + a.x * -b.x + -a.y * b.y + a.z * -b.z;

    return result;
}

inline Quat operator*(f32 f, Quat q)
{
    Quat result;
    result.x = q.x * f;
    result.y = q.y * f;
    result.z = q.z * f;
    result.w = q.w * f;

    return result;
}

inline Quat operator*(Quat q, f32 f)
{
    Quat result = f * q;

    return result;
}

inline f32 Dot(Quat a, Quat b)
{
    f32 result = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    return result;
}

inline Quat Conjugate(Quat a)
{
    Quat result;
    result.x = -a.x;
    result.y = -a.x;
    result.z = -a.x;
    result.w = a.w;
    return result;
}

inline Quat Normalize(Quat a)
{
    Quat result = a;
    f32 length  = Length(a.xyzw);
    result.xyzw /= length;
    return result;
}

inline Mat4 QuatToMatrix(Quat a)
{
    Quat normalized = Normalize(a);
    f32 xx          = normalized.x * normalized.x;
    f32 yy          = normalized.y * normalized.y;
    f32 zz          = normalized.z * normalized.z;
    f32 xy          = normalized.x * normalized.y;
    f32 xz          = normalized.x * normalized.z;
    f32 yz          = normalized.y * normalized.z;
    f32 wx          = normalized.w * normalized.x;
    f32 wy          = normalized.w * normalized.y;
    f32 wz          = normalized.w * normalized.z;

    Mat4 result;
    result.a1 = 1 - 2 * (yy + zz);
    result.a2 = 2 * (xy + wz);
    result.a3 = 2 * (xz - wy);
    result.a4 = 0;
    result.b1 = 2 * (xy - wz);
    result.b2 = 1 - 2 * (xx + zz);
    result.b3 = 2 * (yz + wx);
    result.b4 = 0;
    result.c1 = 2 * (xz + wy);
    result.c2 = 2 * (yz - wx);
    result.c3 = 1 - 2 * (xx + yy);
    result.c4 = 0;
    result.d1 = 0;
    result.d2 = 0;
    result.d3 = 0;
    result.d4 = 1;
    return result;
}

inline Quat MatrixToQuat(Mat4 m)
{
    Quat result;
    if (m.a1 + m.b2 + m.c3 > 0.f)
    {
        f64 t    = m.a1 + m.b2 + m.c3 + 1.;
        f64 s    = SquareRoot(t) * .5;
        result.w = (f32)(s * t);
        result.z = (f32)((m.a2 - m.b1) * s);
        result.y = (f32)((m.c1 - m.a3) * s);
        result.x = (f32)((m.b3 - m.c2) * s);
    }
    else if (m.a1 > m.b2 && m.a1 > m.c3)
    {
        f64 t    = m.a1 - m.b2 - m.c3 + 1.;
        f64 s    = SquareRoot(t) * 0.5;
        result.x = (f32)(s * t);
        result.y = (f32)((m.a2 + m.b1) * s);
        result.z = (f32)((m.c1 + m.a3) * s);
        result.w = (f32)((m.b3 - m.c2) * s);
    }
    else if (m.b2 > m.c3)
    {
        f64 t    = -m.a1 + m.b2 - m.c3 + 1.;
        f64 s    = SquareRoot(t) * 0.5;
        result.y = (f32)(s * t);
        result.x = (f32)((m.a2 + m.b1) * s);
        result.w = (f32)((m.c1 - m.a3) * s);
        result.z = (f32)((m.b3 + m.c2) * s);
    }
    else
    {
        f64 t    = -m.a1 - m.b2 + m.c3 + 1.;
        f64 s    = SquareRoot(t) * .5;
        result.z = (f32)(s * t);
        result.w = (f32)((m.a2 - m.b1) * s);
        result.x = (f32)((m.c1 + m.a3) * s);
        result.y = (f32)((m.b3 + m.c2) * s);
    }

    result = Normalize(result);
    return result;
}

inline Quat Lerp(Quat a, Quat b, f32 t)
{
    Quat result;
    result = (1 - t) * a + t * b;
    return result;
}
inline Quat Nlerp(Quat a, Quat b, f32 t)
{
    Quat result = Normalize(Lerp(a, b, t));
    return result;
}

inline Quat MakeQuat(f32 x, f32 y, f32 z, f32 w)
{
    Quat result;
    result.x = x;
    result.y = y;
    result.z = z;
    result.w = w;
    return result;
}

inline b32 operator==(Quat &a, Quat &b)
{
    b32 result = (a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w) ? true : false;
    return result;
}

/*
 * MATRIX 3X3
 */

inline Mat3 operator*(Mat3 a, Mat3 b)
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
internal Mat4 Identity()
{
    return MakeMat4(1.f);
}
internal V4 Transform(Mat4 a, V4 b)
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

inline V4 operator*(Mat4 a, V4 b)
{
    V4 result = Transform(a, b);
    return result;
}

inline V3 operator*(Mat4 a, V3 b)
{
    V3 result = Transform(a, MakeV4(b, 1.f)).xyz;
    return result;
}

inline Mat4 operator*(Mat4 a, Mat4 b)
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

inline Mat4 operator/(Mat4 a, float b)
{
    Mat4 result;
    f32 divisor = 1.f / b;
    result.columns[0] *= divisor;
    result.columns[1] *= divisor;
    result.columns[2] *= divisor;
    result.columns[3] *= divisor;
    return result;
}

inline Mat4 operator*(Mat4 a, float b)
{
    Mat4 result;
    result.columns[0] *= b;
    result.columns[1] *= b;
    result.columns[2] *= b;
    result.columns[3] *= b;
    return result;
}

inline Mat4 Scale(V3 value)
{
    Mat4 result           = MakeMat4(1.f);
    result.elements[0][0] = value.x;
    result.elements[1][1] = value.y;
    result.elements[2][2] = value.z;
    return result;
}

inline Mat4 Scale(f32 value)
{
    Mat4 result = Identity();
    result.elements[0][0] = value;
    result.elements[1][1] = value;
    result.elements[2][2] = value;
    return result;
}

inline Mat4 Rotate4(V3 axis, f32 theta)
{
    Mat4 result           = MakeMat4(1.f);
    axis                  = Normalize(axis);
    f32 sinTheta          = Sin(theta);
    f32 cosTheta          = Cos(theta);
    f32 cosValue          = 1.f - cosTheta;
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

inline Mat4 Translate(Mat4 m, V3 value)
{
    m.columns[3].xyz = value;
    return m;
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
    Mat4 result           = {};
    f32 cotangent         = 1.f / Tan(fov / 2);
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

// NOTE: right hand
inline Mat4 LookAt4(V3 eye, V3 center, V3 up)
{
    Mat4 result;
    V3 f                  = Normalize(center - eye);
    V3 s                  = Normalize(Cross(f, up));
    V3 u                  = Cross(s, f);
    result.elements[0][0] = s.x;
    result.elements[0][1] = u.x;
    result.elements[0][2] = -f.x;
    result.elements[0][3] = 0.0f;
    result.elements[1][0] = s.y;
    result.elements[1][1] = u.y;
    result.elements[1][2] = -f.y;
    result.elements[1][3] = 0.0f;
    result.elements[2][0] = s.z;
    result.elements[2][1] = u.z;
    result.elements[2][2] = -f.z;
    result.elements[2][3] = 0.0f;
    result.elements[3][0] = -Dot(s, eye);
    result.elements[3][1] = -Dot(u, eye);
    result.elements[3][2] = Dot(f, eye);
    result.elements[3][3] = 1.0f;

    return result;
}

inline Mat4 Mat4Rows3x3(V3 x, V3 y, V3 z)
{
    Mat4 result = {{
        {x.x, y.x, z.x, 0},
        {x.y, y.y, z.y, 0},
        {x.z, y.z, z.z, 0},
        {0, 0, 0, 1},
    }};
    return result;
}

inline Mat4 Mat4Cols3x3(V3 x, V3 y, V3 z)
{
    Mat4 result = {{
        {x.x, x.y, x.z, 0},
        {y.x, y.y, y.z, 0},
        {z.x, z.y, z.z, 0},
        {0, 0, 0, 1},
    }};

    return result;
}

internal Mat4 CameraTransform(V3 x, V3 y, V3 z, V3 p)
{
    Mat4 result         = Mat4Rows3x3(x, y, z);
    V3 ap               = -(result * p);
    result.columns[3].x = ap.x;
    result.columns[3].y = ap.y;
    result.columns[3].z = ap.z;
    return result;
}

internal f32 Determinant(Mat4 a)
{
    f32 result = a.a1 * a.b2 * a.c3 * a.d4 - a.a1 * a.b2 * a.c4 * a.d3 + a.a1 * a.b3 * a.c4 * a.d2 -
                 a.a1 * a.b3 * a.c2 * a.d4 + a.a1 * a.b4 * a.c2 * a.d3 - a.a1 * a.b4 * a.c3 * a.d2 -
                 a.a2 * a.b3 * a.c4 * a.d1 + a.a2 * a.b3 * a.c1 * a.d4 - a.a2 * a.b4 * a.c1 * a.d3 +
                 a.a2 * a.b4 * a.c3 * a.d1 - a.a2 * a.b1 * a.c3 * a.d4 + a.a2 * a.b1 * a.c4 * a.d3 +
                 a.a3 * a.b4 * a.c1 * a.d2 - a.a3 * a.b4 * a.c2 * a.d1 + a.a3 * a.b1 * a.c2 * a.d4 -
                 a.a3 * a.b1 * a.c4 * a.d2 + a.a3 * a.b2 * a.c4 * a.d1 - a.a3 * a.b2 * a.c1 * a.d4 -
                 a.a4 * a.b1 * a.c2 * a.d3 + a.a4 * a.b1 * a.c3 * a.d2 - a.a4 * a.b2 * a.c3 * a.d1 +
                 a.a4 * a.b2 * a.c1 * a.d3 - a.a4 * a.b3 * a.c1 * a.d2 + a.a4 * a.b3 * a.c2 * a.d1;
    return result;
}

internal Mat4 Inverse(Mat4 a)
{
    Mat4 res   = {};
    f32 invdet = Determinant(a);
    if (invdet == 0)
    {
        return res;
    }
    invdet = 1.f / invdet;
    // NOTE: transpose
    res.a1 = invdet * (a.b2 * (a.c3 * a.d4 - a.d3 * a.c4) - a.c2 * (a.b3 * a.d4 - a.d3 * a.b4) +
                       a.d2 * (a.b3 * a.c4 - a.c3 * a.b4));
    res.a2 = -invdet * (a.a2 * (a.c3 * a.d4 - a.d3 * a.c4) - a.c2 * (a.a3 * a.d4 - a.d3 * a.a4) +
                        a.d2 * (a.a3 * a.c4 - a.c3 * a.a4));
    res.a3 = invdet * (a.a2 * (a.b3 * a.d4 - a.d3 * a.b4) - a.b2 * (a.a3 * a.d4 - a.d3 * a.a4) +
                       a.d2 * (a.a3 * a.b4 - a.b3 * a.a4));
    res.a4 = -invdet * (a.a2 * (a.b3 * a.c4 - a.b4 * a.c3) - a.b2 * (a.a3 * a.c4 - a.c3 * a.a4) +
                        a.c2 * (a.a3 * a.b4 - a.b3 * a.a4));
    res.b1 = -invdet * (a.b1 * (a.c3 * a.d4 - a.d3 * a.c4) - a.c1 * (a.b3 * a.d4 - a.d3 * a.b4) +
                        a.d1 * (a.b3 * a.c4 - a.c3 * a.b4));
    res.b2 = invdet * (a.a1 * (a.c3 * a.d4 - a.d3 * a.c4) - a.c1 * (a.a3 * a.d4 - a.d3 * a.a4) +
                       a.d1 * (a.a3 * a.c4 - a.c3 * a.a4));
    res.b3 = -invdet * (a.a1 * (a.b3 * a.d4 - a.d3 * a.b4) - a.b1 * (a.a3 * a.d4 - a.d3 * a.a4) +
                        a.d1 * (a.a3 * a.b4 - a.b3 * a.a4));
    res.b4 = invdet * (a.a1 * (a.b3 * a.c4 - a.b4 * a.c3) - a.b1 * (a.a3 * a.c4 - a.c3 * a.a4) +
                       a.c1 * (a.a3 * a.b4 - a.b3 * a.a4));
    res.c1 = invdet * (a.b1 * (a.c2 * a.d4 - a.d2 * a.c4) - a.c1 * (a.b2 * a.d4 - a.d2 * a.b4) +
                       a.d1 * (a.b2 * a.c4 - a.c2 * a.b4));
    res.c2 = -invdet * (a.a1 * (a.c2 * a.d4 - a.d2 * a.c4) - a.c1 * (a.a2 * a.d4 - a.d2 * a.a4) +
                        a.d1 * (a.a2 * a.c4 - a.c2 * a.a4));
    res.c3 = invdet * (a.a1 * (a.b2 * a.d4 - a.d2 * a.b4) - a.b1 * (a.a2 * a.d4 - a.d2 * a.a4) +
                       a.d1 * (a.a2 * a.b4 - a.b2 * a.a4));
    res.c4 = -invdet * (a.a1 * (a.b2 * a.c4 - a.c2 * a.b4) - a.b1 * (a.a2 * a.c4 - a.c2 * a.a4) +
                        a.c1 * (a.a2 * a.b4 - a.b2 * a.a4));
    res.d1 = -invdet * (a.b1 * (a.c2 * a.d3 - a.d2 * a.c3) - a.c1 * (a.b2 * a.d3 - a.d2 * a.b3) +
                        a.d1 * (a.b2 * a.c3 - a.c2 * a.b3));
    res.d2 = invdet * (a.a1 * (a.c2 * a.d3 - a.c3 * a.d2) - a.c1 * (a.a2 * a.d3 - a.d2 * a.a3) +
                       a.d1 * (a.a2 * a.c3 - a.c2 * a.a3));
    res.d3 = -invdet * (a.a1 * (a.b2 * a.d3 - a.d2 * a.b3) - a.b1 * (a.a2 * a.d3 - a.d2 * a.a3) +
                        a.d1 * (a.a2 * a.b3 - a.b2 * a.a3));
    res.d4 = invdet * (a.a1 * (a.b2 * a.c3 - a.c2 * a.b3) - a.b1 * (a.a2 * a.c3 - a.c2 * a.a3) +
                       a.c1 * (a.a2 * a.b3 - a.b2 * a.a3));

    return res;
}

inline V3 GetTranslation(Mat4 m)
{
    V3 result = m.columns[3].xyz;
    return result;
}

/*
 * RECTANGLE 2
 */

inline Rect2 CreateRectFromCenter(V2 pos, V2 dim)
{
    Rect2 result;
    result.pos  = pos - (dim / 2.f);
    result.size = dim;
    return result;
}

inline Rect2 CreateRectFromBottomLeft(V2 pos, V2 dim)
{
    Rect2 result;
    result.pos  = pos;
    result.size = dim;
    return result;
}

inline b32 IsInRectangle(Rect2 a, V2 pos)
{
    b32 result =
        pos.x >= a.pos.x && pos.x <= a.pos.x + a.size.x && pos.y >= a.pos.y && pos.y <= a.pos.y + a.size.y;
    return result;
}

inline b32 Rect2Overlap(const Rect2 rect1, const Rect2 rect2)
{
    b32 result = (rect1.x <= rect2.x + rect2.width && rect2.x <= rect1.x + rect1.width &&
                  rect1.y <= rect2.y + rect2.height && rect2.y <= rect1.y + rect1.height);
    return result;
}

inline V2 GetRectCenter(Rect2 a)
{
    V2 result = a.pos + 0.5 * a.size;
    return result;
}

/*
 * RECTANGLE 3
 */
inline Rect3 Rect3BottomLeft(V3 pos, V3 size)
{
    Rect3 result;
    result.pos  = pos;
    result.size = size;
    return result;
}

inline Rect3 Rect3Center(V3 pos, V3 size)
{
    Rect3 result;
    result.pos  = pos - size / 2;
    result.size = size / 2;
    return result;
}

inline V3 Center(Rect3 a)
{
    V3 result;
    result = a.pos + a.size / 2;
    return result;
}

/*
 * TRIANGLE
 */
inline V3 Barycentric(V2 a, V2 b, V2 c, V2 p)
{
    // NOTE: From Real Time Collision Detection by Christer Ericson
    V2 v0       = b - a;
    V2 v1       = c - a;
    V2 v2       = p - a;
    float d00   = Dot(v0, v0);
    float d01   = Dot(v0, v1);
    float d11   = Dot(v1, v1);
    float d20   = Dot(v2, v0);
    float d21   = Dot(v2, v1);
    float denom = d00 * d11 - d01 * d01;
    float v     = (d11 * d20 - d01 * d21) / denom;
    float w     = (d00 * d21 - d01 * d20) / denom;
    float u     = 1.0f - v - w;

    V3 result = {u, v, w};
    return result;
}

#define KEEPMOVINGFORWARD_MATH_H
#endif
