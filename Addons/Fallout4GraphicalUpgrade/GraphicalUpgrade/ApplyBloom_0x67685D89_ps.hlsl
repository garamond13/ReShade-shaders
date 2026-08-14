Texture2D<float4> t3 : register(t3);
Texture2D<float4> t2 : register(t2);
Texture2D<float4> t1 : register(t1);
Texture2D<float4> t0 : register(t0);

SamplerState s3_s : register(s3);
SamplerState s2_s : register(s2);
SamplerState s1_s : register(s1);
SamplerState s0_s : register(s0);

cbuffer cb2 : register(b2)
{
  float4 cb2[5];
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

  r0.xyz = t0.Sample(s0_s, v1.xy).xyz;
  r0.w = t3.Sample(s3_s, v1.xy).w;
  r0.w = r0.w * 255 + -4;
  r0.w = cmp(abs(r0.w) < 0.25);
  if (r0.w != 0) {
    o0.xyz = r0.xyz;
    o0.w = 1;
    return;
  }
  r1.xy = cb2[4].xy * v1.xy; // cb2[4].xy should be 1 here.
  r1.xyz = t1.Sample(s1_s, r1.xy).xyz * BLOOM_INTENSITY;
  r0.w = t2.Sample(s2_s, v1.xy).x;
  r1.w = 0.00100000005 + r0.w;
  r1.w = cb2[1].z / r1.w;
  r2.x = cmp(r1.w < cb2[1].y);
  r1.w = r2.x ? cb2[1].y : r1.w;
  r2.x = cmp(cb2[1].x < r1.w);
  r1.w = r2.x ? cb2[1].x : r1.w;
  r0.xyz = r1.xyz + r0.xyz;
  r0.xyz = r0.xyz * r1.www;
  r1.xyz = r0.xyz + r0.xyz;
  r2.xyz = r0.xyz * float3(0.300000012,0.300000012,0.300000012) + float3(0.0500000007,0.0500000007,0.0500000007);
  r3.xy = float2(0.200000003,3.33333325) * cb2[1].ww;
  r2.xyz = r1.xyz * r2.xyz + r3.xxx;
  r0.xyz = r0.xyz * float3(0.300000012,0.300000012,0.300000012) + float3(0.5,0.5,0.5);
  r0.xyz = r1.xyz * r0.xyz + float3(0.0600000024,0.0600000024,0.0600000024);
  r0.xyz = r2.xyz / r0.xyz;
  r0.xyz = -cb2[1].www * float3(3.33333325,3.33333325,3.33333325) + r0.xyz;
  r1.x = cb2[1].w * 0.200000003 + 19.3759995;
  r1.x = r1.x * 0.0408563502 + -r3.y;
  r1.x = 1 / r1.x;
  r1.xyz = r1.xxx * r0.xyz;
  r0.x = dot(r1.xyz, float3(0.212500006,0.715399981,0.0720999986));
  r1.w = 0;
  r1.xyzw = r1.xyzw + -r0.xxxx;
  r1.xyzw = cb2[2].xxxx * r1.xyzw + r0.xxxx;
  r2.xyzw = r0.xxxx * cb2[3].xyzw + -r1.xyzw;
  r1.xyzw = cb2[3].wwww * r2.xyzw + r1.xyzw;
  r1.xyzw = cb2[2].wwww * r1.xyzw + -r0.wwww;
  o0.xyzw = cb2[2].zzzz * r1.xyzw + r0.wwww;
  return;
}