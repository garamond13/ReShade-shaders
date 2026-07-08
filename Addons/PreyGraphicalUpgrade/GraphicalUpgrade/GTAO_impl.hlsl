// GTAO based on XeGTAO (https://github.com/GameTechDev/XeGTAO) adopted for Prey (2017).

#include "Include/GraphicalUpgradeCB.hlsli.h"

cbuffer CBPerViewGlobal : register(b13)
{
	row_major float4x4 CV_ViewProjZeroMatr : packoffset(c0);
	float4 CV_AnimGenParams : packoffset(c4);
	row_major float4x4 CV_ViewProjMatr : packoffset(c5);
	row_major float4x4 CV_ViewProjNearestMatr : packoffset(c9);
	row_major float4x4 CV_InvViewProj : packoffset(c13);
	row_major float4x4 CV_PrevViewProjMatr : packoffset(c17);
	row_major float4x4 CV_PrevViewProjNearestMatr : packoffset(c21);
	row_major float3x4 CV_ScreenToWorldBasis : packoffset(c25);
	float4 CV_TessInfo : packoffset(c28);
	float4 CV_CameraRightVector : packoffset(c29);
	float4 CV_CameraFrontVector : packoffset(c30);
	float4 CV_CameraUpVector : packoffset(c31);
	float4 CV_ScreenSize : packoffset(c32);
	float4 CV_HPosScale : packoffset(c33);
	float4 CV_HPosClamp : packoffset(c34);
	float4 CV_ProjRatio : packoffset(c35);
	float4 CV_NearestScaled : packoffset(c36);
	float4 CV_NearFarClipDist : packoffset(c37);
	float4 CV_SunLightDir : packoffset(c38);
	float4 CV_SunColor : packoffset(c39);
	float4 CV_SkyColor : packoffset(c40);
	float4 CV_FogColor : packoffset(c41);
	float4 CV_TerrainInfo : packoffset(c42);
	float4 CV_DecalZFightingRemedy : packoffset(c43);
	row_major float4x4 CV_FrustumPlaneEquation : packoffset(c44);
	float4 CV_WindGridOffset : packoffset(c48);
	row_major float4x4 CV_ViewMatr : packoffset(c49);
	row_major float4x4 CV_InvViewMatr : packoffset(c53);
	float CV_LookingGlass_SunSelector : packoffset(c57);
	float CV_LookingGlass_DepthScalar : packoffset(c57.y);
	float CV_PADDING0 : packoffset(c57.z);
	float CV_PADDING1 : packoffset(c57.w);
}

cbuffer CBSSDO : register(b0)
{
	struct
	{
		float4 viewSpaceParams;
		float4 ssdoParams;
	} cbSSDO : packoffset(c0);
}

// Must be defined!
//

#ifndef VIEWPORT_PIXEL_SIZE
#define VIEWPORT_PIXEL_SIZE float2(0.0, 0.0)
#endif

//

// User configurable
//

#ifndef GTAO_QUALITY
#define GTAO_QUALITY 2
#endif

#ifndef RADIUS
#define RADIUS (cbSSDO.ssdoParams.x * cbSSDO.viewSpaceParams.x * CV_NearFarClipDist.y)
#endif

#ifndef FALLOFF_RANGE
#define FALLOFF_RANGE 0.005
#endif

#ifndef SAMPLE_DISTRIBUTION_POWER
#define SAMPLE_DISTRIBUTION_POWER 2.0
#endif

#ifndef DEPTH_MIP_SAMPLING_OFFSET
#define DEPTH_MIP_SAMPLING_OFFSET 3.3
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

#define NDC_TO_VIEW_MUL cbSSDO.viewSpaceParams.xy
#define NDC_TO_VIEW_ADD cbSSDO.viewSpaceParams.zw
#define NDC_TO_VIEW_MUL_X_PIXEL_SIZE (NDC_TO_VIEW_MUL * VIEWPORT_PIXEL_SIZE)

