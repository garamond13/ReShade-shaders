// AMD FidelityFX Linear Film Grain Aplicator
// Ported from: https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/54fbaafdc34716811751bea5032700e78f5a0f33/sdk/include/FidelityFX/gpu/fsr1/ffx_fsr1.h

#include "ReShade.fxh"

uniform float a <
	ui_type = "drag";
	ui_label = "Amount";
	ui_min = 0.0;
	ui_max = 1.0;
> = 1.0;

// Reshade uses C rand for random, max cannot be larger than 2^15-1.
uniform int random_value < source = "random"; min = 0; max = 32767; >;

sampler2D samplerColor
{
	Texture = ReShade::BackBufferTex;
	SRGBTexture = true;
};

float3 FsrLfga(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
	
	float3 c = tex2D(samplerColor, texcoord).rgb;
	
	// Generate white noise in range -0.5 to 0.5.
	float t = frac(sin(dot(texcoord * (random_value / 32767.0 + 1.0), float2(12.9898, 78.233))) * 43758.5453) - 0.5;
	
	return c + (t * a) * min(1.0 - c, c);
}

technique LFGA < ui_label = "AMD FidelityFX Linear Film Grain Aplicator"; >
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = FsrLfga;
		SRGBWriteEnable = true;
	}
}
