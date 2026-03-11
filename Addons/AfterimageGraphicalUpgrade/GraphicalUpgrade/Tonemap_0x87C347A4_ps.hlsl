#include "Color.hlsli"

Texture3D<float4> t2 : register(t2); // LUT 32x32x32
Texture2D<float4> t1 : register(t1); // bloom
Texture2D<float4> t0 : register(t0); // scene
SamplerState s2_s : register(s2);
SamplerState s1_s : register(s1);
SamplerState s0_s : register(s0);

cbuffer cb1 : register(b1)
{
  float4 cb1[136];
}

cbuffer cb0 : register(b0)
{
  float4 cb0[66];
}

#ifndef BLOOM_INTENSITY
#define BLOOM_INTENSITY 1.0
#endif

// 3Dmigoto declarations
#define cmp -

void main(
  linear noperspective float2 v0 : TEXCOORD0,
  linear noperspective float2 w0 : TEXCOORD3,
  linear noperspective float4 v1 : TEXCOORD1,
  linear noperspective float4 v2 : TEXCOORD2,
  float2 v3 : TEXCOORD4,
  float4 v4 : SV_POSITION0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  #if 0 // Original dithering.
  r0.x = v2.w * 543.309998 + v2.z;
  r0.x = sin(r0.x);
  r0.x = 493013 * r0.x;
  r0.x = frac(r0.x);
  #endif

  r1.xyzw = t0.Sample(s0_s, v0.xy).xyzw;
  r0.yzw = cb1[135].zzz * r1.xyz;
  r2.xy = cb0[58].zw * v0.xy + cb0[59].xy;
  r2.xy = max(cb0[50].zw, r2.xy);
  r2.xy = min(cb0[51].xy, r2.xy);
  r2.xyz = t1.Sample(s1_s, r2.xy).xyz;
  r2.xyz = cb1[135].zzz * r2.xyz;
  r2.xyz = cb0[61].xyz * r2.xyz;
  r0.yzw = r0.yzw * cb0[60].xyz + r2.xyz * BLOOM_INTENSITY;
  r0.yzw = r0.yzw * v1.xxx + float3(0.00266771927,0.00266771927,0.00266771927);
  r0.yzw = log2(r0.yzw);
  r0.yzw = saturate(r0.yzw * float3(0.0714285746,0.0714285746,0.0714285746) + float3(0.610726953,0.610726953,0.610726953));

  // Original trilinear LUT sampling.
  //r0.yzw = r0.yzw * float3(0.96875,0.96875,0.96875) + float3(0.015625,0.015625,0.015625); // color * scale + offset
  //r0.yzw = t2.Sample(s2_s, r0.yzw).xyz;

  // Tetrahedral LUT sampling.
  r0.yzw = sample_tetrahedral(r0.yzw, t2, 32);

  #if 0 // Original dithering.
  r0.x = r0.x * 0.00390625 + -0.001953125; // 8bit
  r1.xyz = r0.yzw * float3(1.04999995,1.04999995,1.04999995) + r0.xxx; // color * scale + dither
  #else
  r1.xyz = r0.yzw * float3(1.04999995,1.04999995,1.04999995);
  #endif

  // This never runs?
  if (cb0[65].x != 0) {
    r0.xyz = log2(r1.xyz);
    r0.xyz = float3(0.0126833133,0.0126833133,0.0126833133) * r0.xyz;
    r0.xyz = exp2(r0.xyz);
    r2.xyz = float3(-0.8359375,-0.8359375,-0.8359375) + r0.xyz;
    r2.xyz = max(float3(0,0,0), r2.xyz);
    r0.xyz = -r0.xyz * float3(18.6875,18.6875,18.6875) + float3(18.8515625,18.8515625,18.8515625);
    r0.xyz = r2.xyz / r0.xyz;
    r0.xyz = log2(r0.xyz);
    r0.xyz = float3(6.27739477,6.27739477,6.27739477) * r0.xyz;
    r0.xyz = exp2(r0.xyz);
    r0.xyz = float3(10000,10000,10000) * r0.xyz;
    r0.xyz = r0.xyz / cb0[64].www;

    // Linear to sRGB.
    r0.xyz = max(float3(6.10351999e-005,6.10351999e-005,6.10351999e-005), r0.xyz);
    r2.xyz = float3(12.9200001,12.9200001,12.9200001) * r0.xyz;
    r0.xyz = max(float3(0.00313066994,0.00313066994,0.00313066994), r0.xyz);
    r0.xyz = log2(r0.xyz);
    r0.xyz = float3(0.416666657,0.416666657,0.416666657) * r0.xyz;
    r0.xyz = exp2(r0.xyz);
    r0.xyz = r0.xyz * float3(1.05499995,1.05499995,1.05499995) + float3(-0.0549999997,-0.0549999997,-0.0549999997);
    r1.xyz = min(r2.xyz, r0.xyz);
  }

  // Original out.
  //o0.xyzw = r1.xyzw;

  // At this point r1 should be in sRGB.
  r1.xyz = srgb_to_linear(r1.xyz);
  o0.xyzw = r1.xyzw;

  return;
}