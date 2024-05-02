#ifndef RENDER_CORE_H
#define RENDER_CORE_H

typedef u64 R_BufferHandle;
typedef u64 VC_Handle;
union R_Handle
{
    u64 u64[2];
    u32 u32[4];
};

#endif
