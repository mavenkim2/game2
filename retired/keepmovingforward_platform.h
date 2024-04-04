// #ifndef KEEPMOVINGFORWARD_PLATFORM_H
// #define KEEPMOVINGFORWARD_PLATFORM_H
//
// // TODO: this file needs to go
// #include "crack.h"
// #ifdef LSP_INCLUDE
// #include "render/render.h"
// #endif
//
// #if INTERNAL
// struct DebugPlatformHandle
// {
//     void *handle;
// };
//
// #define DEBUG_PLATFORM_GET_RESOLUTION(name) V2 name(DebugPlatformHandle handle)
// typedef DEBUG_PLATFORM_GET_RESOLUTION(DebugPlatformGetResolutionFunctionType);
// #endif
//
// // Forward declarations
// struct RenderState;
//
// typedef void PlatformToggleCursorFunctionType(b32 value);
//
// void PlatformToggleCursor(b32 value);
//
// // struct GameMemory
// // {
// //     b32 isInitialized;
// //
// //     u64 PersistentStorageSize;
// //     void *PersistentStorageMemory;
// //
// //     u64 TransientStorageSize;
// //     void *TransientStorageMemory;
// //
// //     PlatformToggleCursorFunctionType *PlatformToggleCursor;
// //
// //     r_allocate_texture_2D *R_AllocateTexture2D;
// //     r_delete_texture_2D *R_DeleteTexture2D;
// //     r_allocate_buffer *R_AllocateBuffer;
// // };
// //
// // struct GameOffscreenBuffer
// // {
// //     void *memory;
// //     int width;
// //     int height;
// //     int pitch;
// //     int bytesPerPixel;
// // };
//
// #endif
