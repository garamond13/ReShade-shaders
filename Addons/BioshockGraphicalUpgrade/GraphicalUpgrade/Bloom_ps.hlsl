// Bloom
//
// Based on:
// https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare/
// https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom

cbuffer _Globals : register(b13)
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

SamplerState smp : register(s1); // Should be linear clamp.
Texture2D tex : register(t0);


#define BLOOM_THRESHOLD PWLThreshold
#define BLOOM_SOFT_KNEE BLOOM_THRESHOLD
#define BLOOM_TINT float3(1.0, 1.35, 1.0)


// Prefilter + downsample PS.
//

float get_luma(float3 color)
{
	return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

float get_karis_weight(float3 color)
{
	return rcp(1.0 + get_luma(color));
}

float3 karis_average(float3 a, float3 b, float3 c, float3 d)
{
	float4 sum = float4(a.rgb, 1.0) * get_karis_weight(a);
	sum += float4(b.rgb, 1.0) * get_karis_weight(b);
	sum += float4(c.rgb, 1.0) * get_karis_weight(c);
	sum += float4(d.rgb, 1.0) * get_karis_weight(d);
	return sum.rgb / sum.a;
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

float4 bloom_prefilter_ps(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	// a - b - c
	// - d - e -
	// f - g - h
	// - i - j -
	// k - l - m
	const float3 a = tex.SampleLevel(smp, texcoord, 0.0, int2(-2, -2)).rgb;
	const float3 b = tex.SampleLevel(smp, texcoord, 0.0, int2(0, -2)).rgb;
	const float3 c = tex.SampleLevel(smp, texcoord, 0.0, int2(2, -2)).rgb;
	const float3 d = tex.SampleLevel(smp, texcoord, 0.0, int2(-1, -1)).rgb;
	const float3 e = tex.SampleLevel(smp, texcoord, 0.0, int2(1, -1)).rgb;
	const float3 f = tex.SampleLevel(smp, texcoord, 0.0, int2(-2, 0)).rgb;
	const float3 g = tex.SampleLevel(smp, texcoord, 0.0).rgb;
	const float3 h = tex.SampleLevel(smp, texcoord, 0.0, int2(2, 0)).rgb;
	const float3 i = tex.SampleLevel(smp, texcoord, 0.0, int2(-1, 1)).rgb;
	const float3 j = tex.SampleLevel(smp, texcoord, 0.0, int2(1, 1)).rgb;
	const float3 k = tex.SampleLevel(smp, texcoord, 0.0, int2(-2, 2)).rgb;
	const float3 l = tex.SampleLevel(smp, texcoord, 0.0, int2(0, 2)).rgb;
	const float3 m = tex.SampleLevel(smp, texcoord, 0.0, int2(2, 2)).rgb;

	// Apply partial Karis average in blocks of 4 samples.
	float3 groups[5];
	groups[0] = karis_average(d, e, i, j);
	groups[1] = karis_average(a, b, g, f);
	groups[2] = karis_average(b, c, h, g);
	groups[3] = karis_average(f, g, l, k);
	groups[4] = karis_average(g, h, m, l);

	// Apply weighted distribution.
	float3 color = groups[0] * 0.125 + groups[1] * 0.03125 + groups[2] * 0.03125 + groups[3] * 0.03125 + groups[4] * 0.03125;

	// Apply threshold.
	color = quadratic_threshold(color);

	// Apply tint.
	const float luma = get_luma(color);
	color *= BLOOM_TINT;
	color *= luma * rcp(max(1e-6, get_luma(color)));

	return float4(color, 1.0);
}

//

// Downsample PS.
float4 bloom_downsample_ps(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	// a - b - c
	// - d - e -
	// f - g - h
	// - i - j -
	// k - l - m
	const float3 a = tex.SampleLevel(smp, texcoord, 0.0, int2(-2, -2)).rgb;
	const float3 b = tex.SampleLevel(smp, texcoord, 0.0, int2(0, -2)).rgb;
	const float3 c = tex.SampleLevel(smp, texcoord, 0.0, int2(2, -2)).rgb;
	const float3 d = tex.SampleLevel(smp, texcoord, 0.0, int2(-1, -1)).rgb;
	const float3 e = tex.SampleLevel(smp, texcoord, 0.0, int2(1, -1)).rgb;
	const float3 f = tex.SampleLevel(smp, texcoord, 0.0, int2(-2, 0)).rgb;
	const float3 g = tex.SampleLevel(smp, texcoord, 0.0).rgb;
	const float3 h = tex.SampleLevel(smp, texcoord, 0.0, int2(2, 0)).rgb;
	const float3 i = tex.SampleLevel(smp, texcoord, 0.0, int2(-1, 1)).rgb;
	const float3 j = tex.SampleLevel(smp, texcoord, 0.0, int2(1, 1)).rgb;
	const float3 k = tex.SampleLevel(smp, texcoord, 0.0, int2(-2, 2)).rgb;
	const float3 l = tex.SampleLevel(smp, texcoord, 0.0, int2(0, 2)).rgb;
	const float3 m = tex.SampleLevel(smp, texcoord, 0.0, int2(2, 2)).rgb;

	// Apply weighted distribution.
	float3 color = g * 0.125;
	color += (a + c + k + m) * 0.03125;
	color += (b + f + h + l) * 0.0625;
	color += (d + e + i + j) * 0.125;

	return float4(color, 1.0);
}

// Upsample PS.
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