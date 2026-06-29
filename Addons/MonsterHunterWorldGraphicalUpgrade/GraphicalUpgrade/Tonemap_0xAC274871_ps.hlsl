#include "Include/Common.hlsli"

cbuffer CBSystem : register(b3)
{
  uint4 iRegion : packoffset(c0);
  float fSourceMipLevel : packoffset(c1) = {0};
  float fGammaCorrect : packoffset(c1.y) = {0.454545468};
  float fGammaCorrectEx : packoffset(c1.z) = {1};
}

SamplerState SSSystemCopy_s : register(s0);
Texture2D<float4> tBaseMap : register(t0);

#ifndef GAMMA
#define GAMMA 2.2
#endif

// 3Dmigoto declarations
#define cmp -

void main(
  float4 v0 : SV_POSITION0,
  float2 v1 : TexCoord0,
  out float4 o0 : SV_TARGET0)
{
  float4 r0,r1,r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyz = tBaseMap.SampleLevel(SSSystemCopy_s, v1.xy, fSourceMipLevel).xyz;
  r0.xyz = saturate(r0.xyz);
  r0.w = fGammaCorrectEx * 0.0500000007 + 0.949999988;
  r0.w = min(1, r0.w);
  r0.xyz = -r0.xyz * r0.www + float3(1,1,1);
  r0.xyz = log2(r0.xyz);
  r0.xyz = fGammaCorrectEx * r0.xyz;
  r0.xyz = exp2(r0.xyz);
  r0.xyz = float3(1,1,1) + -r0.xyz;

  // Original linear to sRGB.
  //r1.xyz = log2(r0.xyz);
  //r1.xyz = float3(0.416666657,0.416666657,0.416666657) * r1.xyz;
  //r1.xyz = exp2(r1.xyz);
  //r1.xyz = r1.xyz * float3(1.05499995,1.05499995,1.05499995) + float3(-0.0549999997,-0.0549999997,-0.0549999997);
  //r2.xyz = cmp(float3(0.00313080009,0.00313080009,0.00313080009) >= r0.xyz);
  //r0.xyz = float3(12.9200001,12.9200001,12.9200001) * r0.xyz;
  //o0.xyz = r2.xyz ? r0.xyz : r1.xyz;

  #ifdef SRGB
  o0.xyz = linear_to_srgb(r0.xyz);
  #else
  o0.xyz = linear_to_gamma(r0.xyz, GAMMA);
  #endif

  o0.w = 1;
  return;
}