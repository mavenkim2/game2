#include "globals.hlsli"
Texture2D<float> mipInSRV : register(t0);
RWTexture2D<float> mipOutUAV : register(u0);
SamplerState mipSampler : register(s0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 dim;
    mipInSRV.GetDimensions(dim.x, dim.y);
    if (any(DTid.xy >= dim))
       return;

    float2 uv = float2(DTid.xy + 0.5f) * 2;//push.texelSize;
    //float depth = mipInSRV.SampleLevel(samplerNearestClamp, uv, 0).r;
    mipOutUAV[DTid.xy] = 1.f;//depth;
}
