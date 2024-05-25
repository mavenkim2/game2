#include "globals.hlsli"
#include "ShaderInterop_Mesh.h"

[[vk::push_constant]] SkinningPushConstants push;

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint vertexID = DTid.x;

    uint length;
    Buffer<float4> posBuffer = bindlessBuffersFloat4[push.vertexPos];
    posBuffer.GetDimensions(length);
    
    [branch]
    if (vertexID >= length) // TODO: is this necessary?
    {
        return;
    }

    ByteAddressBuffer skinningBuffer = bindlessBuffers[push.skinningBuffer];

    float3 pos = GetFloat3(push.vertexPos, vertexID);
    float3 nor = GetFloat3(push.vertexNor, vertexID);
    float3 tan = GetFloat3(push.vertexTan, vertexID);

    uint4 boneIds = GetUint4(push.vertexBoneId, vertexID);
    float4 boneWeights = GetFloat4(push.vertexBoneWeight, vertexID);

    RWByteAddressBuffer outPos = bindlessStorageBuffers[push.soPos];
    RWByteAddressBuffer outNor = bindlessStorageBuffers[push.soNor];
    RWByteAddressBuffer outTan = bindlessStorageBuffers[push.soTan];

    float4x4 boneTransform = skinningBuffer.Load<float4x4>((push.skinningOffset + boneIds[0]) * sizeof(float4x4)) * boneWeights[0];
    boneTransform += skinningBuffer.Load<float4x4>((push.skinningOffset + boneIds[1]) * sizeof(float4x4)) * boneWeights[1];
    boneTransform += skinningBuffer.Load<float4x4>((push.skinningOffset + boneIds[2]) * sizeof(float4x4)) * boneWeights[2];
    boneTransform += skinningBuffer.Load<float4x4>((push.skinningOffset + boneIds[3]) * sizeof(float4x4)) * boneWeights[3];

    pos = mul(boneTransform, float4(pos, 1.0)).xyz;
    nor = normalize(mul((float3x3)boneTransform, nor));
    tan = normalize(mul((float3x3)boneTransform, tan));

    outPos.Store<float3>(vertexID * sizeof(float3), pos);
    outNor.Store<float3>(vertexID * sizeof(float3), nor);
    outTan.Store<float3>(vertexID * sizeof(float3), tan);
}
