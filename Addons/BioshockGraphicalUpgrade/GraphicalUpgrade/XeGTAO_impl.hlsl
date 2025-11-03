#define RADIUS_MULTIPLIER 1.59
#define EFFECT_FALLOFF_RANGE 0.8
#define CLIP_NEAR 0.1
#define CLIP_FAR 100.0
#include "XeGTAO.hlsli"

Texture2D<float> tex0 : register(t0);
Texture2D<float> tex1 : register(t1);
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
	#ifdef XE_GTAO_HILBERT_LUT_AVAILABLE // load from lookup texture...
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
	XeGTAO_PrefilterDepths_mip0(pos.xy, tex0, out_working_depth_mip0);
}

// For mips 1 to 4.
void prefilter_depths_ps(float4 pos : SV_Position, float2 texcoord : TEXCOORD, out float out_working_depth : SV_Target)
{
	// tex0 = out_working_depth_mip[N]
	XeGTAO_PrefilterDepths(texcoord, tex0, smp, out_working_depth);
}

void main_pass_ps(float4 pos : SV_Position, out float out_working_ao_term : SV_Target0, out float out_working_edges : SV_Target1)
{
	// tex0 = g_srcWorkingDepth
	// smp = g_samplerPointClamp
	XeGTAO_MainPass(pos.xy, SpatioTemporalNoise(pos.xy, 0), tex0, smp, out_working_ao_term, out_working_edges);
}

float4 denoise_pass_ps(float4 pos : SV_Position) : SV_Target
{
	float final_ao_term;

	// tex0 = g_srcWorkingAOTerm
	// tex1 = g_srcWorkingEdges
	XeGTAO_Denoise(pos.xy, tex0, tex1, final_ao_term, true);

	return float4(final_ao_term.xxx, 1.0);
}