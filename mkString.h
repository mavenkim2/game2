#ifndef KEEPMOVINGFORWARD_STRING_H
#define KEEPMOVINGFORWARD_STRING_H

#include "mkCrack.h"
#ifdef LSP_INCLUDE
#include "keepmovingforward_common.h"
#include "keepmovingforward_memory.h"
#endif

static const i32 MAX_OS_PATH = 256;

struct string
{
    u8 *str;
    u64 size;

    string();
    string(const char *text);
    ~string();
    void operator=(const char *text);
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
internal u64 CalculateCStringLength(const char *cstr);
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
    MatchFlag_CaseInsensitive  = (1 << 0),
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
internal i32 HashFromString(string string);
internal u64 HashStruct_(void *ptr, u64 size);
#define HashStruct(ptr) HashStruct_((ptr), sizeof(*(ptr)))

//////////////////////////////
// String token building/reading
//

struct StringBuilderNode
{
    string str;
};

struct StringBuilderChunkNode
{
    StringBuilderNode *values;
    StringBuilderChunkNode *next;

    u32 count;
    u32 cap;
};

struct StringBuilder
{
    StringBuilderChunkNode *first;
    StringBuilderChunkNode *last;

    u64 totalSize;
    Arena *arena;
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
internal void Get(Tokenizer *tokenizer, void *ptr, u32 size);
inline u8 *GetPointer_(Tokenizer *tokenizer);

internal u64 Put(StringBuilder *builder, void *data, u64 size);
internal u64 Put(StringBuilder *builder, string str);
internal u64 Put(StringBuilder *builder, u32 value);
internal string CombineBuilderNodes(StringBuilder *builder);
internal b32 WriteEntireFile(StringBuilder *builder, string filename);
inline u64 PutPointer(StringBuilder *builder, u64 address);
inline void ConvertPointerToOffset(u8 *buffer, u64 location, u64 offset);

#define ConvertOffsetToPointer(buffer, ptr, type) (*(ptr)) = (type *)((u8 *)(buffer) + (u64)(*(ptr)))
#define PutPointerValue(builder, ptr)             Put(builder, ptr, sizeof(*ptr))
#define PutStruct(builder, s)                     PutPointerValue(builder, &s);
#define AppendArray(builder, ptr, count)          Put(builder, ptr, sizeof(*ptr) * count)
#define PutArray(builder, array, count)           Put((builder), (array), sizeof((array)[0]) * (count));

#define GetPointerValue(tokenizer, ptr) Get(tokenizer, ptr, sizeof(*ptr))
#define GetPointer(tokenizer, type)     (type *)GetPointer_(tokenizer)
#define GetArray(tokenizer, array, count_)                            \
    do                                                                \
    {                                                                 \
        array.count = count_;                                         \
        Get(tokenizer, array.items, sizeof(array.items[0]) * count_); \
    } while (0)

#define GetTokenCursor(tokenizer, type) (type *)GetCursor_(tokenizer)

#endif
