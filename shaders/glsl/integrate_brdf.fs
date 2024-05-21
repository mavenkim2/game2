#version 330 core

in vec2 uv;
out vec2 FragColor;

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

vec3 ImportanceSampleGGX(vec2 xi, float roughness) 
{
    float a = roughness * roughness;

    // azimuth between 0 and 2pi for hemisphere
    float phi = 2 * PI * xi.x;
    // higher roughness -> flatter & wider specular lobe?
    float cosTheta = sqrt((1 - xi.y) / (1 + (a * a - 1) * xi.y));
    float sinTheta = sqrt(1 - cosTheta * cosTheta);

    // Spherical to cartesian. theta is zenith, phi is azimuth
    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

float GeometrySmith(float nDotL, float nDotV, float roughness)
{
    float a = roughness;
    // IBL k term
    float k = (a * a) / 2.f;

    float vDenom = nDotV * (1.f - k) + k;
    float lDenom = nDotL * (1.f - k) + k;

    return (nDotV * nDotL) / (vDenom * lDenom);
}

// Create LUT with x = theta, y = roughness
vec2 IntegrateBRDF(float nDotV, float roughness)
{
    nDotV = max(nDotV, 0.001);
    // ???
    vec3 v = vec3(sqrt(1.0 - nDotV * nDotV), 0.0, nDotV);

    float a = 0.0;
    float b = 0.0;

    // vec3 n = vec3(0.0, 0.0, 1.0);

    const uint SAMPLES_COUNT = 1024u; 

    for (uint i = 0u; i < SAMPLES_COUNT; i++)
    {
        vec2 xi = Hammersley(i, SAMPLES_COUNT);
        // don't care about the basis here
        vec3 h = ImportanceSampleGGX(xi, roughness);
        vec3 l = normalize(2 * dot(h, v) * h - v);

        float nDotL = l.z;
        float nDotH = h.z;
        float vDotH = max(dot(v, h), 0.0);

        if (nDotL > 0.0)
        {
            float g = GeometrySmith(nDotL, nDotV, roughness);
            // where does this come from?
            float gVis = g * vDotH / (nDotH * nDotV);
            float fc = pow(1.0 - vDotH, 5.f);

            a += (1.0 - fc) * gVis;
            b += fc * gVis;
        }
    }

    return vec2(a, b) / SAMPLES_COUNT;
}

void main()
{
    vec2 integratedBRDF = IntegrateBRDF(uv.x, uv.y);
    FragColor = integratedBRDF;
}
