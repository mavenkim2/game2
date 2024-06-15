[[vk::push_constant]] TriangleCullPushConstant push;

RWStructuredBuffer<DrawIndexedIndirectCommand> indirectCommands : register(u0);
RWStructuredBuffer<uint> outputIndices : register(u1);

groupshared uint groupIndexCount;

[numthreads(BATCH_SIZE, 1, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID, uint3 groupID: SV_GroupID, uint3 groupThreadID: SV_GroupThreadID)
{
    MeshBatch batch = bindlessMeshBatches[push.meshBatchDescriptor][groupID.x];
    uint drawID = batch.drawID;
    MeshGeometry geo = bindlessMeshGeometry[push.meshGeometryDescriptor][batch.meshIndex];
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

    float3 vertices[3] = 
    {
        mul(mvp, float4(GetFloat3(geo.vertexPos, indices[0]), 1.0)),
        mul(mvp, float4(GetFloat3(geo.vertexPos, indices[1]), 1.0)),
        mul(mvp, float4(GetFloat3(geo.vertexPos, indices[2]), 1.0))
    };

    // Backface triangle culling
    float3x3 m = 
    {
        vertices[0].xyw, vertices[1].xyw, vertices[2].xyw
    };

    bool cull = false;
    cull = cull || (determinant(m) > 0);

// TODO: figure out what's happening on the hardware side of things here
    uint indexAppendCount = WaveActiveCountBits(!cull);
    uint waveOffset = 0;
    if (WaveIsFirstLane() && indexAppendCount > 0)
    {
        InterlockedAdd(indirectCommands[drawID].indexCount, indexAppendCount, waveOffset);
    }
    waveOffset = WaveReadLaneFirst(waveOffset);

    if (!cull)
    {
        uint indexIndex = WavePrefixSum(3);
        // TODO: additional offset since this is one giant buffer for all meshes
        outputIndices[waveOffset + indexIndex + 0] = indices[0];
        outputIndices[waveOffset + indexIndex + 1] = indices[1];
        outputIndices[waveOffset + indexIndex + 2] = indices[2];
    }

    if (groupThreadID.x == 0 && groupID.x == batch.firstBatch)
    {
        indirectCommands[drawID].instanceCount = 1;
        indirectCommands[drawID].firstIndex = waveOffset;
        indirectCommands[drawID].vertexOffset = 0;
        indirectCommands[drawID].firstInstance = drawID;
    }
}
