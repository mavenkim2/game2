#include "globals.hlsli"
#include "ShaderInterop_Mesh.h"
#include "ShaderInterop_Culling.h"
#include "cull.hlsli"

[[vk::push_constant]] InstanceCullPushConstants push;

StructuredBuffer<GPUView> views : register(t1);

RWStructuredBuffer<DispatchIndirect> dispatchIndirectBuffer : register(u0);
RWStructuredBuffer<MeshChunk> meshChunks : register(u1);
RWStructuredBuffer<uint> occludedInstances : register(u2);

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

    bool skipFrustumCull = 0;//(PASS == SECOND_PASS);
    FrustumCullResults cullResults = ProjectBoxAndFrustumCull(params.minP, params.maxP, mvp,
                                                              views[0].p22, views[0].p23, skipFrustumCull);
    bool visible = cullResults.isVisible;


#if PASS == FIRST_PASS
    skipFrustumCull = false;
    // use last frame's stuff
    float4x4 lastFrameMvp = mul(views[0].prevWorldToClip, params.localToWorld); // TODO: prevlocaltoworld
    FrustumCullResults lastFrameResults = ProjectBoxAndFrustumCull(params.minP, params.maxP, lastFrameMvp,
                                                                   views[0].prevP22, views[0].prevP23, skipFrustumCull);
    OcclusionResults occlusionResults = HZBOcclusionTest(lastFrameResults, push.screenSize);

    uint occludedInstanceOffset;
    WaveInterlockedAddScalarInGroupsTest(dispatchIndirectBuffer[INSTANCE_SECOND_PASS_DISPATCH_OFFSET].commandCount, 
                                 dispatchIndirectBuffer[INSTANCE_SECOND_PASS_DISPATCH_OFFSET].groupCountX, 
                                 64, occlusionResults.wasOccluded, 1, occludedInstanceOffset);
    if (0)//occlusionResults.wasOccluded)
    {
        occludedInstances[occludedInstanceOffset] = meshIndex;
    }
#elif PASS == SECOND_PASS
    OcclusionResults occlusionResults = HZBOcclusionTest(cullResults, push.screenSize); 
#endif // PASS == FIRST_PASS || PASS == SECOND_PASS
    //visible = occlusionResults.isVisible;

    // my understanding of this process:
    // there is a hierarchiacl z buffer (hzb), which just contains mips of a depth buffer down sampled 2x 
    // (so each texel in mip n + 1 represents the minimum depth of a 2x2 region in mip n). we find an aabb of the mesh sphere bounds 
    // projected into screen space, and then find the corresponding mip level where the aabb is one texel ( i think). 
    // we sample the hzb, do the comparison, and if it's greater it's occluded.
    // since the visible objects from last frame should be similar to the objects visible this frame, 
    // we render objects in two passes. in the first pass, we render all objects visible last frame, construct the 
    // hzb from these, create the pyramid mip chain from it, and then render false negatives (how?). something along these 
    // lines :)
    // If it's the first pass, or if it's the second pass and the object wasn't drawn in the first pass

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
