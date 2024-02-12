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
    Model *model;
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

    DebugRenderer debugRenderer;
};

internal void PushTexture(Texture texture, Model *model);
internal void PushModel(RenderState *state, Model *model, Mat4 *finalTransforms);
