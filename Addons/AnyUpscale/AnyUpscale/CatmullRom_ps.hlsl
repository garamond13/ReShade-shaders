// Fast Catmull-Rom approximation using 5 samples.

Texture2D tex : register(t0);
SamplerState smp : register(s0);

// MUST BE DEFINED!
#ifndef SRC_SIZE
#define SRC_SIZE float2(0.0, 0.0)
#endif

#ifndef INV_SRC_SIZE
#define INV_SRC_SIZE (1.0 / SRC_SIZE)
#endif

float4 main(float4 pos : SV_Position, noperspective float2 texcoord : TEXCOORD) : SV_Target
{
	const float2 f = frac(texcoord * SRC_SIZE - 0.5);
	const float2 tc = texcoord - f * INV_SRC_SIZE;
	const float2 f2 = f * f;
	const float2 f3 = f2 * f;

	// Catmull-Rom weights.
	const float2 w0 = f2 - 0.5 * (f3 + f);
	const float2 w1 = 1.5 * f3 - 2.5 * f2 + 1.0;
	const float2 w3 = 0.5 * (f3 - f2);
	const float2 w2 = 1.0 - w0 - w1 - w3;
	const float2 w12 = w1 + w2;

	// Texel coords.
	const float2 tc0 = tc - 1.0 * INV_SRC_SIZE;
	const float2 tc3 = tc + 2.0 * INV_SRC_SIZE;
	const float2 tc12 = tc + w2 * rcp(w12) * INV_SRC_SIZE;

	// Combined weights.
	const float w12w0 = w12.x * w0.y;
	const float w0w12 = w0.x * w12.y;
	const float w12w12 = w12.x * w12.y;
	const float w3w12 = w3.x * w12.y;
	const float w12w3 = w12.x * w3.y;

	float3 c = tex.SampleLevel(smp, float2(tc12.x, tc0.y), 0.0).rgb * w12w0;
	c += tex.SampleLevel(smp, float2(tc0.x, tc12.y), 0.0).rgb * w0w12;
	c += tex.SampleLevel(smp, tc12, 0.0).rgb * w12w12;
	c += tex.SampleLevel(smp, float2(tc3.x, tc12.y), 0.0).rgb * w3w12;
	c += tex.SampleLevel(smp, float2(tc12.x, tc3.y), 0.0).rgb * w12w3;

	// Normalize.
	c *= rcp(w12w0 + w0w12 + w12w12 + w3w12 + w12w3);

	return float4(c, 1.0);
}