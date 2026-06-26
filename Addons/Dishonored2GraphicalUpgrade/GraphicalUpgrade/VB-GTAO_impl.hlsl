// VB-GTAO addopted for Dishonored 2.
//
// Visibility Bitmask - Ground Truth Ambient Occlusion, (https://cdrinmatane.github.io/posts/ssaovb-code/) based on XeGTAO (https://github.com/GameTechDev/XeGTAO).

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

// Must be defined!
//

#ifndef VIEWPORT_PIXEL_SIZE
#define VIEWPORT_PIXEL_SIZE float2(0.0, 0.0)
#endif

//

// User configurable
//

// 0 - Low, 1 - Medium, 2 - High, 3 - Very High, 4 - Ultra
#ifndef VB_GTAO_QUALITY
#define VB_GTAO_QUALITY 2
#endif

#ifndef RADIUS
#define RADIUS 1.1
#endif

#ifndef THICKNESS
#define THICKNESS 0.2
#endif

#ifndef SAMPLE_DISTRIBUTION_POWER
#define SAMPLE_DISTRIBUTION_POWER 2.0
#endif

#ifndef FINAL_VALUE_POWER
#define FINAL_VALUE_POWER 4.5
#endif

#ifndef DEPTH_MIP_SAMPLING_OFFSET
#define DEPTH_MIP_SAMPLING_OFFSET 4.5
#endif

#ifndef STEPS_PER_SLICE
#define STEPS_PER_SLICE 3.0
#endif

//

// If TAA is used.
#if VB_GTAO_QUALITY == 0 // Low
	#define SLICE_COUNT 2.0
#elif VB_GTAO_QUALITY == 1 // Medium
	#define SLICE_COUNT 4.0
#elif VB_GTAO_QUALITY == 2 // High
	#define SLICE_COUNT 6.0
#elif VB_GTAO_QUALITY == 3 // Very High
	#define SLICE_COUNT 7.0
#elif VB_GTAO_QUALITY == 4 // Ultra
	#define SLICE_COUNT 9.0
#endif

// If TAA is not used.
//#if VB_GTAO_QUALITY == 0 // Low
//	#define SLICE_COUNT 4.0
//#elif VB_GTAO_QUALITY == 1 // Medium
//	#define SLICE_COUNT 7.0
//#elif VB_GTAO_QUALITY == 2 // High
//	#define SLICE_COUNT 10.0
//#elif VB_GTAO_QUALITY == 3 // Very High
//	#define SLICE_COUNT 13.0
//#elif VB_GTAO_QUALITY == 4 // Ultra
//	#define SLICE_COUNT 16.0
//#endif

#define TAN_HALF_FOV_X rcp(cb_projectionmatrix._m00)
#define TAN_HALF_FOV_Y rcp(cb_projectionmatrix._m11)
#define NDC_TO_VIEW_MUL float2(TAN_HALF_FOV_X * 2.0, TAN_HALF_FOV_Y * -2.0)
#define NDC_TO_VIEW_ADD float2(-TAN_HALF_FOV_X, TAN_HALF_FOV_Y)
#define NDC_TO_VIEW_MUL_X_PIXEL_SIZE (NDC_TO_VIEW_MUL * VIEWPORT_PIXEL_SIZE)

#define DEPTH_MIP_LEVELS 5.0
#define SECTOR_COUNT 32

#define PI 3.1415926535897932384626433832795
#define PI_HALF 1.5707963267948966192313216916398

float ScreenSpaceToViewSpaceDepth(float screenDepth)
{
	return -cb_inverseprojectionmatrix._m23 / (cb_inverseprojectionmatrix._m32 * screenDepth + cb_inverseprojectionmatrix._m33);
}

// This is also a good place to do non-linear depth conversion for cases where one wants the 'radius' (effectively the threshold between near-field and far-field GI),
// is required to be non-linear (i.e. very large outdoors environments).
float ClampDepth(float depth)
{
	return clamp(depth, 0.0, 3.402823466e+38);
}

