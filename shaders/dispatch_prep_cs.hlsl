#include "globals.hlsli"
#include "ShaderInterop_Culling.h"
RWStructuredBuffer<DispatchIndirect> dispatch : register(u0);

[[vk::push_constant]] DispatchPrepPushConstant push;

[numthreads(64, 1, 1)]
void main(uint3 groupThreadID : SV_GroupThreadID)
{
    if (groupThreadID.x == 0)
    {
        dispatch[push.index].groupCountX = 64;
        dispatch[push.index].groupCountY = min((dispatch[push.index].commandCount + 63) / 64, 65535);
        dispatch[push.index].groupCountZ = 1;
    }
}
