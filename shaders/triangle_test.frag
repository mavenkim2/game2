#version 450

layout(binding = 1) uniform sampler2D diffuse;
layout(location = 0) out vec4 outColor;

in VS_OUT
{
    layout (location = 0) in vec2 uv;
} fragment;

void main()
{
    vec3 color = texture(diffuse, fragment.uv).rgb;
    color = pow(color, vec3(1/2.2));
    outColor = vec4(color, 1.0);
}