float DepthMIPFilter(float depth0, float depth1, float depth2, float depth3)
{
	float maxDepth = max(max(depth0, depth1), max(depth2, depth3));
	return maxDepth;
}

groupshared float g_scratchDepths[8][8];
void PrefilterDepths16x16(uint2 dispatchThreadID, uint2 groupThreadID, Texture2D sourceNDCDepth, SamplerState depthSampler, RWTexture2D<float> outDepth0, RWTexture2D<float> outDepth1, RWTexture2D<float> outDepth2, RWTexture2D<float> outDepth3, RWTexture2D<float> outDepth4)
{
	// MIP 0
	const uint2 baseCoord = dispatchThreadID;
	const uint2 pixCoord = baseCoord * 2;
	float4 depths4 = sourceNDCDepth.GatherRed(depthSampler, float2(pixCoord * VIEWPORT_PIXEL_SIZE), int2(1, 1));
	float depth0 = ClampDepth(ScreenSpaceToViewSpaceDepth(depths4.w));
	float depth1 = ClampDepth(ScreenSpaceToViewSpaceDepth(depths4.z));
	float depth2 = ClampDepth(ScreenSpaceToViewSpaceDepth(depths4.x));
	float depth3 = ClampDepth(ScreenSpaceToViewSpaceDepth(depths4.y));
	outDepth0[pixCoord + uint2(0, 0)] = depth0;
	outDepth0[pixCoord + uint2(1, 0)] = depth1;
	outDepth0[pixCoord + uint2(0, 1)] = depth2;
	outDepth0[pixCoord + uint2(1, 1)] = depth3;

	// MIP 1
	float dm1 = DepthMIPFilter(depth0, depth1, depth2, depth3);
	outDepth1[baseCoord] = dm1;
	g_scratchDepths[groupThreadID.x][groupThreadID.y] = dm1;

	GroupMemoryBarrierWithGroupSync();

	// MIP 2
	[branch]
	if (all((groupThreadID.xy % 2) == 0)) {
		float inTL = g_scratchDepths[groupThreadID.x + 0][groupThreadID.y + 0];
		float inTR = g_scratchDepths[groupThreadID.x + 1][groupThreadID.y + 0];
		float inBL = g_scratchDepths[groupThreadID.x + 0][groupThreadID.y + 1];
		float inBR = g_scratchDepths[groupThreadID.x + 1][groupThreadID.y + 1];

		float dm2 = DepthMIPFilter(inTL, inTR, inBL, inBR);
		outDepth2[baseCoord / 2] = dm2;
		g_scratchDepths[groupThreadID.x][groupThreadID.y] = dm2;
	}

	GroupMemoryBarrierWithGroupSync();

	// MIP 3
	[branch]
	if (all(( groupThreadID.xy % 4) == 0)) {
		float inTL = g_scratchDepths[groupThreadID.x + 0][groupThreadID.y + 0];
		float inTR = g_scratchDepths[groupThreadID.x + 2][groupThreadID.y + 0];
		float inBL = g_scratchDepths[groupThreadID.x + 0][groupThreadID.y + 2];
		float inBR = g_scratchDepths[groupThreadID.x + 2][groupThreadID.y + 2];

		float dm3 = DepthMIPFilter(inTL, inTR, inBL, inBR);
		outDepth3[baseCoord / 4] = dm3;
		g_scratchDepths[groupThreadID.x][groupThreadID.y] = dm3;
	}

	GroupMemoryBarrierWithGroupSync();

	// MIP 4
	[branch]
	if (all((groupThreadID.xy % 8) == 0)) {
		float inTL = g_scratchDepths[groupThreadID.x + 0][groupThreadID.y + 0];
		float inTR = g_scratchDepths[groupThreadID.x + 4][groupThreadID.y + 0];
		float inBL = g_scratchDepths[groupThreadID.x + 0][groupThreadID.y + 4];
		float inBR = g_scratchDepths[groupThreadID.x + 4][groupThreadID.y + 4];

		float dm4 = DepthMIPFilter(inTL, inTR, inBL, inBR);
		outDepth4[baseCoord / 8] = dm4;
		//g_scratchDepths[ groupThreadID.x ][ groupThreadID.y ] = dm4;
	}
}

