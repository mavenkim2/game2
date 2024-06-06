#ifndef ASSET_H
#define ASSET_H

#include "mkCrack.h"
#ifdef LSP_INCLUDE
#include "mkCommon.h"
#include "mkMath.h"
#include "mkString.h"
#endif

#define MAX_MATRICES_PER_VERTEX 4

union AS_Handle
{
    i64 i64[1];
    i32 i32[2];
};

enum TextureType
{
    TextureType_Diffuse,
    TextureType_Normal,
    TextureType_MR,
    TextureType_Height,
    TextureType_Count,
};

struct Heightmap
{
    VC_Handle vertexHandle;
    VC_Handle indexHandle;
    i32 width;
    i32 height;
};

struct Iter
{
    u8 *cursor;
    u8 *end;
};

enum OBJLineType
{
    OBJ_Vertex,
    OBJ_Normal,
    OBJ_Texture,
    OBJ_Face,
    OBJ_Invalid,
};

struct StaticVertex
{
    V3 position;
    V3 normal;
    V2 uv;
    V3 tangent;
};

struct MeshVertex
{
    V3 position;
    V3 normal;
    V2 uv;
    V3 tangent;

    u32 boneIds[MAX_MATRICES_PER_VERTEX];
    f32 boneWeights[MAX_MATRICES_PER_VERTEX];
};

struct LoadedSkeleton
{
    u32 count;
    string *names;
    i32 *parents;
    Mat4 *inverseBindPoses;
    Mat4 *transformsToParent;
};

struct AnimationTransform
{
    V3 translation;
    Quat rotation;
    V3 scale;
};

inline AnimationTransform Lerp(AnimationTransform t1, AnimationTransform t2, f32 t)
{
    AnimationTransform result;
    result.translation = Lerp(t1.translation, t2.translation, t);
    // NEIGHBORHOOD
    if (Dot(t1.rotation, t2.rotation) < 0)
    {
        result.rotation = Nlerp(t1.rotation, -t2.rotation, t);
    }
    else
    {
        result.rotation = Nlerp(t1.rotation, t2.rotation, t);
    }
    result.scale = Lerp(t1.scale, t2.scale, t);
    return result;
}

struct MeshNodeInfo
{
    string name;
    string parentName;
    b32 hasParent;
    Mat4 transformToParent;
};

struct MeshNodeInfoArray
{
    MeshNodeInfo *items;
    u32 count;
    u32 cap;
};

struct AnimationPosition
{
    V3 position;
    f32 time;
};

struct AnimationScale
{
    V3 scale;
    f32 time;
};

struct AnimationRotation
{
    // Quat rotation;
    u16 rotation[4];
    f32 time;
};

struct BoneChannel
{
    string name;
    AnimationPosition *positions;
    AnimationScale *scales;
    AnimationRotation *rotations;

    u32 numPositionKeys;
    u32 numScalingKeys;
    u32 numRotationKeys;
};

struct KeyframedAnimation
{
    BoneChannel *boneChannels;
    u32 numNodes;

    f32 duration;
};

struct AnimationPlayer
{
    AS_Handle anim;
    KeyframedAnimation *currentAnimation;
    f32 currentTime;
    f32 duration;

    u32 currentPositionKey[300];
    u32 currentScaleKey[300];
    u32 currentRotationKey[300];

    // u32 *currentPositionKey;
    // u32 *currentScaleKey;
    // u32 *currentRotationKey;

    b32 isLooping;
    b8 loaded;
};

struct Mesh
{
    MeshFlags flags;

    V3 *positions;
    V3 *normals;
    V3 *tangents;
    V2 *uvs;
    UV4 *boneIds;
    V4 *boneWeights;
    u32 *indices;

    struct MeshSubset
    {
        u32 indexStart;
        u32 indexCount;
        MaterialHandle materialHandle;
    };
    MeshSubset *subsets;
    u32 numSubsets;

    Rect3 bounds;

    i32 meshIndex;
    i32 skinningIndex;
    i32 transformIndex;
    i32 aabbIndex; // valid for one frame only

    u32 vertexCount;
    u32 indexCount;

