#version 460 core
#define f32 float
#define u32 int unsigned
#define V2 vec2
#define V3 vec3
#define V4 vec4
#define Mat4 mat4

layout (std140, binding = 2) uniform globalUniforms
{
    Mat4 viewPerspectiveMatrix;
    Mat4 viewMatrix;
    V4 viewPosition;
    // TODO: light struct
    V4 lightDir;
    V4 cascadeDistances;
};

const int MAX_BONES = 200;

