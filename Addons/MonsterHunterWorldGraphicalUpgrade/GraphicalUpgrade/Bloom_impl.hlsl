#include "Include/GraphicalUpgradeCB.hlsli.h"

// From BloomThreshold_0xC1DD61BF_ps.
cbuffer CBLuminance : register(b12)
{
	uint iView;
	float fKeyValue;
	bool bClearHistory;
	float fLuminanceLogScale;
	float fLuminanceLogBias;
	float fLuminanceExpScale;
	float fLuminanceExpBias;
	float fWhiteRange;
	float fExposureValue;
	float fDarkSensitivity;
	float fLightSensitivity;
	float fDarkAdaptationLimit;
	float fLightAdaptationLimit;
}

cbuffer CBBloom : register(b3)
{
	float fBloomThreshold : packoffset(c0);
	float fBloomRenormalize : packoffset(c0.y);
	bool bGamutSrgb : packoffset(c0.z);
	float4 fDirtColor : packoffset(c1);
}

// From BloomThreshold_0xC1DD61BF_ps.
struct LuminanceBuffer
{
	float minimum;
	float maximum;
	float average;
	float current;
	float factor;
	float padding[3];
};

SamplerState smp : register(s0); // Should be linear clamp.
Texture2D tex : register(t0);
StructuredBuffer<LuminanceBuffer> gLuminanceBufferSRV : register(t1); // From BloomThreshold_0xC1DD61BF_ps.

// User configurable
//

#ifndef BLOOM_THRESHOLD
#define BLOOM_THRESHOLD fBloomThreshold
#endif

#ifndef BLOOM_SOFT_KNEE
#define BLOOM_SOFT_KNEE (BLOOM_THRESHOLD * 0.5)
#endif

//

float3 eye_adaptation(float3 color)
{
	if (any(isnan(color) | isinf(color) | color < 0.0)) {
		return 0.0;
	}
	else {
		return color * gLuminanceBufferSRV[iView].factor * fExposureValue;
	}
}

float3 quadratic_threshold(float3 color)
{
	const float br = bGamutSrgb ? dot(color, float3(0.2126, 0.7152, 0.0722)) : dot(color, float3(0.2990, 0.5870, 0.1140));

	// Under the threshold part, a quadratic curve.
	// Above the threshold part will be a linear curve.
	const float3 curve = float3(BLOOM_THRESHOLD - BLOOM_SOFT_KNEE, BLOOM_SOFT_KNEE * 2.0, 0.25 / BLOOM_SOFT_KNEE);
	float rq = clamp(br - curve.x, 0.0, curve.y);
	rq = curve.z * rq * rq;

	// Combine and apply the brightness response curve.
	return color * max(rq, br - BLOOM_THRESHOLD) * rcp(max(1e-6, br));
}

float get_gaussian_weight(float x)
{
	return exp(-x * x * rcp(2.0 * sigma * sigma));
}

// Fullscreen triangle VS.
void main_vs(uint vid : SV_VertexID, out float4 pos : SV_Position, out float2 texcoord : TEXCOORD)
{
    texcoord = float2((vid << 1) & 2, vid & 2);
    pos = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

// Prefilter should only be used as the second axis pass on the first MIP.
// Samples one axis at a time.
float4 prefilter_ps(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	// Calculate fractional part and texel center.
	const float f = dot(frac(texcoord * src_size - 0.5), axis);
	const float2 tc = texcoord - f * inv_src_size * axis;

	float3 csum = 0.0;
	float wsum = 0.0;

	// Calculate kernel radius.
	const float radius = ceil(sigma * 3.0);

	for (float i = 1.0 - radius; i <= radius; ++i) {
		const float weight = get_gaussian_weight(i - f);
		csum += tex.SampleLevel(smp, tc + i * inv_src_size * axis, 0.0).rgb * weight;
		wsum += weight;
	}

	// Normalize.
	csum *= rcp(wsum);

	// Apply eye adaptation and threshold.
	float3 color = eye_adaptation(csum);
	color = quadratic_threshold(color);

	return float4(color, 1.0);
}

// Samples one axis at a time.
float4 downsample_ps(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	// Calculate fractional part and texel center.
	const float f = dot(frac(texcoord * src_size - 0.5), axis);
	const float2 tc = texcoord - f * inv_src_size * axis;

	float3 csum = 0.0;
	float wsum = 0.0;

	// Calculate kernel radius.
	const float radius = ceil(sigma * 3.0);

	for (float i = 1.0 - radius; i <= radius; ++i) {
		const float weight = get_gaussian_weight(i - f);
		csum += tex.SampleLevel(smp, tc + i * inv_src_size * axis, 0.0).rgb * weight;
		wsum += weight;
	}

	// Normalize.
	csum *= rcp(wsum);

	return float4(csum, 1.0);
}

// Bicubic upsampling in 4 texture fetches.
//
// f(x) = (4 + 3 * |x|^3 – 6 * |x|^2) / 6 for 0 <= |x| <= 1
// f(x) = (2 – |x|)^3 / 6 for 1 < |x| <= 2
// f(x) = 0 otherwise
//
// Source: https://www.researchgate.net/publication/220494113_Efficient_GPU-Based_Texture_Interpolation_using_Uniform_B-Splines
float4 upsample_ps(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	// transform the coordinate from [0,extent] to [-0.5, extent-0.5]
	float2 coord_grid = texcoord * src_size - 0.5;
	float2 index = floor(coord_grid);
	float2 fraction = coord_grid - index;
	float2 one_frac = 1.0 - fraction;
	float2 one_frac2 = one_frac * one_frac;
	float2 fraction2 = fraction * fraction;
	float2 w0 = 1.0 / 6.0 * one_frac2 * one_frac;
	float2 w1 = 2.0 / 3.0 - 0.5 * fraction2 * (2.0 - fraction);
	float2 w2 = 2.0 / 3.0 - 0.5 * one_frac2 * (2.0 - one_frac);
	float2 w3 = 1.0 / 6.0 * fraction2 * fraction;
	float2 g0 = w0 + w1;
	float2 g1 = w2 + w3;

	// h0 = w1/g0 - 1, move from [-0.5, extent-0.5] to [0, extent]
	float2 h0 = ((w1 / g0) - 0.5 + index) * inv_src_size;
	float2 h1 = ((w3 / g1) + 1.5 + index) * inv_src_size;

	// fetch the four linear interpolations
	float3 tex00 = tex.SampleLevel(smp, float2(h0.x, h0.y), 0.0).rgb;
	float3 tex10 = tex.SampleLevel(smp, float2(h1.x, h0.y), 0.0).rgb;
	float3 tex01 = tex.SampleLevel(smp, float2(h0.x, h1.y), 0.0).rgb;
	float3 tex11 = tex.SampleLevel(smp, float2(h1.x, h1.y), 0.0).rgb;

	// weigh along the y-direction
	tex00 = lerp(tex01, tex00, g0.y);
	tex10 = lerp(tex11, tex10, g0.y);

	// weigh along the x-direction
	return float4(lerp(tex10, tex00, g0.x), 1.0);
}
