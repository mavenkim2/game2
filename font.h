#ifndef FONT_H
#define FONT_H

#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb_truetype.h"

//////////////////////////////
// Font cache
//
struct F_Data
{
    stbtt_fontinfo font;
    i32 ascent;
    i32 descent;
    i32 lineGap;
};

struct F_RasterInfo
{
    R_Handle handle;
    i32 width;
    i32 height;
    i32 advance;
};

struct F_StyleNode
{
    F_StyleNode *next;
    F_RasterInfo *info;
    u64 hash;
    f32 scale;
    i32 ascent;
    i32 descent;
};

struct F_StyleSlot
{
    F_StyleNode *first;
    F_StyleNode *last;
};

struct F_State
{
    Arena *arena;
    F_StyleSlot *styleSlots;
    u32 numStyleSlots;
};

//////////////////////////////
// Font run
//
struct F_Piece
{
    R_Handle texture;
    i32 width;
    i32 height;
    i32 advance;
};

struct F_PieceChunkNode
{
    F_PieceChunkNode *next;
    F_Piece *pieces;
    u32 count;
    u32 cap;
};

struct F_Run
{
    F_PieceChunkNode *first;
    F_PieceChunkNode *last;
    i32 ascent;
    i32 descent;
};

#endif
