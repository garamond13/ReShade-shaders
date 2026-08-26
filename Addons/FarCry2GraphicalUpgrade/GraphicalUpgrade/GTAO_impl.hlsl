// VB-GTAO addopted for Far Cry 2.
//
// Ported from compute shader SM 5.0+ to pixel shader SM 4.1.
//
// Visibility Bitmask - Ground Truth Ambient Occlusion, (https://cdrinmatane.github.io/posts/ssaovb-code/) based on XeGTAO (https://github.com/GameTechDev/XeGTAO).

cbuffer ViewportShaderParameterProvider : register(b12)
{
	float4x4 _ViewRotProjectionMatrix : packoffset(c0);
	float4x4 _ViewProjectionMatrix : packoffset(c4);
	float4x4 _ProjectionMatrix : packoffset(c8);
	float4x4 _ViewMatrix : packoffset(c12);
	float4x4 _InvProjectionMatrix : packoffset(c16);
	float4x4 _InvProjectionMatrixDepth : packoffset(c20);
	float4x4 _DepthTextureTransform : packoffset(c24);
	float4x4 _WaterReflectionTransform : packoffset(c28);
	float4x4 _GrassCylindricalBillboardMatrix : packoffset(c32);
	float4x4 _InvViewMatrix : packoffset(c36);
	float4 _CameraDistances : packoffset(c40);
	float4 _WaterReflectionPlane : packoffset(c41);
	float4 _WaterLevelAdjustment : packoffset(c42);
	float4 _ViewportSize : packoffset(c43);
	float4 _WaterReflectionColor : packoffset(c44);
	float4 _CameraPosition_DistanceScale : packoffset(c45);
	float3 _CameraDirection : packoffset(c46);
	float3 _ViewPoint : packoffset(c47);
	float3 _FogColorVector : packoffset(c48);
	float3 _FogColor : packoffset(c49);
	float3 _FogColorRange : packoffset(c50);
	float3 _FogValues : packoffset(c51);
	float4 _FogHeightValues : packoffset(c52);
	float3 _CameraRight : packoffset(c53);
	float3 _CameraUp : packoffset(c54);
	float4 _CameraNearPlaneSize : packoffset(c55);
	float3 _UncompressDepthWeights : packoffset(c56);
	float3 _UncompressDepthWeightsWS : packoffset(c57);
	float _BloomAdaptationFactor : packoffset(c57.w);
	float3 _CameraPositionFractions : packoffset(c58);
	float4 _CurvedHorizonFactors : packoffset(c59);
	float4 _DepthTextureRcpSize : packoffset(c60);
	float _ShadowProjDepthMinValue : packoffset(c61);
	float _SunOcclusionFactor : packoffset(c61.y);
	float4 _WaterReflectionTexCoordRange : packoffset(c62);
}

// User configurable
//

#ifndef GTAO_QUALITY
#define GTAO_QUALITY 2
#endif

#ifndef EFFECT_RADIUS
#define EFFECT_RADIUS 0.5
#endif

#ifndef THICKNESS
#define THICKNESS 0.2
#endif

#ifndef SAMPLE_DISTRIBUTION_POWER
#define SAMPLE_DISTRIBUTION_POWER 2.0
#endif

#ifndef FINAL_VALUE_POWER
#define FINAL_VALUE_POWER 0.45
#endif

#ifndef DEPTH_MIP_SAMPLING_OFFSET
#define DEPTH_MIP_SAMPLING_OFFSET 4.5
#endif

#ifndef STEPS_PER_SLICE
#define STEPS_PER_SLICE 3.0
#endif

#ifndef DENOISE_BLUR_BETA
#define DENOISE_BLUR_BETA 1.2
#endif

//

#if GTAO_QUALITY == 0 // Low
	#define SLICE_COUNT 4.0
#elif GTAO_QUALITY == 1 // Medium
	#define SLICE_COUNT 6.0
#elif GTAO_QUALITY == 2 // High
	#define SLICE_COUNT 8.0
#elif GTAO_QUALITY == 3 // Very High
	#define SLICE_COUNT 10.0
#elif GTAO_QUALITY == 4 // Ultra
	#define SLICE_COUNT 12.0
#endif

#define VIEWPORT_PIXEL_SIZE _ViewportSize.zw

