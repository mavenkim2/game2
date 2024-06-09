//////////////////////////////
// Types
//
typedef u32 Entity;

//////////////////////////////
// Handles
//
union MeshHandle
{
    u64 u64[2];
    u32 u32[4];
};

union MaterialHandle
{
    u64 u64[2];
    u32 u32[4];
};

union TransformHandle
{
    u64 u64[2];
    u32 u32[4];
};

union HierarchyHandle
{
    u64 u64[2];
    u32 u32[4];
};

union SkeletonHandle
{
    u64 u64[2];
    u32 u32[4];
};

typedef u64 R_BufferHandle;
typedef u64 VC_Handle;
union R_Handle
{
    u64 u64[2];
    u32 u32[4];
};

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

//////////////////////////////
// Enums
//
typedef u32 MaterialFlag;
enum
{
    MaterialFlag_Valid = 1 << 0,
};

typedef u32 MeshFlags;
enum
{
    MeshFlags_Valid   = 1 << 0,
    MeshFlags_Skinned = 1 << 1,
    MeshFlags_Uvs     = 1 << 2,
};

typedef u32 HierarchyFlag;
enum
{
    HierarchyFlag_Valid = 1 << 0,
};

enum SkeletonFlag : u32
{
    SkeletonFlag_Valid = 1U << 31,
};

inline b32 HasFlags(u32 lhs, u32 rhs)
{
    return (lhs & rhs) == rhs;
}
