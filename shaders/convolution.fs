#version 330 core
#define PI 3.1415926535

in vec3 localPos;
out vec4 FragColor;

uniform samplerCube environmentMap;

void main()
{
    vec3 normal = normalize(localPos);
    
    // ????????
    vec3 up = normal.z < 0.95 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

    vec3 irradiance = vec3(0.0);
    float sampleDelta = 0.025;
    float nSamples = 0;

    for (float phi = 0.f; phi < 2 * PI; phi += sampleDelta)
    {
        for (float theta = 0.f; theta < PI / 2; theta += sampleDelta)
        {
            vec3 tangent = vec3(sin(phi) * cos(theta), sin(phi) * sin(theta), cos(phi));
            vec3 sampleVec = tangent.x * right + tangent.y * up + tangent.z * normal;

            irradiance += texture(environmentMap, sampleVec).rgb * cos(theta) * sin(theta);
            nSamples++;
        }
    }
    irradiance = PI * irradiance / nSamples;
    FragColor = vec4(irradiance, 1.0);
}
