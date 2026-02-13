#include "Color.hlsli"

Texture2D tex : register(t0);

float4 main(float4 pos : SV_Position) : SV_Target
{	
	float4 color = tex.Load(int3(pos.xy, 0));
	return float4(linear_to_srgb(color.rgb), color.a);
}