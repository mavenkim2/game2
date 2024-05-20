#version 450
#include "global.glsl"

layout(location = 0) out vec4 outColor;

layout(binding = MODEL_PARAMS_BIND) uniform ModelBufferObject
{
    Ubo ubo;
};

layout(binding = SHADOW_MAP_BIND) uniform sampler2DArray shadowMaps;

in VS_OUT
{
    layout(location = 0) in vec2 uv;
    layout(location = 1) in vec4 viewFragPos;
    layout(location = 2) in vec4 worldFragPos;
    layout(location = 3) in mat3 tbn;
} fragment;

layout(push_constant) uniform PushConstant_
{
    PushConstant push;
};

void main()
{
    vec3 albedo = vec3(1, 1, 1);
    vec3 normal = vec3(1, 1, 1);
    if (push.albedo >= 0)
    {
        albedo = texture(bindlessTextures[nonuniformEXT(push.albedo)], fragment.uv).rgb;
    }
    if (push.normal >= 0)
    {
        normal = normalize(texture(bindlessTextures[nonuniformEXT(push.normal)], fragment.uv).rgb * 2 - 1);
    }
    float viewZ = abs(fragment.viewFragPos.z);

    int shadowIndex = 3;
    for (int i = 0; i < 3; i++)
    {
        if (viewZ <= ubo.cascadeDistances[i])
        {
            shadowIndex = i;
            break;
        }
    }

    vec4 lightSpacePos = ubo.lightViewProjectionMatrices[shadowIndex] * fragment.worldFragPos;
    lightSpacePos.xyz /= lightSpacePos.w;

    lightSpacePos.xy = lightSpacePos.xy * 0.5f + 0.5f;

    // Shadow bias
    float bias = max(0.10 * (1.0 - dot(normal, ubo.lightDir.xyz)), 0.01);
    float biasModifier = 4;

    bias *= biasModifier / (ubo.cascadeDistances[shadowIndex]);
    bias += 0.005;

    // PCF
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMaps, 0));
    float shadow = 0.f;
    for (float x = -1.5f; x <= 1.5f; x += 1.f)
    {
        for (float y = -1.5f; y <= 1.5f; y += 1.f)
        {
            vec2 shadowUV = lightSpacePos.xy + vec2(x, y) * texelSize;
            float depth = texture(shadowMaps, vec3(shadowUV, shadowIndex)).r;
            shadow += (lightSpacePos.z - bias) > depth ? 1.f : 0.f;
        }
    }
    shadow /= 16.f;

    normal = normalize(fragment.tbn * normal);
    vec3 l = ubo.lightDir.xyz;
    vec3 v = normalize(ubo.viewPos.xyz - fragment.worldFragPos.xyz);

    float nDotL = max(dot(normal, l), 0.0);
    vec3 h = normalize(l + v);
    float nDotH = max(dot(normal, h), 0.0);

    //AMBIENT
    vec3 ambient = 0.3f * albedo;
    //DIFFUSE
    vec3 diffuse = nDotL * albedo;

    //SPECULAR
    float shininess = 16;
    float specularStrength = pow(nDotH, shininess);
    // vec3 reflectVector = -l + 2 * dot(n, l) * n;
    // float specularStrength = pow(max(dot(reflectVector, v), 0.f), 64);

    float spec = specularStrength;
    vec3 specular = spec * albedo;

    vec3 color = (ambient + diffuse + specular); // * (1 - .8 * shadow);
    outColor = vec4(color, 1.0);

    // outColor = vec4(fragment.boneWeights);

    //outColor = vec4(vec3(viewZ/ubo.cascadeDistances[3], 0, 0), 1.0);
    //outColor = vec4(vec3(4 - shadowIndex) * .25, 1.f);

    // vec3 color =
    // color = pow(color, vec3(1/2.2));
    // outColor = vec4(color, 1.0);
}
