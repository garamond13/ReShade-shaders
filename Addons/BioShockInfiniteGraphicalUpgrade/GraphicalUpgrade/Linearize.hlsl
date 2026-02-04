#include "Color.hlsli"

Texture2D tex : register(t0);
RWTexture2D<float4> uav : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{	
	float4 color = tex.Load(int3(dtid.xy, 0));
	uav[dtid.xy] = float4(srgb_to_linear(color.rgb), color.a);
}