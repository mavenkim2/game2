#version 330 core

in vec3 localPos;
out vec4 FragColor;

uniform samplerCube envMap;
uniform float roughness;

#define PI 3.1415926535

float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u); // swaps high and low half word
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);

    return float(bits) * 2.3283064365386963e-10;
}

// Low discrepancy sampling (random, but spread out) of sample i, sample size N
vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}

vec3 ImportanceSampleGGX(vec2 xi, float roughness, vec3 normal)
{
    float a = roughness * roughness;

    // azimuth between 0 and 2pi for hemisphere
    float phi = 2 * PI * xi.x;
    // higher roughness -> flatter & wider specular lobe?
    float cosTheta = sqrt((1 - xi.y) / (1 + (a * a - 1) * xi.y));
    float sinTheta = sqrt(1 - cosTheta * cosTheta);

    // Spherical to cartesian. theta is zenith, phi is azimuth
    vec3 h;
    h.x = sinTheta * cos(phi);
    h.y = sinTheta * sin(phi);
    h.z = cosTheta;

    // TODO: branchless?
    vec3 up = abs(normal.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
    // orthonormal basis
    vec3 tangentX = normalize(cross(up, normal));
    vec3 tangentY = normalize(cross(normal, tangentX));

    // tangent to world space
    return tangentX * h.x + tangentY * h.y + normal * h.z;
}

float DistributionGGX(vec3 n, vec3 h, float roughness)
{
    float alpha = roughness * roughness;
    float a2 = alpha * alpha;

    float nDotH = dot(n, h);

    float denom = (nDotH * nDotH) * (a2 - 1.f) + 1.f;

    return a2 / (PI * denom * denom);
}

void main() 
{
    vec3 r = normalize(localPos);
    vec3 n = r;
    vec3 v = r;
    vec3 prefilteredColor = vec3(0.0);
    float totalWeight = 0.0;

    vec2 inputSize = vec2(textureSize(envMap, 0));
    // Solid angle associated with a single texel (4pi steradians for the whole cubemap, 6 faces)
    float wt = 4.f * PI / (6 * inputSize.x * inputSize.y);
    const uint SAMPLE_COUNT = 1024u;
    for (uint i = 0u; i < SAMPLE_COUNT; i++)
    {
        // get the next value in the random sequence
        vec2 xi = Hammersley(i, SAMPLE_COUNT);
        // importance sample, meaning biasing the sample vector towards the specular lobe, defined by an input roughness
        vec3 h = ImportanceSampleGGX(xi, roughness, n);
        // reflect the view vector about the microfacet normal
        vec3 l = normalize(2 * dot(h, v) * h - v);
        float nDotL = max(dot(n, l), 0.0); 

        if (nDotL > 0.0)
        {
            // NOTE: pdf = D * nDotH / (4 * hDotV), but since v = n in this model they cancel
            float pdf = DistributionGGX(n, h, roughness) / 4;

            // ???
            // solid angle associated with this sample
            float ws = 1.f / (SAMPLE_COUNT * pdf);
            float mipLevel = max(0.5 * log2(ws / wt) + 1.0, 0.0);

            // NOTE: instead of weighing each sample equally, weigh by nDotL
            prefilteredColor += textureLod(envMap, l, mipLevel).rgb * nDotL;
            totalWeight += nDotL;
        }
    }
    prefilteredColor /= totalWeight;
    FragColor = vec4(prefilteredColor, 1.0);
}
