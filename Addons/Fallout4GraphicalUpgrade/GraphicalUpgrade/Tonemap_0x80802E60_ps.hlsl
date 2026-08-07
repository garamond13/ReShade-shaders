#include "Include/Common.hlsli"

Texture3D<float4> t6 : register(t6); // 16x16x16 LUT
Texture3D<float4> t5 : register(t5); // 16x16x16 LUT
Texture3D<float4> t4 : register(t4); // 16x16x16 LUT
Texture3D<float4> t3 : register(t3); // 16x16x16 LUT
Texture2D<float4> t0 : register(t0); // Scene, linear color space

SamplerState s6_s : register(s6);
SamplerState s5_s : register(s5);
SamplerState s4_s : register(s4);
SamplerState s3_s : register(s3);
SamplerState s0_s : register(s0);

cbuffer cb2 : register(b2)
{
  float4 cb2[2];
}

// 3Dmigoto declarations
#define cmp -

void main(
  float4 v0 : SV_POSITION0,
  float2 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = t0.Sample(s0_s, v1.xy).xyzw;

  // Saturate immediately color for tetrahedral LUT sampling
  // cause otherwise we get strange color artifacts sometimes (fully saturated color patches).
  r0.xyz = saturate(r0.xyz);

  // Linear to 2.2 gamma.
  r0.xyz = log2(r0.xyz);
  o0.w = r0.w;
  r0.xyz = float3(0.454545468,0.454545468,0.454545468) * r0.xyz;
  r0.xyz = exp2(r0.xyz);

  // Original with trilinear LUT sampling.
  //r0.xyz = r0.xyz * float3(0.9375,0.9375,0.9375) + float3(0.03125,0.03125,0.03125);
  //r1.xyz = t4.Sample(s4_s, r0.xyz).xyz;
  //r1.xyz = cb2[1].yyy * r1.xyz;
  //r2.xyz = t3.Sample(s3_s, r0.xyz).xyz;
  //r1.xyz = r2.xyz * cb2[1].xxx + r1.xyz;
  //r2.xyz = t5.Sample(s5_s, r0.xyz).xyz;
  //r0.xyz = t6.Sample(s6_s, r0.xyz).xyz;
  //r1.xyz = r2.xyz * cb2[1].zzz + r1.xyz;
  //o0.xyz = r0.xyz * cb2[1].www + r1.xyz;

  // With tetrahedral LUT sampling.
  r1.xyz = sample_tetrahedral(t4, r0.xyz, 16).xyz;
  r1.xyz = cb2[1].yyy * r1.xyz;
  r2.xyz = sample_tetrahedral(t3, r0.xyz, 16).xyz;
  r1.xyz = r2.xyz * cb2[1].xxx + r1.xyz;
  r2.xyz = sample_tetrahedral(t5, r0.xyz, 16).xyz;
  r0.xyz = sample_tetrahedral(t6, r0.xyz, 16).xyz;
  r1.xyz = r2.xyz * cb2[1].zzz + r1.xyz;
  o0.xyz = r0.xyz * cb2[1].www + r1.xyz;

  return;
}