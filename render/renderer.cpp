/* internal RenderGroup BeginRenderGroup(OpenGL* openGL) {
    RenderGroup* group = &openGL->group;
    // TODO: if the number is any higher, then u16 index cannot represent
    u32 maxQuadCountPerFrame = 1 << 14;
    group.maxVertexCount = maxQuadCountPerFrame * 4;
    group.maxIndexCount = maxQuadCountPerFrame * 6;
    group.indexCount = 0;
    group.vertexCount = 0;

    return group;
} */

// internal void PushCube(RenderCommand* commands, V3 pos, V3 radius, V4 color)
// {
//     f32 minX = pos.x - radius.x;
//     f32 maxX = pos.x + radius.x;
//     f32 minY = pos.y - radius.y;
//     f32 maxY = pos.y + radius.y;
//     f32 minZ = pos.z - radius.z;
//     f32 maxZ = pos.z + radius.z;
//
//     V3 p0 = {minX, minY, minZ};
//     V3 p1 = {maxX, minY, minZ};
//     V3 p2 = {maxX, maxY, minZ};
//     V3 p3 = {minX, maxY, minZ};
//     V3 p4 = {minX, minY, maxZ};
//     V3 p5 = {maxX, minY, maxZ};
//     V3 p6 = {maxX, maxY, maxZ};
//     V3 p7 = {minX, maxY, maxZ};
//
//     Mesh mesh;
//
//     // NOTE: winding must be counterclockwise if it's a front face
//     //  -X
//     PushQuad(commands, p3, p0, p4, p7, V3{-1, 0, 0}, color);
//     // +X
//     PushQuad(commands, p1, p2, p6, p5, V3{1, 0, 0}, color);
//     // -Y
//     PushQuad(commands, p0, p1, p5, p4, V3{0, -1, 0}, color);
//     // +Y
//     PushQuad(commands, p2, p3, p7, p6, V3{0, 1, 0}, color);
//     // -Z
//     PushQuad(commands, p3, p2, p1, p0, V3{0, 0, -1}, color);
//     // +Z
//     PushQuad(commands, p4, p5, p6, p7, V3{0, 0, 1}, color);
//
// }
//
// internal void PushQuad(RenderCommand *commands, V3 p0, V3 p1, V3 p2, V3 p3, V3 n, V4 color)
// {
//     // NOTE: per quad there is 4 new vertices, 6 indices, but the 6 indices are
//     // 4 different numbers.
//     Mesh
//     u32 vertexIndex = group->vertexCount;
//     u32 indexIndex  = group->indexCount;
//     group->vertexCount += 4;
//     group->indexCount += 6;
//     Assert(group->vertexCount <= group->maxVertexCount);
//     Assert(group->indexCount <= group->maxIndexCount);
//
//     RenderVertex *vertex = group->vertexArray + vertexIndex;
//     u16 *index           = group->indexArray + indexIndex;
//
//     vertex[0].p     = MakeV4(p0, 1.f);
//     vertex[0].color = color.rgb;
//     vertex[0].n     = n;
//
//     vertex[1].p     = MakeV4(p1, 1.f);
//     vertex[1].color = color.rgb;
//     vertex[1].n     = n;
//
//     vertex[2].p     = MakeV4(p2, 1.f);
//     vertex[2].color = color.rgb;
//     vertex[2].n     = n;
//
//     vertex[3].p     = MakeV4(p3, 1.f);
//     vertex[3].color = color.rgb;
//     vertex[3].n     = n;
//
//     // TODO: this will blow out the vertex count fast
//     u16 baseIndex = (u16)vertexIndex;
//     Assert((u32)baseIndex == vertexIndex);
//     index[0] = baseIndex + 0;
//     index[1] = baseIndex + 1;
//     index[2] = baseIndex + 2;
//     index[3] = baseIndex + 0;
//     index[4] = baseIndex + 2;
//     index[5] = baseIndex + 3;
//
//     group->quadCount += 1;
// }

internal void PushTexture(Texture texture, Model *model)
{
    ArrayPush(&model->textures, texture);
}

internal void PushModel(RenderState *state, Model *model, Mat4 *finalTransforms = 0)
{
    RenderCommand command;
    command.model      = model;
    command.transform = Identity();
    if (finalTransforms)
    {
        command.finalBoneTransforms = finalTransforms;
    }
    ArrayPush(&state->commands, command);
}
// internal void PushModel(Model *model)
// {
//     for (u32 i = 0; i < model->meshCount; i++)
//     {
//         PushMesh(&model->meshes[i]);
//     }
// }