#define XE_GTAO_DEPTH_MIP_LEVELS 5.0

#define GTAO_PI 3.1415926535897932384626433832795
#define GTAO_PI_HALF 1.5707963267948966192313216916398

float GTAO_ScreenSpaceToViewSpaceDepth(float screenDepth)
{
	// The depth is already linearized, just need to scale it.
	return screenDepth * CV_NearFarClipDist.y;
}

// This is also a good place to do non-linear depth conversion for cases where one wants the 'radius' (effectively the threshold between near-field and far-field GI),
// is required to be non-linear (i.e. very large outdoors environments).
float GTAO_ClampDepth(float depth)
{
	return clamp(depth, 0.0, 3.402823466e+38);
}

// weighted average depth filter
float GTAO_DepthMIPFilter(float depth0, float depth1, float depth2, float depth3)
{
	float maxDepth = max(max(depth0, depth1), max(depth2, depth3));

	const float depthRangeScaleFactor = 0.75; // found empirically :)
	const float effectRadius = depthRangeScaleFactor * RADIUS;
	const float falloffRange = FALLOFF_RANGE * effectRadius;
	const float falloffFrom = effectRadius * (1.0 - FALLOFF_RANGE);

	// fadeout precompute optimisation
	const float falloffMul = -rcp(falloffRange);
	const float falloffAdd = falloffFrom * rcp(falloffRange) + 1.0;

	float weight0 = saturate((maxDepth - depth0) * falloffMul + falloffAdd);
	float weight1 = saturate((maxDepth - depth1) * falloffMul + falloffAdd);
	float weight2 = saturate((maxDepth - depth2) * falloffMul + falloffAdd);
	float weight3 = saturate((maxDepth - depth3) * falloffMul + falloffAdd);

	float weightSum = weight0 + weight1 + weight2 + weight3;
	return (weight0 * depth0 + weight1 * depth1 + weight2 * depth2 + weight3 * depth3) * rcp(weightSum);
}

groupshared float g_scratchDepths[8][8];
void GTAO_PrefilterDepths16x16(uint2 dispatchThreadID, uint2 groupThreadID, Texture2D sourceNDCDepth, SamplerState depthSampler, RWTexture2D<float> outDepth0, RWTexture2D<float> outDepth1, RWTexture2D<float> outDepth2, RWTexture2D<float> outDepth3, RWTexture2D<float> outDepth4)
{
	// MIP 0
	const uint2 baseCoord = dispatchThreadID;
	const uint2 pixCoord = baseCoord * 2;
	float4 depths4 = sourceNDCDepth.GatherRed(depthSampler, float2(pixCoord * VIEWPORT_PIXEL_SIZE), int2(1, 1));
	float depth0 = GTAO_ClampDepth(GTAO_ScreenSpaceToViewSpaceDepth(depths4.w));
	float depth1 = GTAO_ClampDepth(GTAO_ScreenSpaceToViewSpaceDepth(depths4.z));
	float depth2 = GTAO_ClampDepth(GTAO_ScreenSpaceToViewSpaceDepth(depths4.x));
	float depth3 = GTAO_ClampDepth(GTAO_ScreenSpaceToViewSpaceDepth(depths4.y));
	outDepth0[pixCoord + uint2(0, 0)] = depth0;
	outDepth0[pixCoord + uint2(1, 0)] = depth1;
	outDepth0[pixCoord + uint2(0, 1)] = depth2;
	outDepth0[pixCoord + uint2(1, 1)] = depth3;

	// MIP 1
	float dm1 = GTAO_DepthMIPFilter(depth0, depth1, depth2, depth3);
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

		float dm2 = GTAO_DepthMIPFilter(inTL, inTR, inBL, inBR);
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

		float dm3 = GTAO_DepthMIPFilter(inTL, inTR, inBL, inBR);
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

		float dm4 = GTAO_DepthMIPFilter(inTL, inTR, inBL, inBR);
		outDepth4[baseCoord / 8] = dm4;
		//g_scratchDepths[ groupThreadID.x ][ groupThreadID.y ] = dm4;
	}
}

