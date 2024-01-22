#include <string.h>

internal String8 Str8(u8 *str, u64 size)
{
    String8 result;
    result.str = str;
    result.size = size;
    return result;
}

internal u64 CalculateCStringLength(char *cstr)
{
    u64 length = 0;
    for (; cstr[length]; length++)
    {
    }
    return length;
}

internal String8 Str8PathChopLastSlash(String8 string)
{
    u64 onePastLastSlash = string.size;
    for (u64 count = 0; count < string.size; count++)
    {
        if (string.str[count] == '\\')
        {
            onePastLastSlash = count;
        }
    }
    string.size = onePastLastSlash;
    return string;
}

internal String8 Str8PathChopPastLastSlash(String8 string)
{
    // TODO: implement find substring
    u64 onePastLastSlash = string.size;
    for (u64 count = 0; count < string.size; count++)
    {
        if (string.str[count] == '\\')
        {
            onePastLastSlash = count + 1;
        }
    }
    string.size = onePastLastSlash;
    return string;
}

// internal String8 Concat(Arena *arena, String8 a, String8 b)
// {
//     String8 result = {};
//     result.size = a.size + b.size;
//     result.str = PushArray(arena, u8, result.size + 1);
//
//     u8 *ptr = result.str;
//     memcpy(ptr, a.str, a.size);
//     ptr += a.size;
//     memcpy(ptr, b.str, b.size);
//     result.str[result.size] = 0;
//     return result;
// }

internal String8 PushStr8FV(Arena *arena, char *fmt, va_list args)
{
    String8 result = {};
    va_list args2;
    va_copy(args2, args);
    u64 neededBytes = stbsp_vsnprintf(0, 0, fmt, args) + 1;
    result.str = PushArray(arena, u8, neededBytes);
    result.size = neededBytes - 1;
    stbsp_vsnprintf((char *)result.str, (int)neededBytes, fmt, args2);
    return result;
}

internal String8 PushStr8F(Arena *arena, char *fmt, ...)
{
    String8 result = {};
    va_list args;
    va_start(args, fmt);
    result = PushStr8FV(arena, fmt, args);
    va_end(args);
    return result;
}
