// Orhogonal resampling using Power of Garamond windowed Sinc as a kernel function.
// Samples one axis at a time.

Texture2D tex : register(t0);
SamplerState smp : register(s0);

// MUST BE DEFINED!
//

#ifndef SRC_SIZE
#define SRC_SIZE float2(0.0, 0.0)
#endif

// X axis float2(1.0, 0.0)
// Y axis float2(0.0, 1.0)
#ifndef AXIS
#define AXIS float2(1.0, 0.0)
#endif

//

// User configurable.
//

#ifndef BLUR
#define BLUR 1.0
#endif

#ifndef SUPPORT
#define SUPPORT 2.0
#endif

// Power of Garamond window parameters.
// Has to be satisfied: N >= 0, M >= 0.
// M = 1.0: Garamond window.
// N = 2.0, M = 1.0: Welch window.
// N = 1.0, M = 1.0: Linear window.
// N -> inf, M <= 1.0: Box window.
// N = 0.0: Box window.
// M = 0.0: Box window.
////

#ifndef N
#define N 2.0
#endif

#ifndef M
#define M 1.0
#endif

////

//

#ifndef INV_SRC_SIZE
#define INV_SRC_SIZE (1.0 / SRC_SIZE)
#endif

#ifndef RADIUS
#define RADIUS ceil(SUPPORT)
#endif

#define PI 3.14159265358979323846
#define FLT_EPS 1e-6

// Normalized version sinc: x == 0.0 ? 1.0 : sin(PI * x / BLUR) / (PI * x / BLUR).
float sinc(float x)
{
	return x < FLT_EPS ? PI / BLUR : sin(PI / BLUR * x)  * rcp(x);
}

float power_of_garamond(float x)
{
	return abs(N) < FLT_EPS ? 1.0 : pow(1.0 - pow(x / SUPPORT, N), M);
}

float get_weight(float x)
{
	return x <= SUPPORT ? sinc(x) * power_of_garamond(x) : 0.0;
}

float4 main(float4 pos : SV_Position, noperspective float2 texcoord : TEXCOORD) : SV_Target
{
	const float f = dot(frac(texcoord * SRC_SIZE - 0.5), AXIS);
	const float2 tc = texcoord - f * INV_SRC_SIZE * AXIS;
	float3 csum = 0.0;
	float wsum = 0.0;

	// Antiringing.
	float3 lo = 1e9;
	float3 hi = -1e9;

	[unroll]
	for (float i = 1.0 - RADIUS; i <= RADIUS; ++i) {
		const float3 color = tex.SampleLevel(smp, tc + i * INV_SRC_SIZE * AXIS, 0.0).rgb;
		const float weight = get_weight(abs(i - f));
		csum += color * weight;
		wsum += weight;

		// Antiringing.
		if (i >= 0.0 && i <= 1.0) {
			lo = min(lo, color);
			hi = max(hi, color);
		}
	}

	// Normalize.
	csum *= rcp(wsum);

	return float4(clamp(csum, lo, hi), 1.0);
}
