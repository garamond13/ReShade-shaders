// AMD FFX LFGA
//
// Source: https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/54fbaafdc34716811751bea5032700e78f5a0f33/sdk/include/FidelityFX/gpu/cas/ffx_cas.h

#include "Include/Common.hlsli"
#include "Include/GraphicalUpgradeCB.hlsli.h"

// Should be in linear color space.
Texture2D tex : register(t0);

#ifndef AMOUNT
#define AMOUNT 1.0
#endif

float4 main(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	// Noise in range [-0.5, 0.5].
	// Compiler should optimize out this if AMOUNT is 0.
	const float noise = perlin_noise(float3(pos.xy + float2(white_noise(texcoord.yx), white_noise(texcoord.xy)), noise_index)) * 0.5;

	float4 color = tex.Load(int3(pos.xy, 0));
	color.rgb += noise * AMOUNT * min(1.0 - color.rgb, color.rgb);

	// Delinearize.
	color.rgb = linear_to_srgb(color.rgb);

	return color;
}