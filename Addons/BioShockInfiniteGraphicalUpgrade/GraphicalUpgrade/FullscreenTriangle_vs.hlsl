#include "FullscreenTriangle.hlsli"

void main(uint vid : SV_VertexID, out float4 pos : SV_Position, out float2 texcoord : TEXCOORD)
{
	fullscreen_triangle(vid, pos, texcoord);
}