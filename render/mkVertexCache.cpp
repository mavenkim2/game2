#include "../mkCrack.h"
#ifdef LSP_INCLUDE
#include "../keepmovingforward_common.h"
#include "vertex_cache.h"
#endif

// #if 0
// void VertexCacheState::VC_Init()
// {
//     VertexCache *staticCache = &mStaticData;
//     staticCache->mType       = BufferUsage_Static;
//
//     GPUBuffer *buffer = &staticCache->mVertexBuffer;
//     buffer->mType     = BufferType_Vertex;
//     renderer.R_InitializeBuffer(buffer, staticCache->mType, VERTEX_CACHE_BUFFER_SIZE);
//
//     buffer        = &staticCache->mIndexBuffer;
//     buffer->mType = BufferType_Index;
//     renderer.R_InitializeBuffer(buffer, staticCache->mType, VERTEX_CACHE_BUFFER_SIZE);
//
//     for (i32 i = 0; i < ArrayLength(mFrameData); i++)
//     {
//         VertexCache *dynamicCache = &mFrameData[i];
//         dynamicCache->mType       = BufferUsage_Dynamic;
//
//         buffer        = &dynamicCache->mVertexBuffer;
//         buffer->mType = BufferType_Vertex;
//         renderer.R_InitializeBuffer(buffer, staticCache->mType, VERTEX_CACHE_BUFFER_SIZE);
//
//         buffer        = &dynamicCache->mIndexBuffer;
//         buffer->mType = BufferType_Index;
//         renderer.R_InitializeBuffer(buffer, staticCache->mType, VERTEX_CACHE_BUFFER_SIZE);
//
//         buffer        = &dynamicCache->mUniformBuffer;
//         buffer->mType = BufferType_Uniform;
//         renderer.R_InitializeBuffer(buffer, staticCache->mType, VERTEX_CACHE_BUFFER_SIZE);
//     }
//
//     renderer.R_MapGPUBuffer(&staticCache->mVertexBuffer);
//     renderer.R_MapGPUBuffer(&staticCache->mIndexBuffer);
//
//     renderer.R_MapGPUBuffer(&mFrameData[mCurrentIndex].mVertexBuffer);
//     renderer.R_MapGPUBuffer(&mFrameData[mCurrentIndex].mIndexBuffer);
// }
//
// // TODO: static data asynchronously loaded needs to be queued. this only works with persistent mapping
// VC_Handle VertexCacheState::VC_AllocateBuffer(BufferType bufferType, BufferUsageType usageType, void *data, i32 elementSize,
//                                               i32 count)
// {
//     VC_Handle handle = 0;
//
//     // TODO: this cannot be aligned because the size of MeshVertex is 76, so with an alignment of 16 the
//     // MeshVertex will not be placed at an appropriate boundary in the buffer
//     // technically it's possible but vertexattrib pointer would have to be weird so it's not worth it now
//
//     GPUBuffer *buffer = 0;
//     switch (usageType)
//     {
//         case BufferUsage_Static:
//         {
//             switch (bufferType)
//             {
//                 case BufferType_Vertex:
//                 {
//                     buffer = &mStaticData.mVertexBuffer;
//                     break;
//                 }
//                 case BufferType_Index:
//                 {
//                     buffer = &mStaticData.mIndexBuffer;
//                     break;
//                 }
//                 default: Assert(!"Invalid default case");
//             }
//             break;
//         }
//         case BufferUsage_Dynamic:
//         {
//             VertexCache *frameCache = &mFrameData[mCurrentIndex];
//             switch (bufferType)
//             {
//                 case BufferType_Vertex:
//                 {
//                     buffer = &frameCache->mVertexBuffer;
//                     break;
//                 }
//                 case BufferType_Index:
//                 {
//                     buffer = &frameCache->mIndexBuffer;
//                     break;
//                 }
//                 case BufferType_Uniform:
//                 {
//                     buffer = &frameCache->mUniformBuffer;
//                     break;
//                 }
//                 default: Assert(!"Invalid default case");
//             }
//             break;
//         }
//     }
//
//     // TODO: not sure about this. maybe in the future to keep things simpler all the vertex types will need to be
//     // power of 2 sizes
//
//     for (;;)
//     {
//         i32 initialOffset = buffer->mOffset;
//         i32 alignSize     = elementSize - (initialOffset % elementSize); // size + size; AlignPow2(size, 16);
//         i32 commitSize    = elementSize * count;
//         i32 totalSize     = commitSize + alignSize;
//
//         Assert(buffer->mSize >= initialOffset + totalSize);
//         Assert(totalSize <= VERTEX_CACHE_SIZE_MASK);
//         // i32 offset = AtomicAddI32(&buffer->offset, alignedSize);
//         if (AtomicCompareExchange(&buffer->mOffset, initialOffset + totalSize, initialOffset) == initialOffset)
//         {
//             i32 offset = initialOffset + alignSize;
//             renderer.R_UpdateBuffer(buffer, usageType, data, offset, commitSize);
//
//             handle = ((u64)(offset & VERTEX_CACHE_OFFSET_MASK) << VERTEX_CACHE_OFFSET_SHIFT) |
//                      ((u64)(commitSize & VERTEX_CACHE_SIZE_MASK) << VERTEX_CACHE_SIZE_SHIFT) |
//                      ((u64)(mCurrentFrame & VERTEX_CACHE_FRAME_MASK) << VERTEX_CACHE_FRAME_SHIFT);
//
//             if (usageType == BufferUsage_Static)
//             {
//                 handle |= ((u64)VERTEX_CACHE_STATIC_MASK);
//             }
//             break;
//         }
//     }
//
//     return handle;
// }
//
// b32 VertexCacheState::CheckStatic(VC_Handle handle)
// {
//     b32 result = handle & VERTEX_CACHE_STATIC_MASK;
//     return result;
// }
//
// b32 VertexCacheState::CheckSubmitted(VC_Handle handle)
// {
//     if (CheckStatic(handle))
//     {
//         return true;
//     }
//     i32 currentFrame = (i32)((handle >> VERTEX_CACHE_FRAME_SHIFT) & VERTEX_CACHE_FRAME_MASK);
//     if (currentFrame != ((mCurrentFrame - 1) & VERTEX_CACHE_FRAME_MASK))
//     {
//         Printf("Joints not previously submitted in the uniform buffer");
//         return false;
//     }
//     return true;
// }
//
// b32 VertexCacheState::CheckCurrent(VC_Handle handle)
// {
//     if (CheckStatic(handle))
//     {
//         return true;
//     }
//     i32 currentFrame = (i32)((handle >> VERTEX_CACHE_FRAME_SHIFT) & VERTEX_CACHE_FRAME_MASK);
//     if (currentFrame != ((mCurrentFrame)&VERTEX_CACHE_FRAME_MASK))
//     {
//         Printf("Joints not previously submitted in the uniform buffer");
//         return false;
//     }
//     return true;
// }
//
// GPUBuffer *VertexCacheState::VC_GetBufferFromHandle(VC_Handle handle, BufferType type)
// {
//     GPUBuffer *buffer = 0;
//     if (CheckStatic(handle))
//     {
//         switch (type)
//         {
//             case BufferType_Vertex:
//             {
//                 buffer = &mStaticData.mVertexBuffer;
//                 break;
//             }
//             case BufferType_Index:
//             {
//                 buffer = &mStaticData.mIndexBuffer;
//                 break;
//             }
//         }
//     }
//     else
//     {
//         i32 frameNum = (i32)((handle >> VERTEX_CACHE_FRAME_SHIFT) & VERTEX_CACHE_FRAME_MASK);
//         if (frameNum != mCurrentFrame - 1)
//         {
//             Assert(!"Vertex buffer invalid");
//         }
//         else
//         {
//             switch (type)
//             {
//                 case BufferType_Vertex:
//                 {
//                     buffer = &mFrameData[mCurrentDrawIndex].mVertexBuffer;
//                     break;
//                 }
//                 case BufferType_Index:
//                 {
//                     buffer = &mFrameData[mCurrentDrawIndex].mIndexBuffer;
//                     break;
//                 }
//             }
//         }
//     }
//     return buffer;
// }
//
// // Unmap currently mapped buffers (if not persistently mapped), map the next buffers (if needed)
// void VertexCacheState::VC_BeginGPUSubmit()
// {
//     // VertexCache *staticCache = &mStaticData;
//     // GPUBuffer *mVertexBuffer  = &staticCache->mVertexBuffer;
//
//     VertexCache *frameCache  = &mFrameData[mCurrentIndex];
//     VertexCache *staticCache = &mStaticData;
//
//     renderer.R_UnmapGPUBuffer(&frameCache->mIndexBuffer);
//     renderer.R_UnmapGPUBuffer(&frameCache->mVertexBuffer);
//     renderer.R_UnmapGPUBuffer(&frameCache->mUniformBuffer);
//     renderer.R_UnmapGPUBuffer(&staticCache->mVertexBuffer);
//     renderer.R_UnmapGPUBuffer(&staticCache->mIndexBuffer);
//
//     mCurrentFrame++;
//     mCurrentDrawIndex = mCurrentIndex;
//     mCurrentIndex     = mCurrentFrame % ArrayLength(mFrameData);
//
//     VertexCache *newFrame            = &mFrameData[mCurrentIndex];
//     newFrame->mIndexBuffer.mOffset   = 0;
//     newFrame->mVertexBuffer.mOffset  = 0;
//     newFrame->mUniformBuffer.mOffset = 0;
//
//     renderer.R_MapGPUBuffer(&newFrame->mIndexBuffer);
//     renderer.R_MapGPUBuffer(&newFrame->mVertexBuffer);
//     renderer.R_MapGPUBuffer(&frameCache->mUniformBuffer);
//     renderer.R_MapGPUBuffer(&staticCache->mVertexBuffer);
//     renderer.R_MapGPUBuffer(&staticCache->mIndexBuffer);
// }
//
// inline u64 VertexCacheState::GetOffset(VC_Handle handle)
// {
//     u64 offset = (u64)((handle >> VERTEX_CACHE_OFFSET_SHIFT) & VERTEX_CACHE_OFFSET_MASK);
//     return offset;
// }
//
// inline u64 VertexCacheState::GetSize(VC_Handle handle)
// {
//     u64 size = (u64)((handle >> VERTEX_CACHE_SIZE_SHIFT) & VERTEX_CACHE_SIZE_MASK);
//     return size;
// }
// #endif
