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
    arena     = ArenaAlloc();
    passCount = 0;
    // graphics::GPUBufferDesc desc;
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

    RenderPass *pass = &passes[handle];
    pass->name       = PushStr8Copy(arena, passName);
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

    u32 numPassDependencies = 0;
    // First pass, calculate the total # of dependencies for the pass
    EnumerateShaderBuffers(baseParams, [&](PassResource *resource, BufferView *view) {
        view->type = ResourceViewType::Buffer;
        SetResourceAccess(resource, view);
        RenderGraphBuffer *buffer = &buffers[view->handle];
        buffer->numUses++;

        if (buffer->firstPass == INVALID_PASS_HANDLE)
        {
            buffer->firstPass = handle;
        }
        buffer->lastPass = handle;

        switch (resource->usage)
        {
            case ResourceUsage::SRV:
            {
                if (IsValidHandle(buffer->lastPassWriteHandle))
                {
                    numPassDependencies++;
                }
                // buffer->lastPassReadHandle = handle;
            }
            break;
            case ResourceUsage::UAV:
            {
                if (IsValidHandle(buffer->lastPassWriteHandle))
                {
                    numPassDependencies++;
                }
                if (IsValidHandle(buffer->lastPassReadHandle))
                {
                    numPassDependencies++;
                }
                // buffer->lastPassWriteHandle = handle;
            }
            break;
        }
    });

    // Allocate the dependencies
    PassHandle *dependencies     = PushArray(arena, PassHandle, numPassDependencies);
    passDependencies[handle]     = dependencies;
    passDependencyCounts[handle] = numPassDependencies;

    // Second pass, fill in an array which contains the handles of the passes this pass is dependent on
    u32 index = 0;
    EnumerateShaderBuffers(baseParams, [&](PassResource *resource, BufferView *view) {
        RenderGraphBuffer *buffer = &buffers[view->handle];
        switch (resource->usage)
        {
            case ResourceUsage::SRV:
            {
                // Read after write
                if (IsValidHandle(buffer->lastPassWriteHandle))
                {
                    dependencies[index++] = buffer->lastPassWriteHandle;
                }
                buffer->lastPassReadHandle = handle;
            }
            case ResourceUsage::UAV:
            {
                // Write after write (and potentially read after write)
                if (IsValidHandle(buffer->lastPassWriteHandle))
                {
                    dependencies[index++] = buffer->lastPassWriteHandle;
                }
                // Write after read
                if (IsValidHandle(buffer->lastPassReadHandle))
                {
                    dependencies[index++] = buffer->lastPassReadHandle;
                }
                buffer->lastPassWriteHandle = handle;
            }
            break;
        }
    });

    EnumerateShaderTextures(baseParams, [&](PassResource *resource, TextureView *view) {
        view->type = ResourceViewType::Texture;
        SetResourceAccess(resource, view);
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
    // 1. Topological sort
    PassHandle *topologicalSortResult = PushArray(arena, PassHandle, passCount);
    u32 topSortIndex                  = 0;
    {
        // TODO: this is kind of arbitrary
        i32 stackCount     = passCount * 16;
        PassHandle *stack  = PushArray(temp.arena, PassHandle, stackCount);
        u32 *visitedBitmap = PushArray(temp.arena, u32, (passCount + 31) >> 5);
        u32 *addedBitmap   = PushArray(temp.arena, u32, (passCount + 31) >> 5);
        // used to check for cycles
        u32 *stackBitmap = PushArray(temp.arena, u32, (passCount + 31) >> 5);
        for (PassHandle handle = 0; handle < passCount; handle++)
        {
            if (CheckBitmap(visitedBitmap, handle)) continue;

            i32 top    = 0;
            stack[top] = handle;
            while (top != -1)
            {
                PassHandle topHandle = stack[top];
                // The second time the stack visits a pass, add it to the topological sort (should be after children
                // and all of their dependents have been visited and added to the sort result)
                if (CheckBitmap(visitedBitmap, topHandle))
                {
                    ClearInBitmap(stackBitmap, topHandle);
                    top--;
                    if (!CheckBitmap(addedBitmap, topHandle))
                    {
                        topologicalSortResult[topSortIndex++] = topHandle;
                        AddToBitmap(addedBitmap, topHandle);
                    }
                    continue;
                }
                AddToBitmap(visitedBitmap, topHandle);
                AddToBitmap(stackBitmap, topHandle);
                for (u32 i = 0; i < passDependencyCounts[topHandle]; i++)
                {
                    PassHandle dependentHandle = passDependencies[topHandle][i];
                    if (CheckBitmap(stackBitmap, dependentHandle))
                    {
                        // Assert if there is a cycle
                        Assert(!"Cycle found in render graph.");
                        return;
                    }
                    top++;
                    Assert(top < stackCount);
                    stack[top] = dependentHandle;
                }
            }
        }
    }
    // The top of the topological sort result list contains the first passes to be executed
    // 2. idk what to do next lol
    for (u32 i = 0; i < passCount; i++)
    {
        PassHandle handle = topologicalSortResult[i];
        Printf("Pass: %S, Index: %u\n", passes[handle].name, handle);
    }
}

// NOTE: For now, dependencies will be built from submission order.
BufferHandle RenderGraph::CreateBuffer(string name)
{
    u32 hash                  = Hash(name);
    BufferHandle bufferHandle = bufferNameHashTable.Find(hash, bufferStringHashes);

    if (bufferNameHashTable.IsValid(bufferHandle))
    {
        return bufferHandle;
    }
    else
    {
        bufferHandle                = numBuffers++;
        RenderGraphBuffer *buffer   = &buffers[bufferHandle];
        buffer->firstPass           = INVALID_PASS_HANDLE;
        buffer->lastPass            = INVALID_PASS_HANDLE;
        buffer->lastPassWriteHandle = INVALID_PASS_HANDLE;
        buffer->lastPassReadHandle  = INVALID_PASS_HANDLE;
        bufferNameHashTable.Add(hash, bufferHandle);
        bufferStringHashes[bufferHandle] = hash;
        return bufferHandle;
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
