// SMAA implementation
// See Demo for the reference https://github.com/iryoku/smaa

#define SMAA_PRESET_ULTRA
#define SMAA_CUSTOM_SL
SamplerState LinearSampler : register(s1);
SamplerState PointSampler : register(s0);
#define SMAATexture2D(tex) Texture2D tex
#define SMAATexturePass2D(tex) tex
#define SMAASampleLevelZero(tex, coord) tex.SampleLevel(LinearSampler, coord, 0)
#define SMAASampleLevelZeroPoint(tex, coord) tex.SampleLevel(PointSampler, coord, 0)
#define SMAASampleLevelZeroOffset(tex, coord, offset) tex.SampleLevel(LinearSampler, coord, 0, offset)
#define SMAASample(tex, coord) tex.Sample(LinearSampler, coord)
#define SMAASamplePoint(tex, coord) tex.Sample(PointSampler, coord)
#define SMAASampleOffset(tex, coord, offset) tex.Sample(LinearSampler, coord, offset)
#define SMAA_FLATTEN [flatten]
#define SMAA_BRANCH [branch]
#define SMAATexture2DMS2(tex) Texture2DMS<float4, 2> tex
#define SMAALoad(tex, pos, sample) tex.Load(pos, sample)
#define SMAAGather(tex, coord) tex.Gather(LinearSampler, coord, 0)
#include "SMAA.hlsli"

#include "FullscreenTriangle.hlsli"

Texture2D tex : register(t0);
Texture2D tex1 : register(t1);
Texture2D tex2 : register(t2);

// SMAAEdgeDetection
//

void smaa_edge_detection_vs(uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD0, out float4 offset[3] : TEXCOORD1)
{
	fullscreen_triangle(id, position, texcoord);
	SMAAEdgeDetectionVS(texcoord, offset);
}

float2 smaa_edge_detection_ps(float4 position : SV_Position, float2 texcoord : TEXCOORD0, float4 offset[3] : TEXCOORD1) : SV_Target
{
	// tex = colorTexGamma
	return SMAAColorEdgeDetectionPS(texcoord, offset, tex);
}

//

// SMAABlendingWeightCalculation
//

void smaa_blending_weight_calculation_vs(uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD0, out float2 pixcoord : TEXCOORD1, out float4 offset[3] : TEXCOORD2)
{
	fullscreen_triangle(id, position, texcoord);
	SMAABlendingWeightCalculationVS(texcoord, pixcoord, offset);
}

float4 smaa_blending_weight_calculation_ps(float4 position : SV_Position, float2 texcoord : TEXCOORD0, float2 pixcoord : TEXCOORD1, float4 offset[3] : TEXCOORD2) : SV_Target
{
	// tex = edgesTex
	// tex1 = areaTex
	// tex2 = searchTex
	return SMAABlendingWeightCalculationPS(texcoord, pixcoord, offset, tex, tex1, tex2, 0);
}

//

// SMAANeighborhoodBlending
//

void smaa_neighborhood_blending_vs(uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD0, out float4 offset : TEXCOORD1)
{
	fullscreen_triangle(id, position, texcoord);
	SMAANeighborhoodBlendingVS(texcoord, offset);
}

float4 smaa_neighborhood_blending_ps(float4 position : SV_Position, float2 texcoord : TEXCOORD0, float4 offset : TEXCOORD1) : SV_Target
{
	// tex = colorTex
	// tex1 = blendTex
	return SMAANeighborhoodBlendingPS(texcoord, offset, tex, tex1);
}

//