    graphics::GPUBuffer buffer;
    graphics::GPUBuffer streamBuffer; // for skinning
    struct BufferView
    {
        u64 offset        = ~0ull;
        u64 size          = 0ull;
        i32 srvIndex      = -1;
        i32 srvDescriptor = -1;
        i32 uavIndex      = -1;
        i32 uavDescriptor = -1;

        b32 IsValid()
        {
            return offset != ~0ull;
        }
    };
    BufferView indexView;
    BufferView vertexPosView;
    BufferView vertexNorView;
    BufferView vertexTanView;
    BufferView vertexUvView;
    BufferView vertexBoneIdView;
    BufferView vertexBoneWeightView;
    BufferView soPosView;
    BufferView soNorView;
    BufferView soTanView;

    inline b32 IsValid();
    inline b32 IsRenderable();
};

struct LoadedModel
{
    Entity rootEntity;
    i32 transformIndex;

    AS_Handle skeleton;
    // Mesh *meshes;
    u32 numMeshes;
    Rect3 bounds;
};

// NOTE: Temporary hash
struct FaceVertex
{
    V3I32 indices;
    u16 index;

    FaceVertex *nextInHash;
};

#pragma pack(push, 1)
struct TGAHeader
{
    u8 idLength;
    u8 colorMapType;
    u8 imageType;
    u16 colorMapOrigin;
    u16 colorMapLength;
    u8 colorMapEntrySize;
    u16 xOrigin;
    u16 yOrigin;
    u16 width;
    u16 height;
    u8 bitsPerPixel;
    u8 imageDescriptor;
};
#pragma pack(pop)

struct TGAResult
{
    u8 *contents;
    u32 width;
    u32 height;
};

//////////////////////////////
// Function
//
inline u16 CompressRotationChannel(f32 q)
{
    u16 result = (u16)CompressFloat(q, -1.f, 1.f, 16u);
    return result;
}

inline f32 DecompressRotationChannel(u16 q)
{
    f32 result = DecompressFloat((u32)q, -1.f, 1.f, 16u);
    return result;
}
internal Heightmap CreateHeightmap(string filename);

//////////////////////////////
// DDS
//

enum PixelFormatFlagBits
{
    PixelFormatFlagBits_AlphaPixels = 0x1,
    PixelFormatFLagBits_Alpha       = 0x2,
    PixelFormatFlagBits_FourCC      = 0x4,
    PixelFormatFlagBits_RGB         = 0x40,
    PixelFormatFlagBits_YUV         = 0x200,
    PixelFormatFlagBits_Luminance   = 0x20000,
};

enum HeaderFlagBits
{
    HeaderFlagBits_Caps   = 0x00000001,
    HeaderFlagBits_Height = 0x00000002,
    HeaderFlagBits_Width  = 0x00000004,
    HeaderFlagBits_Pitch  = 0x00000008,

    HeaderFlagBits_PixelFormat = 0x00001000,
    HeaderFlagBits_Mipmap      = 0x00020000,
    HeaderFlagBits_LinearSize  = 0x00080000,
    HeaderFlagBits_Depth       = 0x00800000,
};

