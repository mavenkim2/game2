#include "../crack.h"
#ifdef LSP_INCLUDE
#include "../keepmovingforward_common.h"
#include "vertex_cache.h"
#endif

const i32 VERTEX_CACHE_STATIC_MASK  = 1;
const i32 VERTEX_CACHE_OFFSET_SHIFT = 1;
const i32 VERTEX_CACHE_OFFSET_MASK  = 0x1ffffff;
const i32 VERTEX_CACHE_SIZE_SHIFT   = 26;
const i32 VERTEX_CACHE_SIZE_MASK    = 0x1ffffff;
const i32 VERTEX_CACHE_FRAME_SHIFT  = 51;
const i32 VERTEX_CACHE_FRAME_MASK   = 0x1fff;

// must be smaller than 2^25 - 1
const i32 VERTEX_CACHE_BUFFER_SIZE = 31 * 1024 * 1024;

internal void VC_Init()
{
    VertexCache *staticCache = &gVertexCache.staticData;
    staticCache->type        = BufferUsage_Static;

    GPUBuffer *buffer = &staticCache->vertexBuffer;
    buffer->type      = BufferType_Vertex;
    R_InitializeBuffer(buffer, staticCache->type, VERTEX_CACHE_BUFFER_SIZE);

    buffer       = &staticCache->indexBuffer;
    buffer->type = BufferType_Index;
    R_InitializeBuffer(buffer, staticCache->type, VERTEX_CACHE_BUFFER_SIZE);

    for (i32 i = 0; i < ArrayLength(gVertexCache.frameData); i++)
    {
        VertexCache *dynamicCache = &gVertexCache.frameData[i];
        dynamicCache->type        = BufferUsage_Dynamic;

        buffer       = &dynamicCache->vertexBuffer;
        buffer->type = BufferType_Vertex;
        R_InitializeBuffer(buffer, staticCache->type, VERTEX_CACHE_BUFFER_SIZE);

        buffer       = &dynamicCache->indexBuffer;
        buffer->type = BufferType_Index;
        R_InitializeBuffer(buffer, staticCache->type, VERTEX_CACHE_BUFFER_SIZE);
    }

    R_MapGPUBuffer(&staticCache->vertexBuffer);
    R_MapGPUBuffer(&staticCache->indexBuffer);

    R_MapGPUBuffer(&gVertexCache.frameData[gVertexCache.currentIndex].vertexBuffer);
    R_MapGPUBuffer(&gVertexCache.frameData[gVertexCache.currentIndex].indexBuffer);
}

// TODO: static data asynchronously loaded needs to be queued. this only works with persistent mapping
internal VC_Handle VC_AllocateBuffer(BufferType bufferType, BufferUsageType usageType, void *data, i32 elementSize,
                                     i32 count)
{
    VC_Handle handle = 0;

    // TODO: this cannot be aligned because the size of MeshVertex is 76, so with an alignment of 16 the
    // MeshVertex will not be placed at an appropriate boundary in the buffer
    // technically it's possible but vertexattrib pointer would have to be weird so it's not worth it now

    GPUBuffer *buffer = 0;
    switch (usageType)
    {
        case BufferUsage_Static:
        {
            switch (bufferType)
            {
                case BufferType_Vertex:
                {
                    buffer = &gVertexCache.staticData.vertexBuffer;
                    break;
                }
                case BufferType_Index:
                {
                    buffer = &gVertexCache.staticData.indexBuffer;
                    break;
                }
                default: Assert(!"Invalid default case");
            }
            break;
        }
        case BufferUsage_Dynamic:
        {
            VertexCache *frameCache = &gVertexCache.frameData[gVertexCache.currentIndex];
            switch (bufferType)
            {
                case BufferType_Vertex:
                {
                    buffer = &frameCache->vertexBuffer;
                    break;
                }
                case BufferType_Index:
                {
                    buffer = &frameCache->indexBuffer;
                    break;
                }
                default: Assert(!"Invalid default case");
            }
            break;
        }
    }

    // TODO: not sure about this. maybe in the future to keep things simpler all the vertex types will need to be
    // power of 2 sizes

    for (;;)
    {
        i32 initialOffset = buffer->offset;
        i32 alignSize     = elementSize - (initialOffset % elementSize); // size + size; AlignPow2(size, 16);
        i32 commitSize    = elementSize * count;
        i32 totalSize     = commitSize + alignSize;

        Assert(buffer->size >= initialOffset + totalSize);
        Assert(totalSize <= VERTEX_CACHE_SIZE_MASK);
        // i32 offset = AtomicAddI32(&buffer->offset, alignedSize);
        if (AtomicCompareExchange(&buffer->offset, initialOffset + totalSize, initialOffset) == initialOffset)
        {
            i32 offset = initialOffset + alignSize;
            R_UpdateBuffer(buffer, usageType, data, offset, commitSize);

            handle = ((u64)(offset & VERTEX_CACHE_OFFSET_MASK) << VERTEX_CACHE_OFFSET_SHIFT) |
                     ((u64)(commitSize & VERTEX_CACHE_SIZE_MASK) << VERTEX_CACHE_SIZE_SHIFT) |
                     ((u64)(gVertexCache.currentFrame & VERTEX_CACHE_FRAME_MASK) << VERTEX_CACHE_FRAME_SHIFT);

            if (usageType == BufferUsage_Static)
            {
                handle |= ((u64)VERTEX_CACHE_STATIC_MASK);
            }
            break;
        }
    }

    return handle;
}

