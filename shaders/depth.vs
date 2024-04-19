layout (location = 0) in V3 pos;
layout (location = 1) in V3 n;
layout (location = 2) in V2 uv;
layout (location = 3) in V3 tangent;

layout (location = 4) in uvec4 boneIds;
layout (location = 5) in vec4 boneWeights;

void main()
{
    Mat4 modelTransform = rMeshParams[gl_DrawID].mModelToWorldMatrix;
    int skinningOffset = rMeshParams[gl_DrawID].mJointOffset;

    if (skinningOffset != -1)
    {
        Mat4 boneTransform = rBoneTransforms[skinningOffset + boneIds[0]] * boneWeights[0];
        boneTransform     += rBoneTransforms[skinningOffset + boneIds[1]] * boneWeights[1];
        boneTransform     += rBoneTransforms[skinningOffset + boneIds[2]] * boneWeights[2];
        boneTransform     += rBoneTransforms[skinningOffset + boneIds[3]] * boneWeights[3];
        gl_Position = modelTransform * boneTransform * V4(pos, 1.0);
    }
    else 
    {
        gl_Position = modelTransform * V4(pos, 1.0);
    }
}