float4 CalculateEdges(float centerZ, float leftZ, float rightZ, float topZ, float bottomZ)
{
	float4 edgesLRTB = float4(leftZ, rightZ, topZ, bottomZ) - centerZ;

	float slopeLR = (edgesLRTB.y - edgesLRTB.x) * 0.5;
	float slopeTB = (edgesLRTB.w - edgesLRTB.z) * 0.5;
	float4 edgesLRTBSlopeAdjusted = edgesLRTB + float4(slopeLR, -slopeLR, slopeTB, -slopeTB);
	edgesLRTB = min(abs(edgesLRTB), abs(edgesLRTBSlopeAdjusted));
	return saturate(1.25 - edgesLRTB * rcp(centerZ * 0.011));
}

// Inputs are screen XY and viewspace depth, output is viewspace position
float3 ComputeViewspacePosition(float2 screenPos, float viewspaceDepth)
{
	float3 ret;
	ret.xy = (NDC_TO_VIEW_MUL * screenPos.xy + NDC_TO_VIEW_ADD) * viewspaceDepth;
	ret.z = viewspaceDepth;
	return ret;
}

float3 ComputeViewspaceNormal(float2 uv, float z, float lZ, float rZ, float tZ, float bZ, float4 edgesLRTB)
{
	const float3 centerPos = ComputeViewspacePosition(uv, z);
	const float3 deltaL = ComputeViewspacePosition(uv + float2(-1.0, 0.0) * VIEWPORT_PIXEL_SIZE, lZ) - centerPos;
	const float3 deltaR = ComputeViewspacePosition(uv + float2(1.0, 0.0) * VIEWPORT_PIXEL_SIZE, rZ) - centerPos;
	const float3 deltaT = ComputeViewspacePosition(uv + float2(0.0, -1.0) * VIEWPORT_PIXEL_SIZE, tZ) - centerPos;
	const float3 deltaB = ComputeViewspacePosition(uv + float2(0.0, 1.0) * VIEWPORT_PIXEL_SIZE, bZ) - centerPos;

	const float4 w = max(float4(edgesLRTB.x * edgesLRTB.z, edgesLRTB.z * edgesLRTB.y, edgesLRTB.y * edgesLRTB.w, edgesLRTB.w * edgesLRTB.x), 1e-6);

	return normalize(w.x * cross(deltaL, deltaT) + w.y * cross(deltaT, deltaR) + w.z * cross(deltaR, deltaB) + w.w * cross(deltaB, deltaL));
}

// http://h14s.p5r.org/2012/09/0x5f3759df.html, [Drobot2014a] Low Level Optimizations for GCN, https://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pdf slide 63
float FastSqrt(float x)
{
	return asfloat(0x1fbd1df5 + (asint(x) >> 1));
}

// input [-1, 1] and output [0, PI], from https://seblagarde.wordpress.com/2014/12/01/inverse-trigonometric-functions-gpu-optimization-for-amd-gcn-architecture/
float FastACos(float inX)
{
	const float pi = 3.141593;
	const float half_pi = 1.570796;
	float x = abs(inX);
	float res = -0.156583 * x + half_pi;
	res *= FastSqrt(1.0 - x);
	return inX >= 0.0 ? res : pi - res;
}

uint UpdateSectors(float minHorizon, float maxHorizon, uint globalOccludedBitfield)
{
	uint startHorizonInt = minHorizon * SECTOR_COUNT;
	uint angleHorizonInt = floor((maxHorizon - minHorizon) * SECTOR_COUNT);
	uint angleHorizonBitfield = angleHorizonInt > 0 ? (0xFFFFFFFFu >> (SECTOR_COUNT - angleHorizonInt)) : 0;
	uint currentOccludedBitfield = angleHorizonBitfield << startHorizonInt;
	return globalOccludedBitfield | currentOccludedBitfield;
}