#define TAN_HALF_FOV_X (_InvProjectionMatrix._m00)
#define TAN_HALF_FOV_Y (_InvProjectionMatrix._m11)
#define NDC_TO_VIEW_MUL float2(TAN_HALF_FOV_X * 2.0, TAN_HALF_FOV_Y * -2.0)
#define NDC_TO_VIEW_ADD float2(-TAN_HALF_FOV_X, TAN_HALF_FOV_Y)

#ifndef NDC_TO_VIEW_MUL_X_PIXEL_SIZE
#define NDC_TO_VIEW_MUL_X_PIXEL_SIZE (NDC_TO_VIEW_MUL * VIEWPORT_PIXEL_SIZE)
#endif

#define DEPTH_MIP_LEVELS 5.0
#define SECTOR_COUNT 32
#define OCCLUSION_TERM_SCALE 1.5

#define PI 3.1415926535897932384626433832795
#define PI_HALF 1.5707963267948966192313216916398

// This is also a good place to do non-linear depth conversion for cases where one wants the 'radius' (effectively the threshold between near-field and far-field GI),
// is required to be non-linear (i.e. very large outdoors environments).
float ClampDepth(float depth)
{
	return clamp(depth, 0.0, 3.402823466e+38);
}

// weighted average depth filter
float DepthMIPFilter(float depth0, float depth1, float depth2, float depth3)
{
	float maxDepth = max(max(depth0, depth1), max(depth2, depth3));
	return maxDepth;
}

void PrefilterDepths_mip0(float2 pos, Texture2D<float> sourceNDCDepth, out float outDepth)
{
	// Depth is already linearized.
	outDepth = sourceNDCDepth.Load(int3(pos.xy, 0));
	outDepth = ClampDepth(outDepth);
}

void PrefilterDepths(float2 texcoord, Texture2D<float> sourceDepth, SamplerState depthSampler, out float outDepth)
{
	float4 depths4 = sourceDepth.Gather(depthSampler, texcoord);
	float depth0 = depths4.w;
	float depth1 = depths4.z;
	float depth2 = depths4.x;
	float depth3 = depths4.y;
	outDepth = DepthMIPFilter(depth0, depth1, depth2, depth3);
}

float4 CalculateEdges(float centerZ, float leftZ, float rightZ, float topZ, float bottomZ)
{
	float4 edgesLRTB = float4(leftZ, rightZ, topZ, bottomZ) - centerZ;

	float slopeLR = (edgesLRTB.y - edgesLRTB.x) * 0.5;
	float slopeTB = (edgesLRTB.w - edgesLRTB.z) * 0.5;
	float4 edgesLRTBSlopeAdjusted = edgesLRTB + float4(slopeLR, -slopeLR, slopeTB, -slopeTB);
	edgesLRTB = min(abs(edgesLRTB), abs(edgesLRTBSlopeAdjusted));
	return saturate(1.25 - edgesLRTB / (centerZ * 0.011));
}

// packing/unpacking for edges; 2 bits per edge mean 4 gradient values (0, 0.33, 0.66, 1) for smoother transitions!
float PackEdges(float4 edgesLRTB)
{
	// integer version:
	// edgesLRTB = saturate(edgesLRTB) * 2.9.xxxx + 0.5.xxxx;
	// return (((uint)edgesLRTB.x) << 6) + (((uint)edgesLRTB.y) << 4) + (((uint)edgesLRTB.z) << 2) + (((uint)edgesLRTB.w));
	//
	// optimized, should be same as above
	edgesLRTB = round(saturate(edgesLRTB) * 2.9);
	return dot(edgesLRTB, float4(64.0 / 255.0, 16.0 / 255.0, 4.0 / 255.0, 1.0 / 255.0));
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
	uint angleHorizonInt = round((maxHorizon - minHorizon) * SECTOR_COUNT);
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

	// Shift sample from V to normal, map to [0..1] with smoothstep
	frontBackHorizon = smoothstep(0.0, 1.0, (samplingDirection * -frontBackHorizon - N + PI_HALF) / PI);

	// Sampling direction inverts min/max angles
	frontBackHorizon = samplingDirection >= 0 ? frontBackHorizon.yx : frontBackHorizon.xy;

	globalOccludedBitfield = UpdateSectors(frontBackHorizon.x, frontBackHorizon.y, globalOccludedBitfield);
}

