// Without depth this is impossible to balance between ghosting, flickering (still scene) and instability in motion.

#include "Include/Common.hlsli"

struct postfx_luminance_autoexposure_t
{
	float EngineLuminanceFactor;   // Offset:    0
	float LuminanceFactor;         // Offset:    4
	float MinLuminanceLDR;         // Offset:    8
	float MaxLuminanceLDR;         // Offset:   12
	float MiddleGreyLuminanceLDR;  // Offset:   16
	float EV;                      // Offset:   20
	float Fstop;                   // Offset:   24
	uint PeakHistogramValue;       // Offset:   28
};

cbuffer PerInstanceCB : register(b2)
{
	float4 cb_positiontoviewtexture : packoffset(c0);
	float4 cb_taatexsize : packoffset(c1);
	float4 cb_taaditherandviewportsize : packoffset(c2);
	float4 cb_postfx_tonemapping_tonemappingparms : packoffset(c3);
	float4 cb_postfx_tonemapping_tonemappingcoeffsinverse1 : packoffset(c4);
	float4 cb_postfx_tonemapping_tonemappingcoeffsinverse0 : packoffset(c5);
	float4 cb_postfx_tonemapping_tonemappingcoeffs1 : packoffset(c6);
	float4 cb_postfx_tonemapping_tonemappingcoeffs0 : packoffset(c7);
	uint2 cb_postfx_luminance_exposureindex : packoffset(c8);
	float2 cb_prevresolutionscale : packoffset(c8.z);
	float cb_env_tonemapping_white_level : packoffset(c9);
	float cb_view_white_level : packoffset(c9.y);
	float cb_taaamount : packoffset(c9.z);
	float cb_postfx_luminance_customevbias : packoffset(c9.w);
}

cbuffer PerViewCB : register(b1)
{
	float4 cb_alwaystweak : packoffset(c0);
	float4 cb_viewrandom : packoffset(c1);
	float4x4 cb_viewprojectionmatrix : packoffset(c2);
	float4x4 cb_viewmatrix : packoffset(c6);
	float4 cb_subpixeloffset : packoffset(c10);
	float4x4 cb_projectionmatrix : packoffset(c11);
	float4x4 cb_previousviewprojectionmatrix : packoffset(c15);
	float4x4 cb_previousviewmatrix : packoffset(c19);
	float4x4 cb_previousprojectionmatrix : packoffset(c23);
	float4 cb_mousecursorposition : packoffset(c27);
	float4 cb_mousebuttonsdown : packoffset(c28);
	float4 cb_jittervectors : packoffset(c29);
	float4x4 cb_inverseviewprojectionmatrix : packoffset(c30);
	float4x4 cb_inverseviewmatrix : packoffset(c34);
	float4x4 cb_inverseprojectionmatrix : packoffset(c38);
	float4 cb_globalviewinfos : packoffset(c42);
	float3 cb_wscamforwarddir : packoffset(c43);
	uint cb_alwaysone : packoffset(c43.w);
	float3 cb_wscamupdir : packoffset(c44);
	uint cb_usecompressedhdrbuffers : packoffset(c44.w);
	float3 cb_wscampos : packoffset(c45);
	float cb_time : packoffset(c45.w);
	float3 cb_wscamleftdir : packoffset(c46);
	float cb_systime : packoffset(c46.w);
	float2 cb_jitterrelativetopreviousframe : packoffset(c47);
	float2 cb_worldtime : packoffset(c47.z);
	float2 cb_shadowmapatlasslicedimensions : packoffset(c48);
	float2 cb_resolutionscale : packoffset(c48.z);
	float2 cb_parallelshadowmapslicedimensions : packoffset(c49);
	float cb_framenumber : packoffset(c49.z);
	uint cb_alwayszero : packoffset(c49.w);
}

SamplerState smp_linearclamp_s : register(s0);
Texture2D ro_taahistory_read : register(t0);
Texture2D ro_motionvectors : register(t1);
Texture2D ro_viewcolormap : register(t2);
StructuredBuffer<postfx_luminance_autoexposure_t> ro_postfx_luminance_buffautoexposure : register(t3);
RWTexture2D<float4> rw_taahistory_write : register(u0);
RWTexture2D<float4> rw_taaresult : register(u1);

