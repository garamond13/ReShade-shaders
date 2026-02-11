// AMD FidelityFX Robust Contrast Adaptive Sharpening
// Ported from: https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/54fbaafdc34716811751bea5032700e78f5a0f33/sdk/include/FidelityFX/gpu/fsr1/ffx_fsr1.h

#include "Color.hlsli"

// Should be in linear color space.
Texture2D tex : register(t0);

// User configurable.
//

#ifndef SHARPNESS
#define SHARPNESS 1.0
#endif

#ifndef FSR_RCAS_DENOISE
#define FSR_RCAS_DENOISE 1
#endif

//

#define min3(x,y,z) min(x, min(y, z))
#define max3(x,y,z) max(x, max(y, z))

// This is set at the limit of providing unnatural results for sharpening.
#define FSR_RCAS_LIMIT (0.25 - (1.0 / 16.0))

float4 main(float4 pos : SV_Position) : SV_Target
{
	// Algorithm uses minimal 3x3 pixel neighborhood.
	//    b
	//  d e f
	//    h
	float3 b = tex.Load(int3(pos.xy, 0), int2(0, -1)).rgb;
	float3 d = tex.Load(int3(pos.xy, 0), int2(-1, 0)).rgb;
	float4 e = tex.Load(int3(pos.xy, 0));
	float3 f = tex.Load(int3(pos.xy, 0), int2(1, 0)).rgb;
	float3 h = tex.Load(int3(pos.xy, 0), int2(0, 1)).rgb;

	// Luma times 2.
	float bL = b.b * 0.5 + (b.r * 0.5 + b.g);
	float dL = d.b * 0.5 + (d.r * 0.5 + d.g);
	float eL = e.b * 0.5 + (e.r * 0.5 + e.g);
	float fL = f.b * 0.5 + (f.r * 0.5 + f.g);
	float hL = h.b * 0.5 + (h.r * 0.5 + h.g);

	// Noise detection.
	float nz = 0.25 * bL + 0.25 * dL + 0.25 * fL + 0.25 * hL - eL;
	nz = saturate(abs(nz) * rcp(max3(max3(bL, dL, eL), fL, hL) - min3(min3(bL, dL, eL), fL, hL)));
	nz = -0.5 * nz + 1.0;

	// Min and max of ring.
	float3 mn4 = min(min3(b, d, f), h);
	float3 mx4 = max(max3(b, d, f), h);
	
	// Immediate constants for peak range.
	float2 peakC = float2(1.0, -1.0 * 4.0);

	// Limiters, these need to be high precision RCPs.
	float3 hitMin = mn4 * rcp(4.0 * mx4);
	float3 hitMax = (peakC.x - mx4) * rcp(4.0 * mn4 + peakC.y);
	float3 lobeRGB = max(-hitMin, hitMax);
	float lobe = max(-FSR_RCAS_LIMIT, min(max3(lobeRGB.r, lobeRGB.g, lobeRGB.b), 0.0)) * SHARPNESS;

	// Apply noise removal.
	#if FSR_RCAS_DENOISE
	lobe *= nz;
	#endif

	// Resolve, which needs the medium precision rcp approximation to avoid visible tonality changes.
	float rcpL = rcp(4.0 * lobe + 1.0);
	e.rgb = (lobe * b + lobe * d + lobe * h + lobe * f + e.rgb) * rcpL;

	// Delinearize.
	#ifdef SRGB
	e.rgb = linear_to_srgb(e.rgb);
	#else
	e.rgb = linear_to_gamma(e.rgb);
	#endif

	return e;
}