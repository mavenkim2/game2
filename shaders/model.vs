layout (location = 0) in V3 pos;
layout (location = 1) in V3 n;
layout (location = 2) in V2 uv;
layout (location = 3) in V3 tangent;

layout (location = 4) in uvec4 boneIds;
layout (location = 5) in vec4 boneWeights;

out VS_OUT
{
    out V2 outUv;
    out V3 tangentLightDir;
    out V3 tangentViewPos;
    out V3 tangentFragPos;

    out V4 viewFragPos;
    out V3 worldFragPos;
    out V3 worldLightDir;
    flat out int drawId;
} result;

uniform Mat4 transform; 
uniform Mat4 boneTransforms[MAX_BONES];

uniform Mat4 model;
uniform Mat4 modelViewMatrix;
uniform V3 lightDir; 
uniform V3 viewPos;

void main()
{ 
#ifdef SKINNED
    Mat4 boneTransform = boneTransforms[boneIds[0]] * boneWeights[0];
    boneTransform     += boneTransforms[boneIds[1]] * boneWeights[1];
    boneTransform     += boneTransforms[boneIds[2]] * boneWeights[2];
    boneTransform     += boneTransforms[boneIds[3]] * boneWeights[3];


    // gl_Position = transform * V4(localPos, 1.0);
    gl_Position = transform * boneTransform * V4(pos, 1.0);
#else
    gl_Position = transform * V4(pos, 1.0);
#endif
    V3 localPos = (model * V4(pos, 1.0)).xyz;

    // TODO: this should probably be calculated once on the cpu instead 
    // of for every vertex
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    V3 t = normalize(normalMatrix * tangent);
    V3 n = normalize(normalMatrix * n);
    t = normalize(t - dot(t, n) * n);
    V3 b = cross(n, t);
    // Inverse of orthonormal basis is just the transpose
    mat3 tbn = transpose(mat3(t, b, n));

    result.outUv = uv;
    result.tangentLightDir = tbn * lightDir;
    result.tangentViewPos = tbn * viewPos;
    result.tangentFragPos = tbn * localPos;

    result.viewFragPos = modelViewMatrix * V4(pos, 1.0);
    result.worldFragPos = localPos;
    result.worldLightDir = lightDir;
    result.drawId = gl_DrawID;
}
