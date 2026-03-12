// AMD FFX LFGA
//
// Source: https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/54fbaafdc34716811751bea5032700e78f5a0f33/sdk/include/FidelityFX/gpu/cas/ffx_cas.h

#include "Color.hlsli"

cbuffer graphical_upgrade : register(b13)
{
	float tex_noise_index;
}

// Should be in linear color space.
Texture2D tex : register(t0);

Texture2DArray<float> tex_noise : register(t1);

// int2(width, height)
#ifndef TEX_NOISE_DIMS
#define TEX_NOISE_DIMS int2(256, 256)
#endif

#ifndef AMOUNT
#define AMOUNT 1.0
#endif

float4 main(float4 pos : SV_Position) : SV_Target
{
	const int2 noise_xy = int2(pos.xy) % TEX_NOISE_DIMS;
	float noise = tex_noise.Load(int4(noise_xy, tex_noise_index, 0));

	// Scale noise in range [-0.5, 0.5].
	noise -= 0.5;

	float4 color = tex.Load(int3(pos.xy, 0));
	color.rgb += noise * AMOUNT * min(1.0 - color.rgb, color.rgb);

	// Delinearize.
	#ifdef SRGB
	color.rgb = linear_to_srgb(color.rgb);
	#else
	color.rgb = linear_to_gamma(color.rgb);
	#endif

	return color;
}