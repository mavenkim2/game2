#define MULTI_DRAW_TEXTURE_ARRAY 1

// Single draw texture map uniforms
#ifdef SINGLE_DRAW
uniform sampler2D diffuseMap;
uniform sampler2D normalMap;
#endif

// Multi draw texture map uniform
#ifdef MULTI_DRAW
uniform sampler2D textureMaps[16];
#endif

// Multi draw texture array ssbo
#ifdef MULTI_DRAW_TEXTURE_ARRAY
struct TexAddress 
{
    unsigned int container;
    f32 slice;
};

layout (std430, binding = 1) buffer shaderStorageData
{
    TexAddress addresses[];
};

uniform sampler2DArray textureMaps[32];
#endif

in VS_OUT
{
    in V2 outUv;
    in V3 tangentLightDir;
    in V3 tangentViewPos;
    in V3 tangentFragPos;

    in V4 viewFragPos;
    in V3 worldFragPos;
    in V3 worldLightDir;
    flat in int drawId;
} fragment;

out V4 FragColor;

layout (std140, binding = 0) buffer lightMatrices 
{
    Mat4 lightViewProjectionMatrices[16];
};
uniform f32 cascadeDistances[4];
uniform sampler2DArray shadowMaps;

uniform Mat4 viewMatrix;

#define TEXTURES_PER_MATERIAL 2
#define DIFFUSE_INDEX 0
#define NORMAL_INDEX 1

void main()
{
    // Normal texture units
#ifdef SINGLE_DRAW
    V3 normal = normalize(texture(normalMap, fragment.outUv).rgb * 2 - 1);
    V3 color = texture(diffuseMap, fragment.outUv).rgb;
#endif

    // Array of texture units
#ifdef MULTI_DRAW
    V3 normal = normalize(texture(textureMaps[TEXTURES_PER_MATERIAL * fragment.drawId + NORMAL_INDEX], fragment.outUv).rgb * 2 - 1);

    V3 color = texture(textureMaps[TEXTURES_PER_MATERIAL * fragment.drawId + DIFFUSE_INDEX], fragment.outUv).rgb;
#endif


    // Array of texture arrays
#ifdef MULTI_DRAW_TEXTURE_ARRAY
    TexAddress diffuseAddress = addresses[fragment.drawId * TEXTURES_PER_MATERIAL + DIFFUSE_INDEX];
    TexAddress normAddress = addresses[fragment.drawId * TEXTURES_PER_MATERIAL + NORMAL_INDEX];

    V3 normal = normalize(texture(textureMaps[normAddress.container], vec3(fragment.outUv, normAddress.slice)).rgb * 2 - 1);
    V3 color = texture(textureMaps[diffuseAddress.container], vec3(fragment.outUv, diffuseAddress.slice)).rgb;
#endif

    // Shadow mapping
    f32 viewZ = -fragment.viewFragPos.z;

// TODO: cvar
    int shadowIndex = 4;
    for (int i = 0; i < 4; i++)
    {
        if (viewZ < cascadeDistances[i])
        {
            shadowIndex = i;
            break;
        }
    }

    // I think mostly everything above here is fine.
    V4 lightSpacePos = lightViewProjectionMatrices[shadowIndex] * V4(fragment.worldFragPos, 1.f);
    lightSpacePos.xyz /= lightSpacePos.w;

    lightSpacePos = lightSpacePos * 0.5f + 0.5f;

    // Shadow bias
    // TODO: this normal is in tangent space I believe. need the tbn
    f32 bias = max(0.05 * (1.0 - dot(normal, fragment.tangentLightDir)), 0.005);
    f32 biasModifier = 2;
    bias *= biasModifier / (cascadeDistances[shadowIndex]);

    // PCF
    V2 texelSize = 1.0 / V2(textureSize(shadowMaps, 0));
    f32 shadow = 0.f;
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            V2 shadowUV = lightSpacePos.xy + V2(x, y) * texelSize;
            f32 depth = texture(shadowMaps, V3(shadowUV, shadowIndex)).r;
            shadow += (lightSpacePos.z - bias) > depth ? 1.f : 0.f;
        }
    }
    shadow /= 9.f;

    // Direction Phong Light
    V3 lightDir = normalize(fragment.tangentLightDir);

    //AMBIENT
    V3 ambient = 0.1f * color;
    //DIFFUSE
    f32 diffuseCosAngle = max(dot(normal, lightDir), 0.f);
    V3 diffuse = diffuseCosAngle * color;
    //SPECULAR
    V3 toViewPosition = normalize(fragment.tangentViewPos - fragment.tangentFragPos);

#ifndef BLINN_PHONG
    V3 reflectVector = -lightDir + 2 * dot(normal, lightDir) * normal;
    f32 specularStrength = pow(max(dot(reflectVector, toViewPosition), 0.f), 64);

#else
    // Blinn phong 
    V3 halfway = normalize(lightDir + toViewPosition);
    f32 shininess = 16;
    f32 specularStrength = pow(max(dot(normal, halfway), 0.f), shininess);
#endif

    f32 spec = specularStrength;
    V3 specular = spec * color;

    V3 outColor = (ambient + diffuse + specular) * shadow;
    // V3 outColor = V3(1, 1, 1) * lightSpacePos.z;

    // Debug
    // FragColor = V4(normal, 1.0);
    // FragColor = V4(color, 1.0);

    FragColor = V4(outColor, 1.0);
}
