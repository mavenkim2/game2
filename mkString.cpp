#include "mkCrack.h"
#ifdef LSP_INCLUDE
#include <cstring>
#include "keepmovingforward_common.h"
#include "keepmovingforward_memory.h"
#include "keepmovingforward_string.h"
#include "keepmovingforward_math.h"
#include "platform_inc.h"
#endif

//////////////////////////////
// Constructors
//

// TODO: leak, use small memory allocator
string::string(const char *text)
{
    if (text)
    {
        size = CalculateCStringLength(text);
        str  = (u8 *)malloc(size + 1);
        MemoryCopy(str, text, size);
        str[size] = 0;

        // *this = Str8C(text);
    }
}

string::~string()
{
    // if (str)
    // {
    // free(str);
    // }
}

string::string()
{
    str  = 0;
    size = 0;
}

// TODO: allocations
void string::operator=(const char *text)
{
    if (text)
    {
        // *this = Str8C(text);
        *this = string(text);
    }
}

//////////////////////////////
// Char
//
inline b32 CharIsWhitespace(u8 c)
{
    return c == ' ';
}

internal b32 CharIsAlpha(u8 c)
{
    return CharIsAlphaUpper(c) || CharIsAlphaLower(c);
}

internal b32 CharIsAlphaUpper(u8 c)
{
    return c >= 'A' && c <= 'Z';
}

internal b32 CharIsAlphaLower(u8 c)
{
    return c >= 'a' && c <= 'z';
}
internal b32 CharIsDigit(u8 c)
{
    return (c >= '0' && c <= '9');
}

internal u8 CharToLower(u8 c)
{
    u8 result = (c >= 'A' && c <= 'Z') ? ('a' + (c - 'A')) : c;
    return result;
}
internal u8 CharToUpper(u8 c)
{
    u8 result = (c >= 'a' && c <= 'z') ? ('A' + (c - 'a')) : c;
    return result;
}
internal b32 CharIsSlash(u8 c)
{
    return (c == '/' || c == '\\');
}
internal u8 CharCorrectSlash(u8 c)
{
    if (CharIsSlash(c))
    {
        c = '/';
    }
    return c;
}

//////////////////////////////
// Creating Strings
//
internal string Str8(u8 *str, u64 size)
{
    string result;
    result.str  = str;
    result.size = size;
    return result;
}
inline string Substr8(string str, u64 min, u64 max)
{
    if (max > str.size)
    {
        max = str.size;
    }
    if (min > str.size)
    {
        min = str.size;
    }
    if (min > max)
    {
        Swap(u64, min, max);
    }
    str.size = max - min;
    str.str += min;
    return str;
}
internal u64 CalculateCStringLength(const char *cstr)
{
    u64 length = 0;
    for (; cstr[length]; length++)
    {
    }
    return length;
}

internal string PushStr8FV(Arena *arena, char *fmt, va_list args)
{
    string result = {};
    va_list args2;
    va_copy(args2, args);
    u64 neededBytes = stbsp_vsnprintf(0, 0, fmt, args) + 1;
    result.str      = PushArray(arena, u8, neededBytes);
    result.size     = neededBytes - 1;
    stbsp_vsnprintf((char *)result.str, (int)neededBytes, fmt, args2);
    return result;
}

internal string PushStr8Copy(Arena *arena, string str)
{
    string res;
    res.size = str.size;
    res.str  = PushArrayNoZero(arena, u8, str.size + 1);
    MemoryCopy(res.str, str.str, str.size);
    res.str[str.size] = 0;
    return res;
}

internal string PushStr8F(Arena *arena, char *fmt, ...)
{
    string result = {};
    va_list args;
    va_start(args, fmt);
    result = PushStr8FV(arena, fmt, args);
    va_end(args);
    return result;
}

internal string StrConcat(Arena *arena, string s1, string s2)
{
    string result;
    result.size = s1.size + s2.size;
    result.str  = PushArrayNoZero(arena, u8, result.size + 1);
    MemoryCopy(result.str, s1.str, s1.size);
    MemoryCopy(result.str + s1.size, s2.str, s2.size);
    result.str[result.size] = 0;
    return result;
}

//////////////////////////////
// Finding Strings
//

// NOTE: assumes string a already has a backing buffer of size at least b.size. If need to copy, use
// PushStr8Copy()
internal void StringCopy(string *out, string in)
{
    u8 *ptr = out->str;
    for (u64 i = 0; i < in.size; i++)
    {
        *ptr++ = *in.str++;
    }
    out->size = in.size;
}

b32 string::operator==(const string &a)
{
    b32 result = false;
    if (size == a.size)
    {
        for (int i = 0; i < a.size; i++)
        {
            result = (str[i] == a.str[i]);
            if (!result)
            {
                break;
            }
        }
    }
    return result;
}