void MainPass(uint2 pixCoord, float2 localNoise, Texture2D<float> sourceViewspaceDepth, SamplerState depthSampler, out float2 outWorkingAOTermAndEdges)
{
	float2 normalizedScreenPos = (pixCoord + 0.5) * VIEWPORT_PIXEL_SIZE;

	float4 valuesUL = sourceViewspaceDepth.Gather(depthSampler, float2(pixCoord * VIEWPORT_PIXEL_SIZE));
	float4 valuesBR = sourceViewspaceDepth.Gather(depthSampler, float2(pixCoord * VIEWPORT_PIXEL_SIZE), int2(1, 1));

	// viewspace Z at the center
	float viewspaceZ = valuesUL.y; //sourceViewspaceDepth.SampleLevel( depthSampler, normalizedScreenPos, 0 ).x;

	// viewspace Zs left top right bottom
	const float pixLZ = valuesUL.x;
	const float pixTZ = valuesUL.z;
	const float pixRZ = valuesBR.z;
	const float pixBZ = valuesBR.x;

	float4 edgesLRTB = CalculateEdges(viewspaceZ, pixLZ, pixRZ, pixTZ, pixBZ);

	float3 viewspaceNormal = ComputeViewspaceNormal(normalizedScreenPos, viewspaceZ, pixLZ, pixRZ, pixTZ, pixBZ, edgesLRTB);

	// Move center pixel slightly towards camera to avoid imprecision artifacts due to depth buffer imprecision; offset depends on depth texture format used
	viewspaceZ *= 0.99999; // this is good for FP32 depth buffer

	const float3 pixCenterPos = ComputeViewspacePosition(normalizedScreenPos, viewspaceZ);
	const float3 viewVec = normalize(-pixCenterPos);

	float visibility = 0.0;

	const float noiseSlice = localNoise.x;
	const float noiseSample = localNoise.y;

	// quality settings / tweaks / hacks
	const float pixelTooCloseThreshold = 1.3; // if the offset is under approx pixel size (pixelTooCloseThreshold), push it out to the minimum distance

	// approx viewspace pixel size at pixCoord; approximation of NDCToViewspace( normalizedScreenPos.xy + consts.ViewportPixelSize.xy, pixCenterPos.z ).xy - pixCenterPos.xy;
	const float2 pixelDirRBViewspaceSizeAtCenterZ = viewspaceZ.xx * NDC_TO_VIEW_MUL_X_PIXEL_SIZE;

	float screenspaceRadius = EFFECT_RADIUS / pixelDirRBViewspaceSizeAtCenterZ.x;

	// this is the min distance to start sampling from to avoid sampling from the center pixel (no useful data obtained from sampling center pixel)
	const float minS = pixelTooCloseThreshold / screenspaceRadius;

	[unroll]
	for (float slice = 0.0; slice < SLICE_COUNT; slice++) {
		// lines 5, 6 from the paper
		float phi = (slice + noiseSlice) * (PI / SLICE_COUNT);
		float cosPhi = cos(phi);
		float sinPhi = sin(phi);
		float2 omega = float2(cosPhi, -sinPhi);

		// convert to screen units (pixels) for later use
		omega *= screenspaceRadius;

		// line 8 from the paper
		const float3 directionVec = float3(cosPhi, sinPhi, 0.0);

		// line 9 from the paper
		const float3 orthoDirectionVec = directionVec - dot(directionVec, viewVec) * viewVec;

		// line 10 from the paper
		//axisVec is orthogonal to directionVec and viewVec, used to define projectedNormal
		const float3 axisVec = normalize(cross(orthoDirectionVec, viewVec));

		// line 11 from the paper
		float3 projectedNormalVec = viewspaceNormal - axisVec * dot(viewspaceNormal, axisVec);

		// line 13 from the paper
		float signNorm = sign(dot(orthoDirectionVec, projectedNormalVec));

		// line 14 from the paper
		float projectedNormalVecLength = length(projectedNormalVec);
		float cosNorm = saturate(dot(projectedNormalVec, viewVec) / projectedNormalVecLength);

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
			float2 sampleScreenPos1 = normalizedScreenPos - sampleOffset;
			float SZ0 = sourceViewspaceDepth.SampleLevel(depthSampler, sampleScreenPos0, mipLevel).x;
			float SZ1 = sourceViewspaceDepth.SampleLevel(depthSampler, sampleScreenPos1, mipLevel).x;
			float3 samplePos0 = ComputeViewspacePosition(sampleScreenPos0, SZ0);
			float3 samplePos1 = ComputeViewspacePosition(sampleScreenPos1, SZ1);

			float3 sampleDelta0 = samplePos0 - pixCenterPos;
			float3 sampleDelta1 = samplePos1 - pixCenterPos;

			ProcessSample(sampleDelta0, viewVec, n, -1.0, globalOccludedBitfield);
 			ProcessSample(sampleDelta1, viewVec, n, 1.0, globalOccludedBitfield);
		}
		visibility += 1.0 - float(countbits(globalOccludedBitfield)) / float(SECTOR_COUNT);
	}
	visibility /= SLICE_COUNT;

	// The AO gets drawn on top of fog. Not much we can do about it, there is no dedicated fog shader.
	// We will reduce the final value power (intensity) with distance as a workaround using Generalized Normal Window (GNW).
	const float s = 45.0;
	const float n = 3.0;
	const float final_value_power = FINAL_VALUE_POWER * exp(-pow(viewspaceZ / s, n));

	visibility = pow(visibility, final_value_power);
	visibility = max(0.03, visibility); // disallow total occlusion (which wouldn't make any sense anyhow since pixel is visible but also helps with packing bent normals)

	outWorkingAOTermAndEdges.x = saturate(visibility / OCCLUSION_TERM_SCALE);
	outWorkingAOTermAndEdges.y = PackEdges(edgesLRTB);
}

