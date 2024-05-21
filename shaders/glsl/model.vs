layout (location = 0) in V3 pos;
layout (location = 1) in V3 n;
layout (location = 2) in V2 uv;
layout (location = 3) in V3 tangent;

layout (location = 4) in uvec4 boneIds;
layout (location = 5) in vec4 boneWeights;

out VS_OUT
{
    out V2 uv;
    // out V3 tangentLightDir;
    // out V3 tangentViewPos;
    // out V3 tangentFragPos;

    out V4 viewFragPos;
    out V3 worldFragPos;

    flat out int drawId;
    out mat3 tbn;
} result;

void main()
{ 
    V4 worldSpacePos;
    // Tangent space normal and tangent
    V3 tN;
    V3 tT;
    int skinningOffset = rMeshParams[gl_DrawID].mJointOffset;
    Mat4 modelToWorldMatrix = rMeshParams[gl_DrawID].mModelToWorldMatrix;
    // Skinned
    if (skinningOffset != -1)
    {
        Mat4 boneTransform = rBoneTransforms[skinningOffset + boneIds[0]] * boneWeights[0];
        boneTransform     += rBoneTransforms[skinningOffset + boneIds[1]] * boneWeights[1];
        boneTransform     += rBoneTransforms[skinningOffset + boneIds[2]] * boneWeights[2];
        boneTransform     += rBoneTransforms[skinningOffset + boneIds[3]] * boneWeights[3];

        // V4 modelSpacePos = boneTransform * V4(pos, 1.0);
        Mat4 worldSpaceMatrixTransform = modelToWorldMatrix * boneTransform;
        worldSpacePos = worldSpaceMatrixTransform * V4(pos, 1.0);
        gl_Position = viewPerspectiveMatrix * worldSpacePos;

        mat3 modelToWorld = mat3(worldSpaceMatrixTransform);
        // modelToWorld = transpose(inverse(modelToWorld));
        tN = normalize(modelToWorld * n);
        tT = normalize(modelToWorld * tangent);
        // NOTE: only have to do this if there is non uniform scale
        tT = normalize(tT - dot(tT, tN) * tN);
    }
    // Not skinned
    else 
    {
        gl_Position = viewPerspectiveMatrix * modelToWorldMatrix * V4(pos, 1.0);
        worldSpacePos = modelToWorldMatrix * V4(pos, 1.0);
        mat3 normalMatrix = mat3(modelToWorldMatrix);
        //transpose(inverse(mat3(modelToWorldMatrix)));
        tN = normalize(normalMatrix * n);
        tT = normalize(normalMatrix * tangent);
        tT = normalize(tT - dot(tT, tN) * tN);
    }

    V3 tB = cross(tN, tT);
    // Inverse of orthonormal basis is just the transpose
    // mat3 tbn = transpose(mat3(tT, tB, tN));
    mat3 tbn = mat3(tT, tB, tN);

    result.tbn = tbn;
    result.uv = uv;
    // result.tangentLightDir = tbn * lightDir.xyz;
    // result.tangentViewPos = tbn * viewPosition.xyz;
    // result.tangentFragPos = tbn * worldSpacePos.xyz;

    result.viewFragPos = viewMatrix * worldSpacePos;
    result.worldFragPos = worldSpacePos.xyz;

    result.drawId = gl_DrawID;
}