internal b32 VC_CheckStatic(VC_Handle handle)
{
    b32 result = handle & VERTEX_CACHE_STATIC_MASK;
    return result;
}

internal GPUBuffer *VC_GetBufferFromHandle(VC_Handle handle, BufferType type)
{
    GPUBuffer *buffer = 0;
    if (VC_CheckStatic(handle))
    {
        switch (type)
        {
            case BufferType_Vertex:
            {
                buffer = &gVertexCache.staticData.vertexBuffer;
                break;
            }
            case BufferType_Index:
            {
                buffer = &gVertexCache.staticData.indexBuffer;
                break;
            }
        }
    }
    else
    {
        i32 frameNum = (i32)((handle >> VERTEX_CACHE_FRAME_SHIFT) & VERTEX_CACHE_FRAME_MASK);
        if (frameNum != gVertexCache.currentFrame - 1)
        {
            Assert(!"Vertex buffer invalid");
        }
        else
        {
            switch (type)
            {
                case BufferType_Vertex:
                {
                    buffer = &gVertexCache.frameData[gVertexCache.currentDrawIndex].vertexBuffer;
                    break;
                }
                case BufferType_Index:
                {
                    buffer = &gVertexCache.frameData[gVertexCache.currentDrawIndex].indexBuffer;
                    break;
                }
            }
        }
    }
    return buffer;
}

// Unmap currently mapped buffers (if not persistently mapped), map the next buffers (if needed)
internal void VC_BeginGPUSubmit()
{
    // VertexCache *staticCache = &gVertexCache.staticData;
    // GPUBuffer *vertexBuffer  = &staticCache->vertexBuffer;

    VertexCache *frameCache  = &gVertexCache.frameData[gVertexCache.currentIndex];
    VertexCache *staticCache = &gVertexCache.staticData;

    R_UnmapGPUBuffer(&frameCache->indexBuffer);
    R_UnmapGPUBuffer(&frameCache->vertexBuffer);
    R_UnmapGPUBuffer(&staticCache->vertexBuffer);
    R_UnmapGPUBuffer(&staticCache->indexBuffer);

    gVertexCache.currentFrame++;
    gVertexCache.currentDrawIndex = gVertexCache.currentIndex;
    gVertexCache.currentIndex     = gVertexCache.currentFrame % ArrayLength(gVertexCache.frameData);

    VertexCache *newFrame         = &gVertexCache.frameData[gVertexCache.currentIndex];
    newFrame->indexBuffer.offset  = 0;
    newFrame->vertexBuffer.offset = 0;

    R_MapGPUBuffer(&newFrame->indexBuffer);
    R_MapGPUBuffer(&newFrame->vertexBuffer);
    R_MapGPUBuffer(&staticCache->vertexBuffer);
    R_MapGPUBuffer(&staticCache->indexBuffer);
}
