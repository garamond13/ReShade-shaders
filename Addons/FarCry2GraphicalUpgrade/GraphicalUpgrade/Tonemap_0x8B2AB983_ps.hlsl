#include "Include/Common.hlsli"

cbuffer _Globals : register(b0)
{
  float4 _UserClipPlane : packoffset(c0);
  float4 QuadParams : packoffset(c1);
  float2 TexCoordScale : packoffset(c2);
  float4 SampleOffsets[13] : packoffset(c3);
  float4 SampleWeights[13] : packoffset(c16);
  float Brightness : packoffset(c29);
  float2 BloomParams : packoffset(c29.y);
  float2 LuminanceBlendFactors : packoffset(c30);
  float2 LuminanceRange : packoffset(c30.z);
  float Saturation : packoffset(c31);
  float SinCityEffectIntensity : packoffset(c31.y);
  float3 ColorRemapData : packoffset(c32);
  float3 ContrastData : packoffset(c33);
  float2 LuminanceAdaptationRange : packoffset(c34);
  float2 VignetteTextureAverage : packoffset(c34.z);
}

SamplerState PointTextureSampler_s : register(s0);
SamplerState CombinedBloomSampler_s : register(s1);
SamplerState AdaptedLuminanceSampler_s : register(s2);
Texture2D<float4> PointTextureSampler : register(t0);
Texture2D<float4> CombinedBloomSampler : register(t1);
Texture2D<float4> AdaptedLuminanceSampler : register(t2);

#ifndef BLOOM_INTENSITY
#define BLOOM_INTENSITY 1.0
#endif

// 3Dmigoto declarations
#define cmp -

void main(
  float4 v0 : SV_Position0,
  linear centroid float4 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0 = AdaptedLuminanceSampler.Sample(AdaptedLuminanceSampler_s, float2(0.5,0.5));
  r0.y = max(LuminanceRange.x, r0.x);
  r0.y = min(LuminanceRange.y, r0.y);
  r0.x = r0.y / r0.x;
  r0.x = max(LuminanceAdaptationRange.x, r0.x);
  r0.x = min(LuminanceAdaptationRange.y, r0.x);
  r1 = PointTextureSampler.Sample(PointTextureSampler_s, v1.xy);
  r0.xyzw = r1.xyzz * r0.xxxx;
  r1.x = dot(r0.xyw, float3(0.298900008,0.587000012,0.114));
  r1.x = saturate(1 + -r1.x);
  r2 = CombinedBloomSampler.Sample(CombinedBloomSampler_s, v1.zw) * BLOOM_INTENSITY;
  r1.xyzw = r2.xyzz * r1.xxxx;
  r0.xyzw = saturate(r1.xyzw * BloomParams.xxxx + r0.xyzw);
  r0.xyzw = log2(r0.xyzw);
  r0.xyzw = ColorRemapData.xyzz * r0.xyzw;
  r0.xyzw = exp2(r0.xyzw);
  r1.xyzw = r0.xyww * ContrastData.xxxx + ContrastData.yyyy;
  r1.xyzw = r0.xyww * r1.xyzw + ContrastData.zzzz;
  r2.xyz = r1.xyw * r0.xyw;
  r2.x = dot(float3(0.308600008,0.609399974,0.0820000023), r2.xyz);
  r0.xyzw = r1.xyzw * r0.xyzw + -r2.xxxx;

  // Original output.
  //o0.xyzw = Saturation * r0.xyzw + r2.xxxx;

  // Linearized output.
  r1.xyzw = Saturation * r0.xyzw + r2.xxxx;
  r1.xyz = srgb_to_linear(r1.xyz);
  o0.xyzw = r1.xyzw;

  return;
}