// MUST BE DEFINED!
//

#ifndef VIEWPORT_SIZE
#define VIEWPORT_SIZE cb_taatexsize.xy
#endif

#ifndef INV_VIEWPORT_SIZE
#define INV_VIEWPORT_SIZE cb_taatexsize.zw
#endif

//

// User configurable
//

#ifndef MIN_GAMMA
#define MIN_GAMMA 1.0
#endif

#ifndef MAX_GAMMA
#define MAX_GAMMA 3.0
#endif

//

// Replicate what the game's native TAA is doing
// pre and post tonemap.
//

float3 pre_tonemap(float3 color, const float exposure, const float factor)
{
	color = cb_usecompressedhdrbuffers ? color * factor : color;
	color = max(0.0, color);
	color = min(cb_env_tonemapping_white_level, color);
	return color * exposure;
}

float3 post_tonemap(float3 color, const float exposure, const float factor)
{
	color *= rcp(exposure);
	color = cb_usecompressedhdrbuffers ? color * rcp(factor) : color;
	return color;
}

//

float3 tonemap(float3 color)
{
	return color * rcp(max(1e-6, 1.0 + get_luma(color)));
}

float3 inv_tonemap(float3 color)
{
	return color * rcp(max(1e-6, 1.0 - get_luma(color)));
}

float3 rgb_to_ycocg(float3 color)
{
	const float y = dot(color, float3(0.25, 0.5, 0.25));
	const float co = dot(color, float3(0.5, 0.0, -0.5));
	const float cg = dot(color, float3(-0.25, 0.5, -0.25));
	return float3(y, co, cg);
}

float3 ycocg_to_rgb(float3 color)
{
	const float r = dot(color, float3(1.0, 1.0, -1.0));
	const float g = dot(color, float3(1.0, 0.0, 1.0));
	const float b = dot(color, float3(1.0, -1.0, -1.0));
	return float3(r, g, b);
}

float4 sample_catmull_rom_aprox(Texture2D tex, SamplerState smp, float2 uv)
{
	const float2 f = frac(uv * VIEWPORT_SIZE - 0.5);
	const float2 tc = uv - f * INV_VIEWPORT_SIZE;
	const float2 f2 = f * f;
	const float2 f3 = f2 * f;

	// Catmull-Rom weights.
	const float2 w0 = f2 - 0.5 * (f3 + f);
	const float2 w1 = 1.5 * f3 - 2.5 * f2 + 1.0;
	const float2 w3 = 0.5 * (f3 - f2);
	const float2 w2 = 1.0 - w0 - w1 - w3;
	const float2 w12 = w1 + w2;

	// Texel coords.
	const float2 tc0 = tc - 1.0 * INV_VIEWPORT_SIZE;
	const float2 tc3 = tc + 2.0 * INV_VIEWPORT_SIZE;
	const float2 tc12 = tc + w2 / w12 * INV_VIEWPORT_SIZE;

	// Combined weights.
	const float w12w0 = w12.x * w0.y;
	const float w0w12 = w0.x * w12.y;
	const float w12w12 = w12.x * w12.y;
	const float w3w12 = w3.x * w12.y;
	const float w12w3 = w12.x * w3.y;

	float4 c = tex.SampleLevel(smp, float2(tc12.x, tc0.y), 0.0) * w12w0;
	c += tex.SampleLevel(smp, float2(tc0.x, tc12.y), 0.0) * w0w12;
	c += tex.SampleLevel(smp, tc12, 0.0) * w12w12;
	c += tex.SampleLevel(smp, float2(tc3.x, tc12.y), 0.0) * w3w12;
	c += tex.SampleLevel(smp, float2(tc12.x, tc3.y), 0.0) * w12w3;

	// Normalize.
	c *= rcp(w12w0 + w0w12 + w12w12 + w3w12 + w12w3);

	return c;
}

float3 clip_to_aabb(float3 color, float3 minc, float3 maxc)
{
	const float3 center = (minc + maxc) * 0.5;
	const float3 extent = (maxc - minc) * 0.5;
	const float3 offset = color - center;
	const float3 units = abs(offset * rcp(max(1e-6, extent)));
	const float max_unit = max(max(units.x, units.y), max(units.z, 1.0));
	return center + offset * rcp(max_unit);
}