float4 UnpackEdges(float _packedVal)
{
	uint packedVal = uint(_packedVal * 255.5);
	float4 edgesLRTB;
	edgesLRTB.x = float((packedVal >> 6) & 0x03) / 3.0; // there's really no need for mask (as it's an 8 bit input) but I'll leave it in so it doesn't cause any trouble in the future
	edgesLRTB.y = float((packedVal >> 4) & 0x03) / 3.0;
	edgesLRTB.z = float((packedVal >> 2) & 0x03) / 3.0;
	edgesLRTB.w = float((packedVal >> 0) & 0x03) / 3.0;

	return saturate(edgesLRTB);
}

void AddSample(float ssaoValue, float edgeValue, inout float sum, inout float sumWeight)
{
	float weight = edgeValue;

	sum += weight * ssaoValue;
	sumWeight += weight;
}

void Denoise(int2 pixCoordBase, Texture2D<float2> sourceAOTermAndEdges,
#ifdef FINAL_APPLY
out float4 o
#else
out float2 o
#endif
)
{
	#ifdef FINAL_APPLY
	const float blurAmount = DENOISE_BLUR_BETA;
	#else
	const float blurAmount = DENOISE_BLUR_BETA / 5.0;
	#endif

	const float diagWeight = 0.85 * 0.5;

	// Get AOTerm and Edges.
	// Originally they are in 2 separate textures.
	float2 C = sourceAOTermAndEdges.Load(int3(pixCoordBase, 0));
	float2 L = sourceAOTermAndEdges.Load(int3(pixCoordBase, 0), int2(-1, 0));
	float2 R = sourceAOTermAndEdges.Load(int3(pixCoordBase, 0), int2(1, 0));
	float2 T = sourceAOTermAndEdges.Load(int3(pixCoordBase, 0), int2(0, -1));
	float2 B = sourceAOTermAndEdges.Load(int3(pixCoordBase, 0), int2(0, 1));
	float TL = sourceAOTermAndEdges.Load(int3(pixCoordBase, 0), int2(-1, -1)).x;
	float TR = sourceAOTermAndEdges.Load(int3(pixCoordBase, 0), int2(1, -1)).x;
	float BL = sourceAOTermAndEdges.Load(int3(pixCoordBase, 0), int2(-1, 1)).x;
	float BR = sourceAOTermAndEdges.Load(int3(pixCoordBase, 0), int2(1, 1)).x;

	float4 edgesC_LRTB = UnpackEdges(C.y);
	float4 edgesL_LRTB = UnpackEdges(L.y);
	float4 edgesR_LRTB = UnpackEdges(R.y);
	float4 edgesT_LRTB = UnpackEdges(T.y);
	float4 edgesB_LRTB = UnpackEdges(B.y);

	// Edges aren't perfectly symmetrical: edge detection algorithm does not guarantee that a left edge on the right pixel will match the right edge on the left pixel (although
	// they will match in majority of cases). This line further enforces the symmetricity, creating a slightly sharper blur. Works real nice with TAA.
	edgesC_LRTB *= float4(edgesL_LRTB.y, edgesR_LRTB.x, edgesT_LRTB.w, edgesB_LRTB.z);

#if 1   // this allows some small amount of AO leaking from neighbours if there are 3 or 4 edges; this reduces both spatial and temporal aliasing
	const float leak_threshold = 2.5;
	const float leak_strength = 0.5;
	float edginess = (saturate(4.0 - leak_threshold - dot(edgesC_LRTB, 1.0)) / (4.0 - leak_threshold)) * leak_strength;
	edgesC_LRTB = saturate(edgesC_LRTB + edginess);
#endif

	float weightTL = diagWeight * (edgesC_LRTB.x * edgesL_LRTB.z + edgesC_LRTB.z * edgesT_LRTB.x);
	float weightTR = diagWeight * (edgesC_LRTB.z * edgesT_LRTB.y + edgesC_LRTB.y * edgesR_LRTB.z);
	float weightBL = diagWeight * (edgesC_LRTB.w * edgesB_LRTB.x + edgesC_LRTB.x * edgesL_LRTB.w);
	float weightBR = diagWeight * (edgesC_LRTB.y * edgesR_LRTB.w + edgesC_LRTB.w * edgesB_LRTB.y);

	float ssaoValue = C.x;
	float ssaoValueL = L.x;
	float ssaoValueT = T.x;
	float ssaoValueR = R.x;
	float ssaoValueB = B.x;
	float ssaoValueTL = TL;
	float ssaoValueBR = BR;
	float ssaoValueTR = TR;
	float ssaoValueBL = BL;

	float sumWeight = blurAmount;
	float sum = ssaoValue * sumWeight;

	AddSample(ssaoValueL, edgesC_LRTB.x, sum, sumWeight);
	AddSample(ssaoValueR, edgesC_LRTB.y, sum, sumWeight);
	AddSample(ssaoValueT, edgesC_LRTB.z, sum, sumWeight);
	AddSample(ssaoValueB, edgesC_LRTB.w, sum, sumWeight);

	AddSample(ssaoValueTL, weightTL, sum, sumWeight);
	AddSample(ssaoValueTR, weightTR, sum, sumWeight);
	AddSample(ssaoValueBL, weightBL, sum, sumWeight);
	AddSample(ssaoValueBR, weightBR, sum, sumWeight);

	float aoTerm = sum / sumWeight;

	#ifdef FINAL_APPLY
	o = float4(aoTerm.xxx * OCCLUSION_TERM_SCALE, 1.0);
	#else
	o = float2(aoTerm, C.y);
	#endif
}

// Implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Texture2D<float> tex0 : register(t0);
Texture2D<float2> tex1 : register(t1);
SamplerState smp : register(s0);

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

// For the mip0.
void prefilter_depths_mip0_ps(float4 pos : SV_Position, out float out_working_depth_mip0 : SV_Target)
{
	// tex0 = g_srcRawDepth
	PrefilterDepths_mip0(pos.xy, tex0, out_working_depth_mip0);
}

// For mips 1 to 4.
void prefilter_depths_ps(float4 pos : SV_Position, float2 texcoord : TEXCOORD, out float out_working_depth : SV_Target)
{
	// tex0 = out_working_depth_mip[N]
	PrefilterDepths(texcoord, tex0, smp, out_working_depth);
}

void main_pass_ps(float4 pos : SV_Position, out float2 out_working_ao_term_and_edges : SV_Target)
{
	// tex0 = g_srcWorkingDepth
	// smp = g_samplerPointClamp
	MainPass(pos.xy, SpatioTemporalNoise(pos.xy, 0), tex0, smp, out_working_ao_term_and_edges);
}

void denoise_pass_ps(float4 pos : SV_Position,
#ifdef FINAL_APPLY
out float4 o : SV_Target
#else
out float2 o : SV_Target
#endif
)
{
	// tex1 = g_srcWorkingAOTerm and g_srcWorkingEdges, packed.
	Denoise(pos.xy, tex1, o);
}