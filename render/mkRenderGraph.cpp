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
    transientResourceAllocator.Init();
    barrierBatches.Init();
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

inline u8 *GetPushConstantPtr(BaseShaderParamType *params)
{
    u8 *ptr = (u8 *)params + sizeof(BaseShaderParamType) + sizeof(PassResource) * params->numResources +
              sizeof(TextureView) * params->numTextures + sizeof(BufferView) * params->numBuffers;
    return ptr;
}

inline u32 GetPushConstantSize(BaseShaderParamType *params)
{
    u32 paramsSize = *((u32 *)params - 1);
    u32 pcSize     = paramsSize - (sizeof(BaseShaderParamType) + sizeof(PassResource) * params->numResources +
                               sizeof(TextureView) * params->numTextures + sizeof(BufferView) * params->numBuffers);
    return pcSize;
}

inline void EnumerateShaderBuffers(RenderPass *pass, const BufferEnumerateFunction &func)
{
    BufferView *bufferViews       = GetBufferViews(pass->parameters);
    PassResource *bufferResources = GetBufferResources(pass->parameters);
    for (u32 i = 0; i < pass->parameters->numBuffers; i++)
    {
        func(&bufferResources[i], &bufferViews[i]);
    }
}

inline void EnumerateShaderTextures(RenderPass *pass, const TextureEnumerateFunction &func)
{
    TextureView *textureViews      = GetTextureViews(pass->parameters);
    PassResource *textureResources = GetTextureResources(pass->parameters);
    for (u32 i = 0; i < pass->parameters->numTextures; i++)
    {
        func(&textureResources[i], &textureViews[i]);
    }
}

inline void EnumerateShaderResources(RenderPass *pass, const ResourceEnumerateFunction &func)
{
    EnumerateShaderBuffers(pass, (const BufferEnumerateFunction)func);
    EnumerateShaderTextures(pass, (const TextureEnumerateFunction)func);
}

template <typename ParameterType>
ParameterType *RenderGraph::AllocParameters()
{
    // TODO: this is kind of jank. maybe I just need to use templates for these
    u8 *bytes             = PushArray(arena, u8, sizeof(ParameterType) + sizeof(u32));
    *(u32 *)bytes         = sizeof(ParameterType);
    ParameterType *result = new (bytes + sizeof(u32)) ParameterType();

    return result;
}

inline PassHandle RenderGraph::AddPass(string passName, void *params, PipelineState *state, const ExecuteFunction &func)
{
    return AddPassInternal(passName, params, state, func, PassFlags::None);
}

inline PassHandle RenderGraph::AddPass(string passName, void *params, PipelineState *state, PassFlags flags, const ExecuteFunction &func)
{
    return AddPassInternal(passName, params, state, func, flags);
}

PassHandle RenderGraph::AddPassInternal(string passName, void *params, PipelineState *state, const ExecuteFunction &func, PassFlags flags)
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

    RenderPass *pass    = &passes[handle];
    pass->name          = PushStr8Copy(arena, passName);
    pass->flags         = flags;
    pass->parameters    = baseParams;
    pass->func          = func;
    pass->pipelineState = state;

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
#if 0
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
#endif
        EnumerateShaderTextures(pass, [&](const PassResource *resource, TextureView *view) {
            RenderGraphResource *rgResource = &resources[view->handle];
            graphics::Texture *texture      = rgResource->texture;
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

    // 4. Resource allocations
    for (u32 i = NULL_HANDLE + 1; i < numResources; i++)
    {
        RenderGraphResource *resource = &resources[i];

        if (!EnumHasAnyFlags(resource->flags, ResourceFlags::NotTransient))
        {
            if (EnumHasAnyFlags(resource->flags, ResourceFlags::Buffer))
            {
                resource->bufferRange = transientResourceAllocator.CreateBuffer(resource->bufferSize);
            }
            if (EnumHasAnyFlags(resource->flags, ResourceFlags::Texture))
            {
                resource->texture = transientResourceAllocator.CreateTexture(resource->textureDesc);
            }
        }
        // should already be allocated
        else
        {
            // not handled yet
            if (EnumHasAnyFlags(resource->flags, ResourceFlags::Buffer))
            {
                Assert(0);
            }
        }
    }

    // 5. Barrier transitions
    barrierBatches.Resize(passCount);
    for (u32 i = 0; i < passCount; i++)
    {
        RenderPass *pass = &passes[i];
        if (!EnumHasAnyFlags(pass->flags, PassFlags::NotCulled)) continue;

        BarrierBatch &batch = barrierBatches[i];
        batch.barriers.Init();
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
                        barrier = GPUBarrier::Buffer(resource->bufferRange.transientBackingBuffer,
                                                     ConvertAccess(resource->lastAccess),
                                                     ConvertAccess(view->access),
                                                     resource->bufferRange.offset,
                                                     resource->bufferRange.size);
                    }
                    break;
                    case ResourceType::Texture:
                    {
                        barrier = GPUBarrier::Image(resource->texture,
                                                    ConvertAccess(resource->lastAccess),
                                                    ConvertAccess(view->access));
                    }
                    break;
                }
                resource->lastAccess = view->access;
            }
        });
    }
}

