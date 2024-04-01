// ALWAYS WRITE THE CODE YOU KNOW HOW TO WRITE FIRST, DON'T START IN THE PLACE WITH THE QUESTION MARK
// IF YOU KNOW HOW YOU WANT TO USE A SYSTEM, WRITE THE USAGE CODE FIRST!
#include "crack.h"
#ifdef LSP_INCLUDE
#include "font.h"
#include "asset_cache.h"
#include "keepmovingforward_platform.h"
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "third_party/stb_image_write.h"
// internal void DrawText(string text, u32 fontSize, AS_Handle font, u32 x, u32 y) {}

global F_State *f_state;

struct AS_Node;

internal void F_Init()
{
    Arena *arena           = ArenaAlloc();
    f_state                = PushStruct(arena, F_State);
    f_state->arena         = arena;
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
        existingNode->ascent  = (i32)(f->fontData.ascent * existingNode->scale);
        existingNode->descent = (i32)(f->fontData.descent * existingNode->scale);
        existingNode->info    = PushArray(f_state->arena, F_RasterInfo, 256);
        QueuePush(slot->first, slot->last, existingNode);
    }
    return existingNode;
}

internal F_RasterInfo *F_Raster(Font *font, F_StyleNode *node, string str)
{
    u8 c = str.str[0];
    Assert(c < 256);
    F_RasterInfo *info = node->info + c;
    if (info->rect.maxX == 0 && info->rect.maxY == 0)
    {
        i32 width, height, advance;

        u8 *bitmap =
            stbtt_GetCodepointBitmap(&font->fontData.font, node->scale, node->scale, c, &width, &height, 0, 0);

        i32 pitch     = width * 4;
        u8 *outBitmap = (u8 *)malloc(pitch * height);

        u8 *destRow = outBitmap + (height - 1) * pitch;
        u8 *src     = bitmap;
        for (i32 j = 0; j < height; j++)
        {
            u32 *pixel = (u32 *)destRow;
            for (i32 i = 0; i < width; i++)
            {
                u8 gray  = *src++;
                u8 alpha = 0xff;
                *pixel++ = ((alpha << 24) | (gray << 16) | (gray << 8) | (gray << 0));
            }
            destRow -= pitch;
        }

        stbtt_FreeBitmap(bitmap, 0);

        // NOTE: i guess opengl doesn't like one channel textures that aren't power of 2 size? or im missing
        // something maybe revisit this when/if text is in an atlas
        //
        // for (i32 j = 0; j * 2 < height; j++)
        // {
        //     i32 index1 = j * width;
        //     i32 index2 = (height - 1 - j) * width;
        //     for (i32 i = width; i > 0; --i)
        //     {
        //         Swap(u8, bitmap[index1], bitmap[index2]);
        //         ++index1;
        //         ++index2;
        //     }
        // }
        //
        // u8 *outBitmap = bitmap;

        i32 lsb;
        stbtt_GetCodepointHMetrics(&font->fontData.font, c, &advance, &lsb);
        i32 x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(&font->fontData.font, c, node->scale, node->scale, &x0, &y0, &x1, &y1);

        info->width   = width;
        info->height  = height;
        info->rect    = {x0, -y1, x1, -y0};
        info->advance = (i32)(advance * node->scale);
        if (width != 0 && height != 0)
        {
            info->handle = R_AllocateTexture2D(outBitmap, width, height, R_TexFormat_RGBA8);
        }
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

    Font *f = GetFont(font);
    // TODO: properly set up nil so I don't have to have this conditional
    if (!IsFontNil(f))
    {
        // Only support ascii characters for now
        for (u64 i = 0; i < str.size; i++)
        {
            u8 c = str.str[i];
            if (c >= 128)
            {
                Assert(!"Character not supported yet");
            }
            F_RasterInfo *info = F_Raster(f, styleNode, Substr8(str, i, i + 1));
            Assert(info);
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
            piece->offset  = info->rect;
            // piece->width   = info->width;
            // piece->height  = info->height;
            piece->advance = info->advance;
        }
    }
    return result;
}
