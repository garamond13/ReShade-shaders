#include "Color.hlsli"

Texture2D tex : register(t0);
SamplerState smp : register(s1);

float4 main(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
	float4 color = tex.SampleLevel(smp, texcoord, 0.0);
	color.rgb = srgb_to_linear(color.rgb);
	return float4(color.rgb, color.a);
}