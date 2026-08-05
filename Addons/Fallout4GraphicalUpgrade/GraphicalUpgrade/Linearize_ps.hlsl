#include "Include/Common.hlsli"

Texture2D tex0 : register(t0);

float4 main(float4 pos : SV_Position) : SV_Target
{
	float4 color = tex0.Load(int3(pos.xy, 0));
	return float4(srgb_to_linear(color.rgb), color.a);	
}