#ifndef KEEPMOVINGFORWARD_RENDERER_H
struct RenderVertex {
    V4 p;
    V3 n;
    V3 color;
};


struct RenderGroup {
    u32 quadCount;

    u32 vertexCount;
    u32 maxVertexCount;
    RenderVertex* vertexArray;

    u32 indexCount;
    u32 maxIndexCount;
    u16* indexArray;

    Model model;

    Texture texture;
    Mat4* finalTransforms;
};


// internal RenderGroup BeginRenderGroup(OpenGL* openGL);
internal void PushQuad(RenderGroup* group, V3 p0, V3 p1, V3 p2, V3 p3, V3 n, V4 color);
internal void PushCube(RenderGroup* group, V3 pos, V3 size, V4 color);
internal void PushTexture(RenderGroup *group, Texture texture);

#define KEEPMOVINGFORWARD_RENDERER_H
#endif
