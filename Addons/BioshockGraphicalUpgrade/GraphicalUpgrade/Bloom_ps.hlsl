// Bloom
//

cbuffer _Globals : register(b12)
{
	float4 fogColor : packoffset(c0);
	float3 fogTransform : packoffset(c1);
	float4x3 screenDataToCamera : packoffset(c2);
	float globalScale : packoffset(c5);
	float sceneDepthAlphaMask : packoffset(c5.y);
	float globalOpacity : packoffset(c5.z);
	float distortionBufferScale : packoffset(c5.w);
	float2 wToZScaleAndBias : packoffset(c6);
	float4 screenTransform[2] : packoffset(c7);
	float4 textureToPixel : packoffset(c9);
	float4 pixelToTexture : packoffset(c10);
	float maxScale : packoffset(c11);
	float bloomAlpha : packoffset(c11.y);
	float sceneBias : packoffset(c11.z);
	float exposure : packoffset(c11.w);
	float deltaExposure : packoffset(c12);
	float4 SampleOffsets[8] : packoffset(c13);
	float4 SampleWeights[16] : packoffset(c21);
	float4 PWLConstants : packoffset(c37);
	float PWLThreshold : packoffset(c38);
	float ShadowEdgeDetectThreshold : packoffset(c38.y);
	float4 ColorFill : packoffset(c39);
}

cbuffer graphical_upgrade : register(b13)
{
	float2 src_size;
	float2 inv_src_size;
	float2 axis;
	float sigma;
	float tex_noise_index;
}

SamplerState smp : register(s1); // Should be linear clamp.
Texture2D tex : register(t0);

#define BLOOM_THRESHOLD PWLThreshold
#define BLOOM_SOFT_KNEE BLOOM_THRESHOLD
#define BLOOM_TINT float3(1.0, 1.3, 1.0)

float get_luma(float3 color)
{
	return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

float3 quadratic_threshold(float3 color)
{
	const float epsilon = 1e-6;

	// Pixel brightness.
	float br = max(max(color.r, color.g), color.b);
	br = max(epsilon, br);

	// Under the threshold part, a quadratic curve.
	// Above the threshold part will be a linear curve.
	const float k = max(epsilon, BLOOM_SOFT_KNEE);
	const float3 curve = float3(BLOOM_THRESHOLD - k, k * 2.0, 0.25 / k);
	float rq = clamp(br - curve.x, 0.0, curve.y);
	rq = curve.z * rq * rq;

	// Combine and apply the brightness response curve.
	return color * max(rq, br - BLOOM_THRESHOLD) * rcp(br);
}

float get_gaussian_weight(float x)
{
	return exp(-x * x * rcp(2.0 * sigma * sigma));
}

// Prefilter should only be used as the second axis pass on the first MIP.
// Samples one axis at a time.
float4 bloom_prefilter_ps(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
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

	// Apply threshold.
	float3 color = quadratic_threshold(csum);

	// Apply tint.
	const float luma = get_luma(color);
	color *= BLOOM_TINT;
	color *= luma * rcp(max(1e-6, get_luma(color)));

	return float4(color, 1.0);
}

// Samples one axis at a time.
float4 bloom_downsample_ps(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
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

float4 bloom_upsample_ps(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	// a - b - c
	// d - e - f
	// g - h - i
	const float3 a = tex.SampleLevel(smp, texcoord, 0.0, int2(-1, 1)).rgb;
	const float3 b = tex.SampleLevel(smp, texcoord, 0.0, int2(0, 1)).rgb;
	const float3 c = tex.SampleLevel(smp, texcoord, 0.0, int2(1, 1)).rgb;
	const float3 d = tex.SampleLevel(smp, texcoord, 0.0, int2(-1, 0)).rgb;
	const float3 e = tex.SampleLevel(smp, texcoord, 0.0).rgb;
	const float3 f = tex.SampleLevel(smp, texcoord, 0.0, int2(1, 0)).rgb;
	const float3 g = tex.SampleLevel(smp, texcoord, 0.0, int2(-1, -1)).rgb;
	const float3 h = tex.SampleLevel(smp, texcoord, 0.0, int2(0, -1)).rgb;
	const float3 i = tex.SampleLevel(smp, texcoord, 0.0, int2(1, -1)).rgb;

	// Apply weighted distribution.
	float3 color = e * 0.25;
	color += (b + d + f + h) * 0.125;
	color += (a + c + g + i) * 0.0625;

	return float4(color, 1.0);
}