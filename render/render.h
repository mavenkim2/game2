#ifndef RENDER_H
#define RENDER_H
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
#include "../crack.h"
#ifdef LSP_INCLUDE
#include "../keepmovingforward_common.h"
#include "../keepmovingforward_string.h"
#include "../keepmovingforward_math.h"
#include "../asset.h"
#include "../asset_cache.h"
#endif

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
struct Material;
struct LoadedSkeleton;
struct LoadedModel;

struct RenderCommand
{
    Model *model;
    Mat4 transform;

    Mat4 *finalBoneTransforms;

    Material *materials;
    u32 numMaterials;

    // TODO: not thrilled about these pointers
    LoadedModel *loadedModel;
    LoadedSkeleton *skeleton;
    // R_Handle textureHandles[4];
    // u32 numHandles;
};

struct AS_CacheState;

enum R_PassType
{
    R_PassType_UI,
    R_PassType_3D,
    R_PassType_StaticMesh,
    R_PassType_SkinnedMesh,
    R_PassType_Count,
};

struct R_3DInst
{
    Mat4 transform;
    V4 color;
};

struct R_3DParams
{
    u32 vertexCount;
    u32 indexCount;
};

enum R_Topology
{
    R_Topology_Points,
    R_Topology_Lines,
    R_Topology_Triangles,
    R_Topology_TriangleStrip,
};

enum R_Primitive
{
    R_Primitive_Lines,
    R_Primitive_Points,
    R_Primitive_Cube,
    R_Primitive_Sphere,
    R_Primitive_Count,
};

// Basic 3D Data for instance
struct R_Batch3DParams
{
    V3 *vertices;
    u32 *indices;
    u32 vertexCount;
    u32 indexCount;
    R_Topology topology;
    R_Primitive primType;
};

// Basic 3D Per Instance Data
struct R_Batch
{
    u8 *data;
    u32 byteCount;
    u32 byteCap;
};

struct R_BatchNode
{
    R_Batch val;
    R_BatchNode *next;
};

struct R_BatchList
{
    R_BatchNode *first;
    R_BatchNode *last;

    u32 bytesPerInstance;
    u32 numInstances;
};

struct R_Batch3DGroup
{
    R_Batch3DParams params;
    R_BatchList batchList;
};

struct R_PassUI
{
};

struct R_Pass3D
{
    R_Batch3DGroup *groups;
    u32 numGroups;
};

struct R_PassStaticMesh
{
};

struct R_PassSkinnedMesh
{
};

struct R_Pass
{
    // R_PassType type;
    union
    {
        void *ptr;
        R_PassUI *passUI;
        R_Pass3D *pass3D;
        R_PassStaticMesh *passStatic;
        R_PassSkinnedMesh *passSkinned;
    };
};

struct R_LineInst
{
    DebugVertex v[2];
};

struct R_PrimitiveInst
{
    V4 color;
    Mat4 transform;
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
    R_Pass passes[R_PassType_Count];

    // DebugRenderer debugRenderer;
    AS_CacheState *as_state;
};

internal void PushModel(Model *model, Mat4 *finalTransforms);

#endif
