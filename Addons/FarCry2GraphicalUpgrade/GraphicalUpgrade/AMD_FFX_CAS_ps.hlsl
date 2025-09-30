// User configurable modified AMD FFX CAS
// Adopted and optimized for shader model 4.1.
// Rescaled the effect of sharpening, now sharpness = 0 means no sharpening and sharpness = 1 is same as before.
// Source: https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/54fbaafdc34716811751bea5032700e78f5a0f33/sdk/include/FidelityFX/gpu/cas/ffx_cas.h

#include "Color.hlsli"

// Should be in linear color space.
Texture2D tex : register(t0);

SamplerState smp : register(s1);

#ifndef SHARPNESS
#define SHARPNESS 0.4
#endif

float3 min3(float3 x, float3 y, float3 z)
{
	return min(x, min(y, z));
}

float3 max3(float3 x, float3 y, float3 z)
{
	return max(x, max(y, z));
}

float4 main(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	// Load a collection of samples in a 3x3 neighorhood, where e is the current pixel.
	// a b c
	// d e f
	// g h i
	float3 sampleA = tex.SampleLevel(smp, texcoord, 0.0, int2(-1, -1)).rgb;
	float3 sampleB = tex.SampleLevel(smp, texcoord, 0.0, int2(0, -1)).rgb;
	float3 sampleC = tex.SampleLevel(smp, texcoord, 0.0, int2(1, -1)).rgb;
	float3 sampleD = tex.SampleLevel(smp, texcoord, 0.0, int2(-1, 0)).rgb;
	float4 sampleE = tex.SampleLevel(smp, texcoord, 0.0);
	float3 sampleF = tex.SampleLevel(smp, texcoord, 0.0, int2(1, 0)).rgb;
	float3 sampleG = tex.SampleLevel(smp, texcoord, 0.0, int2(-1, 1)).rgb; 
	float3 sampleH = tex.SampleLevel(smp, texcoord, 0.0, int2(0, 1)).rgb;
	float3 sampleI = tex.SampleLevel(smp, texcoord, 0.0, int2(1, 1)).rgb;

	// Soft min and max.
	//  a b c             b
	//  d e f * 0.5  +  d e f * 0.5
	//  g h i             h
	// These are 2.0x bigger (factored out the extra multiply).
	float3 minimumRGB = min3(min3(sampleD, sampleE.rgb, sampleF), sampleB, sampleH);
	float3 minimumRGB2 = min3(min3(minimumRGB, sampleA, sampleC), sampleG, sampleI);
	minimumRGB += minimumRGB2;
	float3 maximumRGB = max3(max3(sampleD, sampleE.rgb, sampleF), sampleB, sampleH);
	float3 maximumRGB2 = max3(max3(maximumRGB, sampleA, sampleC), sampleG, sampleI);
	maximumRGB += maximumRGB2;

	// Smooth minimum distance to signal limit divided by smooth max.
	float3 amplifyRGB = saturate(min(minimumRGB, 2.0 - maximumRGB) / maximumRGB);
	
	// Shaping amount of sharpening.
	amplifyRGB = sqrt(amplifyRGB);

	// Filter shape.
	//  0 w 0
	//  w 1 w
	//  0 w 0
	float peak = -0.2 * sqrt(saturate(SHARPNESS));
	float3 weight = amplifyRGB * peak;

	// Filter using green coef only, depending on dead code removal to strip out the extra overhead.
	sampleE.rgb = saturate((sampleB * weight.g + sampleD * weight.g + sampleF * weight.g + sampleH * weight.g + sampleE.rgb) / (1.0 + 4.0 * weight.g));

	// Delinearize.
	sampleE.rgb = linear_to_srgb(sampleE.rgb);

	return float4(sampleE.rgb, sampleE.a);
}