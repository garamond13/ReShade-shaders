// Bloom

cbuffer graphical_upgrade : register(b13)
{
	float2 src_size;
	float2 inv_src_size;
	float2 axis;
	float sigma;
}

cbuffer _Globals : register(b12)
{
	float4 SceneShadowsAndDesaturation : packoffset(c0);
	float4 SceneInverseHighLights : packoffset(c1);
	float4 SceneMidTones : packoffset(c2);
	float4 SceneScaledLuminanceWeights : packoffset(c3);
	float4 GammaColorScaleAndInverse : packoffset(c4);
	float4 GammaOverlayColor : packoffset(c5);
	float4 RenderTargetExtent : packoffset(c6);
	float2 DownsampledDepthScale : packoffset(c7);
	float2 BloomScaleAndThreshold : packoffset(c7.z);
	float4 PackedParameters : packoffset(c8);
	float4 PackedParameters2 : packoffset(c9);
	float4 MinMaxBlurClamp : packoffset(c10);
	float4x4 CameraDelta : packoffset(c11);
	float4 MotionBlurSettings : packoffset(c15);
}

SamplerState smp : register(s1); // Should be linear clamp.
Texture2D tex : register(t0);

float get_luma(float3 color)
{
	return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

float3 threshold(float3 color)
{
	float br = max(max(color.r, color.g), color.b);
	float mask = saturate(0.5 * (br - BloomScaleAndThreshold.y));
	return color * mask;
}

float get_gaussian_weight(float x)
{
	return exp(-x * x * rcp(2.0 * sigma * sigma));
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

	// Apply threshold.
	float3 color = threshold(csum);

	return float4(color * BloomScaleAndThreshold.x, 1.0);
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
	float2 h0 = (w1 / g0) - 0.5 + index;
	float2 h1 = (w3 / g1) + 1.5 + index;

	// fetch the four linear interpolations
	float3 tex00 = tex.SampleLevel(smp, float2(h0.x, h0.y) * inv_src_size, 0.0).rgb;
	float3 tex10 = tex.SampleLevel(smp, float2(h1.x, h0.y) * inv_src_size, 0.0).rgb;
	float3 tex01 = tex.SampleLevel(smp, float2(h0.x, h1.y) * inv_src_size, 0.0).rgb;
	float3 tex11 = tex.SampleLevel(smp, float2(h1.x, h1.y) * inv_src_size, 0.0).rgb;

	// weigh along the y-direction
	tex00 = lerp(tex01, tex00, g0.y);
	tex10 = lerp(tex11, tex10, g0.y);

	// weigh along the x-direction
	return float4(lerp(tex10, tex00, g0.x), 1.0);
}