#include "Color.hlsli"

SamplerState smp : register(s0);
Texture2D tex : register(t0);

float4 main(float4 pos : SV_Position) : SV_Target
{
	float4 color = tex.Load(int3(pos.xy, 0));
	
	#ifdef SRGB
	color.rgb = linear_to_srgb(color.rgb);
	#else
	color.rgb = linear_to_gamma(color.rgb);
	#endif

	return float4(color.rgb, color.a);
}