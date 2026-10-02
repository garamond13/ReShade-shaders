Texture2DMS<float4> tex : register(t0);

float4 main(float4 pos : SV_Position) : SV_Target
{
	uint w, h, n;
	tex.GetDimensions(w, h, n);

	// max
	float4 c = 0.0;
	for (uint i = 0; i < n; ++i) {
		c = max(c, tex.Load(int2(pos.xy), i));
	}

	return c;
}