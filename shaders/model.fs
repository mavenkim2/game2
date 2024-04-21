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

// TODO: bindless sparse :)
#ifdef MULTI_DRAW_TEXTURE_ARRAY
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
    // in V3 worldLightDir;
    flat in int drawId;
} fragment;

out V4 FragColor;

layout (std140, binding = 0) uniform lightMatrices 
{
    Mat4 lightViewProjectionMatrices[16];
};

uniform sampler2DArray shadowMaps;

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
    //TODO; might be y
    unsigned int diffuseContainer = rMeshParams[fragment.drawId].mIndex[DIFFUSE_INDEX].x;
    unsigned int normalContainer = rMeshParams[fragment.drawId].mIndex[NORMAL_INDEX].x;

    float diffuseSlice = float(rMeshParams[fragment.drawId].mSlice[DIFFUSE_INDEX]);
    float normalSlice = float(rMeshParams[fragment.drawId].mSlice[NORMAL_INDEX]);

    V3 color = texture(textureMaps[diffuseContainer], vec3(fragment.outUv, diffuseSlice)).rgb;
    V3 normal = normalize(texture(textureMaps[normalContainer], vec3(fragment.outUv, normalSlice)).rgb * 2 - 1);
#endif

    // Shadow mapping
    f32 viewZ = -fragment.viewFragPos.z;

// TODO: cvar
    int shadowIndex = 4;
    for (int i = 0; i < 4; i++)
    {
        if (viewZ <= cascadeDistances[i])
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
    f32 bias = max(0.1 * (1.0 - dot(normal, fragment.tangentLightDir)), 0.005);
    f32 biasModifier = 2;

    // TODO: doesn't work for shadows in the furthest frustum, since cascadeDistances is only 4 big
    bias *= biasModifier / (cascadeDistances[shadowIndex]);
    // bias += 0.0015;

    // PCF
    V2 texelSize = 1.0 / V2(textureSize(shadowMaps, 0));
    f32 shadow = 0.f;
    for (float x = -1.5f; x <= 1.5f; x+=1.f)
    {
        for (float y = -1.5f; y <= 1.5f; y+=1.f)
        {
            V2 shadowUV = lightSpacePos.xy + V2(x, y) * texelSize;
            f32 depth = texture(shadowMaps, V3(shadowUV, shadowIndex)).r;
            shadow += (lightSpacePos.z - bias) > depth ? 1.f : 0.f;
        }
    }
    shadow /= 16.f;

    // Direction Phong Light
    V3 lightDir = normalize(fragment.tangentLightDir);

    //AMBIENT
    V3 ambient = 0.3f * color;
    //DIFFUSE
    f32 diffuseCosAngle = max(dot(normal, lightDir), 0.f);
    V3 diffuse = diffuseCosAngle * color;
    //SPECULAR
    V3 toViewPosition = normalize(fragment.tangentViewPos - fragment.tangentFragPos);

#if BLINN_PHONG
    V3 halfway = normalize(lightDir + toViewPosition);
    f32 shininess = 16;
    f32 specularStrength = pow(max(dot(normal, halfway), 0.f), shininess);

#else
    V3 reflectVector = -lightDir + 2 * dot(normal, lightDir) * normal;
    f32 specularStrength = pow(max(dot(reflectVector, toViewPosition), 0.f), 64);
    // Blinn phong 
#endif

    f32 spec = specularStrength;
    V3 specular = spec * color;

    // V3 outColor = V3(1, 1, 1) * ((4 - shadowIndex) * .25);
    // V3 outColor = V3(lightSpacePos.z, 0, 0);
    V3 outColor = (ambient + diffuse + specular) * (1 - .8 * shadow);

    // Debug
    // FragColor = V4(normal, 1.0);
    // FragColor = V4(color, 1.0);

    FragColor = V4(outColor, 1.0);
}
