#ifndef __AMD_FFX_CAS_HLSLI__
#define __AMD_FFX_CAS_HLSLI__

// Modified AMD FFX CAS
//
// Adopted and optimized for shader model 4.1.
// Rescaled the effect of sharpening, now sharpness = 0 means no sharpening and sharpness = 1 is same as before.
// Instead of using only green channel it uses luma.
// Optional HDR source.
//
// Source: https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/54fbaafdc34716811751bea5032700e78f5a0f33/sdk/include/FidelityFX/gpu/cas/ffx_cas.h

#include "Include/Math.hlsli"
#include "Include/SmoothSaturate.hlsli"

// sharpness - should be in range [0, 1]
// hdr - is source hdr
// m - mid/anchor point, should be in range [0, 2)
// p - compression strength, for m = 0 should be in range (0, +inf) and for m > 0 should be in range (1, +inf)
float4 amd_ffx_cas(Texture2D<float4> tex, int2 pos, float sharpness, bool hdr = false, float m = 0.0, float p = 2.0)
{
    // Load a collection of samples in a 3x3 neighorhood, where e is the current pixel.
    // a b c
    // d e f
    // g h i
    float3 sampleA = tex.Load(int3(pos.xy, 0), int2(-1, -1)).rgb;
    float3 sampleB = tex.Load(int3(pos.xy, 0), int2(0, -1)).rgb;
    float3 sampleC = tex.Load(int3(pos.xy, 0), int2(1, -1)).rgb;
    float3 sampleD = tex.Load(int3(pos.xy, 0), int2(-1, 0)).rgb;
    float4 sampleE = tex.Load(int3(pos.xy, 0));
    float3 sampleF = tex.Load(int3(pos.xy, 0), int2(1, 0)).rgb;
    float3 sampleG = tex.Load(int3(pos.xy, 0), int2(-1, 1)).rgb;
    float3 sampleH = tex.Load(int3(pos.xy, 0), int2(0, 1)).rgb;
    float3 sampleI = tex.Load(int3(pos.xy, 0), int2(1, 1)).rgb;

    // Get lumas for samples.
    float lumaA = get_luma(sampleA);
    float lumaB = get_luma(sampleB);
    float lumaC = get_luma(sampleC);
    float lumaD = get_luma(sampleD);
    float lumaE = get_luma(sampleE.rgb);
    float lumaF = get_luma(sampleF);
    float lumaG = get_luma(sampleG);
    float lumaH = get_luma(sampleH);
    float lumaI = get_luma(sampleI);

    // Soft min and max.
    //  a b c             b
    //  d e f * 0.5  +  d e f * 0.5
    //  g h i             h
    // These are 2.0x bigger (factored out the extra multiply).
    float minimumLuma = min3(min3(lumaD, lumaE, lumaF), lumaB, lumaH);
    float minimumLuma2 = min3(min3(minimumLuma, lumaA, lumaC), lumaG, lumaI);
    minimumLuma += minimumLuma2;
    float maximumLuma = max3(max3(lumaD, lumaE, lumaF), lumaB, lumaH);
    float maximumLuma2 = max3(max3(maximumLuma, lumaA, lumaC), lumaG, lumaI);
    maximumLuma += maximumLuma2;

    // Smooth saturate soft min and max for HDR source.
    // Since min and max are 2x bigger the peak is at 2.
    if (hdr) {
        minimumLuma = smooth_saturate(minimumLuma, 2.0, m, p);
        maximumLuma = smooth_saturate(maximumLuma, 2.0, m, p);
    }

    // Smooth minimum distance to signal limit divided by smooth max.
    float amplifyLuma = saturate(min(minimumLuma, 2.0 - maximumLuma) / maximumLuma);

    // Shaping amount of sharpening.
    amplifyLuma = sqrt(amplifyLuma);

    // Filter shape.
    //  0 w 0
    //  w 1 w
    //  0 w 0
    float peak = -0.2 * sqrt(sharpness);
    float weight = amplifyLuma * peak;

    // Filter.
    sampleE.rgb = (sampleB * weight + sampleD * weight + sampleF * weight + sampleH * weight + sampleE.rgb) / (1.0 + 4.0 * weight);

    if (!hdr) {
        sampleE.rgb = saturate(sampleE.rgb);
    }

    return float4(sampleE.rgb, sampleE.a);
}

#endif // __AMD_FFX_CAS_HLSLI__