b32 string::operator==(const char *text)
{
    string test = Str8C(text);
    return *this == test;
}

internal string SkipWhitespace(string str)
{
    u32 start = 0;
    for (u32 i = 0; i < str.size; i++)
    {
        start = i;
        if (!CharIsWhitespace(str.str[i]))
        {
            break;
        }
    }
    string result = Substr8(str, start, str.size);
    return result;
}

internal b32 StartsWith(string a, string b)
{
    b32 result = true;
    if (a.size >= b.size)
    {
        for (i32 i = 0; i < b.size; i++)
        {
            if (a.str[i] != b.str[i])
            {
                result = false;
                break;
            }
        }
    }
    return result;
}

internal b32 MatchString(string a, string b, MatchFlags flags)
{
    b32 result = 0;
    if (a.size == b.size || flags & MatchFlag_RightSideSloppy)
    {
        result               = 1;
        u64 size             = Min(a.size, b.size);
        b32 caseInsensitive  = (flags & MatchFlag_CaseInsensitive);
        b32 slashInsensitive = (flags & MatchFlag_SlashInsensitive);
        for (u64 i = 0; i < size; i++)
        {
            u8 charA = a.str[i];
            u8 charB = b.str[i];
            if (caseInsensitive)
            {
                charA = CharToLower(charA);
                charB = CharToLower(charB);
            }
            if (slashInsensitive)
            {
                charA = CharCorrectSlash(charA);
                charB = CharCorrectSlash(charB);
            }
            if (charA != charB)
            {
                result = 0;
                break;
            }
        }
    }
    return result;
}

internal u64 FindSubstring(string haystack, string needle, u64 startPos, MatchFlags flags)
{
    u64 foundIndex = haystack.size;
    for (u64 i = startPos; i < haystack.size; i++)
    {
        if (i + needle.size <= haystack.size)
        {
            string substr = Substr8(haystack, i, i + needle.size);
            if (MatchString(substr, needle, flags))
            {
                foundIndex = i;
                if (!(flags & MatchFlag_FindLast))
                {
                    break;
                }
                break;
            }
        }
    }
    return foundIndex;
}

internal b8 Contains(string haystack, string needle, MatchFlags flags)
{
    u64 index = FindSubstring(haystack, needle, 0, flags);
    return index != haystack.size;
}

internal u32 ConvertToUint(string word)
{
    u32 result = 0;
    while (CharIsDigit(*word.str))
    {
        result *= 10;
        result += *word.str++ - '0';
    }
    return result;
}

internal string SkipToNextWord(string line)
{
    u32 index;
    for (index = 0; index < line.size && line.str[index] != ' '; index++) continue;
    Assert(index + 1 < line.size && line.str[index + 1] != ' ');
    line = Substr8(line, index + 1, line.size);
    return line;
}

internal string GetFirstWord(string line)
{
    u32 index;
    for (index = 0; index < line.size && line.str[index] != ' '; index++) continue;
    line = Substr8(line, 0, index);
    return line;
}

//////////////////////////////
// File path helpers
//
internal string GetFileExtension(string str)
{
    for (u64 size = str.size; size > 0;)
    {
        size--;
        if (str.str[size] == '.')
        {
            u64 amt = Min(size + 1, str.size);
            str.str += amt;
            str.size -= amt;
            break;
        }
    }
    return str;
}

internal string RemoveFileExtension(string str)
{
    for (u64 size = str.size; size > 0;)
    {
        size--;
        if (str.str[size] == '.')
        {
            str.size = Min(size, str.size);
            break;
        }
    }
    return str;
}

internal string Str8PathChopLastSlash(string string)
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

internal string Str8PathChopPastLastSlash(string string)
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

internal string PathSkipLastSlash(string str)
{
    for (u64 size = str.size; size > 0;)
    {
        size--;
        if (CharIsSlash(str.str[size]))
        {
            u64 amt = Min(size + 1, str.size);
            str.str += amt;
            str.size -= amt;
            break;
        }
    }
    return str;
}

//////////////////////////////
// Hash
//
internal i32 HashFromString(string string)
{
#if 0
    i32 result = 5381;
    for (u64 i = 0; i < string.size; i += 1)
    {
        result = ((result << 5) + result) + string.str[i];
    }
#endif
    i32 result = 0;
    for (u64 i = 0; i < string.size; i++)
    {
        result += (string.str[i]) * ((i32)i + 119);
    }
    return result;
}

internal u64 HashStruct_(void *ptr, u64 size)
{
    string str;
    str.str    = (u8 *)(ptr);
    str.size   = size;
    u64 result = HashFromString(str);
    return result;
}

