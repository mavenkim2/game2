/* TODO: this is what I'm thinking
 * everything is technically a "mesh". instead of storing data as interleaved in memory however,
 * have arrays of positions, colors, bones, etc etc. thus, these arrays can be null and optionally
 * remain unloaded.
 *
 * also find some way to not have to keep doing openGL->glGetUniformLocation followed by setting the uniform.
 * glGetActiveAttrib seems interesting? that way a shader can just be one struct
 *
 * speaking of which, it seems like the material used determines the shader used. or maybe I can just go for the
 * uber shader??? or maybe you sort by shader type. who knows.
 *
 * get rid of render group
 *
 * general flow:
 * begin render frame
 * push meshes/materials/texxures onto render commands list (in game update and render)
 * end frame draws everything
 *
 * when meshes are loaded though, textures are also loaded (eventually async). when they are finished
 * loading, they are sent to the gpu then freed
 */

struct RenderVertex
{
    V4 p;
    V3 n;
    V3 color;
};

struct RenderCommand
{
    Mesh *mesh;
    Mat4 transform;

    Mat4 *finalBoneTransforms;
};

struct RenderState
{
    Camera camera;
    Mat4 transform;
    i32 width;
    i32 height;

    // TODO: switch to push buffer w/ headers instead of array, to allow for different types of 
    // commands
    // u8 commands[65536];
    ArrayDef(RenderCommand) commands;
};

// struct RenderGroup
// {
//     u32 quadCount;
//
//     u32 vertexCount;
//     u32 maxVertexCount;
//     RenderVertex *vertexArray;
//
//     u32 indexCount;
//     u32 maxIndexCount;
//     u16 *indexArray;
//
//     Model model;
//
//     Texture textures[MAX_TEXTURES];
//     u32 textureCount;
//     Mat4 *finalTransforms;
// };

// internal RenderGroup BeginRenderGroup(OpenGL* openGL);
// internal void PushQuad(RenderGroup *group, V3 p0, V3 p1, V3 p2, V3 p3, V3 n, V4 color);
// internal void PushCube(RenderGroup *group, V3 pos, V3 size, V4 color);
internal void PushTexture(Texture *texture, Mesh* mesh, void *contents);
internal void PushMesh(RenderState *state, Mesh *mesh, Mat4 *finalTransforms);
