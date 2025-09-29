// User configurable AMD FFX LFGA
// Source: https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/54fbaafdc34716811751bea5032700e78f5a0f33/sdk/include/FidelityFX/gpu/cas/ffx_cas.h

#include "Color.hlsli"

// Should be in linear color space.
Texture2D tex : register(t0);

Texture2DArray<float> tex_noise : register(t1);
SamplerState smp : register(s1);

// Should be point, wrap.
SamplerState smp_noise : register(s0);

cbuffer cb : register(b13)
{
	float tex_noise_index;
}

#ifndef WIEWPORT_DIMS
#define WIEWPORT_DIMS float2(0.0, 0.0)
#endif

// float2(width, height)
#ifndef TEX_NOISE_DIMS
#define TEX_NOISE_DIMS float2(128.0, 128.0)
#endif

#ifndef AMOUNT
#define AMOUNT 1.0
#endif

float4 main(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	float grain = tex_noise.SampleLevel(smp_noise, float3(texcoord * WIEWPORT_DIMS / TEX_NOISE_DIMS, tex_noise_index), 0.0);

	// Scale grain in range [-0.5, 0.5].
	grain -= 0.5;

	float4 color = tex.SampleLevel(smp, texcoord, 0.0);
	color.rgb += grain * AMOUNT * min(1.0 - color.rgb, color.rgb);

	// Delinearize.
	#ifdef SRGB
	color.rgb = linear_to_srgb(color.rgb);
	#else
	color.rgb = linear_to_gamma(color.rgb);
	#endif

	return color;
}