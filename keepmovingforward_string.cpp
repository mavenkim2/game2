#include <string.h>

internal string Str8(u8 *str, u64 size)
{
    string result;
    result.str  = str;
    result.size = size;
    return result;
}

inline b32 CharIsWhitespace(u8 c)
{
    return c == ' ';
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

internal u64 CalculateCStringLength(char *cstr)
{
    u64 length = 0;
    for (; cstr[length]; length++)
    {
    }
    return length;
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

// internal string Concat(Arena *arena, string a, string b)
// {
//     string result = {};
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

internal b32 operator==(string a, string b)
{
    b32 result = false;
    if (a.size == b.size)
    {
        for (int i = 0; i < a.size; i++)
        {
            result = (a.str[i] == b.str[i]);
            if (!result)
            {
                break;
            }
        }
    }
    return result;
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

//////////////////////////////
// File path helpers
//
internal string GetFileExtension(string path)
{
    string result = path;

    u32 index = 0;
    u32 loc   = 0;
    while (index++ < path.size)
    {
        if (path.str[index] == '.')
        {
            loc = index + 1;
        }
    }
    result.str  = path.str + loc;
    result.size = path.size - loc;
    return result;
}

//////////////////////////////
// Hash
//
internal u64 HashFromString(string string)
{
    u64 result = 5381;
    for (u64 i = 0; i < string.size; i += 1)
    {
        result = ((result << 5) + result) + string.str[i];
    }
    return result;
}

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

inline b32 EndOfBuffer(Tokenizer *tokenizer)
{
    b32 result = tokenizer->cursor >= tokenizer->input.str + tokenizer->input.size;
    return result;
}

// TODO: maybe I don't want to advance if the end of the buffer is reached
internal string ReadLine(Tokenizer *tokenizer)
{
    string result;
    result.str  = tokenizer->cursor;
    result.size = 0;

    while (*tokenizer->cursor++ != '\n' && !EndOfBuffer(tokenizer))
    {
        result.size++;
    }
    return result;
}

internal void Put(StringBuilder *builder, void *data, u32 size)
{
    StringBuilderNode *node = PushStruct(builder->scratch.arena, StringBuilderNode);
    node->str.str           = PushArray(builder->scratch.arena, u8, size);
    node->str.size          = size;

    builder->totalSize += size;

    MemoryCopy(node->str.str, data, size);
    QueuePush(builder->first, builder->last, node);
}

internal void Put(StringBuilder *builder, string str)
{
    StringBuilderNode *node = PushStruct(builder->scratch.arena, StringBuilderNode);
    u32 size                = (u32)str.size;
    node->str               = str;

    builder->totalSize += size;
    QueuePush(builder->first, builder->last, node);
}

internal void Put(StringBuilder *builder, u32 value)
{
    StringBuilderNode *node = PushStruct(builder->scratch.arena, StringBuilderNode);
    u32 size                = sizeof(value);
    node->str.str           = PushArray(builder->scratch.arena, u8, size);
    node->str.size          = size;

    MemoryCopy(node->str.str, &value, size);
    builder->totalSize += size;
    QueuePush(builder->first, builder->last, node);
}

internal b32 WriteEntireFile(StringBuilder *builder, string filename)
{
    string result;
    result.str  = PushArray(builder->scratch.arena, u8, builder->totalSize);
    result.size = builder->totalSize;

    StringBuilderNode *node = builder->first;
    u8 *cursor              = result.str;
    while (node)
    {
        MemoryCopy(cursor, node->str.str, node->str.size);
        cursor += node->str.size;
        node = node->next;
    }
    b32 success = WriteFile(filename, result.str, (u32)result.size);
    return success;
}

internal void Get(Tokenizer *tokenizer, void *ptr, u32 size)
{
    Assert(tokenizer->cursor + size <= tokenizer->input.str + tokenizer->input.size);
    MemoryCopy(ptr, tokenizer->cursor, size);
    Advance(tokenizer, size);
}

#define PutPointer(builder, ptr) Put(builder, ptr, sizeof(*ptr))
#define PutArray(builder, array) Put(builder, array.items, sizeof(array.items[0]) * array.count)

#define GetPointer(tokenizer, ptr) Get(tokenizer, ptr, sizeof(*ptr))
#define GetArray(tokenizer, array, count_)                                                                        \
    do                                                                                                            \
    {                                                                                                             \
        array.count = count_;                                                                                     \
        Get(tokenizer, array.items, sizeof(array.items[0]) * count_);                                             \
    } while (0)
