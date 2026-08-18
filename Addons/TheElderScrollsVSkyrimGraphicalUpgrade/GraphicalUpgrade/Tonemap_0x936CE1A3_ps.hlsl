#include "Include/Common.hlsli"

Texture2D<float4> t2 : register(t2);
Texture2D<float4> t1 : register(t1);
Texture2D<float4> t0 : register(t0);

SamplerState s2_s : register(s2);
SamplerState s1_s : register(s1);
SamplerState s0_s : register(s0);

cbuffer cb2 : register(b2)
{
  float4 cb2[5];
}

cbuffer cb12 : register(b12)
{
  float4 cb12[45];
}

// 3Dmigoto declarations
#define cmp -

#ifndef BLOOM_INTENSITY
#define BLOOM_INTENSITY 1.0
#endif

void main(
  float4 v0 : SV_POSITION0,
  float2 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2,r3;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = cb12[43].xy * v1.xy;
  r0.xy = max(float2(0,0), r0.xy);
  r1.x = min(cb12[44].z, r0.x);
  r1.y = min(cb12[43].y, r0.y);
  r0.xyz = t1.Sample(s1_s, r1.xy).xyz;
  r0.w = cmp(0.5 < cb2[0].x);
  if (r0.w != 0) {
    r1.xyz = t0.Sample(s0_s, r1.xy).xyz;
  } else {
    r1.xyz = t0.Sample(s0_s, v1.xy).xyz;
  }

  // Added, convert bloom to sRGB and apply intensity.
  r1.xyz = linear_to_srgb(r1.xyz * BLOOM_INTENSITY);

  r2.xy = t2.Sample(s2_s, v1.xy).xy;
  r0.w = dot(float3(0.212500006,0.715399981,0.0720999986), r0.xyz);
  r0.w = max(9.99999975e-006, r0.w);
  r1.w = r2.y / r2.x;
  r2.y = r1.w * r0.w;
  r2.z = cmp(0.5 < cb2[2].z);
  r3.xy = r1.ww * r0.ww + float2(-0.00400000019,1);
  r1.w = max(0, r3.x);
  r3.xz = r1.ww * float2(6.19999981,6.19999981) + float2(0.5,1.70000005);
  r2.w = r3.x * r1.w;
  r1.w = r1.w * r3.z + 0.0599999987;
  r1.w = r2.w / r1.w;
  r1.w = log2(r1.w);
  r1.w = 2.20000005 * r1.w;
  r1.w = exp2(r1.w);
  r1.w = cb2[2].y * r1.w;
  r2.w = r2.y * cb2[2].y + 1;
  r2.y = r2.y * r2.w;
  r2.y = r2.y / r3.y;
  r1.w = r2.z ? r1.w : r2.y;
  r0.w = r1.w / r0.w;
  r1.w = saturate(cb2[2].x + -r1.w);
  r1.xyz = r1.www * r1.xyz;
  r0.xyz = r0.xyz * r0.www + r1.xyz;
  r1.x = dot(r0.xyz, float3(0.212500006,0.715399981,0.0720999986));
  r0.w = 1;
  r0.xyzw = -r1.xxxx + r0.xyzw;
  r0.xyzw = cb2[3].xxxx * r0.xyzw + r1.xxxx;
  r1.xyzw = r1.xxxx * cb2[4].xyzw + -r0.xyzw;
  r0.xyzw = cb2[4].wwww * r1.xyzw + r0.xyzw;
  r0.xyzw = cb2[3].wwww * r0.xyzw + -r2.xxxx;
  r0.xyzw = cb2[3].zzzz * r0.xyzw + r2.xxxx;
  r0.xyz = saturate(r0.xyz);
  r0.xyz = log2(r0.xyz);
  r0.xyz = cb12[42].xxx * r0.xyz;
  o0.xyz = exp2(r0.xyz);
  o0.w = r0.w;
  return;
}