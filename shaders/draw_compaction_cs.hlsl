#include "globals.hlsli"
#include "ShaderInterop_Culling.h"

//[[vk::push_constant]] DrawCompactionPushConstant push;

StructuredBuffer<DrawIndexedIndirectCommand> indirectCommands : register(t0);
StructuredBuffer<uint> meshClusterIndices : register(t1);
StructuredBuffer<DispatchIndirect> dispatchIndirectBuffer : register(t2);

RWStructuredBuffer<uint> commandCount : register(u0);
RWStructuredBuffer<DrawIndexedIndirectCommand> outputIndirectCommands : register(u1);

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint clusterCount = dispatchIndirectBuffer[TRIANGLE_DISPATCH_OFFSET].groupCountX;
    uint clusterIndexIndex = dispatchThreadID.x;
    if (clusterIndexIndex >= clusterCount)
        return;

    uint clusterIndex = meshClusterIndices[clusterIndexIndex];

    DrawIndexedIndirectCommand cmd = indirectCommands[clusterIndex];
    bool hasDraw = cmd.indexCount > 0;

    uint drawOffset;
    WaveInterlockedAddScalarTest(commandCount[0], hasDraw, 1, drawOffset);
    if (hasDraw)
    {
        outputIndirectCommands[drawOffset] = cmd;
    }
}
