#ifndef KEEPMOVINGFORWARD_MATH_H
#include "keepmovingforward_types.h"
#include <math.h>

// TODO: use intrinsics
inline float Max(float a, float b)
{
    float result = a > b ? a : b;
    return result;
}

inline float Min(float a, float b)
{
    float result = a < b ? a : b;
    return result;
}

inline float Abs(float a)
{
    float result = a > 0 ? a : -a;
    return result;
}

inline float Sign(float a)
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

union Vector2
{
    struct
    {
        float x, y;
    };
    float e[2];
};

typedef Vector2 v2;

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
inline Vector2 operator*(Vector2 a, float b)
{
    Vector2 result;
    result.x = a.x * b;
    result.y = a.y * b;
    return result;
}

inline Vector2 operator*(float b, Vector2 a)
{
    Vector2 result = a * b;
    return result;
}

inline Vector2 operator*=(Vector2 &a, float b)
{
    a = a * b;
    return a;
}
inline bool operator==(Vector2 &a, Vector2 b)
{
    bool result = a.x == b.x && a.y == b.y ? true : false;
    return result;
}
inline float Dot(Vector2 a, Vector2 b)
{
    float result = a.x * b.x + a.y * b.y;
    return result;
}
inline v2 Cross(float a, Vector2 b)
{
    v2 result = v2{-a * b.y + a * b.x};
    return result;
}

inline float Cross(Vector2 a, Vector2 b)
{
    float result = a.x * b.y - a.y * b.x;
    return result;
}

inline uint32 RoundFloatToUint32(float value)
{
    uint32 result = (uint32)(value + 0.5f);
    return result;
}
inline uint32 RoundFloatToInt32(float value)
{
    int32 result = (int32)(value + 0.5f);
    return result;
}

inline float SquareRoot(float a)
{
    float result = sqrtf(a);
    return result;
}

inline float Length(Vector2 a)
{
    float result = SquareRoot(Dot(a, a));
    return result;
}

inline v2 Normalize(Vector2 a)
{
    float length = Length(a);
    v2 result = a * (1.f / length);
    return result;
}

#define KEEPMOVINGFORWARD_MATH_H
#endif
