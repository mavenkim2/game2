#ifndef KEEPMOVINGFORWARD_MATH_H
#include "keepmovingforward_common.h"
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

union Rect2
{
    struct
    {
        v2 pos;
        v2 size;
    };
    struct
    {
        float x;
        float y;
        float width;
        float height;
    };
};


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

inline Vector2 operator/(Vector2 a, float b)
{
    Vector2 result = a * (1.f/b);
    return result;
}

inline Vector2 operator/=(Vector2 &a, float b)
{
    a = a * (1.f / b);
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

inline Rect2 CreateRectFromCenter(v2 pos, v2 dim) {
    Rect2 result; 
    result.pos = pos - (dim / 2.f);
    result.size = dim;
    return result;
}

inline Rect2 CreateRectFromBottomLeft(v2 pos, v2 dim) {
    Rect2 result; 
    result.pos = pos; 
    result.size = dim; 
    return result;
}

inline bool IsInRectangle(Rect2 a, v2 pos) {
    bool result = pos.x >= a.pos.x && pos.x <= a.pos.x + a.size.x && pos.y >= a.pos.y && pos.y <= a.pos.y + a.size.y; 
    return result; 
}

inline bool Rect2Overlap(const Rect2 rect1, const Rect2 rect2)
{
    bool result = (rect1.x <= rect2.x + rect2.width && rect2.x <= rect1.x + rect1.width &&
                   rect1.y <= rect2.y + rect2.height && rect2.y <= rect1.y + rect1.height);
    return result;
}

inline v2 GetRectCenter(Rect2 a) {
    v2 result = a.pos + 0.5 * a.size;
    return result;
}

inline u32 RoundFloatToUint32(float value)
{
    u32 result = (u32)roundf(value);
    return result;
}
inline u32 RoundFloatToInt32(float value)
{
    i32 result = (i32)roundf(value);
    return result;
}

inline u32 FloorFloatToUint32(float value)
{
    u32 result = (u32)(floorf(value));
    return result;
}


#define KEEPMOVINGFORWARD_MATH_H
#endif
