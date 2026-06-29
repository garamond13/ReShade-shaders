Texture2D<uint> tGBuffer4 : register(t4); // Packed MVs.
RWTexture2D<float2> uav : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	uint packed_mvs = tGBuffer4.Load(int3(dtid.xy, 0)).x;

	// Unpack MVs.
	float2 mvs;
	mvs.x = f16tof32(packed_mvs >> 16);
	mvs.y = f16tof32(packed_mvs & 0xFFFE);

	uav[dtid.xy] = mvs;
}