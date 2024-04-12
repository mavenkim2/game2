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

in V2 outUv;
in V3 tangentLightDir;
in V3 tangentViewPos;
in V3 tangentFragPos;
flat in int drawId;

in V3 outN;

out V4 FragColor;

#define TEXTURES_PER_MATERIAL 2
#define DIFFUSE_INDEX 0
#define NORMAL_INDEX 1

void main()
{
    // Normal texture units
#ifdef SINGLE_DRAW
    V3 normal = normalize(texture(normalMap, outUv).rgb * 2 - 1);
    V3 color = texture(diffuseMap, outUv).rgb;
#endif

    // Array of texture units
#ifdef MULTI_DRAW
    V3 normal = normalize(texture(textureMaps[TEXTURES_PER_MATERIAL * drawId + NORMAL_INDEX], outUv).rgb * 2 - 1);

    V3 color = texture(textureMaps[TEXTURES_PER_MATERIAL * drawId + DIFFUSE_INDEX], outUv).rgb;
#endif


    // Array of texture arrays
#ifdef MULTI_DRAW_TEXTURE_ARRAY
    TexAddress diffuseAddress = addresses[drawId * TEXTURES_PER_MATERIAL + DIFFUSE_INDEX];
    TexAddress normAddress = addresses[drawId * TEXTURES_PER_MATERIAL + NORMAL_INDEX];

    V3 normal = normalize(texture(textureMaps[normAddress.container], vec3(outUv, normAddress.slice)).rgb * 2 - 1);
    V3 color = texture(textureMaps[diffuseAddress.container], vec3(outUv, diffuseAddress.slice)).rgb;
#endif

    // Direction Phong Light
    V3 lightDir = normalize(tangentLightDir);

    // TODO IMPORTANT: the light's color should be used for diffuse and specular, not the texture map color.
    // also support light positions as well instead of just light directions

    //AMBIENT
    V3 ambient = 0.1f * color;
    //DIFFUSE
    f32 diffuseCosAngle = max(dot(normal, lightDir), 0.f);
    V3 diffuse = diffuseCosAngle * color;
    //SPECULAR
    V3 toViewPosition = normalize(tangentViewPos - tangentFragPos);
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

    FragColor = V4(ambient + diffuse + specular, 1.0);
    // FragColor = V4(normal, 1.0);
    // FragColor = V4(color, 1.0);
}
