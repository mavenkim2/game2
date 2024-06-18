struct MeshCluster
{
    float3 minP;
    float3 maxP;
};

StructuredBuffer<MeshChunk> meshChunks : register(t0);
StructuredBuffer<uint> chunkCount : register(t1);
StructuredBuffer<MeshCluster> meshClusters : register(t2);
Texture2D depthPyramid : register(t3);

RWStructuredBuffer<uint> clusterVisibility : register(u0);
RWStructuredBuffer<MeshCluster> outputClusters : register(t3);

[[vk::push_constant]] BatchCullPushConstants push;

[numthreads(BATCH_GROUP_SIZE, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID)
{
    uint chunkID = groupID.x;
    if (chunkID > chunkCount[0]) 
        return;

    MeshChunk chunk = meshChunks[chunkID];

    uint clusterID = groupThreadID.x;
    if (clusterID >= chunk.numClusters)
        return;

    clusterID = chunk.clusterOffset + clusterID;
    MeshCluster cluster = meshClusters[clusterID];

    MeshBatch batch = chunk.chunkOffset
    uint clusterVisibilityResult = clusterVisibility[clusterID >> 5];
    uint isClusterVisible = clusterVisibilityResult & (1u << (clusterID & 31));

    // In the first pass, only render objects visible last frame
    bool skip = false;
    bool visible = true;
    if (!push.isSecondPass && isClusterVisible == 0)
        visible = false;

    // In the second pass, don't render objects that were previously rendered
    if (push.isSecondPass && chunk.wasVisibleLastFrame && isClusterVisible)
        skip = true;

    float4 aabb;
    float minZ;
    bool visible = ProjectBoxAndFrustumCull(cluster.minP, cluster.maxP, push.viewProjection, push.nearZ, 
                                            push.isSecondPass, push.p22, push.p23, aabb, minZ);
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
    if (visible && !skip)
    {
    }
}
