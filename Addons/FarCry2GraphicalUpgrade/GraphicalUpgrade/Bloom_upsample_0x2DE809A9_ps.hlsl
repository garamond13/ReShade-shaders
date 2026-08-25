#include "Include/Common.hlsli"

SamplerState BloomSampler0_s : register(s0);
SamplerState BloomSampler1_s : register(s1);
SamplerState BloomSampler2_s : register(s2);
SamplerState BloomSampler3_s : register(s3);
Texture2D<float4> BloomSampler0 : register(t0);
Texture2D<float4> BloomSampler1 : register(t1);
Texture2D<float4> BloomSampler2 : register(t2);
Texture2D<float4> BloomSampler3 : register(t3);

// 3Dmigoto declarations
#define cmp -

#if 0 // Original shader (run once).
void main(
  float4 v0 : SV_Position0,
  linear centroid float2 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0 = BloomSampler0.Sample(BloomSampler0_s, v1.xy);
  r1 = BloomSampler1.Sample(BloomSampler1_s, v1.xy);
  r0.xyzw = r1.xyzw + r0.xyzw;
  r1 = BloomSampler2.Sample(BloomSampler2_s, v1.xy);
  r0.xyzw = r1.xyzw + r0.xyzw;
  r1 = BloomSampler3.Sample(BloomSampler3_s, v1.xy);
  o0.xyzw = r1.xyzw + r0.xyzw;
  return;
}
#else // We are doing progressive mip upsampling and accumulation.
void main(
  float4 v0 : SV_Position0,
  linear centroid float2 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1;
  uint4 bitmask, uiDest;
  float4 fDest;

  o0 = sample_bicubic(BloomSampler0, BloomSampler0_s, v1.xy);
}
#endif