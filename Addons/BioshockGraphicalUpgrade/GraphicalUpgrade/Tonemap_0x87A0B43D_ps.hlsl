#include "Include/SmoothSaturate.hlsli"
#include "Include/Sampling.hlsli"
#include "Include/Common.hlsli"
#include "Include/AMD_FFX_CAS.hlsli"
#include "Include/Noise.hlsli"
#include "Include/GraphicalUpgradeCB.hlsli.h"

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

#ifndef SHARPNESS
#define SHARPNESS 0.0
#endif

#ifndef AMOUNT
#define AMOUNT 0.0
#endif

// 3Dmigoto declarations
#define cmp -

void main(
  float4 v0 : SV_Position0,
  float2 v1 : TEXCOORD0,
  float2 w1 : TEXCOORD1,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1;
  uint4 bitmask, uiDest;
  float4 fDest;

  // Original linear bloom sampling.
  //r0 = s_bloom.Sample(s_bloom_s, w1.xy);

  // Bicubic bloom sampling.
  r0 = sample_bicubic(s_bloom, s_bloom_s, w1.xy);

  // Compiler should optimize out the branch.
  if (SHARPNESS <= 0.0) {
    r1 = s_framebuffer.Sample(s_framebuffer_s, v1.xy);
  }
  else {
    r1 = amd_ffx_cas(s_framebuffer, v0.xy, SHARPNESS, true);
  }

  r0.xyz = r0.xyz * bloomAlpha * BLOOM_INTENSITY + r1.xyz;
  o0.w = r1.w;

  // Original saturate.
  //r0.xyz = saturate(sceneBias * r0.xyz);

  // Smooth saturate tonemapping.
  r0.xyz = smooth_saturate(sceneBias * r0.xyz, 1.0, 0.18, 12.0, 1.18);

  // Original delinearization, gamma 2.2.
  //r0.xyz = log2(r0.xyz);
  //r0.xyz = float3(0.454545468,0.454545468,0.454545468) * r0.xyz;
  //o0.xyz = exp2(r0.xyz);

  r0.xyz = film_grain(r0.xyz, uint4(v0.xy, noise_index, 0u), AMOUNT);
  r0.xyz = linear_to_srgb(r0.xyz);
  r0.xyz = dither(r0.xyz, uint3(v0.xy, noise_index), 10);
  o0.xyz = r0.xyz;

  return;
}