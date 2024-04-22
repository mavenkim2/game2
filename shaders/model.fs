// Single draw texture map uniforms
#if SINGLE_DRAW
uniform sampler2D diffuseMap;
uniform sampler2D normalMap;
#endif

// Multi draw texture map uniform
#if MULTI_DRAW
uniform sampler2D textureMaps[16];
#endif

// TODO: bindless sparse :)
#if MULTI_DRAW_TEXTURE_ARRAY
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

#define TEXTURES_PER_MATERIAL 3
#define DIFFUSE_INDEX 0
#define NORMAL_INDEX 1
#define MR_INDEX 2

// F0 approximates to 0.04 for dielectrics, for metals it's lerped b/t 0.04 and the albedo based on the metalness
void main()
{
    // Normal texture units
#if SINGLE_DRAW
    V3 normal = normalize(texture(normalMap, fragment.outUv).rgb * 2 - 1);
    V3 albedo = texture(diffuseMap, fragment.outUv).rgb;
#endif

    // Array of texture units
#if MULTI_DRAW
    V3 normal = normalize(texture(textureMaps[TEXTURES_PER_MATERIAL * fragment.drawId + NORMAL_INDEX], fragment.outUv).rgb * 2 - 1);
    V3 albedo = texture(textureMaps[TEXTURES_PER_MATERIAL * fragment.drawId + DIFFUSE_INDEX], fragment.outUv).rgb;
#endif

    // Array of texture arrays
#if MULTI_DRAW_TEXTURE_ARRAY
    //TODO; might be y
    unsigned int diffuseContainer = rMeshParams[fragment.drawId].mIndex[DIFFUSE_INDEX].x;
    unsigned int normalContainer = rMeshParams[fragment.drawId].mIndex[NORMAL_INDEX].x;
    unsigned int metallicContainer = rMeshParams[fragment.drawId].mIndex[MR_INDEX].x;

    float diffuseSlice = float(rMeshParams[fragment.drawId].mSlice[DIFFUSE_INDEX]);
    float normalSlice = float(rMeshParams[fragment.drawId].mSlice[NORMAL_INDEX]);
    float metallicSlice = float(rMeshParams[fragment.drawId].mSlice[MR_INDEX]);

    V3 albedo = texture(textureMaps[diffuseContainer], vec3(fragment.outUv, diffuseSlice)).rgb;
    V3 normal = normalize(texture(textureMaps[normalContainer], vec3(fragment.outUv, normalSlice)).rgb * 2 - 1);

    V3 mr = texture(textureMaps[metallicContainer], vec3(fragment.outUv, metallicSlice)).rgb;
    f32 metalness = mr.b;
    f32 roughness = mr.g;
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

    // L and V
    V3 l = normalize(fragment.tangentLightDir);
    V3 v = normalize(fragment.tangentViewPos - fragment.tangentFragPos);

    V3 h = normalize(l + v);
    f32 nDotV = max(dot(normal, v), 0.0);
    f32 nDotL = max(dot(normal, l), 0.0);

    V3 outColor = V3(0.0);
    if (rMeshParams[fragment.drawId].mIsPBR == 1)
    {
        f32 a = roughness * roughness;
        f32 a2 = a * a;

        // Distribution: % surface area of microfacets w/ normal of the halfway vector
        // Trowbridge-Reitz, dependent on h (microfacet normal)
        f32 nDotH = max(dot(normal, h), 0.f);
        f32 nDotH2 = nDotH * nDotH;
        f32 denom = nDotH2 * (a2 - 1.f) + 1.f;
        denom = PI * denom * denom;
        f32 distribution = a2 / denom;

        // Geometry: factor based on surface obstruction/surface shadowing, based on l, v, roughness
        // NOTE: this divides the smith geometry factor by 4(n dot l)(n dot v), the cook torrance denominator
        // Smith Schlick, dependent on roughness, n, v, l
        f32 k = roughness + 1.f;
        k = (k * k) / 8.f;
         
        f32 geometryV = nDotV * (1.f - k) + k;
        f32 geometryL = nDotL * (1.f - k) + k;
        f32 geometryAndCTDenom = 1.f / (geometryV * geometryL * 4.f);

        // Fresnel: measures the reflectiveness of the surface based on the viewing angle, using F0. 
        // all surfaces approach 1.0 reflectiveness at 90 incident angle (grazing)
        // Fresnel Schlick
        f32 hDotV = max(dot(h, v), 0.0);

        // For dielectrics, approximate the base reflectivity at 4%. Otherwise, mix based on metalness.
        // This works because metals absorb (almost?) all light, meaning there's no refraction/diffuse, leaving the 
        // specular
        V3 f0 = V3(0.04);
        f0 = mix(f0, albedo.rgb, metalness);
        V3 fresnel = f0 + (1 - f0) * pow(1 - hDotV, 5);

        V3 spec = V3(distribution) * V3(geometryAndCTDenom) * fresnel;

        // lambert
        V3 kD = (V3(1.0) - fresnel) * (1 - metalness);

        V3 radiance = V3(1, 1, 1);//V3(253/255.f, 184/255.f, 19/255.f);//lightColor;// * nDotL;

        outColor += /*V3(.1) * albedo + */((kD * albedo / PI) + spec) * nDotL * radiance * (1 - shadow);
        // outColor = outColor / (outColor + V3(1.0));
        // outColor += specular;
        // outColor = V3(metalness);
        // outColor = V3(roughness);
    }
    else 
    {
        //AMBIENT
        V3 ambient = 0.3f * albedo;
        //DIFFUSE
        V3 diffuse = nDotL * albedo;

#if BLINN_PHONG
        //SPECULAR
        f32 shininess = 16;
        f32 specularStrength = pow(max(dot(normal, h), 0.f), shininess);
#else
        V3 reflectVector = -l + 2 * dot(normal, l) * normal;
        f32 specularStrength = pow(max(dot(reflectVector, v), 0.f), 64);
#endif

        f32 spec = specularStrength;
        V3 specular = spec * albedo;

        // V3 outColor = V3(1, 1, 1) * ((4 - shadowIndex) * .25);
        // V3 outColor = V3(lightSpacePos.z, 0, 0);
        outColor = (ambient + diffuse + specular) * (1 - .8 * shadow);
    }

    // Debug
    // FragColor = V4(normal, 1.0);
    // FragColor = V4(color, 1.0);

    FragColor = V4(outColor, 1.0);
}
