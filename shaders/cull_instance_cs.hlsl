#include "globals.hlsli"
#include "ShaderInterop_Mesh.h"
#include "ShaderInterop_Culling.h"

[[vk::push_constant]] InstanceCullPushConstants push;

StructuredBuffer<InstanceDrawData> instanceDrawData : register(t0);
Texture2D depthPyramid : register(t1);

RWStructuredBuffer<uint> chunkCounter : register(u0);
RWStructuredBuffer<MeshChunkGroup> meshChunks : register(u1);

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint instanceIndex = dispatchThreadID.x;
    if (instanceIndex > drawCount) 
        return;

    uint meshIndex = instanceDrawData[instanceIndex].meshIndex;
    MeshParams params = bindlessMeshParams[meshParamsDescriptor][meshIndex];

    //float3 center = params.modelMatrix * params.center;
    //float radius = params.scale * params.radius;

    float4 aabb;
    float minBoxZ;

    bool visible = ProjectBoxAndFrustumCull(params.minP, params.maxP, params.transform, push.zNear, 
                                            push.isSecondPass, push.A, push.B, aabb, minBoxZ);
    if (visible && push.isSecondPass)
    {
        float width = (aabb.z - aabb.x) * push.pyramidWidth;
        float height = (aabb.w - aabb.y) * push.pyramidHeight;

        int lod = ceil(log2(max(width, height)));
        float depth = SampleLevel(samplerNearestClamp, depthPyramid, lod).x;

        visible = visible && minBoxZ < depth;
    }

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

    if (visible && (!push.isSecondPass || drawVisibility[instanceIndex] == 0))
    {
        uint chunkCount = (params.batchCount + CHUNK_GROUP_SIZE - 1) / CHUNK_GROUP_SIZE;
        uint chunkStartIndex;

        InterlockedAdd(chunkCounter[0], chunkCount, chunkStartIndex);
        for (uint i = 0; i < chunkCount; i++)
        {
            meshChunks[chunkStartIndex + i].meshIndex = meshIndex;
            meshChunks[chunkStartIndex + i].clusterOffset = params.batchOffset + i * CHUNK_GROUP_SIZE;
            meshChunks[chunkStartIndex + i].numClusters = min(CHUNK_GROUP_SIZE, params.batchCount - i * CHUNK_GROUP_SIZE);
            meshChunks[chunkStartIndex + i].wasVisibleLastFrame = drawVisibility[instanceIndex];
        }
    }

    if (visible && push.isSecondPass)
    {
        drawVisibility[instanceIndex] = visible ? 1 : 0;
    }
}

#if 0
    // Frustum AABB culling
    // a lot of registers
    float maxX0 = projection[0][0] * params.maxP.x;
    float minX0 = projection[0][0] * params.minP.x;
    float maxY0 = projection[0][1] * params.maxP.y;
    float minY0 = projection[0][1] * params.minP.y;
    float maxZ0 = projection[0][2] * params.maxP.z + projection[0][3];
    float minZ0 = projection[0][2] * params.minP.z + projection[0][3];

    float maxX1 = projection[1][0] * params.maxP.x;
    float minX1 = projection[1][0] * params.minP.x;
    float maxY1 = projection[1][1] * params.maxP.y;
    float minY1 = projection[1][1] * params.minP.y;
    float maxZ1 = projection[1][2] * params.maxP.z + projection[1][3];
    float minZ1 = projection[1][2] * params.minP.z + projection[1][3];

    float maxX2 = projection[3][0] * params.maxP.x;
    float minX2 = projection[3][0] * params.minP.x;
    float maxY2 = projection[3][1] * params.maxP.y;
    float minY2 = projection[3][1] * params.minP.y;
    float maxZ2 = projection[3][2] * params.maxP.z + projection[3][3];
    float minZ2 = projection[3][2] * params.minP.z + projection[3][3];

    bool visible = 1;
    for (uint i = 0; i < 8; i++)
    {
        float4 corner;
        corner.x = (i & 1) ? maxX0 : minX0;
        corner.x += (i & 2) ? maxY0 : minY0;
        corner.x += (i & 4) ? maxZ0 : minZ0;

        corner.y = (i & 1) ? maxX1 : minX1;
        corner.y += (i & 2) ? maxY1 : minY1;
        corner.y += (i & 4) ? maxZ1 : minZ1;

        corner.w = (i & 1) ? maxX2 : minX2;
        corner.w += (i & 2) ? maxY2 : minY2;
        corner.w += (i & 4) ? maxZ2 : minZ2;

        visible = visible && (-corner.w <= corner.x) && (corner.x <= corner.w);
        visible = visible && (-corner.w <= corner.y) && (corner.y <= corner.w);
        visible = visible && (zNear <= corner.w) && (corner.w <= zFar);
    }
#endif
