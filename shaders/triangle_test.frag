#version 450

layout(location = 0) out vec4 outColor;

struct ModelParams
{
    mat4 transform;
    mat4 modelViewMatrix;
    mat4 modelMatrix;
};

layout(binding = 0) uniform ModelBufferObject 
{
    ModelParams params[8];
    mat4 lightViewProjectionMatrices;
    vec4 cascadeDistances;
    vec4 lightDir;
    vec4 viewPos;
} ubo;

layout(binding = 2) uniform sampler2D albedo;
layout(binding = 3) uniform sampler2D normalSampler;
layout(binding = 4) uniform sampler2DArray shadowMaps;

in VS_OUT
{
    layout (location = 0) in vec2 uv;
    layout (location = 1) in vec4 viewFragPos;
    layout (location = 2) in vec4 worldFragPos;
    layout (location = 3) in mat3 tbn;
} fragment;

void main()
{
    vec3 albedo = texture(albedo, fragment.uv).rgb;
    vec3 normal = normalize(texture(normalSampler, fragment.uv).rgb * 2 - 1);
    float viewZ = -fragment.viewFragPos.z;

    int shadowIndex = 4;
    for (int i = 0; i < 4; i++)
    {
        if (viewZ <= ubo.cascadeDistances[i])
        {
            shadowIndex = i;
            break;
        }
    }

    vec4 lightSpacePos = ubo.lightViewProjectionMatrices[shadowIndex] * fragment.worldFragPos;
    lightSpacePos.xyz /= lightSpacePos.w;

    lightSpacePos = lightSpacePos * 0.5f + 0.5f;

    // Shadow bias
    float bias = max(0.1 * (1.0 - dot(normal, ubo.lightDir.xyz)), 0.005);
    float biasModifier = 2;

    bias *= biasModifier / (ubo.cascadeDistances[shadowIndex]);
    bias += 0.0015;

    // PCF
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMaps, 0));
    float shadow = 0.f;
    for (float x = -1.5f; x <= 1.5f; x+=1.f)
    {
        for (float y = -1.5f; y <= 1.5f; y+=1.f)
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

    vec3 color = (ambient + diffuse + specular);// * (1 - .8 * shadow);
    outColor = vec4(color, 1.0);

    // vec3 color = 
    // color = pow(color, vec3(1/2.2));
    // outColor = vec4(color, 1.0);
}
