// XeGTAO addopted for Bioshock.
//
// Ported from compute shader SM 5.0+ to pixel shader SM 4.1.
//
// Source: https://github.com/GameTechDev/XeGTAO

#ifndef __XE_GTAO_HLSLI__
#define __XE_GTAO_HLSLI__

// THESE MUST BE DEFINED!
//

#ifndef VIEWPORT_PIXEL_SIZE
#define VIEWPORT_PIXEL_SIZE float2(0.0, 0.0)
#endif

#ifndef NDC_TO_VIEW_MUL
#define NDC_TO_VIEW_MUL float2(0.0, 0.0)
#endif

#ifndef NDC_TO_VIEW_ADD
#define NDC_TO_VIEW_ADD float2(0.0, 0.0)
#endif

//

// User configurable
//

#ifndef EFFECT_RADIUS
#define EFFECT_RADIUS 0.5
#endif

#ifndef RADIUS_MULTIPLIER
#define RADIUS_MULTIPLIER 1.457
#endif

#ifndef EFFECT_FALLOFF_RANGE
#define EFFECT_FALLOFF_RANGE 0.615
#endif

#ifndef SAMPLE_DISTRIBUTION_POWER
#define SAMPLE_DISTRIBUTION_POWER 2.0
#endif

#ifndef THIN_OCCLUDER_COMPENSATION
#define THIN_OCCLUDER_COMPENSATION 0.0
#endif

#ifndef FINAL_VALUE_POWER
#define FINAL_VALUE_POWER 2.2
#endif

#ifndef DEPTH_MIP_SAMPLING_OFFSET
#define DEPTH_MIP_SAMPLING_OFFSET 3.3
#endif

#ifndef SLICE_COUNT
#define SLICE_COUNT 3.0
#endif

#ifndef STEPS_PER_SLICE
#define STEPS_PER_SLICE 3.0
#endif

#ifndef DENOISE_BLUR_BETA
#define DENOISE_BLUR_BETA 1.2
#endif

#ifndef CLIP_NEAR
#define CLIP_NEAR 1.0
#endif

#ifndef CLIP_FAR
#define CLIP_FAR 1000.0
#endif

//

#ifndef NDC_TO_VIEW_MUL_X_PIXEL_SIZE
#define NDC_TO_VIEW_MUL_X_PIXEL_SIZE (NDC_TO_VIEW_MUL * VIEWPORT_PIXEL_SIZE)
#endif

#define XE_GTAO_DEPTH_MIP_LEVELS 5.0
#define XE_GTAO_OCCLUSION_TERM_SCALE 1.5

#define XE_GTAO_PI 3.1415926535897932384626433832795
#define XE_GTAO_PI_HALF 1.5707963267948966192313216916398

float XeGTAO_ScreenSpaceToViewSpaceDepth(float screenDepth)
{
	float depthLinearizeMul = CLIP_FAR * CLIP_NEAR / (CLIP_FAR - CLIP_NEAR);
	float depthLinearizeAdd = CLIP_FAR / (CLIP_FAR - CLIP_NEAR);

	// correct the handedness issue. need to make sure this below is correct, but I think it is.
	if (depthLinearizeMul * depthLinearizeAdd < 0.0) {
		depthLinearizeAdd = -depthLinearizeAdd;
	}

	// Optimised version of "-cameraClipNear / (cameraClipFar - projDepth * (cameraClipFar - cameraClipNear)) * cameraClipFar"
	return depthLinearizeMul / (depthLinearizeAdd - screenDepth);
}

// This is also a good place to do non-linear depth conversion for cases where one wants the 'radius' (effectively the threshold between near-field and far-field GI), 
// is required to be non-linear (i.e. very large outdoors environments).
float XeGTAO_ClampDepth(float depth)
{
	return clamp(depth, 0.0, 3.402823466e+38);
}

