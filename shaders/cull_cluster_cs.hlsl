#include "globals.hlsli"
#include "cull.hlsli"
#include "ShaderInterop_Mesh.h"
#include "ShaderInterop_Culling.h"

[[vk::push_constant]] ClusterCullPushConstants push;

StructuredBuffer<MeshChunk> meshChunks : register(t0);
StructuredBuffer<GPUView> views : register(t1);

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
    float4x4 mvp = mul(views[0].worldToClip, params.localToWorld);

    bool skip = false;
    bool visible = true;

    float4 aabb;
    float minZ;
    visible = true;
    //visible = ProjectBoxAndFrustumCull(cluster.minP, cluster.maxP, mvp, push.nearZ, push.farZ,
    //                                   push.isSecondPass, push.p22, push.p23, aabb, minZ);

    bool isClusterVisible = visible && !skip;
    uint clusterOffset;
    WaveInterlockedAddScalarInGroupsTest(dispatchIndirect[TRIANGLE_DISPATCH_OFFSET].groupCountX, 
                                         dispatchIndirect[DRAW_COMPACTION_DISPATCH_OFFSET].groupCountX, 
                                         64, 
                                         isClusterVisible, 
                                         1, 
                                         clusterOffset);

    if (isClusterVisible)
    {
        outputMeshClusterIndex[clusterOffset] = clusterID;
    }
}