enum DXGI_Format
{
    DXGI_FORMAT_UNKNOWN                    = 0,
    DXGI_FORMAT_R32G32B32A32_TYPELESS      = 1,
    DXGI_FORMAT_R32G32B32A32_FLOAT         = 2,
    DXGI_FORMAT_R32G32B32A32_UINT          = 3,
    DXGI_FORMAT_R32G32B32A32_SINT          = 4,
    DXGI_FORMAT_R32G32B32_TYPELESS         = 5,
    DXGI_FORMAT_R32G32B32_FLOAT            = 6,
    DXGI_FORMAT_R32G32B32_UINT             = 7,
    DXGI_FORMAT_R32G32B32_SINT             = 8,
    DXGI_FORMAT_R16G16B16A16_TYPELESS      = 9,
    DXGI_FORMAT_R16G16B16A16_FLOAT         = 10,
    DXGI_FORMAT_R16G16B16A16_UNORM         = 11,
    DXGI_FORMAT_R16G16B16A16_UINT          = 12,
    DXGI_FORMAT_R16G16B16A16_SNORM         = 13,
    DXGI_FORMAT_R16G16B16A16_SINT          = 14,
    DXGI_FORMAT_R32G32_TYPELESS            = 15,
    DXGI_FORMAT_R32G32_FLOAT               = 16,
    DXGI_FORMAT_R32G32_UINT                = 17,
    DXGI_FORMAT_R32G32_SINT                = 18,
    DXGI_FORMAT_R32G8X24_TYPELESS          = 19,
    DXGI_FORMAT_D32_FLOAT_S8X24_UINT       = 20,
    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS   = 21,
    DXGI_FORMAT_X32_TYPELESS_G8X24_UINT    = 22,
    DXGI_FORMAT_R10G10B10A2_TYPELESS       = 23,
    DXGI_FORMAT_R10G10B10A2_UNORM          = 24,
    DXGI_FORMAT_R10G10B10A2_UINT           = 25,
    DXGI_FORMAT_R11G11B10_FLOAT            = 26,
    DXGI_FORMAT_R8G8B8A8_TYPELESS          = 27,
    DXGI_FORMAT_R8G8B8A8_UNORM             = 28,
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB        = 29,
    DXGI_FORMAT_R8G8B8A8_UINT              = 30,
    DXGI_FORMAT_R8G8B8A8_SNORM             = 31,
    DXGI_FORMAT_R8G8B8A8_SINT              = 32,
    DXGI_FORMAT_R16G16_TYPELESS            = 33,
    DXGI_FORMAT_R16G16_FLOAT               = 34,
    DXGI_FORMAT_R16G16_UNORM               = 35,
    DXGI_FORMAT_R16G16_UINT                = 36,
    DXGI_FORMAT_R16G16_SNORM               = 37,
    DXGI_FORMAT_R16G16_SINT                = 38,
    DXGI_FORMAT_R32_TYPELESS               = 39,
    DXGI_FORMAT_D32_FLOAT                  = 40,
    DXGI_FORMAT_R32_FLOAT                  = 41,
    DXGI_FORMAT_R32_UINT                   = 42,
    DXGI_FORMAT_R32_SINT                   = 43,
    DXGI_FORMAT_R24G8_TYPELESS             = 44,
    DXGI_FORMAT_D24_UNORM_S8_UINT          = 45,
    DXGI_FORMAT_R24_UNORM_X8_TYPELESS      = 46,
    DXGI_FORMAT_X24_TYPELESS_G8_UINT       = 47,
    DXGI_FORMAT_R8G8_TYPELESS              = 48,
    DXGI_FORMAT_R8G8_UNORM                 = 49,
    DXGI_FORMAT_R8G8_UINT                  = 50,
    DXGI_FORMAT_R8G8_SNORM                 = 51,
    DXGI_FORMAT_R8G8_SINT                  = 52,
    DXGI_FORMAT_R16_TYPELESS               = 53,
    DXGI_FORMAT_R16_FLOAT                  = 54,
    DXGI_FORMAT_D16_UNORM                  = 55,
    DXGI_FORMAT_R16_UNORM                  = 56,
    DXGI_FORMAT_R16_UINT                   = 57,
    DXGI_FORMAT_R16_SNORM                  = 58,
    DXGI_FORMAT_R16_SINT                   = 59,
    DXGI_FORMAT_R8_TYPELESS                = 60,
    DXGI_FORMAT_R8_UNORM                   = 61,
    DXGI_FORMAT_R8_UINT                    = 62,
    DXGI_FORMAT_R8_SNORM                   = 63,
    DXGI_FORMAT_R8_SINT                    = 64,
    DXGI_FORMAT_A8_UNORM                   = 65,
    DXGI_FORMAT_R1_UNORM                   = 66,
    DXGI_FORMAT_R9G9B9E5_SHAREDEXP         = 67,
    DXGI_FORMAT_R8G8_B8G8_UNORM            = 68,
    DXGI_FORMAT_G8R8_G8B8_UNORM            = 69,
    DXGI_FORMAT_BC1_TYPELESS               = 70,
    DXGI_FORMAT_BC1_UNORM                  = 71,
    DXGI_FORMAT_BC1_UNORM_SRGB             = 72,
    DXGI_FORMAT_BC2_TYPELESS               = 73,
    DXGI_FORMAT_BC2_UNORM                  = 74,
    DXGI_FORMAT_BC2_UNORM_SRGB             = 75,
    DXGI_FORMAT_BC3_TYPELESS               = 76,
    DXGI_FORMAT_BC3_UNORM                  = 77,
    DXGI_FORMAT_BC3_UNORM_SRGB             = 78,
    DXGI_FORMAT_BC4_TYPELESS               = 79,
    DXGI_FORMAT_BC4_UNORM                  = 80,
    DXGI_FORMAT_BC4_SNORM                  = 81,
    DXGI_FORMAT_BC5_TYPELESS               = 82,
    DXGI_FORMAT_BC5_UNORM                  = 83,
    DXGI_FORMAT_BC5_SNORM                  = 84,
    DXGI_FORMAT_B5G6R5_UNORM               = 85,
    DXGI_FORMAT_B5G5R5A1_UNORM             = 86,
    DXGI_FORMAT_B8G8R8A8_UNORM             = 87,
    DXGI_FORMAT_B8G8R8X8_UNORM             = 88,
    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM = 89,
    DXGI_FORMAT_B8G8R8A8_TYPELESS          = 90,
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB        = 91,
    DXGI_FORMAT_B8G8R8X8_TYPELESS          = 92,
    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB        = 93,
    DXGI_FORMAT_BC6H_TYPELESS              = 94,
    DXGI_FORMAT_BC6H_UF16                  = 95,
    DXGI_FORMAT_BC6H_SF16                  = 96,
    DXGI_FORMAT_BC7_TYPELESS               = 97,
    DXGI_FORMAT_BC7_UNORM                  = 98,
    DXGI_FORMAT_BC7_UNORM_SRGB             = 99,
    DXGI_FORMAT_AYUV                       = 100,
    DXGI_FORMAT_Y410                       = 101,
    DXGI_FORMAT_Y416                       = 102,
    DXGI_FORMAT_NV12                       = 103,
    DXGI_FORMAT_P010                       = 104,
    DXGI_FORMAT_P016                       = 105,
    DXGI_FORMAT_420_OPAQUE                 = 106,
    DXGI_FORMAT_YUY2                       = 107,
    DXGI_FORMAT_Y210                       = 108,
    DXGI_FORMAT_Y216                       = 109,
    DXGI_FORMAT_NV11                       = 110,
    DXGI_FORMAT_AI44                       = 111,
    DXGI_FORMAT_IA44                       = 112,
    DXGI_FORMAT_P8                         = 113,
    DXGI_FORMAT_A8P8                       = 114,
    DXGI_FORMAT_B4G4R4A4_UNORM             = 115,
    DXGI_FORMAT_P208                       = 130,
    DXGI_FORMAT_V208                       = 131,
    DXGI_FORMAT_V408                       = 132,
    DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE,
    DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE,
    DXGI_FORMAT_FORCE_UINT = 0xffffffff
};

