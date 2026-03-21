// Modified AMD FidelityFX Contrast Adaptive Sharpening
//
// Rescaled the effect of sharpening, now sharpness = 0 means no sharpening and sharpness = 1 is same as before.
// Instead of using only green channel it uses luma.
//
// Ported from: https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/54fbaafdc34716811751bea5032700e78f5a0f33/sdk/include/FidelityFX/gpu/cas/ffx_cas.h

#include "ReShade.fxh"

uniform float sharpness <
	ui_type = "drag";
	ui_label = "Sharpness";
	ui_min = 0.0;
	ui_max = 1.0;
> = 1.0;

sampler2D smpColor
{
	Texture = ReShade::BackBufferTex;

	#if BUFFER_COLOR_BIT_DEPTH == 8
	SRGBTexture = true;
	#endif
};

#define min3(x,y,z) min(x, min(y, z))
#define max3(x,y,z) max(x, max(y, z))

float3 _linearize(float3 rgb)
{
	return rgb < 0.04045 ? rgb / 12.92 : pow((rgb + 0.055) / 1.055, 2.4);
}

float3 _delinearize(float3 rgb)
{
	return rgb < 0.0031308 ? rgb * 12.92 : pow(rgb, 1.0 / 2.4) * 1.055 - 0.055;
}

#if BUFFER_COLOR_BIT_DEPTH == 8
#define linearize(x) (x)
#define delinearize(x) (x)
#else
#define linearize(x) (_linearize(x))
#define delinearize(x) (_delinearize(x))
#endif

float get_luma(float3 color)
{
	return dot(color, float3(0.2126, 0.7152, 0.0722));
}

// With defined FFX_CAS_BETTER_DIAGONALS and FFX_CAS_USE_PRECISE_MATH.
float3 casFilterNoScaling(float4 pos : SV_Position) : SV_Target
{
	// Load a collection of samples in a 3x3 neighorhood, where e is the current pixel.
	// a b c
	// d e f
	// g h i
	float3 sampleA = linearize(tex2Dfetch(smpColor, int2(pos.xy) + int2(-1, -1), 0).rgb);
	float3 sampleB = linearize(tex2Dfetch(smpColor, int2(pos.xy) + int2(0, -1), 0).rgb);
	float3 sampleC = linearize(tex2Dfetch(smpColor, int2(pos.xy) + int2(1, -1), 0).rgb);
	float3 sampleD = linearize(tex2Dfetch(smpColor, int2(pos.xy) + int2(-1, 0), 0).rgb);
	float3 sampleE = linearize(tex2Dfetch(smpColor, int2(pos.xy), 0).rgb);
	float3 sampleF = linearize(tex2Dfetch(smpColor, int2(pos.xy) + int2(1, 0), 0).rgb);
	float3 sampleG = linearize(tex2Dfetch(smpColor, int2(pos.xy) + int2(-1, 1), 0).rgb); 
	float3 sampleH = linearize(tex2Dfetch(smpColor, int2(pos.xy) + int2(0, 1), 0).rgb);
	float3 sampleI = linearize(tex2Dfetch(smpColor, int2(pos.xy) + int2(1, 1), 0).rgb);

	// Get lumas for samples.
	float lumaA = get_luma(sampleA);
	float lumaB = get_luma(sampleB);
	float lumaC = get_luma(sampleC);
	float lumaD = get_luma(sampleD);
	float lumaE = get_luma(sampleE);
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

	// Smooth minimum distance to signal limit divided by smooth max.
	float amplifyLuma = saturate(min(minimumLuma, 2.0 - maximumLuma) * rcp(maximumLuma));
	
	// Shaping amount of sharpening.
	amplifyLuma = sqrt(amplifyLuma);

	// Filter shape.
	//  0 w 0
	//  w 1 w
	//  0 w 0
	float peak = -0.2 * sqrt(saturate(sharpness));
	float weight = amplifyLuma * peak;

	// Filter using green coef only, depending on dead code removal to strip out the extra overhead.
	float reciprocalWeight = rcp(1.0 + 4.0 * weight);

	return delinearize(saturate((sampleB * weight + sampleD * weight + sampleF * weight + sampleH * weight + sampleE) * reciprocalWeight));
}

technique CAS < ui_label = "Modified AMD FFX CAS"; >
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = casFilterNoScaling;

		#if BUFFER_COLOR_BIT_DEPTH == 8
		SRGBWriteEnable = true;
		#endif
	}
}