// AMD FidelityFX Linear Film Grain Aplicator
// Ported from: https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/54fbaafdc34716811751bea5032700e78f5a0f33/sdk/include/FidelityFX/gpu/fsr1/ffx_fsr1.h

#include "ReShade.fxh"

uniform float a <
	ui_type = "drag";
	ui_label = "Amount";
	ui_min = 0.0;
	ui_max = 1.0;
> = 1.0;

#define NOISE_TEX_SIZE 512

uniform int random_value < source = "random"; min = 0; max = NOISE_TEX_SIZE; >;

texture2D noise < source = "LFGANoise.png"; >
{
	Width = NOISE_TEX_SIZE;
	Height = NOISE_TEX_SIZE;
	Format = R8;
};

sampler2D samplerNoise
{
	Texture = noise;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler2D samplerColor
{
	Texture = ReShade::BackBufferTex;
	SRGBTexture = true;
};

float3 FsrLfga(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
	// Sample noise.
	float2 tiles = float2(BUFFER_WIDTH, BUFFER_HEIGHT) / float(NOISE_TEX_SIZE);
	float t = tex2D(samplerNoise, texcoord * tiles + float(random_value) / float(NOISE_TEX_SIZE));
	
	// Scale noise in range -0.5 to 0.5.
	t -= 0.5;
	
	float3 c = tex2D(samplerColor, texcoord).rgb;
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
