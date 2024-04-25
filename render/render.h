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
    R_TexFormat_RG8,
    R_TexFormat_RGB8,
    R_TexFormat_RGBA8,
    R_TexFormat_SRGB8,
    R_TexFormat_SRGBA8,
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

//////////////////////////////
// Lights
//

enum LightType
{
    LightType_Directional,
    LightType_Point,
};
struct Light
{
    LightType type;
    V3 dir;
    // V3 origin;
    // V3 globalOrigin;
};

//////////////////////////////
// Commands
//

const i32 cFrameBufferSize    = megabytes(64);
const i32 cFrameDataNumFrames = 2;

enum R_CommandType
{
    R_CommandType_Null,
    R_CommandType_Heightmap,
};

struct R_Command
{
    R_CommandType type;
    R_CommandType *next;
};

struct R_CommandHeightMap
{
    R_CommandType type;
    R_CommandType *next;

    Heightmap heightmap;
};

struct R_FrameData
{
    u8 *memory;
    i32 volatile allocated;
    R_Command *head;
    R_Command *tail;
};

struct R_FrameState
{
    Arena *arena;
    // R_TempMemoryNode *tempMemNodes;

    R_FrameData frameData[cFrameDataNumFrames];
    R_FrameData *currentFrameData;
    u32 currentFrame;
};

struct AS_CacheState;

enum R_PassType
{
    R_PassType_3D,
    R_PassType_Mesh,
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

struct D_Surface
{
    VC_Handle vertexBuffer;
    VC_Handle indexBuffer;
    Material *material;
};

struct R_MeshParams
{
    Mat4 transform;
    D_Surface *surfaces;
    VC_Handle jointHandle;
    Rect3 mBounds; // world space bounds
    u32 numSurfaces;
    b32 mIsDirectlyVisible;
};

struct R_MeshParamsNode
{
    R_MeshParams val;
    R_MeshParamsNode *next;
};

struct R_MeshParamsList
{
    R_MeshParamsNode *first;
    R_MeshParamsNode *last;
    u32 mTotalSurfaceCount;
};

// used for view testing
struct ViewLight
{
    LightType type;
    V3 origin;
    V3 globalOrigin;
    V3 dir;
    ViewLight *next;
};

struct R_PassMesh
{
    R_MeshParamsList list;
    ViewLight *viewLight;
};

struct FrustumCorners
{
    V3 corners[8];
};

// generated per frame
struct RenderView
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
        R_PassMesh *passMesh;
    };
};

//////////////////////////////
// d state
//

// Retained information
struct D_State
{
    Arena *arena;
    u64 frameStartPos;
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

// internal R_Handle R_AllocateTemp(u64 size, void **out);
// internal void R_FreeTemp(R_Handle temp);

//////////////////////////////////////////////////////////////////////////////////////////
// End Section
//

//////////////////////////////
// Renderer frontend
//
internal void RenderFrameDataInit();
internal void R_SwapFrameData();
internal void *R_FrameAlloc(const i32 inSize);
internal void *R_CreateCommand(i32 size);
internal void R_EndFrame();

//////////////////////////////
// Renderer backend
//

internal void DrawBox(V3 offset, V3 scale, V4 color);
internal void DrawBox(Rect3 rect, V4 color);
internal void D_PushModel(AS_Handle loadedModel, Mat4 transform, Mat4 &mvp, Mat4 *skinningMatrices,
                          u32 skinningMatricesCount);
internal void D_PushTextF(AS_Handle font, V2 startPos, f32 size, char *fmt, ...);
inline R_Pass *R_GetPassFromKind(R_PassType type);
internal u8 *R_BatchListPush(R_BatchList *list, u32 instCap);

//////////////////////////////
// Shadow mapping
//
const i32 cNumSplits     = 3;
const i32 cNumCascades   = cNumSplits + 1;
const i32 cShadowMapSize = 1024;
internal void R_CascadedShadowMap(const ViewLight *inLight, Mat4 *outLightViewProjectionMatrices,
                                  f32 *outCascadeDistances);

//////////////////////////////
// Prepare GPU Cmd Buffers to submit
//
struct R_IndirectCmd
{
    u32 mCount;
    u32 mInstanceCount;
    u32 mFirstIndex;
    u32 mBaseVertex;
    u32 mBaseInstance;
};

// TODO: ?
struct R_MeshPerDrawParams
{
    Mat4 mTransform;
    u64 mIndex[TextureType_Count];
    u32 mSlice[TextureType_Count];
    i32 mJointOffset;
    i32 mIsPBR;
    i32 _pad[2];
};

struct R_MeshPreparedDrawParams
{
    R_IndirectCmd *mIndirectBuffers;
    R_MeshPerDrawParams *mPerMeshDrawParams;
};

// prepare to submit to gpu
internal R_MeshPreparedDrawParams *D_PrepareMeshes();
internal void R_SetupViewFrustum();
internal void R_CullModelsToLight(ViewLight *light);

//////////////////////////////
// Handles
//
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

// Per frame information
struct RenderState
{
    Camera camera;
    Mat4 viewMatrix;
    Mat4 transform;
    i32 width;
    i32 height;

    f32 fov;
    f32 aspectRatio;
    f32 nearZ;
    f32 farZ;

    R_Pass passes[R_PassType_Count];
    R_Command *head;

    // frustum corners
    FrustumCorners mFrustumCorners;
    Mat4 inverseViewProjection; // convert from clip space to world space

    // Vertex cache
    VertexCacheState vertexCache;

    // Frame cache
    R_FrameState renderFrameState;

    // Shadow maps
    R_MeshPreparedDrawParams *drawParams;
    Mat4 shadowMapMatrices[cNumCascades];
    f32 cascadeDistances[cNumSplits];
};

#endif
