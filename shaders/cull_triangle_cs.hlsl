#include "globals.hlsli"
#include "ShaderInterop_Mesh.h"
#include "ShaderInterop_Culling.h"

[[vk::push_constant]] TriangleCullPushConstant push;

StructuredBuffer<uint> meshClusterIndices : register(t0);
StructuredBuffer<DispatchIndirect> dispatchIndirect : register(t1);

RWStructuredBuffer<DrawIndexedIndirectCommand> indirectCommands : register(u0);
RWStructuredBuffer<uint> outputIndices : register(u1);

[numthreads(CLUSTER_SIZE, 1, 1)]
void main(uint3 groupID : SV_GroupID, uint3 groupThreadID: SV_GroupThreadID, uint groupIndex : SV_GroupIndex)
{
    uint clusterID = meshClusterIndices[groupID.x];//groupIndex];
    uint clusterCount = dispatchIndirect[TRIANGLE_DISPATCH_OFFSET].groupCountX;
    if (clusterID >= clusterCount)
        return;

    MeshCluster cluster = bindlessMeshClusters[push.meshClusterDescriptor][clusterID];
    MeshGeometry geo = bindlessMeshGeometry[push.meshGeometryDescriptor][cluster.meshIndex];
    MeshParams params = bindlessMeshParams[push.meshParamsDescriptor][cluster.meshIndex];

    if (groupThreadID.x >= cluster.indexCount / 3)
        return;

    uint indices[3] = 
    {
        GetUint(geo.vertexInd, cluster.indexOffset + groupThreadID.x * 3 + 0),
        GetUint(geo.vertexInd, cluster.indexOffset + groupThreadID.x * 3 + 1),
        GetUint(geo.vertexInd, cluster.indexOffset + groupThreadID.x * 3 + 2)
    };


    float4 vertices[3] = 
    {
        mul(push.viewProjection, mul(params.modelToWorld, float4(GetFloat3(geo.vertexPos, indices[0]), 1.0))),
        mul(push.viewProjection, mul(params.modelToWorld, float4(GetFloat3(geo.vertexPos, indices[1]), 1.0))),
        mul(push.viewProjection, mul(params.modelToWorld, float4(GetFloat3(geo.vertexPos, indices[2]), 1.0))),
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

    bool isTriangleBehindNearPlane = vertices[0].w < 0 && vertices[1].w < 0 && vertices[2].w < 0;
    bool isTriangleInFrontOfNearPlane = vertices[0].w > 0 && vertices[1].w > 0 && vertices[2].w > 0;
    cull = cull || (isTriangleInFrontOfNearPlane && (edge1.x * edge2.y >= edge1.y * edge2.x));
#endif

    // Small triangle culling
#if 1
    float2 aabbMin = min(clipVertices[0], min(clipVertices[1], clipVertices[2]));
    float2 aabbMax = max(clipVertices[0], max(clipVertices[1], clipVertices[2]));
    // TODO: more robust handling of subpixel precision
    float subpixelPrecision = 1.0 / 256.0;

    cull = cull || (round(aabbMin.x - subpixelPrecision) == round(aabbMax.x) || round(aabbMin.y) == round(aabbMax.y + subpixelPrecision));
#endif

    // Frustum culling
#if 1
    float minX = min(clipVertices[0].x, min(clipVertices[1].x, clipVertices[2].x));
    float maxX = max(clipVertices[0].x, max(clipVertices[1].x, clipVertices[2].x));
    float minY = min(clipVertices[0].y, min(clipVertices[1].y, clipVertices[2].y));
    float maxY = max(clipVertices[0].y, max(clipVertices[1].y, clipVertices[2].y));

    cull = cull || (isTriangleInFrontOfNearPlane && (maxX < 0 || minX > screen.x || maxY < 0 || minY > screen.y));
    cull = cull || isTriangleBehindNearPlane;
#endif

    uint indexAppendCount = WaveActiveCountBits(!cull) * 3;
    uint waveOffset = 0;
    if (WaveIsFirstLane() && indexAppendCount > 0)
    {
        InterlockedAdd(indirectCommands[clusterID].indexCount, indexAppendCount, waveOffset);
    }
    waveOffset = WaveReadLaneFirst(waveOffset) + clusterID * CLUSTER_SIZE * 3;

    uint indexIndex = WavePrefixCountBits(!cull) * 3;
    if (!cull)
    {
        outputIndices[waveOffset + indexIndex + 0] = indices[0];
        outputIndices[waveOffset + indexIndex + 1] = indices[1];
        outputIndices[waveOffset + indexIndex + 2] = indices[2];
    }

    if (groupThreadID.x == 0)
    {
        indirectCommands[clusterID].instanceCount = 1;
        indirectCommands[clusterID].firstIndex = clusterID * CLUSTER_SIZE * 3;
        indirectCommands[clusterID].vertexOffset = 0;
        indirectCommands[clusterID].firstInstance = clusterID;
    }
}
