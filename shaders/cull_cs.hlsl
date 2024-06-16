//struct MeshDrawData
//{
//    uint meshIndex;
//};
//
//struct Camera 
//{
//    mat4 projection;
//    float nearZ;
//    float farZ;
//};
//
//struct MeshDrawCommand
//{
//    uint meshIndex;
//
//    uint indexCount;
//	uint instanceCount;
//	uint firstIndex;
//	uint vertexOffset;
//	uint firstInstance;
//};
//
//RWStructuredBuffer<uint> drawCountBuffer : register(u0);
//RWStructuredBuffer<MeshDrawCommand> meshDrawCommandBuffer : register(u1);
//
//[numthreads(8, 1, 1)]
//void main(uint3 DTid : SV_DispatchThreadID)
//{
//    // TODO: this may have to be a subset index, which maps each subset to a draw call. the subset index is used to get a 
//    // mesh index. the mesh index is used to get the aabb. each subset has info on the count, start, and offset.
//
//    uint meshIndex = DTid.x;
//    if (meshIndex > drawCount) 
//        return;
//
//    MeshParams params = bindlessMeshParams[?][meshIndex];
//    
//    // Frustum AABB culling
//    // a lot of registers
//    float maxX0 = projection[0][0] * params.maxP.x;
//    float minX0 = projection[0][0] * params.minP.x;
//    float maxY0 = projection[0][1] * params.maxP.y;
//    float minY0 = projection[0][1] * params.minP.y;
//    float maxZ0 = projection[0][2] * params.maxP.z + projection[0][3];
//    float minZ0 = projection[0][2] * params.minP.z + projection[0][3];
//
//    float maxX1 = projection[1][0] * params.maxP.x;
//    float minX1 = projection[1][0] * params.minP.x;
//    float maxY1 = projection[1][1] * params.maxP.y;
//    float minY1 = projection[1][1] * params.minP.y;
//    float maxZ1 = projection[1][2] * params.maxP.z + projection[1][3];
//    float minZ1 = projection[1][2] * params.minP.z + projection[1][3];
//
//    float maxX2 = projection[3][0] * params.maxP.x;
//    float minX2 = projection[3][0] * params.minP.x;
//    float maxY2 = projection[3][1] * params.maxP.y;
//    float minY2 = projection[3][1] * params.minP.y;
//    float maxZ2 = projection[3][2] * params.maxP.z + projection[3][3];
//    float minZ2 = projection[3][2] * params.minP.z + projection[3][3];
//
//    bool visible = 1;
//    for (uint i = 0; i < 8; i++)
//    {
//        float4 corner;
//        corner.x = (i & 1) ? maxX0 : minX0;
//        corner.x += (i & 2) ? maxY0 : minY0;
//        corner.x += (i & 4) ? maxZ0 : minZ0;
//
//        corner.y = (i & 1) ? maxX1 : minX1;
//        corner.y += (i & 2) ? maxY1 : minY1;
//        corner.y += (i & 4) ? maxZ1 : minZ1;
//
//        corner.w = (i & 1) ? maxX2 : minX2;
//        corner.w += (i & 2) ? maxY2 : minY2;
//        corner.w += (i & 4) ? maxZ2 : minZ2;
//
//        visible = visible && (-corner.w <= corner.x) && (corner.x <= corner.w);
//        visible = visible && (-corner.w <= corner.y) && (corner.y <= corner.w);
//        visible = visible && (zNear <= corner.w) && (corner.w <= zFar);
//    }
//
//    if (visible)
//    {
//        uint drawCount;
//        InterlockedAdd(drawCountBuffer[0], 1, drawCount);
//        
//        meshDrawCommandBuffer[drawCount].meshIndex = meshIndex;
//        meshDrawCommandBuffer[drawCount].indexCount = bindlessMeshGeometry[?][meshIndex].indexCount;
//        meshDrawCommandBuffer[drawCount].instanceCount = 1;
//        meshDrawCommandBuffer[drawCount].firstIndex = meshSubset.indexStart;
//        meshDrawCommandBuffer[drawCount].vertexOffset = meshSubset.something;
//        meshDrawCommandBuffer[drawCount].firstInstance = 0;
//    }
//}
