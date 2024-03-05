#ifndef KEEPMOVINGFORWARD_STRING_H
#define KEEPMOVINGFORWARD_STRING_H

struct string
{
    u8 *str;
    u64 size;
};

#define STB_SPRINTF_IMPLEMENTATION
#include "third_party/stb_sprintf.h"

//////////////////////////////
// Char
//
inline b32 CharIsWhitespace(u8 c);
internal b32 CharIsAlpha(u8 c);
internal b32 CharIsAlphaUpper(u8 c);
internal b32 CharIsAlphaLower(u8 c);
internal b32 CharIsDigit(u8 c);
internal u8 CharToLower(u8 c);
internal u8 CharToUpper(u8 c);

//////////////////////////////
// Creating Strings
//
internal string Str8(u8 *str, u64 size);
inline string Substr8(string str, u64 min, u64 max);
internal u64 CalculateCStringLength(char *cstr);
internal string PushStr8F(Arena *arena, char *fmt, ...);
internal string PushStr8FV(Arena *arena, char *fmt, va_list args);
internal string PushStr8Copy(Arena *arena, string str);
internal string StrConcat(Arena *arena, string s1, string s2);

#define Str8Lit(s)     Str8((u8 *)(s), sizeof(s) - 1)
#define Str8C(cstring) Str8((u8 *)(cstring), CalculateCStringLength(cstring))

//////////////////////////////
// Finding strings
//
typedef u32 MatchFlags;
enum
{
    MatchFlag_CaseInsensitive = (1 << 0),
    MatchFlag_RightSideSloppy  = (1 << 1),
    MatchFlag_SlashInsensitive = (1 << 2),
    MatchFlag_FindLast         = (1 << 3),
    MatchFlag_KeepEmpties      = (1 << 4),
};

internal string SkipWhitespace(string str);
internal b32 StartsWith(string a, string b);
internal b32 MatchString(string a, string b, MatchFlags flags);
internal u64 FindSubstring(string haystack, string needle, u64 startPos, MatchFlags flags);

//////////////////////////////
// File Path Helpers
//
internal string GetFileExtension(string path);
internal string Str8PathChopPastLastSlash(string string);
internal string Str8PathChopLastSlash(string string);

//////////////////////////////
// Hash
//
internal u64 HashFromString(string string);

//////////////////////////////
// String token building/reading
//

struct StringBuilderNode
{
    string str;
    StringBuilderNode *next;
};

struct StringBuilder
{
    StringBuilderNode *first;
    StringBuilderNode *last;
    u32 totalSize;
    TempArena scratch;
};

struct Tokenizer
{
    string input;
    u8 *cursor;
};

inline void Advance(Tokenizer *tokenizer, u32 size);
inline u8 *GetCursor_(Tokenizer *tokenizer);
inline b32 EndOfBuffer(Tokenizer *tokenizer);
internal string ReadLine(Tokenizer *tokenizer);
internal void Put(StringBuilder *builder, void *data, u32 size);
internal void Put(StringBuilder *builder, string str);
internal void Put(StringBuilder *builder, u32 value);
internal b32 WriteEntireFile(StringBuilder *builder, string filename);
internal void Get(Tokenizer *tokenizer, void *ptr, u32 size);

#define PutPointer(builder, ptr) Put(builder, ptr, sizeof(*ptr))
#define PutArray(builder, array) Put(builder, array.items, sizeof(array.items[0]) * array.count)

#define GetPointer(tokenizer, ptr) Get(tokenizer, ptr, sizeof(*ptr))
#define GetArray(tokenizer, array, count_)                                                                        \
    do                                                                                                            \
    {                                                                                                             \
        array.count = count_;                                                                                     \
        Get(tokenizer, array.items, sizeof(array.items[0]) * count_);                                             \
    } while (0)

#define GetTokenCursor(tokenizer, type) (type *)GetCursor_(tokenizer)

#endif
