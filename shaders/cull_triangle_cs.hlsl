#include "globals.hlsli"
#include "ShaderInterop_Mesh.h"

[[vk::push_constant]] TriangleCullPushConstant push;

RWStructuredBuffer<DrawIndexedIndirectCommand> indirectCommands : register(u0);
RWStructuredBuffer<uint> outputIndices : register(u1);

groupshared uint indexGroupCount;

[numthreads(BATCH_SIZE, 1, 1)]
void main(uint3 groupID: SV_GroupID, uint3 groupThreadID: SV_GroupThreadID)
{
    if (groupThreadID.x == 0)
    {
        indexGroupCount = 0;
    }
    GroupMemoryBarrier();

    MeshBatch batch = bindlessMeshBatches[push.meshBatchDescriptor][groupID.x];
    uint drawID = batch.drawID;

    MeshGeometry geo = bindlessMeshGeometry[push.meshGeometryDescriptor][batch.meshIndex];
    MeshParams params = bindlessMeshParams[push.meshParamsDescriptor][batch.meshIndex];
    uint batchBaseIndexOffset = batch.indexOffset;

    // TODO: cluster backface culling
    // TODO: on the application side the batches are going to have to be split into groups of 64 triangles
    if (groupThreadID.x >= batch.indexCount / 3)
    {
        return;
    }
    uint indices[3] = 
    {
        GetUint(geo.vertexInd, batchBaseIndexOffset + groupThreadID.x * 3 + 0),
        GetUint(geo.vertexInd, batchBaseIndexOffset + groupThreadID.x * 3 + 1),
        GetUint(geo.vertexInd, batchBaseIndexOffset + groupThreadID.x * 3 + 2)
    };

    float4 vertices[3] = 
    {
        mul(params.transform, float4(GetFloat3(geo.vertexPos, indices[0]), 1.0)),
        mul(params.transform, float4(GetFloat3(geo.vertexPos, indices[1]), 1.0)),
        mul(params.transform, float4(GetFloat3(geo.vertexPos, indices[2]), 1.0))
    };

    bool cull = false;
    // Backface triangle culling
    //float3x3 m = 
    //{
    //    vertices[0].xyw, vertices[1].xyw, vertices[2].xyw
    //};

    //cull = cull || (determinant(m) > 0);

    // TODO: this seems like double work
    uint indexAppendCount = WaveActiveCountBits(!cull);
    uint waveOffset = 0;
    if (WaveIsFirstLane() && indexAppendCount > 0)
    {
        InterlockedAdd(indirectCommands[drawID].indexCount, indexAppendCount * 3);
        InterlockedAdd(indexGroupCount, indexAppendCount * 3, waveOffset);
    }
    waveOffset = WaveReadLaneFirst(waveOffset) + batch.outputIndexOffset;

    if (!cull)
    {
        uint indexIndex = WavePrefixSum(3);
        outputIndices[waveOffset + indexIndex + 0] = indices[0];
        outputIndices[waveOffset + indexIndex + 1] = indices[1];
        outputIndices[waveOffset + indexIndex + 2] = indices[2];
    }

// TODO: I can't just have one draw call per mesh subset. this is going to have to change once culling starts
    if (groupThreadID.x == 0 && groupID.x == batch.firstBatch)
    {
        indirectCommands[drawID].instanceCount = 1;
        indirectCommands[drawID].firstIndex = batch.outputIndexOffset;
        indirectCommands[drawID].vertexOffset = 0;
        indirectCommands[drawID].firstInstance = drawID;
    }
}
