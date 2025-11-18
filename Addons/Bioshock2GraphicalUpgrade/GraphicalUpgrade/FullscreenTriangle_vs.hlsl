#include "FullscreenTriangle.hlsli"

void main(uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
{
	fullscreen_triangle(id, position, texcoord);
}