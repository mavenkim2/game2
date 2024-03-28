#if 0
uniform sampler2D diffuseMap;
uniform sampler2D normalMap;
uniform sampler2D textureMaps[16];
#endif

struct TexAddress 
{
    unsigned int container;
    f32 slice;
};

layout (std430, binding = 1) buffer shaderStorageData
{
    TexAddress addresses[];
};

uniform sampler2DArray textureMaps[16];

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
    // Array of texture units
#if 0
    V3 normal = normalize(texture(textureMaps[TEXTURES_PER_MATERIAL * drawId + NORMAL_INDEX], outUv).rgb * 2 - 1);

    V3 color = texture(textureMaps[TEXTURES_PER_MATERIAL * drawId + DIFFUSE_INDEX], outUv).rgb;
#endif

// Normal texture units
#if 0
    V3 normal = normalize(texture(normalMap, outUv).rgb * 2 - 1);
    V3 color = texture(diffuseMap, outUv).rgb;
#endif

    // Array of texture arrays
    TexAddress diffuseAddress = addresses[drawId * TEXTURES_PER_MATERIAL + DIFFUSE_INDEX];
    TexAddress normAddress = addresses[drawId * TEXTURES_PER_MATERIAL + NORMAL_INDEX];

    V3 normal = normalize(texture(textureMaps[normAddress.container], vec3(outUv, normAddress.slice)).rgb * 2 - 1);
    V3 color = texture(textureMaps[diffuseAddress.container], vec3(outUv, diffuseAddress.slice)).rgb;

    // Direction Phong Light
    V3 lightDir = normalize(tangentLightDir);

    //AMBIENT
    V3 ambient = 0.1f * color;
    //DIFFUSE
    f32 diffuseCosAngle = max(dot(normal, lightDir), 0.f);
    V3 diffuse = diffuseCosAngle * color;
    //SPECULAR
    V3 toViewPosition = normalize(tangentViewPos - tangentFragPos);
    V3 reflectVector = -lightDir + 2 * dot(normal, lightDir) * normal;
    f32 specularStrength = pow(max(dot(reflectVector, toViewPosition), 0.f), 64);

    f32 spec = specularStrength;
    V3 specular = spec * color;

    FragColor = V4(ambient + diffuse + specular, 1.0);
    // FragColor = V4(normal, 1.0);
    // FragColor = V4(color, 1.0);
}
