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
    // out V3 worldLightDir;
    flat out int drawId;
} result;

//struct ModelDrawParams 
//{
  //  Mat4 modelToWorldMatrix;
 //   float skinningOffset; 
//};

//layout (std140, binding = 3) uniform perDrawUniforms
//{
 //   ModelDrawParams params[MAX_MODELS];
//};

uniform Mat4 boneTransforms[MAX_BONES];
uniform Mat4 model;

void main()
{ 
#if SKINNED
    Mat4 boneTransform = boneTransforms[boneIds[0]] * boneWeights[0];
    boneTransform     += boneTransforms[boneIds[1]] * boneWeights[1];
    boneTransform     += boneTransforms[boneIds[2]] * boneWeights[2];
    boneTransform     += boneTransforms[boneIds[3]] * boneWeights[3];


    V4 modelSpacePos = boneTransform * V4(pos, 1.0);
    V4 worldSpacePos = model * modelSpacePos;
    gl_Position = viewPerspectiveMatrix * worldSpacePos;

    mat3 modelToWorld = mat3(model * boneTransform);
    modelToWorld = transpose(inverse(modelToWorld));
    V3 n = normalize(modelToWorld * n);
    V3 t = normalize(modelToWorld * tangent);
    // NOTE: only have to do this if there is non uniform scale
    t = normalize(t - dot(t, n) * n);
#else
    gl_Position = viewPerspectiveMatrix * model * V4(pos, 1.0);
    V4 worldSpacePos = model * V4(pos, 1.0);
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    V3 t = normalize(normalMatrix * tangent);
    V3 n = normalize(normalMatrix * n);
    t = normalize(t - dot(t, n) * n);
#endif
    V3 b = cross(n, t);
    // Inverse of orthonormal basis is just the transpose
    mat3 tbn = transpose(mat3(t, b, n));

    result.outUv = uv;
    result.tangentLightDir = tbn * lightDir.xyz;
    result.tangentViewPos = tbn * viewPosition.xyz;
    result.tangentFragPos = tbn * worldSpacePos.xyz;

    result.viewFragPos = viewMatrix * worldSpacePos;
    result.worldFragPos = worldSpacePos.xyz;
    //result.worldLightDir = lightDir.xyz;
    result.drawId = gl_DrawID;
}
