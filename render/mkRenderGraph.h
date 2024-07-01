#ifndef RENDER_GRAPH_H
#define RENDER_GRAPH_H

namespace rendergraph
{

using ExecuteFunction = std::function<void(graphics::CommandList cmd)>;

enum class ResourceType : u32
{
    StructuredBuffer,
    RWStructuredBuffer,

    Texture2D,
    Texture2DArray,
    RWTexture2D,

    // Buffer,
    // ByteAddressBuffer,
};

inline string ConvertResourceTypeToName(ResourceType type)
{
    switch (type)
    {
        case ResourceType::StructuredBuffer: return Str8Lit("ResourceType::StructuredBuffer");
        case ResourceType::RWStructuredBuffer: return Str8Lit("ResourceType::RWStructuredBuffer");
        case ResourceType::Texture2D: return Str8Lit("ResourceType::Texture2D");
        case ResourceType::Texture2DArray: return Str8Lit("ResourceType::Texture2DArray");
        case ResourceType::RWTexture2D: return Str8Lit("ResourceType::RWTexture2D");
        default: Assert(0); return Str8Lit("Invalid");
    }
}

enum class ResourceUsage
{
    CBV, // constant buffer view
    SRV, // shader resource view
    UAV, // unordered access view
    SAM, // sampler
};

struct PassResource
{
    u32 sid;
    ResourceType type;
    i32 binding;
};

struct BufferDesc
{
    u32 size;
};

struct Buffer
{
    u32 size;
    ResourceType type;
    i32 binding;
};

struct PassResources
{
    PassResource *resources;
    u8 resourceCount;

    // [[vk::push_constant]] InstanceCullPushConstants push;
    // StructuredBuffer<GPUView> views : register(t1);
    //
    // RWStructuredBuffer<DispatchIndirect> dispatchIndirectBuffer : register(u0);
    // RWStructuredBuffer<MeshChunk> meshChunks : register(u1);
    // RWStructuredBuffer<uint> occludedInstances : register(u2);
    void CreateBuffer(Buffer *buffer, string name, u32 size = 0)
    {
    }
    // how do I solve the problem of needing a resource to be used by multiple passes?
    // 1. use string ids
};

struct Pass
{
    void *parameters;
    ExecuteFunction func;
};

struct RenderGraph
{
    Arena *arena;
    list<Pass> passes;
    u32 numPasses;
    void AddPass(ExecuteFunction func)
    {
    }
};
} // namespace rendergraph

#endif
