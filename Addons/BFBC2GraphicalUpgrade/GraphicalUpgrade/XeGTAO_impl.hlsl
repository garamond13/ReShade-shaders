// XeGTAO implementation adopted for BFBC2
//
// For the reference see:  https://github.com/GameTechDev/XeGTAO

#define RADIUS_MULTIPLIER 1.59
#define EFFECT_FALLOFF_RANGE 0.8
#include "XeGTAO.hlsli"

Texture2D<float> tex0 : register(t0);
Texture2D<float> tex1 : register(t1);
RWTexture2D<unorm float> uav0 : register(u0);
RWTexture2D<unorm float> uav1 : register(u1);
RWTexture2D<float> out_working_depth_mip0 : register(u2);
RWTexture2D<float> out_working_depth_mip1 : register(u3);
RWTexture2D<float> out_working_depth_mip2 : register(u4);
RWTexture2D<float> out_working_depth_mip3 : register(u5);
RWTexture2D<float> out_working_depth_mip4 : register(u6);
SamplerState smp : register(s0);

#define XE_GTAO_NUMTHREADS_X 8
#define XE_GTAO_NUMTHREADS_Y 8

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
	// tex0 = linearized depth
	// smp = g_samplerPointClamp
	XeGTAO_PrefilterDepths16x16(dtid, gtid, tex0, smp, out_working_depth_mip0, out_working_depth_mip1, out_working_depth_mip2, out_working_depth_mip3, out_working_depth_mip4);
}

[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)]
void main_pass_cs(uint2 dtid : SV_DispatchThreadID)
{
	// tex0 = g_srcWorkingDepth
	// smp = g_samplerPointClamp
	// uav0 = g_outWorkingAOTerm
	// uav1 = g_outWorkingEdges
	XeGTAO_MainPass(dtid, SpatioTemporalNoise(dtid, 0), tex0, smp, uav0, uav1);
}

[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)]
void denoise_pass_cs(uint2 dtid : SV_DispatchThreadID)
{
	// tex0 = g_srcWorkingAOTerm
	// tex1 = g_srcWorkingEdges
	// smp = g_samplerPointClamp
	// uav0 = g_outFinalAOTerm
	const uint2 pix_coord_base = dtid * uint2(2, 1); // we're computing 2 horizontal pixels at a time (performance optimization)
	XeGTAO_Denoise(pix_coord_base, tex0, tex1, smp, uav0, true);
}

// Expects fullscreen triangle VS.
float4 load_ao_ps(float4 pos : SV_Position) : SV_Target
{
	return float4(tex0.Load(int3(pos.xy, 0)).xxx, 1.0);
}