//////////////////////////////
// String reading
//
inline b32 Advance(Tokenizer *tokenizer, string check)
{
    string token;
    if ((u64)(tokenizer->input.str + tokenizer->input.size - tokenizer->cursor) < check.size)
    {
        return false;
    }
    token.size = check.size;
    token.str  = tokenizer->cursor;

    if (token == check)
    {
        tokenizer->cursor += check.size;
        return true;
    }
    return false;
}

inline void Advance(Tokenizer *tokenizer, u32 size)
{
    if (tokenizer->cursor + size <= tokenizer->input.str + tokenizer->input.size)
    {
        tokenizer->cursor += size;
    }
    else
    {
        tokenizer->cursor = tokenizer->input.str + tokenizer->input.size;
    }
}

inline u8 *GetCursor_(Tokenizer *tokenizer)
{
    u8 *result = tokenizer->cursor;
    return result;
}

inline b32 EndOfBuffer(Tokenizer *tokenizer)
{
    b32 result = tokenizer->cursor >= tokenizer->input.str + tokenizer->input.size;
    return result;
}

internal string ReadLine(Tokenizer *tokenizer)
{
    string result;
    result.str  = tokenizer->cursor;
    result.size = 0;

    while (!EndOfBuffer(tokenizer) && *tokenizer->cursor++ != '\n')
    {
        result.size++;
    }
    return result;
}

internal string ReadWord(Tokenizer *tokenizer)
{
    string result;
    result.str  = tokenizer->cursor;
    result.size = 0;

    while (!EndOfBuffer(tokenizer) && *tokenizer->cursor != ' ' && *tokenizer->cursor != '\n')
    {
        result.size++;
        tokenizer->cursor++;
    }
    return result;
}

internal b8 GetBetweenPair(string &out, const string line, const u8 ch)
{
    u8 left = ch;
    u8 right;
    if (ch == '(')
    {
        right = ')';
    }
    else if (ch == '"')
    {
        right = '"';
    }
    else
    {
        Assert(0);
    }

    i32 startIndex = -1;
    u32 endIndex   = (u32)line.size;
    for (u32 i = 0; i < line.size; i++)
    {
        if (line.str[i] == left && startIndex == -1)
        {
            startIndex = i;
        }
        else if (line.str[i] == right && startIndex != -1)
        {
            endIndex = i;
            break;
        }
    }
    if (startIndex != -1)
    {
        out = Substr8(line, (u32)startIndex + 1, endIndex - 1);
        return 1;
    }
    return 0;
}

inline f32 ReadFloat(Tokenizer *iter)
{
    f32 value    = 0;
    i32 exponent = 0;
    u8 c;
    b32 valueSign = (*iter->cursor == '-');
    if (valueSign || *iter->cursor == '+')
    {
        iter->cursor++;
    }
    while (CharIsDigit((c = *iter->cursor++)))
    {
        value = value * 10.0f + (c - '0');
    }
    if (c == '.')
    {
        while (CharIsDigit((c = *iter->cursor++)))
        {
            value = value * 10.0f + (c - '0');
            exponent -= 1;
        }
    }
    if (c == 'e' || c == 'E')
    {
        i32 sign = 1;
        i32 i    = 0;
        c        = *iter->cursor++;
        sign     = c == '+' ? 1 : -1;
        c        = *iter->cursor++;
        while (CharIsDigit(c))
        {
            i = i * 10 + (c - '0');
            c = *iter->cursor++;
        }
        exponent += i * sign;
    }
    while (exponent > 0)
    {
        value *= 10.0f;
        exponent--;
    }
    while (exponent < 0)
    {
        value *= 0.1f;
        exponent++;
    }
    if (valueSign)
    {
        value = -value;
    }
    return value;
}

inline u32 ReadUint(Tokenizer *iter)
{
    u32 result = 0;
    while (CharIsDigit(*iter->cursor))
    {
        result *= 10;
        result += *iter->cursor++ - '0';
    }
    return result;
}

inline void SkipToNextLine(Tokenizer *iter)
{
    while (!EndOfBuffer(iter) && *iter->cursor != '\n')
    {
        iter->cursor++;
    }
    iter->cursor++;
}

internal void Get(Tokenizer *tokenizer, void *ptr, u32 size)
{
    Assert(tokenizer->cursor + size <= tokenizer->input.str + tokenizer->input.size);
    MemoryCopy(ptr, tokenizer->cursor, size);
    Advance(tokenizer, size);
}

inline u8 *GetPointer_(Tokenizer *tokenizer)
{
    u64 offset;
    GetPointerValue(tokenizer, &offset);
    u8 *result = tokenizer->input.str + offset;
    return result;
}

