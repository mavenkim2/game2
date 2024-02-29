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
 * general flow:
 * begin render frame
 * push meshes/materials/texxures onto render commands list (in game update and render)
 * end frame draws everything
 *
 * when meshes are loaded though, textures are also loaded (eventually async). when they are finished
 * loading, they are sent to the gpu then freed
 */

#define MAX_COMMANDS       10
#define MAX_DEBUG_VERTICES 1000

struct Model;

enum R_TexFormat
{
    R_TexFormat_Nil,
    R_TexFormat_RGBA8,
    R_TexFormat_SRGB,
    R_TexFormat_Count,
};
struct DebugVertex
{
    V3 pos;
    V4 color;
};

struct Primitive
{
    u32 vertexCount;
    u32 indexCount;
    // For instancing
    Mat4 *transforms = 0;
    V4 *colors       = 0;
};

struct DebugRenderer
{
    Array(DebugVertex) lines;
    Array(DebugVertex) points;

    Array(V3) indexLines;
    Array(u32) indices;

    Primitive *primitives = 0;

    u32 vbo;
    u32 ebo;

    u32 instanceVao;
    u32 instanceVbo;
    u32 instanceVbo2;
};
struct RenderVertex
{
    V4 p;
    V3 n;
    V3 color;
};

typedef u32 T_Handle;

struct RenderCommand
{
    Model *model;
    Mat4 transform;

    Mat4 *finalBoneTransforms;

    R_Handle textureHandles[4];
    u32 numHandles;
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
    Array(RenderCommand) commands;

    DebugRenderer debugRenderer;
    struct AssetState *assetState;
};

internal void PushTexture(Model *model, u32 id);
internal void PushModel(RenderState *state, Model *model, Mat4 *finalTransforms);
