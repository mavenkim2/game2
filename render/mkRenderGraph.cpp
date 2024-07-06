#include "mkRenderGraph.h"
#include "../generated/render_graph_resources.h"
#include "../mkCrack.h"
#ifdef LSP_INCLUDE
#include "../render/mkGraphics.h"
#include "../mkList.h"
#endif

namespace rendergraph
{
// TODO: IDEA: create a minimum spanning tree for each queue?
void RenderGraph::Init()
{
    arena        = ArenaAlloc();
    passCount    = 0;
    numResources = 1;
    for (u32 i = 0; i < ArrayLength(passDependencies); i++)
    {
        passDependencies[i].Init();
    }
    // graphics::GPUBufferDesc desc;
    // desc.resourceUsage = graphics::ResourceViewType::
}

// TODO: some way to assert that this doesn't give you junk? like a magic #?
inline PassResource *GetTextureResources(BaseShaderParamType *params)
{
    PassResource *resources = (PassResource *)((u8 *)params + sizeof(BaseShaderParamType));
    return resources;
}

inline PassResource *GetBufferResources(BaseShaderParamType *params)
{
    PassResource *resources = (PassResource *)((u8 *)params + sizeof(BaseShaderParamType) +
                                               sizeof(PassResource) * params->numTextures);
    return resources;
}

inline TextureView *GetTextureViews(BaseShaderParamType *params)
{
    TextureView *views = (TextureView *)((u8 *)params + sizeof(BaseShaderParamType) +
                                         sizeof(PassResource) * params->numResources);
    return views;
}

inline BufferView *GetBufferViews(BaseShaderParamType *params)
{
    BufferView *views = (BufferView *)((u8 *)params + sizeof(BaseShaderParamType) +
                                       sizeof(PassResource) * params->numResources + sizeof(TextureView) * params->numTextures);
    return views;
}

inline void EnumerateShaderBuffers(BaseShaderParamType *baseParams, const BufferEnumerateFunction &func)
{
    BufferView *bufferViews       = GetBufferViews(baseParams);
    PassResource *bufferResources = GetBufferResources(baseParams);
    for (u32 i = 0; i < baseParams->numBuffers; i++)
    {
        func(&bufferResources[i], &bufferViews[i]);
    }
}

inline void EnumerateShaderTextures(BaseShaderParamType *baseParams, const TextureEnumerateFunction &func)
{
    TextureView *textureViews      = GetTextureViews(baseParams);
    PassResource *textureResources = GetTextureResources(baseParams);
    for (u32 i = 0; i < baseParams->numTextures; i++)
    {
        func(&textureResources[i], &textureViews[i]);
    }
}

inline void EnumerateShaderResources(RenderPass *pass, const ResourceEnumerateFunction &func)
{
    EnumerateShaderBuffers(pass->parameters, (const BufferEnumerateFunction)func);
    EnumerateShaderTextures(pass->parameters, (const TextureEnumerateFunction)func);
}

// void RenderGraph::BeginFrame()
// {
//     arenaBeginFramePos = ArenaPos(arena);
// }

template <typename ParameterType>
ParameterType *RenderGraph::AllocParameters()
{
    ParameterType *result = new (PushArray(arena, u8, sizeof(ParameterType))) ParameterType();

    return result;
}

inline PassHandle RenderGraph::AddPass(string passName, void *params, const ExecuteFunction &func)
{
    return AddPassInternal(passName, params, func, PassFlags::None);
}

inline PassHandle RenderGraph::AddPass(string passName, void *params, PassFlags flags, const ExecuteFunction &func)
{
    return AddPassInternal(passName, params, func, flags);
}

PassHandle RenderGraph::AddPassInternal(string passName, void *params, const ExecuteFunction &func, PassFlags flags)
{
    u32 passHash = Hash(passName);

    {
        u32 index = renderPassHashTable.Find(passHash, renderPassStringHashes);
        if (renderPassHashTable.IsValid(index)) return index;
    }

    PassHandle handle = passCount++;

    renderPassHashTable.Add(passHash, handle);
    renderPassStringHashes[handle] = handle;

    BaseShaderParamType *baseParams = (BaseShaderParamType *)params;

    RenderPass *pass = &passes[handle];
    pass->name       = PushStr8Copy(arena, passName);
    pass->flags      = flags;
    pass->parameters = baseParams;
    pass->func       = func;

    bool isCompute  = EnumHasAllFlags(baseParams->flags, ShaderParamFlags::Compute);
    bool isGraphics = EnumHasAllFlags(baseParams->flags, ShaderParamFlags::Graphics);

    u32 numPassDependencies = 0;
    // First pass, calculate the total # of dependencies for the pass
    EnumerateShaderResources(pass, [&](const PassResource *resource, ResourceView *view) {
        Assert(IsValidResourceHandle(view->handle));
        view->type = IsBuffer(resource->objectType) ? ResourceType::Buffer : ResourceType::Texture;
        switch (resource->viewType)
        {
            case ResourceViewType::SRV:
            {
                if (isCompute) view->access = ResourceAccess::ComputeSRV;
                else if (isGraphics) view->access = ResourceAccess::GraphicsSRV;
                else Assert(!"Not implemented");
            }
            break;
            case ResourceViewType::UAV:
            {
                if (isCompute) view->access = ResourceAccess::ComputeUAV;
                else if (isGraphics) view->access = ResourceAccess::GraphicsUAV;
                else Assert(!"Not implemented");
            }
            break;
        }

        // Add dependencies
        RenderGraphResource *graphResource = &resources[view->handle];
        graphResource->numUses++;

        if (!IsValidPassHandle(graphResource->firstPass))
        {
            graphResource->firstPass = handle;
        }
        graphResource->lastPass = handle;

        if (IsValidPassHandle(graphResource->lastPassWriteHandle))
        {
            b8 alreadyAdded = 0;
            for (u32 i = 0; i < passDependencies[handle].Length(); i++)
            {
                if (passDependencies[handle][i] == graphResource->lastPassWriteHandle)
                {
                    alreadyAdded = 1;
                    break;
                }
            }
            if (!alreadyAdded)
            {
                passDependencies[handle].Add(graphResource->lastPassWriteHandle);
            }
        }
        if (resource->viewType == ResourceViewType::UAV)
        {
            graphResource->lastPassWriteHandle = handle;
        }
    });
    return handle;
}

inline b32 CheckBitmap(u32 *bitmap, u32 handle)
{
    return bitmap[handle >> 5] & (1 << (handle & (32 - 1)));
}

inline void AddToBitmap(u32 *bitmap, u32 handle)
{
    bitmap[handle >> 5] |= (1 << (handle & (32 - 1)));
}

inline void ClearInBitmap(u32 *bitmap, u32 handle)
{
    bitmap[handle >> 5] &= ~(1 << (handle & (32 - 1)));
}

void RenderGraph::Compile()
{
    TempArena temp = ScratchStart(0, 0);

    // 1. Cull passes
    PassHandle *cullingStack = PushArray(temp.arena, PassHandle, passCount);
    i32 top                  = 0;
    // TODO: better way of demarcating the 0th resource slot as null/unused
    for (u32 i = NULL_HANDLE + 1; i < numResources; i++)
    {
        RenderGraphResource *resource = &resources[i];
        PassHandle lastWritePass      = resource->lastPassWriteHandle;
        if (IsValidPassHandle(lastWritePass))
        {
            cullingStack[top++] = lastWritePass;
        }
    }
    while (top >= 0)
    {
        PassHandle handle = cullingStack[--top];
        RenderPass *pass  = &passes[handle];
        if (!EnumHasAnyFlags(pass->flags, PassFlags::NotCulled))
        {
            pass->flags |= PassFlags::NotCulled;
            for (u32 dependentIndex = 0; dependentIndex < passDependencies[handle].Length(); dependentIndex++)
            {
                PassHandle dependentPassHandle = passDependencies[handle][dependentIndex];
                cullingStack[top++]            = dependentPassHandle;
            }
        }
    }

    // 2. Create resource descriptions
    for (u32 i = 0; i < numPasses; i++)
    {
        RenderPass *pass = &passes[i];
        if (!EnumHasAnyFlags(pass->flags, PassFlags::NotCulled)) continue;
        EnumerateShaderBuffers(pass->parameters, [&](const PassResource *resource, BufferView *view) {
            RenderGraphResource *rgResource = &resources[view->handle];
            graphics::GPUBuffer *buffer     = (graphics::GPUBuffer *)(&rgResource->resource);
            switch (resource->objectType)
            {
                case HLSLType::StructuredBuffer:
                {
                    buffer->desc.resourceUsage |= ResourceUsage::StorageBufferRead;
                }
                break;
                case HLSLType::RWStructuredBuffer:
                {
                    buffer->desc.resourceUsage |= ResourceUsage::StorageBufferRead;
                }
                break;
                default:
                    Assert("Buffers cannot have this hlsl object type.");
            }
        });
        EnumerateShaderTextures(pass->parameters, [&](const PassResource *resource, TextureView *view) {
            RenderGraphResource *rgResource = &resources[view->handle];
            graphics::Texture *texture      = (graphics::Texture *)(&rgResource->resource);
            switch (resource->objectType)
            {
                case HLSLType::Texture2D:
                case HLSLType::Texture2DArray:
                {
                    // TODO: get rid of this initial usage/future usage stuff
                    texture->desc.initialUsage |= ResourceUsage::SampledImage;
                }
                break;
                case HLSLType::RWTexture2D:
                {
                    texture->desc.initialUsage |= ResourceUsage::StorageImage;
                }
                break;
                default:
                    Assert("Textures cannot have this hlsl object type.");
            }
        });
    }

    struct BarrierBatch
    {
        Array<graphics::GPUBarrier> barriers;
        inline void Emplace()
        {
            barriers.Emplace();
        }
        inline graphics::GPUBarrier &Back()
        {
            return barriers.Back();
        }
    };

    Array<BarrierBatch> barrierBatches;
    barrierBatches.Init();
    barrierBatches.Resize(passCount);
    for (u32 i = 0; i < passCount; i++)
    {
        BarrierBatch &batch = barrierBatches[i];
        batch.barriers.Init();
        RenderPass *pass = &passes[i];
        if (!EnumHasAnyFlags(pass->flags, PassFlags::NotCulled)) continue;
        EnumerateShaderResources(pass, [&](const PassResource *passResoure, ResourceView *view) {
            RenderGraphResource *resource = &resources[view->handle];
            if (NeedsTransition(resource->lastAccess, view->access))
            {
                batch.Emplace();
                graphics::GPUBarrier &barrier = batch.Back();
                switch (view->type)
                {
                    case ResourceType::Buffer:
                    {
                        // barrier = graphics::GPUBarrier::Buffer(&resource->resource, resource->lastPipelineStage, ,
                        //                                        resource->lastAccess, view->access);
                    }
                    break;
                    case ResourceType::Texture:
                    {
                        // barrier = graphics::GPUBarrier::Image(&resource->resource, );
                    }
                    break;
                }
                resource->lastAccess = view->access;
            }
        });
    }
}

void RenderGraph::Execute()
{
    for (u32 i = 0; i < numPasses; i++)
    {
    }
}

// NOTE: For now, dependencies will be built from submission order.
ResourceHandle RenderGraph::CreateBuffer(string name)
{
    u32 hash              = Hash(name);
    ResourceHandle handle = resourceNameHashTable.Find(hash, resourceStringHashes);

    if (resourceNameHashTable.IsValid(handle))
    {
        return handle;
    }
    else
    {
        handle                      = numResources++;
        RenderGraphResource *buffer = &resources[handle];
        buffer->lastAccess          = ResourceAccess::None;
        buffer->lastPassFlags       = PassFlags::None;
        buffer->lastPipelineStage   = graphics::PipelineStage::None;

        buffer->firstPass           = INVALID_PASS_HANDLE;
        buffer->lastPass            = INVALID_PASS_HANDLE;
        buffer->lastPassWriteHandle = INVALID_PASS_HANDLE;
        resourceNameHashTable.Add(hash, handle);
        resourceStringHashes[handle] = hash;
        return handle;
    }
}

// void RenderGraph::Init()
// {
//     arena = ArenaAlloc();
// }
//
// void RenderGraph::ExecutePasses()
// {
//    for (u32 i = 0; i < passes.size(); i++)
//     {
//         Pass *pass = &passes[i];
//         pass->func(pass->parameters);
//     }
// }
} // namespace rendergraph
