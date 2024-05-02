internal void GenerateLevel(G_State *gameState, i32 tileWidth, i32 tileHeight)
{
    Level *level       = gameState->level;
    level->levelWidth  = tileWidth;
    level->levelHeight = tileHeight;

    for (i32 y = -tileHeight / 2; y < tileHeight / 2; y++)
    {
        for (i32 x = -tileWidth / 2; x < tileWidth / 2; x++)
        {
            Entity *entity = CreateWall(gameState, gameState->level);

            entity->pos  = V3{(f32)x, (f32)y, 0.f};
            entity->size = V3{0.5f, 0.5f, 0.5f};
            if ((x + tileWidth / 2) % 3 == 0)
            {
                entity->color = V4{1.f, 0.f, 0.f, 1.f};
            }
            else if ((x + tileWidth / 2) % 3 == 1)
            {
                entity->color = V4{0.f, 1.f, 0.f, 1.f};
            }
            else
            {
                entity->color = V4{1.f, 0.f, 1.f, 1.f};
            }
        }
    }
}
