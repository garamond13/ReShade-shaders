#include "Include/GraphicalUpgradeCB.hlsli.h"

SamplerState smp : register(s1); // Should be linear clamp.
Texture2D tex : register(t0);

#ifndef THRESHOLD
#define THRESHOLD 0
#endif

float get_gaussian_weight(float x)
{
    return exp(-x * x / (2.0 * sigma * sigma));
}

// t - soft threshold, should be in range (0, +inf)
// h - knee height, should be in range (0, t)
// p - knee sharpness, should be in range (1, +inf)
//
// f(x) = x - x / (1 + exp2(log2(h / (t - h)) + p * log2(|x| / t)))
//
// This would be equivalent to f(x) = x - x / (1 + h / (t - h) * (|x| / t)^p) implementation, but safe for large p.
float3 threshold(float3 color, float t, float h, float p)
{
    color = max(0.0, color);
    const float k = max(color.r, max(color.g, color.b));
    const float u = log2(h / (t - h)) + p * log2(max(1e-6, k / t));
    return color / (1.0 + exp2(-u)); // Optimized, color * scale(k) / k
}

// Samples one axis at a time.
float4 downsample_ps(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
    // Calculate fractional part and texel center.
    const float f = dot(frac(texcoord * src_size - 0.5), axis);
    const float2 tc = texcoord - f * inv_src_size * axis;

    float3 csum = 0.0;
    float wsum = 0.0;

    // Calculate kernel radius.
    const float radius = ceil(sigma * 3.0);

    // Optimized, we are processing 2 samples per linear texture fetch.
    for (float i = 1.0 - radius; i <= radius; i += 2.0) {
        const float w1 = get_gaussian_weight(i - f);
        const float w2 = get_gaussian_weight(i + 1.0 - f);
        const float w12 = w1 + w2;
        const float offset = (i * w1 + (i + 1.0) * w2) / w12;
        csum += tex.SampleLevel(smp, tc + offset * inv_src_size * axis, 0.0).rgb * w12;
        wsum += w12;
    }

    // Normalize.
    csum /= wsum;

    #if THRESHOLD
    // PWLThreshold, the original bloom threshold value should be 1. 
    csum = threshold(csum, 1.0, 0.05, 3.0);
    #endif

    return float4(csum, 1.0);
}

// From https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare/
float4 upsample_ps(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
    const float3 a = tex.SampleLevel(smp, texcoord, 0.0, int2(-1, 1)).rgb;
    const float3 b = tex.SampleLevel(smp, texcoord, 0.0, int2(0, 1)).rgb;
    const float3 c = tex.SampleLevel(smp, texcoord, 0.0, int2(1, 1)).rgb;
    const float3 d = tex.SampleLevel(smp, texcoord, 0.0, int2(-1, 0)).rgb;
    const float3 e = tex.SampleLevel(smp, texcoord, 0.0).rgb;
    const float3 f = tex.SampleLevel(smp, texcoord, 0.0, int2(1, 0)).rgb;
    const float3 g = tex.SampleLevel(smp, texcoord, 0.0, int2(-1, -1)).rgb;
    const float3 h = tex.SampleLevel(smp, texcoord, 0.0, int2(0, -1)).rgb;
    const float3 i = tex.SampleLevel(smp, texcoord, 0.0, int2(1, -1)).rgb;

    return float4(e * 0.25 + (b + d + f + h) * 0.125 + (a + c + g + i) * 0.0625, 1.0);
}