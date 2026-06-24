#include "Include/Common.hlsli"

cbuffer CBuffer_Data : register(b0)
{
  float2 TexCoordScale0 : packoffset(c0);
  float2 MaxTexCoord0 : packoffset(c0.z);
  float2 Gamma : packoffset(c1);
}

SamplerState TextureSampler_s : register(s0);
Texture2D<float4> SourceBuffer0 : register(t0); // Scene.
Texture2D<float4> SourceBuffer1 : register(t1); // UI, viewed as unorm_srgb.

// 3Dmigoto declarations
#define cmp -

#ifndef GAMMA
#define GAMMA 2.2
#endif

void main(
  float4 v0 : SV_POSITION0,
  float2 v1 : TEXCOORD0,
  out float4 o0 : SV_TARGET0)
{
  float4 r0,r1,r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = SourceBuffer1.SampleLevel(TextureSampler_s, v1.xy, 0).xyzw;
  r0.xyz = log2(r0.xyz);
  r0.w = 1 + -r0.w;
  r0.xyz = Gamma.xxx * r0.xyz;

  // Original linear to sRGB.
  //r1.xyz = float3(0.416666657,0.416666657,0.416666657) * r0.xyz;
  r0.xyz = exp2(r0.xyz);
  //r1.xyz = exp2(r1.xyz);
  //r1.xyz = r1.xyz * float3(1.05499995,1.05499995,1.05499995) + float3(-0.0549999997,-0.0549999997,-0.0549999997);
  //r2.xyz = cmp(r0.xyz < float3(0.00313080009,0.00313080009,0.00313080009));
  //r0.xyz = float3(12.9200001,12.9200001,12.9200001) * r0.xyz;
  //r0.xyz = r2.xyz ? r0.xyz : r1.xyz;

  r1.xy = TexCoordScale0.xy * v1.xy;
  r1.xy = min(MaxTexCoord0.xy, r1.xy);
  r1.xyz = SourceBuffer0.SampleLevel(TextureSampler_s, r1.xy, 0).xyz;

  // Original out.
  //o0.xyz = r1.xyz * r0.www + r0.xyz;

  r0.xyz = r1.xyz * r0.www + r0.xyz;

  #ifdef SRGB
  r0.xyz = linear_to_srgb(r0.xyz);
  #else
  r0.xyz = linear_to_gamma(r0.xyz, GAMMA);
  #endif

  o0.xyz = r0.xyz;

  o0.w = 1;
  return;
}