//////////////////////////////
// Global string table
//

global u32 stringIds[1024];
std::atomic<u32> stringIdCount;

internal u32 Hash(string str)
{
    u32 result = 0;
    for (u64 i = 0; i < str.size; i++)
    {
        result += (str.str[i]) * ((i32)i + 119);
    }
    return result;
}

internal u32 HashCombine(u32 hash1, u32 hash2)
{
    hash1 ^= hash2 + 0x9e3779b9 + (hash1 << 6) + (hash1 >> 2);
    return hash1;
}

internal u32 AddSID(string str)
{
    u32 sid = Hash(str);
#ifdef INTERNAL
    for (u32 i = 0; i < stringIdCount.load(); i++)
    {
        if (sid == stringIds[i])
        {
            Printf("Collision: string %S, sid %i\n", str, sid);
            Assert(!"Hash collision");
        }
    }
#endif
    u32 index        = stringIdCount.fetch_add(1);
    stringIds[index] = sid;
    return sid;
}

internal u32 GetSID(string str)
{
    u32 sid = Hash(str);
    return sid;
}

//////////////////////////////
// String writing
//

internal u64 Put(StringBuilder *builder, void *data, u64 size)
{
    u64 cursor                        = builder->totalSize;
    StringBuilderChunkNode *chunkNode = builder->last;
    if (chunkNode == 0 || chunkNode->count >= chunkNode->cap)
    {
        chunkNode = PushStruct(builder->arena, StringBuilderChunkNode);
        QueuePush(builder->first, builder->last, chunkNode);
        chunkNode->cap    = 256;
        chunkNode->values = PushArray(builder->arena, StringBuilderNode, chunkNode->cap);
    }
    StringBuilderNode *node = &chunkNode->values[chunkNode->count++];
    node->str.str           = PushArray(builder->arena, u8, size);
    node->str.size          = size;

    builder->totalSize += size;

    MemoryCopy(node->str.str, data, size);
    return cursor;
}

inline u8 *PutPointerPlaceholder(StringBuilder *builder)
{
}

internal u64 Put(StringBuilder *builder, string str)
{
    Assert((u32)str.size == str.size);
    u64 result = Put(builder, str.str, (u32)str.size);
    return result;
}

internal u64 Put(StringBuilder *builder, u32 value)
{
    u64 result = PutPointerValue(builder, &value);
    return result;
}

internal u64 PutU64(StringBuilder *builder, u64 value)
{
    u64 result = PutPointerValue(builder, &value);
    return result;
}

internal void PutLine(StringBuilder *builder, u32 indents, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    for (u32 i = 0; i < indents; i++)
    {
        Put(builder, "\t");
    }
    string result = PushStr8FV(builder->arena, fmt, args);
    Put(builder, result);
    Put(builder, "\n");
    va_end(args);
}

internal StringBuilder ConcatBuilders(Arena *arena, StringBuilder *a, StringBuilder *b)
{
    StringBuilder result = {};
    result.first         = a->first;
    result.last          = b->last;
    a->last->next        = b->first;
    result.totalSize     = a->totalSize + b->totalSize;
    result.arena         = arena;
    return result;
}

internal string CombineBuilderNodes(StringBuilder *builder)
{
    string result;
    result.str  = PushArray(builder->arena, u8, builder->totalSize);
    result.size = builder->totalSize;

    u8 *cursor = result.str;
    for (StringBuilderChunkNode *node = builder->first; node != 0; node = node->next)
    {
        for (u32 i = 0; i < node->count; i++)
        {
            StringBuilderNode *n = &node->values[i];
            MemoryCopy(cursor, n->str.str, n->str.size);
            cursor += n->str.size;
        }
    }
    return result;
}

internal b32 WriteEntireFile(StringBuilder *builder, string filename)
{
    string result = CombineBuilderNodes(builder);
    b32 success   = platform.WriteFile(filename, result.str, (u32)result.size);
    return success;
}

internal b32 WriteStringBuilders(StringBuilder *builders, u32 count, string filename)
{
    for (u32 i = 0; i < count; i++)
    {
        StringBuilder *builder = &builders[i];
    }
}

inline u64 PutPointer(StringBuilder *builder, u64 address)
{
    u64 offset = builder->totalSize;
    offset += sizeof(offset) + address;
    PutPointerValue(builder, &offset);
    return offset;
}

inline void ConvertPointerToOffset(u8 *buffer, u64 location, u64 offset)
{
    // MemoryCopy(buffer + location, &offset, sizeof(offset));
    *(u64 *)(buffer + location) = offset;
}

inline u8 *ConvertOffsetToPointer(u8 *base, u64 offset)
{
    u8 *ptr = base + offset;
    return ptr;
}
