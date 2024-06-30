#include "globals.hlsli"
#include "ShaderInterop_Mesh.h"
#include "ShaderInterop_Culling.h"
#include "cull.hlsli"

[[vk::push_constant]] InstanceCullPushConstants push;

StructuredBuffer<GPUView> views : register(t1);

RWStructuredBuffer<DispatchIndirect> dispatchIndirectBuffer : register(u0);
RWStructuredBuffer<MeshChunk> meshChunks : register(u1);
RWStructuredBuffer<uint> occludedInstances : register(u2);

#if 1
RWStructuredBuffer<CullingStatistics> cullingStats : register(u3);
#endif

[numthreads(INSTANCE_CULL_GROUP_SIZE, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
#ifndef PASS
#error Pass is not defined.
#endif

#if PASS == FIRST_PASS
    uint meshIndex = dispatchThreadID.x;
    if (meshIndex > push.numInstances)
        return;
#elif PASS == SECOND_PASS
    if (dispatchThreadID.x > dispatchIndirectBuffer[INSTANCE_SECOND_PASS_DISPATCH_OFFSET].commandCount)
        return;
    uint meshIndex = occludedInstances[dispatchThreadID.x];
#endif

    MeshParams params = bindlessMeshParams[push.meshParamsDescriptor][meshIndex];
    float4x4 mvp = mul(views[0].worldToClip, params.localToWorld);

    bool skipFrustumCull = (PASS == SECOND_PASS);
    FrustumCullResults cullResults = ProjectBoxAndFrustumCull(params.minP, params.maxP, mvp,
                                                              views[0].p22, views[0].p23, skipFrustumCull);
    bool visible = cullResults.isVisible;
#if 1
    WaveInterlockedAddScalarTestNoOutput(cullingStats[0].numFrustumCulled, !visible, 1);
#endif

#if PASS == FIRST_PASS
    if (visible)
    {
        skipFrustumCull = false;
        float4x4 lastFrameMvp = mul(views[0].prevWorldToClip, params.localToWorld); 
        FrustumCullResults lastFrameResults = ProjectBoxAndFrustumCull(params.minP, params.maxP, lastFrameMvp,
                                                                       views[0].p22, views[0].p23, skipFrustumCull);
        bool wasOccluded = HZBOcclusionTest(lastFrameResults, push.screenSize);

        uint occludedInstanceOffset;
        WaveInterlockedAddScalarInGroupsTest(dispatchIndirectBuffer[INSTANCE_SECOND_PASS_DISPATCH_OFFSET].commandCount, 
                                             dispatchIndirectBuffer[INSTANCE_SECOND_PASS_DISPATCH_OFFSET].groupCountX, 
                                             INSTANCE_CULL_GROUP_SIZE, 
                                             wasOccluded,
                                             1,
                                             occludedInstanceOffset);
#if 1
        WaveInterlockedAddScalarTestNoOutput(cullingStats[0].numOcclusionCulled, wasOccluded, 1);
#endif
        if (wasOccluded)
        {
            occludedInstances[occludedInstanceOffset] = meshIndex;
        }
        visible = visible && !wasOccluded;
    }
#elif PASS == SECOND_PASS
    bool isOccluded = HZBOcclusionTest(cullResults, push.screenSize); 
#if 1
    WaveInterlockedAddScalarTestNoOutput(cullingStats[0].numOcclusionCulled, isOccluded, 1);
#endif
    visible = !isOccluded;
#endif // PASS == FIRST_PASS || PASS == SECOND_PASS

    if (visible)
    {
        uint chunkCount = (params.clusterCount + CHUNK_GROUP_SIZE - 1) / CHUNK_GROUP_SIZE;
        uint chunkStartIndex;

        WaveInterlockedAdd(dispatchIndirectBuffer[CLUSTER_DISPATCH_OFFSET].groupCountX, chunkCount, chunkStartIndex);
        for (uint i = 0; i < chunkCount; i++)
        {
            meshChunks[chunkStartIndex + i].clusterOffset = params.clusterOffset + i * CHUNK_GROUP_SIZE;
            meshChunks[chunkStartIndex + i].numClusters = min(CHUNK_GROUP_SIZE, params.clusterCount - i * CHUNK_GROUP_SIZE);
            meshChunks[chunkStartIndex + i].wasVisibleLastFrame = 0;//drawVisibility[instanceIndex];
        }
    }
}
