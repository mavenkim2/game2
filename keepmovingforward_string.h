#ifndef KEEPMOVINGFORWARD_STRING_H
#define KEEPMOVINGFORWARD_STRING_H

struct string
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
internal u64 HashString(string string);

internal string Str8(u8 *str, u64 size);
inline string Substr8(string str, u64 min, u64 max);
internal u64 CalculateCStringLength(char *cstr);
internal string Str8PathChopLastSlash(string string);
internal string Str8PathChopPastLastSlash(string string);
// internal string Concat(Arena *arena, string a, String8 b);

internal string PushStr8F(Arena *arena, char *fmt, ...);
internal string PushStr8FV(Arena *arena, char *fmt, va_list args);

#define Str8Lit(s) Str8((u8 *)(s), sizeof(s) - 1)
#define Str8C(cstring) Str8((u8 *)(cstring), CalculateCStringLength(cstring))

#endif