void ProcessSample(float3 deltaPos, float3 V, float N, float samplingDirection, inout uint globalOccludedBitfield)
{
	float2 frontBackHorizon;
	float3 deltaPosBackface = deltaPos - V * THICKNESS;

	// Project sample onto the unit circle and compute the angle relative to V
	frontBackHorizon = float2(dot(normalize(deltaPos), V), dot(normalize(deltaPosBackface), V));
	frontBackHorizon = float2(FastACos(frontBackHorizon.x), FastACos(frontBackHorizon.y));

	// Shift sample from V to normal, clamp in [0..1]
	frontBackHorizon = saturate((samplingDirection * -frontBackHorizon - N + PI_HALF) / PI);

	// Sampling direction inverts min/max angles
	frontBackHorizon = samplingDirection >= 0 ? frontBackHorizon.yx : frontBackHorizon.xy;

	globalOccludedBitfield = UpdateSectors(frontBackHorizon.x, frontBackHorizon.y, globalOccludedBitfield);
}

void MainPass(uint2 pixCoord, float2 localNoise, Texture2D sourceViewspaceDepth, SamplerState depthSampler, out float4 outWorkingAOTermAndPackedDepth)
{
	float2 normalizedScreenPos = (pixCoord + 0.5) * VIEWPORT_PIXEL_SIZE;

	float4 valuesUL = sourceViewspaceDepth.GatherRed(depthSampler, float2(pixCoord * VIEWPORT_PIXEL_SIZE));
	float4 valuesBR = sourceViewspaceDepth.GatherRed(depthSampler, float2(pixCoord * VIEWPORT_PIXEL_SIZE), int2(1, 1));

	// viewspace Z at the center
	float viewspaceZ = valuesUL.y; //sourceViewspaceDepth.SampleLevel( depthSampler, normalizedScreenPos, 0 ).x;

	// The original SSAO shader depth packing.
	float2 r0;
	float r1;
	r1.x = viewspaceZ;
	r0.x = 1.00390625 * r1.x;
	r0.x = trunc(r0.x);
	r0.xy = float2(256,0.00392156886) * r0.xx;
	r0.x = r1.x * 257 + -r0.x;
	outWorkingAOTermAndPackedDepth.z = r0.y;
	outWorkingAOTermAndPackedDepth.w = 0.00392156886 * r0.x;

	// viewspace Zs left top right bottom
	const float pixLZ = valuesUL.x;
	const float pixTZ = valuesUL.z;
	const float pixRZ = valuesBR.z;
	const float pixBZ = valuesBR.x;

	float4 edgesLRTB = CalculateEdges(viewspaceZ, pixLZ, pixRZ, pixTZ, pixBZ);

	float3 viewspaceNormal = ComputeViewspaceNormal(normalizedScreenPos, viewspaceZ, pixLZ, pixRZ, pixTZ, pixBZ, edgesLRTB);

	const float3 pixCenterPos = ComputeViewspacePosition(normalizedScreenPos, viewspaceZ);
	const float3 viewVec = normalize(-pixCenterPos);

	float visibility = 0.0;

	const float noiseSlice = localNoise.x;
	const float noiseSample = localNoise.y;

	// quality settings / tweaks / hacks
	const float pixelTooCloseThreshold = 1.3; // if the offset is under approx pixel size (pixelTooCloseThreshold), push it out to the minimum distance

	// approx viewspace pixel size at pixCoord; approximation of NDCToViewspace( normalizedScreenPos.xy + consts.ViewportPixelSize.xy, pixCenterPos.z ).xy - pixCenterPos.xy;
	const float2 pixelDirRBViewspaceSizeAtCenterZ = viewspaceZ.xx * NDC_TO_VIEW_MUL_X_PIXEL_SIZE;

	float screenspaceRadius = RADIUS * rcp(pixelDirRBViewspaceSizeAtCenterZ.x);

	// this is the min distance to start sampling from to avoid sampling from the center pixel (no useful data obtained from sampling center pixel)
	const float minS = pixelTooCloseThreshold * rcp(screenspaceRadius);

	[unroll]
	for (float slice = 0.0; slice < SLICE_COUNT; slice++) {
		float sliceK = (slice + noiseSlice) / SLICE_COUNT;
		// lines 5, 6 from the paper
		float phi = sliceK * PI;
		float cosPhi = cos(phi);
		float sinPhi = sin(phi);
		float2 omega = float2(cosPhi, -sinPhi);

		// convert to screen units (pixels) for later use
		omega *= screenspaceRadius;

		// line 8 from the paper
		const float3 directionVec = float3(cosPhi, sinPhi, 0.0);

		// line 9 from the paper
		const float3 orthoDirectionVec = directionVec - (dot(directionVec, viewVec) * viewVec);

		// line 10 from the paper
		//axisVec is orthogonal to directionVec and viewVec, used to define projectedNormal
		const float3 axisVec = normalize(cross(orthoDirectionVec, viewVec));

		// line 11 from the paper
		float3 projectedNormalVec = viewspaceNormal - axisVec * dot(viewspaceNormal, axisVec);

		// line 13 from the paper
		float signNorm = sign(dot(orthoDirectionVec, projectedNormalVec));

		// line 14 from the paper
		float projectedNormalVecLength = length(projectedNormalVec);
		float cosNorm = saturate(dot(projectedNormalVec, viewVec) * rcp(projectedNormalVecLength));

		// line 15 from the paper
		float n = signNorm * FastACos(cosNorm);

		uint globalOccludedBitfield = 0;

		[unroll]
		for (float step = 0.0; step < STEPS_PER_SLICE; step++) {
			// R1 sequence (http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/)
			const float stepBaseNoise = (slice + step * STEPS_PER_SLICE) * 0.6180339887498948482; // <- this should unroll
			float stepNoise = frac(noiseSample + stepBaseNoise);

			// approx line 20 from the paper, with added noise
			float s = (step + stepNoise) / STEPS_PER_SLICE; // + (lpfloat2)1e-6f);

			// additional distribution modifier
			s = pow(s, SAMPLE_DISTRIBUTION_POWER);

			// avoid sampling center pixel
			s += minS;

			// approx lines 21-22 from the paper, unrolled
			float2 sampleOffset = s * omega;

			float sampleOffsetLength = length(sampleOffset);

			// note: when sampling, using point_point_point or point_point_linear sampler works, but linear_linear_linear will cause unwanted interpolation between neighbouring depth values on the same MIP level!
			const float mipLevel = clamp(log2(sampleOffsetLength) - DEPTH_MIP_SAMPLING_OFFSET, 0.0, DEPTH_MIP_LEVELS);

			// Snap to pixel center (more correct direction math, avoids artifacts due to sampling pos not matching depth texel center - messes up slope - but adds other
			// artifacts due to them being pushed off the slice). Also use full precision for high res cases.
			sampleOffset = round(sampleOffset) * VIEWPORT_PIXEL_SIZE;

			float2 sampleScreenPos0 = normalizedScreenPos + sampleOffset;
			float SZ0 = sourceViewspaceDepth.SampleLevel(depthSampler, sampleScreenPos0, mipLevel).x;
			float3 samplePos0 = ComputeViewspacePosition(sampleScreenPos0, SZ0);

			float2 sampleScreenPos1 = normalizedScreenPos - sampleOffset;
			float SZ1 = sourceViewspaceDepth.SampleLevel(depthSampler, sampleScreenPos1, mipLevel).x;
			float3 samplePos1 = ComputeViewspacePosition(sampleScreenPos1, SZ1);

			float3 sampleDelta0 = samplePos0 - pixCenterPos;
			float3 sampleDelta1 = samplePos1 - pixCenterPos;

			ProcessSample(sampleDelta0, viewVec, n, -1.0, globalOccludedBitfield);
 			ProcessSample(sampleDelta1, viewVec, n, 1.0, globalOccludedBitfield);
		}
		visibility += 1.0 - float(countbits(globalOccludedBitfield)) / float(SECTOR_COUNT);
	}
	visibility /= SLICE_COUNT;
	visibility = pow(visibility, FINAL_VALUE_POWER);
	visibility = clamp(visibility, 0.03, 1.0); // disallow total occlusion (which wouldn't make any sense anyhow since pixel is visible but also helps with packing bent normals)

	outWorkingAOTermAndPackedDepth.x = visibility;

	// The original SSAO shader is setting `y` always to 1.
	outWorkingAOTermAndPackedDepth.y = 1.0;
}

// Implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SamplerState smp : register(s0);
Texture2D tex : register(t0);
RWTexture2D<float> out_working_depth_mip0 : register(u0);
RWTexture2D<float> out_working_depth_mip1 : register(u1);
RWTexture2D<float> out_working_depth_mip2 : register(u2);
RWTexture2D<float> out_working_depth_mip3 : register(u3);
RWTexture2D<float> out_working_depth_mip4 : register(u4);

// From https://www.shadertoy.com/view/3tB3z3 - except we're using R2 here
#define XE_HILBERT_LEVEL 6U
#define XE_HILBERT_WIDTH (1U << XE_HILBERT_LEVEL)
#define XE_HILBERT_AREA (XE_HILBERT_WIDTH * XE_HILBERT_WIDTH)
uint HilbertIndex(uint posX, uint posY)
{
	uint index = 0U;

	[unroll]
	for (uint curLevel = XE_HILBERT_WIDTH / 2U; curLevel > 0U; curLevel /= 2U) {
		uint regionX = (posX & curLevel) > 0U;
		uint regionY = (posY & curLevel) > 0U;
		index += curLevel * curLevel * ((3U * regionX) ^ regionY);

		if (regionY == 0U) {
			if (regionX == 1U) {
				posX = XE_HILBERT_WIDTH - 1U - posX;
				posY = XE_HILBERT_WIDTH - 1U - posY;
			}

			uint temp = posX;
			posX = posY;
			posY = temp;
		}
	}

	return index;
}

