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
    R_TexFormat_R8,
    R_TexFormat_RGBA8,
    R_TexFormat_SRGB,
    R_TexFormat_Count,
};
struct DebugVertex
{
    V3 pos;
    V4 color;
};

struct RenderVertex
{
    V4 p;
    V3 n;
    V3 color;
};

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
    R_PassType_3D,
    R_PassType_StaticMesh,
    R_PassType_SkinnedMesh,
    R_PassType_UI,
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

struct R_Batch2DGroup
{
    R_BatchList batchList;
};

struct R_Batch2DGroupNode
{
    R_Batch2DGroup group;
    R_Batch2DGroupNode *next;
};

struct R_PassUI
{
    R_BatchList batchList;
    // R_Batch2DGroupNode *first;
    // R_Batch2DGroupNode *last;
};

struct R_Pass3D
{
    R_Batch3DGroup *groups;
    u32 numGroups;
};

struct R_SkinnedMeshParams
{
    Mat4 transform;
    AS_Handle model;
    Mat4 *skinningMatrices;
    u32 skinningMatricesCount;
};

struct R_SkinnedMeshParamsNode
{
    R_SkinnedMeshParams val;
    R_SkinnedMeshParamsNode *next;
};

struct R_SkinnedMeshParamsList
{
    R_SkinnedMeshParamsNode *first;
    R_SkinnedMeshParamsNode *last;
};

struct R_PassSkinnedMesh
{
    R_SkinnedMeshParamsList list;
};

struct R_StaticMeshParams
{
    Mat4 transform;
    AS_Handle mesh;
};

struct R_StaticMeshParamsNode
{
    R_StaticMeshParams val;
    R_StaticMeshParamsNode *next;
};

struct R_StaticMeshParamsList
{
    R_StaticMeshParamsNode *first;
    R_StaticMeshParamsNode *last;
};

struct R_PassStaticMesh
{
    R_StaticMeshParamsList list;
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

struct RenderState
{
    Camera camera;
    Mat4 transform;
    i32 width;
    i32 height;

    R_Pass passes[R_PassType_Count];

    AS_CacheState *as_state;
};

enum R_BufferType
{
    R_BufferType_Vertex,
    R_BufferType_Index,
};

//////////////////////////////
// Instance information
//
struct R_LineInst
{
    DebugVertex v[2];
};

struct R_PrimitiveInst
{
    V4 color;
    Mat4 transform;
};

struct R_TexAddress
{
    u32 hashIndex;
    f32 slice;
};

struct R_RectInst
{
    V2 pos;
    V2 scale;
    R_Handle handle;
};

#define R_ALLOCATE_TEXTURE_2D(name) R_Handle name(void *data, i32 width, i32 height, R_TexFormat format)
typedef R_ALLOCATE_TEXTURE_2D(r_allocate_texture_2D);
#define R_DELETE_TEXTURE_2D(name) void name(R_Handle handle)
typedef R_DELETE_TEXTURE_2D(r_delete_texture_2D);

#define R_ALLOCATE_BUFFER(name) R_Handle name(R_BufferType type, void *data, u64 size)
typedef R_ALLOCATE_BUFFER(r_allocate_buffer);

// R_ALLOCATE_TEXTURE_2D(R_AllocateTexture2D);
// R_DELETE_TEXTURE_2D(R_DeleteTexture2D);
// R_ALLOCATE_BUFFER(R_AllocateBuffer);

internal void D_PushModel(AS_Handle loadedModel, Mat4 transform, Mat4 *skinningMatrices,
                          u32 skinningMatricesCount);
inline R_Pass *R_GetPassFromKind(R_PassType type);
internal u8 *R_BatchListPush(R_BatchList *list, u32 instCap);

inline b8 R_HandleMatch(R_Handle a, R_Handle b)
{
    b8 result = (a.u64[0] == b.u64[0] && a.u64[1] == b.u64[1]);
    return result;
}

inline R_Handle R_HandleZero()
{
    R_Handle handle = {};
    return handle;
}

#endif
