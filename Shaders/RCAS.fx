// AMD FidelityFX Robust Contrast Adaptive Sharpening
// Ported from: https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/54fbaafdc34716811751bea5032700e78f5a0f33/sdk/include/FidelityFX/gpu/fsr1/ffx_fsr1.h

#include "ReShade.fxh"

uniform float sharpness <
	ui_type = "drag";
	ui_label = "Sharpness";
	ui_min = 0.0;
	ui_max = 1.0;
> = 1.0;

float min3(float x, float y, float z)
{
	return min(x, min(y, z));
}

float3 min3(float3 x, float3 y, float3 z)
{
	return min(x, min(y, z));
}

float max3(float x, float y, float z)
{
	return max(x, max(y, z));
}

float3 max3(float3 x, float3 y, float3 z)
{
	return max(x, max(y, z));
}

float linearize(float x)
{
	return x < 0.04045 ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4);
}

float3 linearize(float3 rgb)
{
	return float3(linearize(rgb.r), linearize(rgb.g), linearize(rgb.b));
}

float delinearize(float x)
{
	return x < 0.0031308 ? x * 12.92 : pow(x, 1.0 / 2.4) * 1.055 - 0.055;
}

float3 delinearize(float3 rgb)
{
	return float3(delinearize(rgb.r), delinearize(rgb.g), delinearize(rgb.b));
}

// This is set at the limit of providing unnatural results for sharpening.
#define FSR_RCAS_LIMIT (0.25 - (1.0 / 16.0))

#ifndef FSR_RCAS_DENOISE
#define FSR_RCAS_DENOISE 1
#endif

float3 FsrRcas(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
	// Algorithm uses minimal 3x3 pixel neighborhood.
	//    b
	//  d e f
	//    h
	float3 b = linearize(tex2D(ReShade::BackBuffer, texcoord, int2(0, -1)).rgb);
	float3 d = linearize(tex2D(ReShade::BackBuffer, texcoord, int2(-1, 0)).rgb);
	float3 e = linearize(tex2D(ReShade::BackBuffer, texcoord).rgb);
	float3 f = linearize(tex2D(ReShade::BackBuffer, texcoord, int2(1, 0)).rgb);
	float3 h = linearize(tex2D(ReShade::BackBuffer, texcoord, int2(0, 1)).rgb);

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
	float lobe = max(-FSR_RCAS_LIMIT, min(max3(lobeRGB.r, lobeRGB.g, lobeRGB.b), 0.0)) * sharpness; // Originaly "sharpness" is unintuitively defined as "sharpness = exp2(-sharpness)".

	// Apply noise removal.
	#if FSR_RCAS_DENOISE
	lobe *= nz;
	#endif

	// Resolve, which needs the medium precision rcp approximation to avoid visible tonality changes.
	float rcpL = rcp(4.0 * lobe + 1.0);
	return delinearize((lobe * b + lobe * d + lobe * h + lobe * f + e) * rcpL);
}

technique RCAS < ui_label = "AMD FidelityFX Robust Contrast Adaptive Sharpening"; >
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = FsrRcas;
	}
}