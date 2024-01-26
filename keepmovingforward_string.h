#ifndef KEEPMOVINGFORWARD_STRING_H
#define KEEPMOVINGFORWARD_STRING_H

// IMPORTANT: STRINGS ARE IMMUTABLE.
struct String8
{
    u8 *str;
    u64 size;
};
#define STB_SPRINTF_IMPLEMENTATION
#include "third_party/stb_sprintf.h"

inline b32 CharIsWhitespace(u8 c);
internal b32 CharIsAlpha(u8 c);
internal b32 CharIsAlphaUpper(u8 c);
internal b32 CharIsAlphaLower(u8 c);
internal b32 CharIsDigit(u8 c);

internal String8 Str8(u8 *str, u64 size);
inline String8 Substr8(String8 str, u64 min, u64 max);
internal u64 CalculateCStringLength(char *cstr);
internal String8 Str8PathChopLastSlash(String8 string);
internal String8 Str8PathChopPastLastSlash(String8 string);
// internal String8 Concat(Arena *arena, String8 a, String8 b);

internal String8 PushStr8F(Arena *arena, char *fmt, ...);
internal String8 PushStr8FV(Arena *arena, char *fmt, va_list args);

#define Str8Lit(s) Str8((u8 *)(s), sizeof(s) - 1)
#define Str8C(cstring) Str8((u8 *)(cstring), CalculateCStringLength(cstring))

#endif
