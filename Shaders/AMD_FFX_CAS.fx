// AMD FidelityFX Contrast Adaptive Sharpening
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

	// Soft min and max.
	//  a b c             b
	//  d e f * 0.5  +  d e f * 0.5
	//  g h i             h
	// These are 2.0x bigger (factored out the extra multiply).
	float3 minimumRGB = min3(min3(sampleD, sampleE, sampleF), sampleB, sampleH);
	float3 minimumRGB2 = min3(min3(minimumRGB, sampleA, sampleC), sampleG, sampleI);
	minimumRGB += minimumRGB2;
	float3 maximumRGB = max3(max3(sampleD, sampleE, sampleF), sampleB, sampleH);
	float3 maximumRGB2 = max3(max3(maximumRGB, sampleA, sampleC), sampleG, sampleI);
	maximumRGB += maximumRGB2;

	// Smooth minimum distance to signal limit divided by smooth max.
	float3 reciprocalMaximumRGB = rcp(maximumRGB);
	float3 amplifyRGB = saturate(min(minimumRGB, 2.0 - maximumRGB) * reciprocalMaximumRGB);	
	
	// Shaping amount of sharpening.
	amplifyRGB = sqrt(amplifyRGB);

	// Filter shape.
	//  0 w 0
	//  w 1 w
	//  0 w 0
	float peak = -rcp(lerp(8.0, 5.0, saturate(sharpness)));
	float3 weight = amplifyRGB * peak;

	// Filter using green coef only, depending on dead code removal to strip out the extra overhead.
	float reciprocalWeight = rcp(1.0 + 4.0 * weight.g);

	return delinearize(saturate((sampleB * weight.g + sampleD * weight.g + sampleF * weight.g + sampleH * weight.g + sampleE) * reciprocalWeight));
}

technique CAS < ui_label = "AMD FidelityFX Contrast Adaptive Sharpening"; >
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