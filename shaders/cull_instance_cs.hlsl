#include "globals.hlsli"
#include "ShaderInterop_Mesh.h"
#include "ShaderInterop_Culling.h"
#include "cull.hlsli"

[[vk::push_constant]] InstanceCullPushConstants push;

//Texture2D depthPyramid : register(t0);

RWStructuredBuffer<DispatchIndirect> dispatchIndirectBuffer : register(u0);
RWStructuredBuffer<MeshChunk> meshChunks : register(u1);
//RWStructuredBuffer<uint> drawVisibility: register(u2);

[numthreads(INSTANCE_CULL_GROUP_SIZE, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint meshIndex = dispatchThreadID.x;
    if (meshIndex > push.numInstances)
        return;

    MeshParams params = bindlessMeshParams[push.meshParamsDescriptor][meshIndex];
    float4x4 mvp = mul(push.worldToClip, params.localToWorld);

    float4 aabb;
    float minBoxZ;

    bool visible = ProjectBoxAndFrustumCull(params.minP, params.maxP, mvp, push.nearZ, push.farZ,
                                            push.isSecondPass, push.p22, push.p23, aabb, minBoxZ);
#if 0
    if (visible && push.isSecondPass)
    {
        float width = (aabb.z - aabb.x) * push.pyramidWidth;
        float height = (aabb.w - aabb.y) * push.pyramidHeight;

        int lod = ceil(log2(max(width, height)));
        float depth = SampleLevel(samplerNearestClamp, depthPyramid, lod).x;

        visible = visible && minBoxZ < depth;
    }
#endif

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

    if (visible)// && (push.isSecondPass == 0))// || drawVisibility[instanceIndex] == 0))
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

    //if (visible && push.isSecondPass)
    //{
    //    drawVisibility[instanceIndex] = visible ? 1 : 0;
    //}
}
