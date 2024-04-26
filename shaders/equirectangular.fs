#version 330 core
in vec3 localPos;
out vec4 FragColor;

uniform sampler2D equirectangular;

#define PI 3.1415926535

const vec2 mapping = vec2(1 / (2 * PI), 1 / (PI));
vec2 SampleEquirectangularMap(vec3 pos)
{
    // calculate azimuth and zenith
    vec2 uv = vec2(atan(pos.y, pos.x), asin(pos.z));
    // map to range -0.5 to 0.5
    uv *= mapping;
    // map to 0-1
    uv += 0.5;
    return uv;
}

void main()
{
    vec2 uv = SampleEquirectangularMap(normalize(localPos));
    vec3 color = texture(equirectangular, uv).rgb;
    // color = color / (color + vec3(1.0));

    FragColor = vec4(color, 1.0);
}
