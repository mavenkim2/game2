#include "mkRenderGraph.h"
#include "../generated/render_graph_resources.h"
#include "../mkCrack.h"
#ifdef LSP_INCLUDE
#include "../render/mkGraphics.h"
#endif

namespace rendergraph
{
// TODO: IDEA: create a minimum spanning tree for each queue?
void RenderGraph::Init()
{
    passCount = 0;
    graphics::GPUBufferDesc desc;
    // desc.resourceUsage = graphics::ResourceUsage::
}

inline BaseShaderParamType *GetBaseShaderParam(void *params)
{
    return (BaseShaderParamType *)params;
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
        func(bufferResources, bufferViews);
    }
}

inline void EnumerateShaderTextures(BaseShaderParamType *baseParams, const TextureEnumerateFunction &func)
{
    TextureView *textureViews      = GetTextureViews(baseParams);
    PassResource *textureResources = GetTextureResources(baseParams);
    for (u32 i = 0; i < baseParams->numBuffers; i++)
    {
        func(textureResources, textureViews);
    }
}

// void RenderGraph::BeginFrame()
// {
//     arenaBeginFramePos = ArenaPos(arena);
// }

PassHandle RenderGraph::AddPassInternal(string passName, void *params, u32 size, const ExecuteFunction &func)
{
    u32 passHash = Hash(passName);

    // Don't recreate pass every frame
    {
        u32 index = renderPassHashTable.Find(passHash, renderPassStringHashes);
        if (renderPassHashTable.IsValid(index)) return index;
    }

    PassHandle handle = passCount++;

    renderPassHashTable.Add(passHash, handle);
    renderPassStringHashes[handle] = handle;

    RenderPass *pass = &passes[passCount++];
    pass->parameters = (void *)PushArray(arena, u8, size);
    MemoryCopy(pass->parameters, params, size);
    pass->func = func;
    pass->size = size;

    BaseShaderParamType *baseParams = GetBaseShaderParam(params);
    bool isCompute                  = EnumHasAllFlags(baseParams->flags, ShaderParamFlags::Compute);
    bool isGraphics                 = EnumHasAllFlags(baseParams->flags, ShaderParamFlags::Graphics);

    // TODO: maybe something to think about for later, but I am kind of wary of nested lambdas in terms of code readability
    auto SetResourceAccess = [&](PassResource *resource, ResourceView *view) {
        switch (resource->usage)
        {
            case ResourceUsage::SRV:
            {
                if (isCompute) view->access = ViewAccess::ComputeSRV;
                else if (isGraphics) view->access = ViewAccess::GraphicsSRV;
                else Assert(!"Not implemented");
            }
            break;
            case ResourceUsage::UAV:
            {
                if (isCompute) view->access = ViewAccess::ComputeUAV;
                else if (isGraphics) view->access = ViewAccess::GraphicsUAV;
                else Assert(!"Not implemented");
            }
            break;
        }
    };

    EnumerateShaderBuffers(baseParams, [&](PassResource *resource, BufferView *view) {
        view->type = ResourceViewType::Buffer;
        SetResourceAccess(resource, view);
        RenderGraphBuffer *buffer = &buffers[view->handle];
        buffer->numUses++;
    });

    EnumerateShaderTextures(baseParams, [&](PassResource *resource, TextureView *view) {
        view->type = ResourceViewType::Texture;
        SetResourceAccess(resource, view);
    });
    return handle;
}

// NOTE: For now, dependencies will be built from submission order.
BufferHandle RenderGraph::CreateBufferSRV(string name)
{
    u32 hash        = Hash(name);
    u32 bufferIndex = bufferNameHashTable.Find(hash, bufferStringHashes);

    if (bufferNameHashTable.IsValid(bufferIndex))
    {
        return bufferIndex;
    }
    else
    {
        return numBuffers++;
    }
}

void RenderGraph::Compile()
{
    for (PassHandle handle = 0; handle < passCount; handle++)
    {
        RenderPass *pass = &passes[handle];

        // PassResource *resources;
        // ResourceView *views;
        // BaseShaderParamType *params;
        // GetParameterData(pass, params, resources, views);
    }
}

// void RenderGraph::Init()
// {
//     arena = ArenaAlloc();
// }
//
// void AddInstanceCull(RenderGraph *graph)
// {
//     PassResources instanceCullResources;
//     instanceCullResources.
//
//         graph->AddPass([&](CommandList cmd) {
//             device->BeginEvent("Instance Cull First Pass");
//             device->EndEvent("Instance Cull First Pass");
//         });
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