// weighted average depth filter
float XeGTAO_DepthMIPFilter(float depth0, float depth1, float depth2, float depth3)
{
	float maxDepth = max(max(depth0, depth1), max(depth2, depth3));

	const float depthRangeScaleFactor = 0.75; // found empirically :)
	const float effectRadius = depthRangeScaleFactor * EFFECT_RADIUS * RADIUS_MULTIPLIER;
	const float falloffRange = EFFECT_FALLOFF_RANGE * effectRadius;
	const float falloffFrom = effectRadius * (1.0 - EFFECT_FALLOFF_RANGE);

	// fadeout precompute optimisation
	const float falloffMul = -1.0 / falloffRange;
	const float falloffAdd = falloffFrom / falloffRange + 1.0;

	float weight0 = saturate((maxDepth - depth0) * falloffMul + falloffAdd);
	float weight1 = saturate((maxDepth - depth1) * falloffMul + falloffAdd);
	float weight2 = saturate((maxDepth - depth2) * falloffMul + falloffAdd);
	float weight3 = saturate((maxDepth - depth3) * falloffMul + falloffAdd);

	float weightSum = weight0 + weight1 + weight2 + weight3;
	return (weight0 * depth0 + weight1 * depth1 + weight2 * depth2 + weight3 * depth3) / weightSum;
}

void XeGTAO_PrefilterDepths_mip0(float2 pos, Texture2D<float> sourceNDCDepth, out float outDepth)
{
	outDepth = sourceNDCDepth.Load(int3(pos.xy, 0));
	outDepth = XeGTAO_ScreenSpaceToViewSpaceDepth(outDepth);
	outDepth = XeGTAO_ClampDepth(outDepth);
}

void XeGTAO_PrefilterDepths(float2 texcoord, Texture2D<float> sourceDepth, SamplerState depthSampler, out float outDepth)
{
	float4 depths4 = sourceDepth.Gather(depthSampler, texcoord);
	float depth0 = depths4.w;
	float depth1 = depths4.z;
	float depth2 = depths4.x;
	float depth3 = depths4.y;
	outDepth = XeGTAO_DepthMIPFilter(depth0, depth1, depth2, depth3);
}

float4 XeGTAO_CalculateEdges(float centerZ, float leftZ, float rightZ, float topZ, float bottomZ)
{
	float4 edgesLRTB = float4(leftZ, rightZ, topZ, bottomZ) - centerZ;

	float slopeLR = (edgesLRTB.y - edgesLRTB.x) * 0.5;
	float slopeTB = (edgesLRTB.w - edgesLRTB.z) * 0.5;
	float4 edgesLRTBSlopeAdjusted = edgesLRTB + float4(slopeLR, -slopeLR, slopeTB, -slopeTB);
	edgesLRTB = min(abs(edgesLRTB), abs(edgesLRTBSlopeAdjusted));
	return saturate(1.25 - edgesLRTB / (centerZ * 0.011));
}

// packing/unpacking for edges; 2 bits per edge mean 4 gradient values (0, 0.33, 0.66, 1) for smoother transitions!
float XeGTAO_PackEdges(float4 edgesLRTB)
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
float3 XeGTAO_ComputeViewspacePosition(float2 screenPos, float viewspaceDepth)
{
	float3 ret;
	ret.xy = (NDC_TO_VIEW_MUL * screenPos.xy + NDC_TO_VIEW_ADD) * viewspaceDepth;
	ret.z = viewspaceDepth;
	return ret;
}

float3 XeGTAO_CalculateNormal(const float4 edgesLRTB, const float3 pixCenterPos, float3 pixLPos, float3 pixRPos, float3 pixTPos, float3 pixBPos)
{
	// Get this pixel's viewspace normal
	float4 acceptedNormals = saturate(float4(edgesLRTB.x * edgesLRTB.z, edgesLRTB.z * edgesLRTB.y, edgesLRTB.y * edgesLRTB.w, edgesLRTB.w * edgesLRTB.x) + 0.01);

	pixLPos = normalize(pixLPos - pixCenterPos);
	pixRPos = normalize(pixRPos - pixCenterPos);
	pixTPos = normalize(pixTPos - pixCenterPos);
	pixBPos = normalize(pixBPos - pixCenterPos);

	float3 pixelNormal = acceptedNormals.x * cross(pixLPos, pixTPos) + acceptedNormals.y * cross(pixTPos, pixRPos) + acceptedNormals.z * cross(pixRPos, pixBPos) + acceptedNormals.w * cross(pixBPos, pixLPos);
	pixelNormal = normalize(pixelNormal);

	return pixelNormal;
}

