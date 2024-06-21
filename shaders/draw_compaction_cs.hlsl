#include "globals.hlsli"
#include "ShaderInterop_Culling.h"

[[vk::push_constant]] DrawCompactionPushConstant push;

StructuredBuffer<DrawIndexedIndirectCommand> indirectCommands : register(t0);
RWStructuredBuffer<uint> commandCount : register(u0);
RWStructuredBuffer<DrawIndexedIndirectCommand> outputIndirectCommands : register(u1);

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= push.drawCount)
        return;

    DrawIndexedIndirectCommand cmd = indirectCommands[dispatchThreadID.x];
    bool hasDraw = cmd.indexCount > 0;

    uint drawOffset;
    WaveInterlockedAddScalarTest(commandCount[0], hasDraw, 1, drawOffset);
    if (hasDraw)
    {
        outputIndirectCommands[drawOffset] = cmd;
    }
}
