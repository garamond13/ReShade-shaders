void main(uint vid : SV_VertexID, out float4 pos : SV_Position, out float2 texcoord : TEXCOORD)
{
	texcoord = float2((vid << 1) & 2, vid & 2);
	pos = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}