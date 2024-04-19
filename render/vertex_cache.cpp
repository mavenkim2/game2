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
    VertexCache *staticCache = &gVertexCache.mStaticData;
    staticCache->mType       = BufferUsage_Static;

    GPUBuffer *buffer = &staticCache->mVertexBuffer;
    buffer->mType     = BufferType_Vertex;
    R_InitializeBuffer(buffer, staticCache->mType, VERTEX_CACHE_BUFFER_SIZE);

    buffer        = &staticCache->mIndexBuffer;
    buffer->mType = BufferType_Index;
    R_InitializeBuffer(buffer, staticCache->mType, VERTEX_CACHE_BUFFER_SIZE);

    for (i32 i = 0; i < ArrayLength(gVertexCache.mFrameData); i++)
    {
        VertexCache *dynamicCache = &gVertexCache.mFrameData[i];
        dynamicCache->mType       = BufferUsage_Dynamic;

        buffer        = &dynamicCache->mVertexBuffer;
        buffer->mType = BufferType_Vertex;
        R_InitializeBuffer(buffer, staticCache->mType, VERTEX_CACHE_BUFFER_SIZE);

        buffer        = &dynamicCache->mIndexBuffer;
        buffer->mType = BufferType_Index;
        R_InitializeBuffer(buffer, staticCache->mType, VERTEX_CACHE_BUFFER_SIZE);

        buffer        = &dynamicCache->mUniformBuffer;
        buffer->mType = BufferType_Uniform;
        R_InitializeBuffer(buffer, staticCache->mType, VERTEX_CACHE_BUFFER_SIZE);
    }

    R_MapGPUBuffer(&staticCache->mVertexBuffer);
    R_MapGPUBuffer(&staticCache->mIndexBuffer);

    R_MapGPUBuffer(&gVertexCache.mFrameData[gVertexCache.mCurrentIndex].mVertexBuffer);
    R_MapGPUBuffer(&gVertexCache.mFrameData[gVertexCache.mCurrentIndex].mIndexBuffer);
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
                    buffer = &gVertexCache.mStaticData.mVertexBuffer;
                    break;
                }
                case BufferType_Index:
                {
                    buffer = &gVertexCache.mStaticData.mIndexBuffer;
                    break;
                }
                default: Assert(!"Invalid default case");
            }
            break;
        }
        case BufferUsage_Dynamic:
        {
            VertexCache *frameCache = &gVertexCache.mFrameData[gVertexCache.mCurrentIndex];
            switch (bufferType)
            {
                case BufferType_Vertex:
                {
                    buffer = &frameCache->mVertexBuffer;
                    break;
                }
                case BufferType_Index:
                {
                    buffer = &frameCache->mIndexBuffer;
                    break;
                }
                case BufferType_Uniform:
                {
                    buffer = &frameCache->mUniformBuffer;
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
        i32 initialOffset = buffer->mOffset;
        i32 alignSize     = elementSize - (initialOffset % elementSize); // size + size; AlignPow2(size, 16);
        i32 commitSize    = elementSize * count;
        i32 totalSize     = commitSize + alignSize;

        Assert(buffer->mSize >= initialOffset + totalSize);
        Assert(totalSize <= VERTEX_CACHE_SIZE_MASK);
        // i32 offset = AtomicAddI32(&buffer->offset, alignedSize);
        if (AtomicCompareExchange(&buffer->mOffset, initialOffset + totalSize, initialOffset) == initialOffset)
        {
            i32 offset = initialOffset + alignSize;
            R_UpdateBuffer(buffer, usageType, data, offset, commitSize);

            handle = ((u64)(offset & VERTEX_CACHE_OFFSET_MASK) << VERTEX_CACHE_OFFSET_SHIFT) |
                     ((u64)(commitSize & VERTEX_CACHE_SIZE_MASK) << VERTEX_CACHE_SIZE_SHIFT) |
                     ((u64)(gVertexCache.mCurrentFrame & VERTEX_CACHE_FRAME_MASK) << VERTEX_CACHE_FRAME_SHIFT);

            if (usageType == BufferUsage_Static)
            {
                handle |= ((u64)VERTEX_CACHE_STATIC_MASK);
            }
            break;
        }
    }

    return handle;
}

b32 VertexCacheState::CheckStatic(VC_Handle handle)
{
    b32 result = handle & VERTEX_CACHE_STATIC_MASK;
    return result;
}

b32 VertexCacheState::CheckSubmitted(VC_Handle handle)
{
    if (CheckStatic(handle))
    {
        return true;
    }
    i32 currentFrame = (i32)((handle >> VERTEX_CACHE_FRAME_SHIFT) & VERTEX_CACHE_FRAME_MASK);
    if (currentFrame != ((gVertexCache.mCurrentFrame - 1) & VERTEX_CACHE_FRAME_MASK))
    {
        Printf("Joints not previously submitted in the uniform buffer");
        return false;
    }
    return true;
}

b32 VertexCacheState::CheckCurrent(VC_Handle handle)
{
    if (CheckStatic(handle))
    {
        return true;
    }
    i32 currentFrame = (i32)((handle >> VERTEX_CACHE_FRAME_SHIFT) & VERTEX_CACHE_FRAME_MASK);
    if (currentFrame != ((gVertexCache.mCurrentFrame) & VERTEX_CACHE_FRAME_MASK))
    {
        Printf("Joints not previously submitted in the uniform buffer");
        return false;
    }
    return true;
}

internal GPUBuffer *VC_GetBufferFromHandle(VC_Handle handle, BufferType type)
{
    GPUBuffer *buffer = 0;
    if (gVertexCache.CheckStatic(handle))
    {
        switch (type)
        {
            case BufferType_Vertex:
            {
                buffer = &gVertexCache.mStaticData.mVertexBuffer;
                break;
            }
            case BufferType_Index:
            {
                buffer = &gVertexCache.mStaticData.mIndexBuffer;
                break;
            }
        }
    }
    else
    {
        i32 frameNum = (i32)((handle >> VERTEX_CACHE_FRAME_SHIFT) & VERTEX_CACHE_FRAME_MASK);
        if (frameNum != gVertexCache.mCurrentFrame - 1)
        {
            Assert(!"Vertex buffer invalid");
        }
        else
        {
            switch (type)
            {
                case BufferType_Vertex:
                {
                    buffer = &gVertexCache.mFrameData[gVertexCache.mCurrentDrawIndex].mVertexBuffer;
                    break;
                }
                case BufferType_Index:
                {
                    buffer = &gVertexCache.mFrameData[gVertexCache.mCurrentDrawIndex].mIndexBuffer;
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
    // VertexCache *staticCache = &gVertexCache.mStaticData;
    // GPUBuffer *mVertexBuffer  = &staticCache->mVertexBuffer;

    VertexCache *frameCache  = &gVertexCache.mFrameData[gVertexCache.mCurrentIndex];
    VertexCache *staticCache = &gVertexCache.mStaticData;

    R_UnmapGPUBuffer(&frameCache->mIndexBuffer);
    R_UnmapGPUBuffer(&frameCache->mVertexBuffer);
    R_UnmapGPUBuffer(&frameCache->mUniformBuffer);
    R_UnmapGPUBuffer(&staticCache->mVertexBuffer);
    R_UnmapGPUBuffer(&staticCache->mIndexBuffer);

    gVertexCache.mCurrentFrame++;
    gVertexCache.mCurrentDrawIndex = gVertexCache.mCurrentIndex;
    gVertexCache.mCurrentIndex     = gVertexCache.mCurrentFrame % ArrayLength(gVertexCache.mFrameData);

    VertexCache *newFrame            = &gVertexCache.mFrameData[gVertexCache.mCurrentIndex];
    newFrame->mIndexBuffer.mOffset   = 0;
    newFrame->mVertexBuffer.mOffset  = 0;
    newFrame->mUniformBuffer.mOffset = 0;

    R_MapGPUBuffer(&newFrame->mIndexBuffer);
    R_MapGPUBuffer(&newFrame->mVertexBuffer);
    R_MapGPUBuffer(&frameCache->mUniformBuffer);
    R_MapGPUBuffer(&staticCache->mVertexBuffer);
    R_MapGPUBuffer(&staticCache->mIndexBuffer);
}

inline u64 VertexCacheState::GetOffset(VC_Handle handle)
{
    u64 offset = (u64)((handle >> VERTEX_CACHE_OFFSET_SHIFT) & VERTEX_CACHE_OFFSET_MASK);
    return offset;
}

inline u64 VertexCacheState::GetSize(VC_Handle handle)
{
    u64 size = (u64)((handle >> VERTEX_CACHE_SIZE_SHIFT) & VERTEX_CACHE_SIZE_MASK);
    return size;
}