enum ResourceDimension
{
    ResourceDimension_Unknown   = 0,
    ResourceDimension_Buffer    = 1,
    ResourceDimension_Texture1D = 2,
    ResourceDimension_Texture2D = 3,
    ResourceDimension_Texture3D = 4
};

enum DDSCaps
{
    DDSCaps_Complex = 0x8,
    DDSCaps_Mipmap  = 0x400000,
    DDSCaps_Texture = 0x1000, // required
};

struct PixelFormat
{
    u32 size;
    u32 flags;
    u32 fourCC;
    u32 rgbBitCount;
    u32 rBitMask;
    u32 gBitMask;
    u32 bBitMask;
    u32 aBitMask;
};
struct DDSHeader
{
    u32 size;
    u32 flags;
    u32 height;
    u32 width;
    u32 pitchOrLinearSize;
    u32 depth;
    u32 mipMapCount;
    u32 reserved1[11];
    PixelFormat format;
    u32 caps;
    u32 caps2;
    u32 caps3;
    u32 caps4;
    u32 reserved2;
};

struct DDSHeaderDXT10
{
    DXGI_Format dxgiFormat;
    ResourceDimension resourceDimension;
    u32 miscFlag;
    u32 arraySize;
    u32 miscFlags2;
};

struct DDSFile
{
    u32 magic;
    DDSHeader header;
};

#define MakeFourCC(a, b, c, d) (((d) << 24) | ((c) << 16) | ((b) << 8) | ((a) << 0))

#endif
