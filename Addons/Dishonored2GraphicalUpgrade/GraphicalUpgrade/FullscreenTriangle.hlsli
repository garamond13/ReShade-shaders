void fullscreen_triangle(uint id, out float4 position, out float2 texcoord)
{
	texcoord = float2((id << 1) & 2, id & 2);
	position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}