// Inputs are screen XY and viewspace depth, output is viewspace position
float3 GTAO_ComputeViewspacePosition(float2 screenPos, float viewspaceDepth)
{
	float3 ret;
	ret.xy = (NDC_TO_VIEW_MUL * screenPos.xy + NDC_TO_VIEW_ADD) * viewspaceDepth;
	ret.z = viewspaceDepth;
	return ret;
}

// http://h14s.p5r.org/2012/09/0x5f3759df.html, [Drobot2014a] Low Level Optimizations for GCN, https://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pdf slide 63
float GTAO_FastSqrt(float x)
{
	return asfloat(0x1fbd1df5 + (asint(x) >> 1));
}

// input [-1, 1] and output [0, PI], from https://seblagarde.wordpress.com/2014/12/01/inverse-trigonometric-functions-gpu-optimization-for-amd-gcn-architecture/
float GTAO_FastACos(float inX)
{
	const float pi = 3.141593;
	const float half_pi = 1.570796;
	float x = abs(inX);
	float res = -0.156583 * x + half_pi;
	res *= GTAO_FastSqrt(1.0 - x);
	return inX >= 0.0 ? res : pi - res;
}

void GTAO_MainPass(uint2 pixCoord, float2 localNoise, float3 viewspaceNormal, Texture2D sourceViewspaceDepth, SamplerState depthSampler, out float4 outWorkingAOTermAndBentNormal)
{
	float2 normalizedScreenPos = (pixCoord + 0.5) * VIEWPORT_PIXEL_SIZE;

	// viewspace Z at the center
	float viewspaceZ = sourceViewspaceDepth.Load(int3(pixCoord, 0)).x;

	// Move center pixel slightly towards camera to avoid imprecision artifacts due to depth buffer imprecision; offset depends on depth texture format used
	viewspaceZ *= 0.99999; // this is good for FP32 depth buffer

	const float3 pixCenterPos = GTAO_ComputeViewspacePosition(normalizedScreenPos, viewspaceZ);
	const float3 viewVec = normalize(-pixCenterPos);

	const float falloffRange = FALLOFF_RANGE * RADIUS;
	const float falloffFrom = RADIUS * (1.0 - FALLOFF_RANGE);

	// fadeout precompute optimisation
	const float falloffMul = -rcp(falloffRange);
	const float falloffAdd = falloffFrom * rcp(falloffRange) + 1.0;

	float4 sh2 = 0;

	// see "Algorithm 1" in https://www.activision.com/cdn/research/Practical_Real_Time_Strategies_for_Accurate_Indirect_Occlusion_NEW%20VERSION_COLOR.pdf
	{
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
			float phi = sliceK * GTAO_PI;
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
			float n = signNorm * GTAO_FastACos(cosNorm);

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
				const float mipLevel = clamp(log2(sampleOffsetLength) - DEPTH_MIP_SAMPLING_OFFSET, 0.0, XE_GTAO_DEPTH_MIP_LEVELS);

				// Snap to pixel center (more correct direction math, avoids artifacts due to sampling pos not matching depth texel center - messes up slope - but adds other
				// artifacts due to them being pushed off the slice). Also use full precision for high res cases.
				sampleOffset = round(sampleOffset) * VIEWPORT_PIXEL_SIZE;

				float2 sampleScreenPos0 = normalizedScreenPos + sampleOffset;
				float2 sampleScreenPos1 = normalizedScreenPos - sampleOffset;
				float SZ0 = sourceViewspaceDepth.SampleLevel(depthSampler, sampleScreenPos0, mipLevel).x;
				float SZ1 = sourceViewspaceDepth.SampleLevel(depthSampler, sampleScreenPos1, mipLevel).x;
				float3 samplePos0 = GTAO_ComputeViewspacePosition(sampleScreenPos0, SZ0);
				float3 samplePos1 = GTAO_ComputeViewspacePosition(sampleScreenPos1, SZ1);

				float3 sampleDelta0 = samplePos0 - pixCenterPos;
				float3 sampleDelta1 = samplePos1 - pixCenterPos;
				float sampleDist0 = length(sampleDelta0);
				float sampleDist1 = length(sampleDelta1);

				// approx lines 23, 24 from the paper, unrolled
				float3 sampleHorizonVec0 = sampleDelta0 * rcp(sampleDist0);
				float3 sampleHorizonVec1 = sampleDelta1 * rcp(sampleDist1);

				// any sample out of radius should be discarded - also use fallof range for smooth transitions; this is a modified idea from "4.3 Implementation details, Bounding the sampling area"
				// this is our own thickness heuristic that relies on sooner discarding samples behind the center
				float weight0 = saturate(sampleDist0 * falloffMul + falloffAdd);
				float weight1 = saturate(sampleDist1 * falloffMul + falloffAdd);

				// This should match the original SSDO, with added falloff.
				//

				// Compute obscurance
				const float radiusWS = RADIUS * viewspaceZ;
				const float emitterScale = 2.5;
				const float emitterArea = (emitterScale * GTAO_PI * radiusWS * radiusWS) / (SLICE_COUNT * STEPS_PER_SLICE * 2.0);
				float fNdotSamp0 = dot(viewspaceNormal, sampleHorizonVec0);
				float fNdotSamp1 = dot(viewspaceNormal, sampleHorizonVec1);
				float fObscurance0 = emitterArea * saturate(fNdotSamp0) * rcp(sampleDist0 * sampleDist0 + emitterArea);
				float fObscurance1 = emitterArea * saturate(fNdotSamp1) * rcp(sampleDist1 * sampleDist1 + emitterArea);

				// Added falloff.
				fObscurance0 *= weight0;
				fObscurance1 *= weight1;

				// Accumulate AO and bent normal as SH basis
				sh2.w += fObscurance0 + fObscurance1;
				sh2.xyz += fObscurance0 * sampleHorizonVec0 + fObscurance1 * sampleHorizonVec1;

				//
			}
		}
	}

	// Normalize.
	sh2 /= (SLICE_COUNT * STEPS_PER_SLICE * 2.0);

	outWorkingAOTermAndBentNormal.xyz = mul(CV_InvViewMatr, float4(sh2.xyz * float3(1, -1, -1), 0.0)).xyz * 0.5 + 0.5;
	outWorkingAOTermAndBentNormal.w = sh2.w;

	// DEBUG, output visibility.
	//outWorkingAOTermAndBentNormal = float4(1.0 - sh2.www, 1.0);
}

// Implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SamplerState smp : register(s0);
Texture2D tex0 : register(t0);
Texture2D tex1 : register(t1);
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
	#ifdef XE_GTAO_HILBERT_LUT_AVAILABLE // load from lookup texture...
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
	GTAO_PrefilterDepths16x16(dtid, gtid, tex0, smp, out_working_depth_mip0, out_working_depth_mip1, out_working_depth_mip2, out_working_depth_mip3, out_working_depth_mip4);
}

void main_pass_ps(float4 pos : SV_Position, out float4 outWorkingAOTermAndPackedDepth : SV_Target)
{
	// tex0 = _tex0_D3D11 (worldspace normal from the original shader)
	// tex1 = g_srcWorkingDepth
	// smp = g_samplerPointClamp

	// Compute normal in view space
	float3 vNormal = normalize(tex0.Load(int3(pos.xy, 0)).xyz * 2.0 - 1.0);
	float3 vNormalVS = normalize(mul(CV_ViewMatr, float4(vNormal, 0)).xyz) * float3(1, -1, -1);

	GTAO_MainPass(pos.xy, SpatioTemporalNoise(pos.xy, gtao_temporal_index), vNormalVS, tex1, smp, outWorkingAOTermAndPackedDepth);
}