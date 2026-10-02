#include "Include/Common.hlsli"

Texture2D t0 : register(t0);

float4 main(float4 pos : SV_Position) : SV_Target
{
    float4 color = t0.Load(int3(pos.xy, 0));
    color.rgb = srgb_to_linear(color.rgb);
    return saturate(color);
}