// http://h14s.p5r.org/2012/09/0x5f3759df.html, [Drobot2014a] Low Level Optimizations for GCN, https://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pdf slide 63
float XeGTAO_FastSqrt(float x)
{
	return asfloat(0x1fbd1df5 + (asint(x) >> 1));
}

// input [-1, 1] and output [0, PI], from https://seblagarde.wordpress.com/2014/12/01/inverse-trigonometric-functions-gpu-optimization-for-amd-gcn-architecture/
float XeGTAO_FastACos(float inX)
{
	const float PI = 3.141593;
	const float HALF_PI = 1.570796;
	float x = abs(inX); 
	float res = -0.156583 * x + HALF_PI;
	res *= XeGTAO_FastSqrt(1.0 - x);
	return inX >= 0.0 ? res : PI - res;
}

void XeGTAO_MainPass(uint2 pixCoord, float2 localNoise, Texture2D<float> sourceViewspaceDepth, SamplerState depthSampler, out float outWorkingAOTerm, out float outWorkingEdges)
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

	float4 edgesLRTB = XeGTAO_CalculateEdges(viewspaceZ, pixLZ, pixRZ, pixTZ, pixBZ);
	outWorkingEdges = XeGTAO_PackEdges(edgesLRTB);

	// Generating screen space normals in-place is faster than generating normals in a separate pass but requires
	// use of 32bit depth buffer (16bit works but visibly degrades quality) which in turn slows everything down. So to
	// reduce complexity and allow for screen space normal reuse by other effects, we've pulled it out into a separate
	// pass.
	// However, we leave this code in, in case anyone has a use-case where it fits better.
	float3 CENTER = XeGTAO_ComputeViewspacePosition(normalizedScreenPos, viewspaceZ);
	float3 LEFT = XeGTAO_ComputeViewspacePosition(normalizedScreenPos + float2(-1.0, 0.0) * VIEWPORT_PIXEL_SIZE, pixLZ);
	float3 RIGHT = XeGTAO_ComputeViewspacePosition(normalizedScreenPos + float2(1.0, 0.0) * VIEWPORT_PIXEL_SIZE, pixRZ);
	float3 TOP = XeGTAO_ComputeViewspacePosition(normalizedScreenPos + float2(0.0, -1.0) * VIEWPORT_PIXEL_SIZE, pixTZ);
	float3 BOTTOM = XeGTAO_ComputeViewspacePosition(normalizedScreenPos + float2(0.0, 1.0) * VIEWPORT_PIXEL_SIZE, pixBZ);
	float3 viewspaceNormal = XeGTAO_CalculateNormal(edgesLRTB, CENTER, LEFT, RIGHT, TOP, BOTTOM);

	// Move center pixel slightly towards camera to avoid imprecision artifacts due to depth buffer imprecision; offset depends on depth texture format used
	viewspaceZ *= 0.99999; // this is good for FP32 depth buffer

	const float3 pixCenterPos = XeGTAO_ComputeViewspacePosition(normalizedScreenPos, viewspaceZ);
	const float3 viewVec = normalize(-pixCenterPos);

	// prevents normals that are facing away from the view vector - xeGTAO struggles with extreme cases, but in Vanilla it seems rare so it's disabled by default
	// viewspaceNormal = normalize( viewspaceNormal + max( 0, -dot( viewspaceNormal, viewVec ) ) * viewVec );

	const float effectRadius = EFFECT_RADIUS * RADIUS_MULTIPLIER;
	const float sampleDistributionPower = SAMPLE_DISTRIBUTION_POWER;
	const float thinOccluderCompensation = THIN_OCCLUDER_COMPENSATION;
	const float falloffRange = EFFECT_FALLOFF_RANGE * effectRadius;
	const float falloffFrom = effectRadius * (1.0 - EFFECT_FALLOFF_RANGE);

	// fadeout precompute optimisation
	const float falloffMul = -1.0 / falloffRange;
	const float falloffAdd = falloffFrom / falloffRange + 1.0;

	float visibility = 0.0;

	// see "Algorithm 1" in https://www.activision.com/cdn/research/Practical_Real_Time_Strategies_for_Accurate_Indirect_Occlusion_NEW%20VERSION_COLOR.pdf
	{
		const float noiseSlice = localNoise.x;
		const float noiseSample = localNoise.y;

		// quality settings / tweaks / hacks
		const float pixelTooCloseThreshold = 1.3; // if the offset is under approx pixel size (pixelTooCloseThreshold), push it out to the minimum distance

		// approx viewspace pixel size at pixCoord; approximation of NDCToViewspace( normalizedScreenPos.xy + consts.ViewportPixelSize.xy, pixCenterPos.z ).xy - pixCenterPos.xy;
		const float2 pixelDirRBViewspaceSizeAtCenterZ = viewspaceZ.xx * NDC_TO_VIEW_MUL_X_PIXEL_SIZE;

		float screenspaceRadius = effectRadius / pixelDirRBViewspaceSizeAtCenterZ.x;

		// fade out for small screen radii 
		visibility += saturate((10.0 - screenspaceRadius) / 100.0) * 0.5;

		// this is the min distance to start sampling from to avoid sampling from the center pixel (no useful data obtained from sampling center pixel)
		const float minS = pixelTooCloseThreshold / screenspaceRadius;

		[unroll]
		for (float slice = 0.0; slice < SLICE_COUNT; slice++) {
			float sliceK = (slice + noiseSlice) / SLICE_COUNT;
			// lines 5, 6 from the paper
			float phi = sliceK * XE_GTAO_PI;
			float cosPhi = cos(phi);
			float sinPhi = sin(phi);
			float2 omega = float2(cosPhi, -sinPhi); //lpfloat2 on omega causes issues with big radii

			// convert to screen units (pixels) for later use
			omega *= screenspaceRadius;

			// line 8 from the paper
			const float3 directionVec = float3(cosPhi, sinPhi, 0.0);

			// line 9 from the paper
			const float3 orthoDirectionVec = directionVec - (dot(directionVec, viewVec) * viewVec);

			// line 10 from the paper
			//axisVec is orthogonal to directionVec and viewVec, used to define projectedNormal
			const float3 axisVec = normalize(cross(orthoDirectionVec, viewVec));

			// alternative line 9 from the paper
			// float3 orthoDirectionVec = cross( viewVec, axisVec );

			// line 11 from the paper
			float3 projectedNormalVec = viewspaceNormal - axisVec * dot(viewspaceNormal, axisVec);

			// line 13 from the paper
			float signNorm = sign(dot(orthoDirectionVec, projectedNormalVec));

			// line 14 from the paper
			float projectedNormalVecLength = length(projectedNormalVec);
			float cosNorm = saturate(dot(projectedNormalVec, viewVec) / projectedNormalVecLength);

			// line 15 from the paper
			float n = signNorm * XeGTAO_FastACos(cosNorm);

			// this is a lower weight target; not using -1 as in the original paper because it is under horizon, so a 'weight' has different meaning based on the normal
			const float lowHorizonCos0 = cos(n + XE_GTAO_PI_HALF);
			const float lowHorizonCos1 = cos(n - XE_GTAO_PI_HALF);

			// lines 17, 18 from the paper, manually unrolled the 'side' loop
			float horizonCos0 = lowHorizonCos0; //-1;
			float horizonCos1 = lowHorizonCos1; //-1;

			[unroll]
			for (float step = 0.0; step < STEPS_PER_SLICE; step++) {
				// R1 sequence (http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/)
				const float stepBaseNoise = (slice + step * STEPS_PER_SLICE) * 0.6180339887498948482; // <- this should unroll
				float stepNoise = frac(noiseSample + stepBaseNoise);

				// approx line 20 from the paper, with added noise
				float s = (step + stepNoise) / STEPS_PER_SLICE; // + (lpfloat2)1e-6f);

				// additional distribution modifier
				s = pow(s, sampleDistributionPower);

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
				float SZ0 = sourceViewspaceDepth.SampleLevel(depthSampler, sampleScreenPos0, mipLevel).x;
				float3 samplePos0 = XeGTAO_ComputeViewspacePosition(sampleScreenPos0, SZ0);

				float2 sampleScreenPos1 = normalizedScreenPos - sampleOffset;
				float SZ1 = sourceViewspaceDepth.SampleLevel(depthSampler, sampleScreenPos1, mipLevel).x;
				float3 samplePos1 = XeGTAO_ComputeViewspacePosition(sampleScreenPos1, SZ1);

				float3 sampleDelta0 = samplePos0 - pixCenterPos; // using lpfloat for sampleDelta causes precision issues
				float3 sampleDelta1 = samplePos1 - pixCenterPos; // using lpfloat for sampleDelta causes precision issues
				float sampleDist0 = length(sampleDelta0);
				float sampleDist1 = length(sampleDelta1);

				// approx lines 23, 24 from the paper, unrolled
				float3 sampleHorizonVec0 = sampleDelta0 / sampleDist0;
				float3 sampleHorizonVec1 = sampleDelta1 / sampleDist1;

				// any sample out of radius should be discarded - also use fallof range for smooth transitions; this is a modified idea from "4.3 Implementation details, Bounding the sampling area"
				// this is our own thickness heuristic that relies on sooner discarding samples behind the center
				float falloffBase0 = length(float3(sampleDelta0.x, sampleDelta0.y, sampleDelta0.z * (1.0 + thinOccluderCompensation)));
				float falloffBase1 = length(float3(sampleDelta1.x, sampleDelta1.y, sampleDelta1.z * (1.0 + thinOccluderCompensation)));
				float weight0 = saturate(falloffBase0 * falloffMul + falloffAdd);
				float weight1 = saturate(falloffBase1 * falloffMul + falloffAdd);

				// sample horizon cos
				float shc0 = dot(sampleHorizonVec0, viewVec);
				float shc1 = dot(sampleHorizonVec1, viewVec);

				// discard unwanted samples
				shc0 = lerp(lowHorizonCos0, shc0, weight0); // this would be more correct but too expensive: cos(lerp( acos(lowHorizonCos0), acos(shc0), weight0 ));
				shc1 = lerp(lowHorizonCos1, shc1, weight1); // this would be more correct but too expensive: cos(lerp( acos(lowHorizonCos1), acos(shc1), weight1 ));

				// thickness heuristic - see "4.3 Implementation details, Height-field assumption considerations"
#if 0   // (disabled, not used) this should match the paper
				float newhorizonCos0 = max(horizonCos0, shc0);
				float newhorizonCos1 = max(horizonCos1, shc1);
				horizonCos0 = horizonCos0 > shc0 ? lerp(newhorizonCos0, shc0, thinOccluderCompensation) : newhorizonCos0;
				horizonCos1 = horizonCos1 > shc1 ? lerp(newhorizonCos1, shc1, thinOccluderCompensation) : newhorizonCos1;
#elif 0 // (disabled, not used) this is slightly different from the paper but cheaper and provides very similar results
				horizonCos0 = lerp(max(horizonCos0, shc0), shc0, thinOccluderCompensation);
				horizonCos1 = lerp(max(horizonCos1, shc1), shc1, thinOccluderCompensation);
#else   // this is a version where thicknessHeuristic is completely disabled
				horizonCos0 = max(horizonCos0, shc0);
				horizonCos1 = max(horizonCos1, shc1);
#endif

			}

#if 1       // I can't figure out the slight overdarkening on high slopes, so I'm adding this fudge - in the training set, 0.05 is close (PSNR 21.34) to disabled (PSNR 21.45)
			projectedNormalVecLength = lerp(projectedNormalVecLength, 1.0, 0.05);
#endif

			// line ~27, unrolled
			float h0 = -XeGTAO_FastACos(horizonCos1);
			float h1 = XeGTAO_FastACos(horizonCos0);
#if 0       // we can skip clamping for a tiny little bit more performance
			h0 = n + clamp(h0 - n, -XE_GTAO_PI_HALF, XE_GTAO_PI_HALF);
			h1 = n + clamp(h1 - n, -XE_GTAO_PI_HALF, XE_GTAO_PI_HALF);
#endif
			float iarc0 = (cosNorm + 2.0 * h0 * sin(n) - cos(2.0 * h0 - n)) / 4.0;
			float iarc1 = (cosNorm + 2.0 * h1 * sin(n) - cos(2.0 * h1 - n)) / 4.0;
			float localVisibility = projectedNormalVecLength * (iarc0 + iarc1);
			visibility += localVisibility;
		}
		visibility /= SLICE_COUNT;
		visibility = pow(visibility, FINAL_VALUE_POWER);
		visibility = max(0.03, visibility); // disallow total occlusion (which wouldn't make any sense anyhow since pixel is visible but also helps with packing bent normals)
	}

	outWorkingAOTerm = saturate(visibility / XE_GTAO_OCCLUSION_TERM_SCALE);
}