CommandList RenderGraph::Execute()
{
    CommandList cmd = device->BeginCommandList(graphics::QueueType_Graphics);
    for (u32 i = 0; i < numPasses; i++)
    {
        RenderPass *pass = &passes[i];
        if (!EnumHasAnyFlags(pass->flags, PassFlags::NotCulled)) continue;

        BarrierBatch &batch = barrierBatches[i];
        device->Barrier(cmd, batch.Data(), batch.Length());

        device->BindPipeline(pass->pipelineState, cmd);
        if (EnumHasAnyFlags(pass->parameters->flags, ShaderParamFlags::Push))
        {
            device->PushConstants(cmd, GetPushConstantSize(pass->parameters), GetPushConstantPtr(pass->parameters));
        }
        EnumerateShaderBuffers(pass, [&](const PassResource *resource, ResourceView *view) {
            RenderGraphResource *rgResource = &resources[view->handle];
            graphics::GPUBuffer *buffer     = rgResource->bufferRange.transientBackingBuffer;
            // TODO: old subresources ranges aren't cleared yet
            i32 subresource = device->CreateSubresource(buffer,
                                                        resource->viewType,
                                                        rgResource->bufferRange.offset,
                                                        rgResource->bufferRange.size);
            device->BindResource(buffer, resource->viewType, resource->binding, cmd, subresource);
        });
        EnumerateShaderTextures(pass, [&](const PassResource *resource, TextureView *view) {
            RenderGraphResource *rgResource = &resources[view->handle];
            device->BindResource(rgResource->texture, resource->viewType, resource->binding, cmd);
        });
        device->UpdateDescriptorSet(cmd);
        pass->func(cmd);
    }
    return cmd;
}

ResourceHandle RenderGraph::CreateResourceInternal(string name, ResourceFlags flags, graphics::TextureDesc desc, u32 size)
{
    u32 hash              = Hash(name);
    ResourceHandle handle = resourceNameHashTable.Find(hash, resourceStringHashes);

    if (resourceNameHashTable.IsValid(handle))
    {
        RenderGraphResource *resource = &resources[handle];
        Assert(EnumHasAnyFlags(resource->flags, flags));
        switch (flags)
        {
            case ResourceFlags::Buffer:
            {
                Assert(resource->bufferSize == size);
            }
            break;
            case ResourceFlags::Texture:
            {
                Assert(resource->textureDesc == desc);
            }
            break;
        }
        // Assert(resource->resource.resourceType == GPUResource::ResourceType::Buffer);
        return handle;
    }
    else
    {
        handle                        = numResources++;
        RenderGraphResource *resource = &resources[handle];
        // NOTE: this relies on the assumption that the resources array is 0 initialized. there's a chance that this could be
        // false if I'm not careful (e.g swap the resources array from c array to the dynamic array type)
        resource->Init(flags);
        switch (flags)
        {
            case ResourceFlags::Buffer:
            {
                resource->bufferSize = size;
            }
            break;
            case ResourceFlags::Texture:
            {
                resource->textureDesc = desc;
            }
            break;
        }

        resourceNameHashTable.Add(hash, handle);
        resourceStringHashes[handle] = hash;
        return handle;
    }
}

ResourceHandle RenderGraph::CreateBuffer(string name, u32 size)
{
    return CreateResourceInternal(name, ResourceFlags::Buffer, {}, size);
}

ResourceHandle RenderGraph::CreateTexture(string name, TextureDesc desc)
{
    return CreateResourceInternal(name, ResourceFlags::Texture, desc);
}

void RenderGraph::ExtendLifetime(ResourceHandle handle, ResourceFlags lifetime)
{
    RenderGraphResource *resource = &resources[handle];
    Assert(EnumHasAnyFlags(resource->flags, ResourceFlags::Texture | ResourceFlags::Buffer));
    resource->flags |= lifetime;
}

