#include "globals.hlsli"
#include "ShaderInterop_Mesh.h"

RWStructuredBuffer<DrawIndexedIndirectCommand> indirectCommands : register(u0);
[numthreads(CLUSTER_SIZE, 1, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    indirectCommands[dispatchThreadID.x].indexCount = 0;
}
