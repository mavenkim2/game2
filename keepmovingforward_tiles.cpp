#include "keepmovingforward_tiles.h"

static void SetTileValue(uint32 *tileMap, uint32 tileX, uint32 tileY, uint32 tileMapXCount, uint32 tileMapYCount, uint32 value)
{
    Assert(tileX >= 0 && tileY >= 0 && tileX < tileMapXCount && tileY < tileMapYCount);
    tileMap[tileY * tileMapXCount + tileX] = value;
}

static void GenerateLevel(uint32 *tileMap, uint32 tileMapXCount, uint32 tileMapYCount)
{
    for (uint32 y = 0; y < tileMapYCount; y++)
    {
        for (uint32 x = 0; x < tileMapXCount; x++)
        {
            uint32 tileValue = 0;
            if (x == 0 || x == tileMapXCount - 1)
            {
                tileValue = 1;
            }
            if (y == 0 || y == tileMapYCount - 1)
            {
                tileValue = 2;
            }
            SetTileValue(tileMap, x, y, tileMapXCount, tileMapYCount, tileValue);
        }
    }
}

static uint32 GetTileValue(uint32 *tileMap, uint32 tileX, uint32 tileY, uint32 tileMapXCount, uint32 tileMapYCount)
{
    Assert(tileX >= 0 && tileY >= 0 && tileX < tileMapXCount && tileY < tileMapYCount);
    uint32 result = tileMap[tileY * tileMapXCount + tileX];
    return result;
}

static bool CheckTileHasCollision(uint32 *tileMap, uint32 tileX, uint32 tileY, uint32 tileMapXCount, uint32 tileMapYCount)
{
    bool result = false;
    uint32 tileValue = GetTileValue(tileMap, tileX, tileY, tileMapXCount, tileMapYCount);
    if (tileValue == 1 || tileValue == 2)
    {
        result = true;
    }
    return result;
}