ResourceHandle RenderGraph::Import(string name, Texture *texture)
{
    u32 hash              = Hash(name);
    ResourceHandle handle = resourceNameHashTable.Find(hash, resourceStringHashes);
    Assert(!resourceNameHashTable.IsValid(handle));

    handle                        = numResources++;
    RenderGraphResource *resource = &resources[handle];
    resource->Init(ResourceFlags::Texture | ResourceFlags::NotTransient);
    resource->texture = texture;
    resourceNameHashTable.Add(hash, handle);
    resourceStringHashes[handle] = hash;
    return handle;
}

inline ResourceHandle RenderGraph::AddComputePass(string passName,
                                                  void *params,
                                                  PipelineState *state,
                                                  V3U32 groupCounts,
                                                  const ComputeFunction &func)
{
    return AddPass(passName, params, state, [groupCounts, callback = std::move(func)](CommandList cmd) {
        if (callback)
        {
            callback();
        }
        device->Dispatch(cmd, groupCounts.x, groupCounts.y, groupCounts.z);
    });
}

inline ResourceHandle RenderGraph::AddComputeIndirectPass(string passName,
                                                          void *params,
                                                          PipelineState *state,
                                                          graphics::GPUBuffer *indirectBuffer,
                                                          u32 offset,
                                                          const ComputeFunction &func)
{
    return AddPass(passName, params, state, [indirectBuffer, offset, callback = std::move(func)](CommandList cmd) {
        if (callback)
        {
            callback();
        }
        device->DispatchIndirect(cmd, indirectBuffer, sizeof(DispatchIndirect) * offset + Offset(DispatchIndirect, groupCountX));
    });
}

void RenderGraph::BeginFrame()
{
    arenaBeginFramePos = ArenaPos(arena);
}

void RenderGraph::EndFrame()
{
    transientResourceAllocator.Reset();
    barrierBatches.Clear();
    ArenaPopTo(arena, arenaBeginFramePos);
}

//////////////////////////////
// TransientResourceAllocator
//
void TransientResourceAllocator::Init()
{
    offsets.Init();
    backingBuffers.Init();
    backingTextures.Init();
    textureInUseMasks.Init();
}

BufferRange TransientResourceAllocator::CreateBuffer(u32 size)
{
    BufferRange range;
    for (u32 i = 0; i < backingBuffers.Length(); i++)
    {
        graphics::GPUBuffer *backingBuffer = &backingBuffers[i];
        u32 totalSize                      = (u32)backingBuffer->desc.size;
        u32 offset                         = offsets[i];
        if (totalSize - offset >= size)
        {
            offsets[i] += size;
            range.transientBackingBuffer = backingBuffer;
            range.transientBufferId      = i;
            range.size                   = size;
            range.offset                 = offset;
            return range;
        }
    }

    u32 bufferSize = Max(pageSize, size);
    graphics::GPUBufferDesc desc;
    desc.size          = size;
    desc.usage         = MemoryUsage::GPU_ONLY;
    desc.resourceUsage = ResourceUsage::StorageBufferRead | ResourceUsage::VertexBuffer | ResourceUsage::IndexBuffer |
                         ResourceUsage::StorageBuffer;
    backingBuffers.Emplace();
    device->CreateBuffer(&backingBuffers.Back(), desc, 0);

    range.transientBufferId = backingBuffers.Length();
    range.offset            = 0;
    range.size              = size;
    return range;
}

Texture *TransientResourceAllocator::CreateTexture(TextureDesc desc)
{
    for (u32 i = 0; i < backingTextures.Length(); i++)
    {
        if (backingTextures[i].desc == desc)
        {
            if (!(textureInUseMasks[i >> 5] & (1 << (i & 31))))
            {
                textureInUseMasks[i >> 5] |= (1 << (i & 31));
                return &backingTextures[i];
            }
        }
    }
    backingTextures.Emplace();
    if (backingTextures.Length() > (textureInUseMasks.Length() << 5))
    {
        offsets.Add(0);
    }
    device->CreateTexture(&backingTextures.Back(), desc, 0);
    return &backingTextures.Back();
}

void TransientResourceAllocator::Reset()
{
    for (u32 i = 0; i < offsets.Length(); i++)
    {
        offsets[i] = 0;
    }
    for (u32 i = 0; i < textureInUseMasks.Length(); i++)
    {
        textureInUseMasks[i] = 0;
    }
    // TODO: clear the subresources
    for (u32 i = 0; i < backingBuffers.Length(); i++)
    {
    }
}

inline void BarrierBatch::Emplace()
{
    barriers.Emplace();
}

inline graphics::GPUBarrier &BarrierBatch::Back()
{
    return barriers.Back();
}

inline graphics::GPUBarrier *BarrierBatch::Data()
{
    return barriers.data;
}

inline u32 BarrierBatch::Length()
{
    return barriers.Length();
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
