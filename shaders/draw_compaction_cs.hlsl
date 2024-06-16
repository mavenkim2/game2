#include "globals.hlsli"
#include "ShaderInterop_Mesh.h"

[[vk::push_constant]] DrawCompactionPushConstant push;

RWStructuredBuffer<DrawIndexedIndirectCommand> indirectCommands : register(u0);
RWStructuredBuffer<uint> commandCount : register(u1);

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= push.drawCount)
        return;

    DrawIndexedIndirectCommand cmd = indirectCommands[dispatchThreadID.x];
    bool hasDraw = cmd.indexCount > 0;
    uint numDrawCalls = WaveActiveCountBits(hasDraw);
    uint waveDrawOffset = 0;
    if (WaveIsFirstLane() && numDrawCalls > 0)
    {
        InterlockedAdd(commandCount[0], numDrawCalls, waveDrawOffset);
    }
    waveDrawOffset = WaveReadLaneFirst(waveDrawOffset);
    uint drawIndex = WavePrefixCountBits(hasDraw);

    if (hasDraw)
    {
        indirectCommands[waveDrawOffset + drawIndex] = cmd;
    }
}
