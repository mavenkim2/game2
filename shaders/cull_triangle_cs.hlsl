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
    GroupMemoryBarrierWithGroupSync();

    MeshBatch batch = bindlessMeshBatches[push.meshBatchDescriptor][groupID.x];
    uint drawID = batch.drawID;

    MeshGeometry geo = bindlessMeshGeometry[push.meshGeometryDescriptor][batch.meshIndex];
    MeshParams params = bindlessMeshParams[push.meshParamsDescriptor][batch.meshIndex];
    uint batchBaseIndexOffset = batch.indexOffset;

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
#if 0
    float3x3 m = 
    {
        vertices[0].xyw, vertices[1].xyw, vertices[2].xyw
    };

    cull = cull || (determinant(m) > 0);
#endif

#if 1
    float2 screen = float2(push.screenWidth, push.screenHeight);
    float2 clipVertices[3] = 
    {
        ((vertices[0].xy / vertices[0].w) * 0.5 + 0.5) * screen,
        ((vertices[1].xy / vertices[1].w) * 0.5 + 0.5) * screen,
        ((vertices[2].xy / vertices[2].w) * 0.5 + 0.5) * screen,
    };

    float2 edge1 = clipVertices[1] - clipVertices[0];
    float2 edge2 = clipVertices[2] - clipVertices[0];

    cull = cull || (edge1.x * edge2.y >= edge1.y * edge2.x);
#endif

    // Small triangle culling
#if 1
    float2 aabbMin = min(clipVertices[0], min(clipVertices[1], clipVertices[2]));
    float2 aabbMax = max(clipVertices[0], max(clipVertices[1], clipVertices[2]));
    // TODO: more robust handling of subpixel precision
    float subpixelPrecision = 1.0 / 256.0;

    cull = cull || (round(aabbMin.x - subpixelPrecision) == round(aabbMax.x) || round(aabbMin.y) == round(aabbMax.y + subpixelPrecision));
#endif

    uint indexAppendCount = WaveActiveCountBits(!cull) * 3;
    uint waveOffset = 0;
    if (WaveIsFirstLane() && indexAppendCount > 0)
    {
        InterlockedAdd(indirectCommands[drawID].indexCount, indexAppendCount, waveOffset);
    }
    waveOffset = WaveReadLaneFirst(waveOffset) + batch.outputIndexOffset;

    uint indexIndex = WavePrefixCountBits(!cull) * 3;
    if (!cull)
    {
        outputIndices[waveOffset + indexIndex + 0] = indices[0];
        outputIndices[waveOffset + indexIndex + 1] = indices[1];
        outputIndices[waveOffset + indexIndex + 2] = indices[2];
    }

    if (groupThreadID.x == 0)
    {
        indirectCommands[drawID].instanceCount = 1;
        indirectCommands[drawID].firstIndex = batch.outputIndexOffset;
        indirectCommands[drawID].vertexOffset = 0;
        indirectCommands[drawID].firstInstance = batch.subsetID;
    }
}
