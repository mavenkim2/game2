#include "globals.hlsli"
#include "ShaderInterop_Mesh.h"

#ifdef SHADOW_PASS
#endif

#ifdef MESH_PASS
#endif

[[vk::push_constant]] PushConstant push;

struct VertexInput
{
    uint vertexID: SV_VertexID;
};

struct FragmentInput
{
    precise float4 pos : SV_POSITION;
#ifdef MESH_PASS
    float2 uv : TEXCOORD;
    float4 viewFragPos : VIEW_FRAG_POS;
    float4 worldFragPos : WORLD_FRAG_POS;
    // TODO: calculate tbn in fragment shader
    float3x3 tbn : TBN;
#endif
};

UNIFORM(skinning, SKINNING_BIND)
{
    float4x4 skinningTransforms[1024];
}

#ifdef COMPILE_VS
// Vertex shader
FragmentInput main(VertexInput input)
{
    FragmentInput output;

    float3 pos = GetFloat3(push.vertexPos, input.vertexID);
    uint4 boneIds = GetUint4(push.vertexBoneId, input.vertexID);
    float4 boneWeights = GetFloat4(push.vertexBoneWeight, input.vertexID);

#ifdef MESH_PASS
    float2 uv = GetFloat2(push.vertexUv, input.vertexID);
    float3 n = GetFloat3(push.vertexNor, input.vertexID);
    float3 tangent = GetFloat3(push.vertexTan, input.vertexID);
#endif

    float4x4 modelToWorldMatrix = rParams[push.modelIndex].modelMatrix;
    float4 modelSpacePos;
    if (push.skinningOffset != -1)
    {
        float4x4 boneTransform = skinningTransforms[push.skinningOffset + boneIds[0]] * boneWeights[0];
        boneTransform += skinningTransforms[push.skinningOffset + boneIds[1]] * boneWeights[1];
        boneTransform += skinningTransforms[push.skinningOffset + boneIds[2]] * boneWeights[2];
        boneTransform += skinningTransforms[push.skinningOffset + boneIds[3]] * boneWeights[3];

        modelSpacePos = mul(boneTransform, float4(pos, 1.0));
        // TODO: append the light vp to the model matrix in cpu code

#ifdef MESH_PASS
        modelToWorldMatrix = modelToWorldMatrix * boneTransform;
#endif

    }
    else 
    {
        modelSpacePos = float4(pos, 1.0);
    }
#ifdef MESH_PASS
    output.pos = mul(rParams[push.modelIndex].transform, modelSpacePos);
#endif
#ifdef SHADOW_PASS
    output.pos = mul(rLightViewProjectionMatrices[push.cascadeNum] * modelToWorldMatrix, float4(pos, 1.0));
#endif

#ifdef MESH_PASS
    output.uv = uv;
    output.viewFragPos = mul(rParams[push.modelIndex].modelViewMatrix, modelSpacePos);
    output.worldFragPos = mul(modelToWorldMatrix, modelSpacePos);

    float3 tN = normalize(mul((float3x3)(modelToWorldMatrix), n));
    float3 tT = normalize(mul((float3x3)(modelToWorldMatrix), tangent));
    float3 tB = cross(tN, tT);
    output.tbn = float3x3(tT, tB, tN);
#endif

    return output;
}
#endif

#ifdef MESH_PASS
Texture2DArray shadowMaps : register(SLOT(t, SHADOW_MAP_BIND));
#endif

// Pixel shader
#ifdef COMPILE_FS
float4 main(FragmentInput fragment) : SV_Target
{
    float3 albedo = float3(1, 1, 1);
    float3 normal = float3(1, 1, 1);
    if (push.albedo >= 0)
    {
        albedo = bindlessTextures[push.albedo].Sample(samplerLinearWrap, fragment.uv).rgb;
    }
    if (push.normal >= 0)
    {
        normal = normalize(bindlessTextures[push.normal].Sample(samplerLinearWrap, fragment.uv).rgb * 2 - 1);
    }
    float viewZ = abs(fragment.viewFragPos.z);

    int shadowIndex = 3;
    for (int i = 0; i < 3; i++)
    {
        if (viewZ <= rCascadeDistances[i])
        {
            shadowIndex = i;
            break;
        }
    }

    float4 lightSpacePos = mul(rLightViewProjectionMatrices[shadowIndex], fragment.worldFragPos);
    lightSpacePos.xyz /= lightSpacePos.w;

    lightSpacePos.xy = lightSpacePos.xy * 0.5f + 0.5f;

    // Shadow bias
    float bias = max(0.1 * (1.0 - dot(normal, rLightDir.xyz)), 0.005);
    float biasModifier = 4;

    bias *= biasModifier / (rCascadeDistances[shadowIndex]);
    bias += 0.0015;

    // PCF
    float3 dim;
    shadowMaps.GetDimensions(dim.x, dim.y, dim.z);

    float2 texelSize = 1.0 / dim.xy;
    float shadow = 0.f;
    for (float x = -1.5f; x <= 1.5f; x += 1.f)
    {
        for (float y = -1.5f; y <= 1.5f; y += 1.f)
        {
            float2 shadowUV = lightSpacePos.xy + float2(x, y) * texelSize;
            float depth = shadowMaps.SampleCmpLevelZero(samplerShadowMap, float3(shadowUV, shadowIndex), lightSpacePos.z - bias).r;
        }
    }
    shadow /= 16.f;

    normal = normalize(mul(fragment.tbn, normal));
    float3 l = rLightDir.xyz;
    float3 v = normalize(rViewPos.xyz - fragment.worldFragPos.xyz);

    float nDotL = max(dot(normal, l), 0.0);
    float3 h = normalize(l + v);
    float nDotH = max(dot(normal, h), 0.0);

    //AMBIENT
    float3 ambient = 0.3f * albedo;
    //DIFFUSE
    float3 diffuse = nDotL * albedo;

    //SPECULAR
    float shininess = 16;
    float specularStrength = pow(nDotH, shininess);
    // float3 reflectVector = -l + 2 * dot(n, l) * n;
    // float specularStrength = pow(max(dot(reflectVector, v), 0.f), 64);

    float spec = specularStrength;
    float3 specular = spec * albedo;

    float3 color = (ambient + diffuse + specular); // * (1 - .8 * shadow);
    float4 outColor = float4(color, 1.0);

    return outColor;
}
#endif
