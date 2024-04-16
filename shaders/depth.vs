layout (location = 0) in V3 pos;
layout (location = 1) in V3 n;
layout (location = 2) in V2 uv;
layout (location = 3) in V3 tangent;

layout (location = 4) in uvec4 boneIds;
layout (location = 5) in vec4 boneWeights;

uniform Mat4 modelTransform;
uniform Mat4 boneTransforms[MAX_BONES];

void main()
{
#ifdef SKINNED
    Mat4 boneTransform = boneTransforms[boneIds[0]] * boneWeights[0];
    boneTransform     += boneTransforms[boneIds[1]] * boneWeights[1];
    boneTransform     += boneTransforms[boneIds[2]] * boneWeights[2];
    boneTransform     += boneTransforms[boneIds[3]] * boneWeights[3];

    gl_Position = boneTransform * V4(pos, 1.0);
#else
    gl_Position = modelTransform * V4(pos, 1.0);
#endif
}