float4 XeGTAO_UnpackEdges(float _packedVal)
{
	uint packedVal = uint(_packedVal * 255.5);
	float4 edgesLRTB;
	edgesLRTB.x = float((packedVal >> 6) & 0x03) / 3.0; // there's really no need for mask (as it's an 8 bit input) but I'll leave it in so it doesn't cause any trouble in the future
	edgesLRTB.y = float((packedVal >> 4) & 0x03) / 3.0;
	edgesLRTB.z = float((packedVal >> 2) & 0x03) / 3.0;
	edgesLRTB.w = float((packedVal >> 0) & 0x03) / 3.0;

	return saturate(edgesLRTB);
}

void XeGTAO_AddSample(float ssaoValue, float edgeValue, inout float sum, inout float sumWeight)
{
	float weight = edgeValue;

	sum += weight * ssaoValue;
	sumWeight += weight;
}

void XeGTAO_Denoise(int2 pixCoordBase, Texture2D<float> sourceAOTerm, Texture2D<float> sourceEdges, out float aoTerm, const uniform bool finalApply)
{
	const float blurAmount = finalApply ? DENOISE_BLUR_BETA : DENOISE_BLUR_BETA / 5.0;
	const float diagWeight = 0.85 * 0.5;

	float4 edgesC_LRTB = XeGTAO_UnpackEdges(sourceEdges.Load(int3(pixCoordBase, 0)));
	float4 edgesL_LRTB = XeGTAO_UnpackEdges(sourceEdges.Load(int3(pixCoordBase + int2(-1, 0), 0)));
	float4 edgesR_LRTB = XeGTAO_UnpackEdges(sourceEdges.Load(int3(pixCoordBase + int2(1, 0), 0)));
	float4 edgesT_LRTB = XeGTAO_UnpackEdges(sourceEdges.Load(int3(pixCoordBase + int2(0, -1), 0)));
	float4 edgesB_LRTB = XeGTAO_UnpackEdges(sourceEdges.Load(int3(pixCoordBase + int2(0, 1), 0)));

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

	float ssaoValue = sourceAOTerm.Load(int3(pixCoordBase, 0));
	float ssaoValueL = sourceAOTerm.Load(int3(pixCoordBase + int2(-1, 0), 0));
	float ssaoValueT = sourceAOTerm.Load(int3(pixCoordBase + int2(0, -1), 0));
	float ssaoValueR = sourceAOTerm.Load(int3(pixCoordBase + int2(1, 0), 0));
	float ssaoValueB = sourceAOTerm.Load(int3(pixCoordBase + int2(0, 1), 0));
	float ssaoValueTL = sourceAOTerm.Load(int3(pixCoordBase + int2(-1, -1), 0));
	float ssaoValueBR = sourceAOTerm.Load(int3(pixCoordBase + int2(1, 1), 0));
	float ssaoValueTR = sourceAOTerm.Load(int3(pixCoordBase + int2(1, -1), 0));
	float ssaoValueBL = sourceAOTerm.Load(int3(pixCoordBase + int2(-1, 1), 0));

	float sumWeight = blurAmount;
	float sum = ssaoValue * sumWeight;

	XeGTAO_AddSample(ssaoValueL, edgesC_LRTB.x, sum, sumWeight);
	XeGTAO_AddSample(ssaoValueR, edgesC_LRTB.y, sum, sumWeight);
	XeGTAO_AddSample(ssaoValueT, edgesC_LRTB.z, sum, sumWeight);
	XeGTAO_AddSample(ssaoValueB, edgesC_LRTB.w, sum, sumWeight);

	XeGTAO_AddSample(ssaoValueTL, weightTL, sum, sumWeight);
	XeGTAO_AddSample(ssaoValueTR, weightTR, sum, sumWeight);
	XeGTAO_AddSample(ssaoValueBL, weightBL, sum, sumWeight);
	XeGTAO_AddSample(ssaoValueBR, weightBR, sum, sumWeight);

	aoTerm = sum / sumWeight;

	aoTerm *= finalApply ? XE_GTAO_OCCLUSION_TERM_SCALE : 1.0;
}

#endif // __XE_GTAO_HLSLI__