// without TAA, temporalIndex is always 0
float2 SpatioTemporalNoise(uint2 pixCoord, uint temporalIndex)
{
	float2 noise;

	// Hilbert curve driving R2 (see https://www.shadertoy.com/view/3tB3z3)
	#ifdef HILBERT_LUT_AVAILABLE // load from lookup texture...
	uint index = g_srcHilbertLUT.Load(uint3(pixCoord % 64, 0)).x;
	#else // ...or generate in-place?
	uint index = HilbertIndex(pixCoord.x, pixCoord.y);
	#endif

	index += 288 * (temporalIndex % 64); // why 288? tried out a few and that's the best so far (with XE_HILBERT_LEVEL 6U) - but there's probably better :)

	// R2 sequence - see http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
	return float2(frac(0.5 + index * float2(0.75487766624669276005, 0.5698402909980532659114)));
}

[numthreads(8, 8, 1)] // <- hard coded to 8x8; each thread computes 2x2 blocks so processing 16x16 block: Dispatch needs to be called with (width + 16-1) / 16, (height + 16-1) / 16
void prefilter_depths16x16_cs(uint2 dtid : SV_DispatchThreadID, uint2 gtid : SV_GroupThreadID)
{
	// tex0 = depth
	// smp = g_samplerPointClamp
	PrefilterDepths16x16(dtid, gtid, tex, smp, out_working_depth_mip0, out_working_depth_mip1, out_working_depth_mip2, out_working_depth_mip3, out_working_depth_mip4);
}

void main_pass_ps(float4 pos : SV_Position, out float4 outWorkingAOTermAndPackedDepth : SV_Target)
{
	// tex0 = g_srcWorkingDepth
	// smp = g_samplerPointClamp
	MainPass(pos.xy, SpatioTemporalNoise(pos.xy, cb_framenumber), tex, smp, outWorkingAOTermAndPackedDepth);
}