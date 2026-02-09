#include "Color.hlsli"

cbuffer _Globals : register(b0)
{
  float4 fogColor : packoffset(c0);
  float3 fogTransform : packoffset(c1);
  float4x3 screenDataToCamera : packoffset(c2);
  float globalScale : packoffset(c5);
  float sceneDepthAlphaMask : packoffset(c5.y);
  float globalOpacity : packoffset(c5.z);
  float distortionBufferScale : packoffset(c5.w);
  float2 wToZScaleAndBias : packoffset(c6);
  float4 screenTransform[2] : packoffset(c7);
  float4 textureToPixel : packoffset(c9);
  float4 pixelToTexture : packoffset(c10);
  float maxScale : packoffset(c11);
  float bloomAlpha : packoffset(c11.y);
  float sceneBias : packoffset(c11.z);
  float exposure : packoffset(c11.w);
  float deltaExposure : packoffset(c12);
  float4 ColorFill : packoffset(c13);
}

SamplerState s_framebuffer_s : register(s0);
SamplerState s_bloom_s : register(s1);
Texture2D s_framebuffer : register(t0);
Texture2D s_bloom : register(t1);

#ifndef BLOOM_INTENSITY
#define BLOOM_INTENSITY 1.0
#endif

// 3Dmigoto declarations
#define cmp -

#if 0 // The original shader.
void main(
  float4 v0 : SV_Position0,
  float2 v1 : TEXCOORD0,
  float2 w1 : TEXCOORD1,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0 = s_bloom.Sample(s_bloom_s, w1.xy);
  r1 = s_framebuffer.Sample(s_framebuffer_s, v1.xy);
  r0.xyz = r0.xyz * bloomAlpha * BLOOM_INTENSITY + r1.xyz;
  o0.w = r1.w;
  r0.xyz = saturate(sceneBias * r0.xyz);

  // Original delinearization, gamma 2.2.
  r0.xyz = log2(r0.xyz);
  r0.xyz = float3(0.454545468,0.454545468,0.454545468) * r0.xyz;
  o0.xyz = exp2(r0.xyz);

  return;
}
#else // We will need both linear and delinearized output.
void main(float4 v0 : SV_Position0, float2 v1 : TEXCOORD0, float2 w1 : TEXCOORD1, out float4 o0 : SV_Target0, out float4 o1 : SV_Target1)
{
  float4 r0,r1;

  r0 = s_bloom.Sample(s_bloom_s, w1.xy);
  r1 = s_framebuffer.Sample(s_framebuffer_s, v1.xy);
  r0.xyz = r0.xyz * bloomAlpha * BLOOM_INTENSITY + r1.xyz;
  o0.w = r1.w;
  r0.xyz = saturate(sceneBias * r0.xyz);

  // Linear output.
  o0.xyz = r0.xyz;

  // Delinearization.
  #ifdef SRGB
  r0.xyz = linear_to_srgb(r0.xyz);
  #else
  r0.xyz = linear_to_gamma(r0.xyz);
  #endif

  // Delinearized output.
  o1 = float4(r0.xyz, r1.w);
}
#endif