[numthreads(16, 16, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	// Neighborhood offsets.
	const int nsamples = 9;
	const int2 offsets[nsamples - 1] = {
		int2(-1, -1),
		int2(0, -1),
		int2(1, -1),
		int2(-1, 0),
		int2(1, 0),
		int2(-1, 1),
		int2(0, 1),
		int2(1, 1)
	};

	const float2 uv = (dtid.xy + 0.5) * INV_VIEWPORT_SIZE;

	// From the game's native TAA.
	const float r0z = ro_postfx_luminance_buffautoexposure[cb_postfx_luminance_exposureindex.y].EngineLuminanceFactor;
	float r0w = exp2(-cb_postfx_luminance_customevbias);
	r0w = r0z * r0w; // Exposure?
	const float r1w = cb_view_white_level * r0z; // Compression factor?

	// Find the longest motion vector.
	float2 longest_mv = ro_motionvectors.Load(int3(dtid.xy, 0)).xy;

	[unroll]
	for (int i = 0; i < nsamples - 1; ++i) {
		const float2 mv = ro_motionvectors.Load(int3(dtid.xy + offsets[i], 0)).xy;
		longest_mv = dot(mv, mv) > dot(longest_mv, longest_mv) ? mv : longest_mv;
	}

	const float velocity = length(longest_mv * VIEWPORT_SIZE);
	const float2 prev_uv = uv - longest_mv;

	// Sample history.
	float4 history = sample_catmull_rom_aprox(ro_taahistory_read, smp_linearclamp_s, prev_uv);
	history.xyz = tonemap(history.xyz);
	history.xyz = rgb_to_ycocg(history.xyz);

	// Calculate variance box in 3x3 neighborhood.
	//

	float3 color = ro_viewcolormap.Load(int3(dtid.xy, 0)).xyz;
	color = pre_tonemap(color, r0w, r1w);
	color = tonemap(color);
	color = rgb_to_ycocg(color);
	float3 m1 = color;
	float3 m2 = color * color;

	[unroll]
	for (int i = 0; i < nsamples - 1; ++i) {
		color = ro_viewcolormap.Load(int3(dtid.xy + offsets[i], 0)).xyz;
		color = pre_tonemap(color, r0w, r1w);
		color = tonemap(color);
		color = rgb_to_ycocg(color);
		m1 += color;
		m2 += color * color;
	}

	m1 /= float(nsamples);
	m2 /= float(nsamples);
	const float3 sigma = sqrt(max(0.0, m2 - m1 * m1));
	float gamma = lerp(MAX_GAMMA, MIN_GAMMA, saturate(velocity / 3.0));

	[flatten]
	if (history.w < gamma) {
		gamma = lerp(history.w, gamma, 1.0 / 8.0);
	}

	const float3 minc = m1 - gamma * sigma;
	const float3 maxc = m1 + gamma * sigma;

	//

	// Calculate the alpha (the final blend).
	//

	float alpha = 0.1;

	// Antiflicker.
	const float dist_to_clamp = min(abs(minc.x - history.x), abs(maxc.x - history.x));
	alpha = clamp((alpha * dist_to_clamp) * rcp(dist_to_clamp + maxc.x - minc.x), 0.0, 1.0);

	//

	history.xyz = clip_to_aabb(history.xyz, minc, maxc);

	// The final color (in YCoCg).
	color = lerp(history.xyz, color, alpha);

	// Outputs.
	color = ycocg_to_rgb(color);
	color = inv_tonemap(color);
	rw_taahistory_write[dtid.xy] = float4(color, gamma);
	color = post_tonemap(color, r0w, r1w);
	rw_taaresult[dtid.xy] = float4(color, 1.0);

	// DEBUG, output velocity.
	//rw_taaresult[dtid.xy] = float4(velocity.xxx, 1.0);

	// DEBUG, output alpha.
	//rw_taaresult[dtid.xy] = float4(alpha.xxx, 1.0);

	// DEBUG, output gamma.
	//rw_taaresult[dtid.xy] = float4(gamma.xxx / MAX_GAMMA, 1.0);
}