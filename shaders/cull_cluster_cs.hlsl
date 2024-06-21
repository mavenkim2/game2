#include "globals.hlsli"
#include "cull.hlsli"
#include "ShaderInterop_Mesh.h"
#include "ShaderInterop_Culling.h"

[[vk::push_constant]] ClusterCullPushConstants push;

StructuredBuffer<MeshChunk> meshChunks : register(t0);
//Texture2D depthPyramid : register(t3);

//RWStructuredBuffer<uint> clusterVisibility : register(u0);
RWStructuredBuffer<DispatchIndirect> dispatchIndirect : register(u0);
RWStructuredBuffer<uint> outputMeshClusterIndex : register(u1);

[numthreads(CLUSTER_CULL_GROUP_SIZE, 1, 1)]
void main(uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID)
{
    uint chunkID = groupID.x;
    uint chunkCount = dispatchIndirect[CLUSTER_DISPATCH_OFFSET].groupCountX;
    if (chunkID >= chunkCount)
        return;

    MeshChunk chunk = meshChunks[chunkID];
    uint clusterID = groupThreadID.x;
    if (clusterID >= chunk.numClusters)
        return;

    clusterID = chunk.clusterOffset + clusterID;

    MeshCluster cluster = bindlessMeshClusters[push.meshClusterDescriptor][clusterID];

    MeshParams params = bindlessMeshParams[push.meshParamsDescriptor][cluster.meshIndex];
    float4x4 mvp = mul(push.worldToClip, params.localToWorld);

    bool skip = false;
    bool visible = true;

#if 0
    uint clusterVisibilityResult = clusterVisibility[clusterID >> 5];
    uint isClusterVisible = clusterVisibilityResult & (1u << (clusterID & 31));

    // In the first pass, only render objects visible last frame
    if (!push.isSecondPass && isClusterVisible == 0)
        visible = false;

    // In the second pass, don't render objects that were previously rendered
    if (push.isSecondPass && chunk.wasVisibleLastFrame && isClusterVisible)
        skip = true;
#endif

    float4 aabb;
    float minZ;
    visible = true;
    //visible = ProjectBoxAndFrustumCull(cluster.minP, cluster.maxP, mvp, push.nearZ, push.farZ,
    //                                   push.isSecondPass, push.p22, push.p23, aabb, minZ);
#if 0
    if (visible && push.isSecondPass)
    {
        float width = (aabb.z - aabb.x) * push.pyramidWidth;
        float height = (aabb.w - aabb.y) * push.pyramidHeight;
        
        int lod = ceil(log2(max(width, height)));
        float depth = SampleLevel(samplerNearestClamp, depthPyramid, lod).x;

        visible = visible && minBoxZ < depth;
    }
    if (push.isSecondPass)
    {
        if (visible)
            InterlockedOr(clusterVisibility[clusterID >> 5], (1u << (clusterID & 31)));
        else 
            InterlockedAnd(clusterVisibility[clusterID >> 5], ~(1u << (clusterID & 31)));
    }
#endif

    bool isClusterVisible = visible && !skip;
    uint clusterOffset;
    WaveInterlockedAddScalarTest(dispatchIndirect[TRIANGLE_DISPATCH_OFFSET].groupCountX, isClusterVisible, 1, clusterOffset);

    if (isClusterVisible)
    {
        outputMeshClusterIndex[clusterOffset] = clusterID;
    }
}
