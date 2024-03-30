// ALWAYS WRITE THE CODE YOU KNOW HOW TO WRITE FIRST, DON'T START IN THE PLACE WITH THE QUESTION MARK
// IF YOU KNOW HOW YOU WANT TO USE A SYSTEM, WRITE THE USAGE CODE FIRST!
#include "crack.h"
#ifdef LSP_INCLUDE
#include "asset_cache.h"
#include "keepmovingforward_platform.h"
#endif
// internal void DrawText(string text, u32 fontSize, AS_Handle font, u32 x, u32 y) {}

global F_State *f_state;

struct AS_Node;

internal void F_Init()
{
    Arena *arena           = ArenaAlloc();
    f_state                = PushStruct(arena, F_State);
    f_state->numStyleSlots = 1024;
    f_state->styleSlots    = PushArray(arena, F_StyleSlot, f_state->numStyleSlots);
}

internal F_Data F_InitializeFont(AS_Node *node, u8 *buffer)
{
    F_Data metrics;
    stbtt_InitFont(&metrics.font, buffer, stbtt_GetFontOffsetForIndex(buffer, 0));
    stbtt_GetFontVMetrics(&metrics.font, &metrics.ascent, &metrics.descent, &metrics.lineGap);
    return metrics;
}

// internal F_Metrics F_GetMetrics(F_Handle font, f32 size) {}
internal F_StyleNode *F_GetStyleFromFontSize(AS_Handle font, f32 size)
{
    u64 buffer[]              = {font.u64[0], font.u64[1], (u64)Round(size)};
    u64 hash                  = HashFromString(Str8((u8 *)buffer, sizeof(buffer)));
    F_StyleSlot *slot         = &f_state->styleSlots[hash & (f_state->numStyleSlots - 1)];
    F_StyleNode *existingNode = 0;
    for (F_StyleNode *node = slot->first; node != 0; node = node->next)
    {
        if (node->hash == hash)
        {
            existingNode = node;
            break;
        }
    }
    if (existingNode == 0)
    {
        Font *f               = GetFont(font);
        existingNode          = PushStruct(f_state->arena, F_StyleNode);
        existingNode->hash    = hash;
        existingNode->scale   = stbtt_ScaleForPixelHeight(&f->fontData.font, size);
        existingNode->ascent  = f->fontData.ascent;
        existingNode->descent = f->fontData.descent;
        existingNode->info    = PushArray(f_state->arena, F_RasterInfo, 256);
        QueuePush(slot->first, slot->last, existingNode);
    }
    return existingNode;
}

internal F_RasterInfo *F_Raster(AS_Handle font, F_StyleNode *node, string str)
{
    u8 c               = str.str[0];
    F_RasterInfo *info = node->info + c;
    Font *f            = GetFont(font);
    if (info->width == 0 && info->height == 0)
    {
        u8 *bitmap = stbtt_GetCodepointBitmap(&f->fontData.font, 0, node->scale, str.str[0], &info->width,
                                              &info->height, 0, 0);
        i32 lsb;
        stbtt_GetCodepointHMetrics(&f->fontData.font, c, &info->advance, &lsb);
        info->handle = R_AllocateTexture2D(bitmap, info->width, info->height, R_TexFormat_R8);
    }
    return info;
}

// TODO: table?
internal F_Run *F_GetFontRun(Arena *arena, AS_Handle font, f32 size, string str)
{
    F_Run *result          = PushStruct(arena, F_Run);
    F_StyleNode *styleNode = F_GetStyleFromFontSize(font, size);
    result->ascent         = styleNode->ascent;
    result->descent        = styleNode->descent;
    // Only support ascii characters for now
    for (u64 i = 0; i < str.size;)
    {
        u8 c = str.str[i];
        if (c >= 256)
        {
            Assert(!"Character not supported yet");
        }
        F_RasterInfo *info          = F_Raster(font, styleNode, Substr8(str, i, i + 1));
        F_PieceChunkNode *chunkNode = result->last;
        if (chunkNode == 0 || chunkNode->count == chunkNode->cap)
        {
            chunkNode         = PushStructNoZero(arena, F_PieceChunkNode);
            chunkNode->next   = 0;
            chunkNode->count  = 0;
            chunkNode->cap    = 256;
            chunkNode->pieces = PushArrayNoZero(arena, F_Piece, chunkNode->cap);
            QueuePush(result->first, result->last, chunkNode);
        }
        F_Piece *piece = chunkNode->pieces + chunkNode->count++;
        piece->texture = info->handle;
        piece->width   = info->width;
        piece->height  = info->height;
        piece->advance = info->advance;
    }
    return result;
}
