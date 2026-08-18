Texture2D<float4> t0 : register(t0);

SamplerState s0_s : register(s0);

cbuffer cb2 : register(b2)
{
  float4 cb2[1];
}

cbuffer cb12 : register(b12)
{
  float4 cb12[45];
}

// 3Dmigoto declarations
#define cmp -

void main(
  float4 v0 : SV_POSITION0,
  float2 v1 : TEXCOORD0,
  out float o0 : SV_Target0)
{
  float4 r0,r1;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = cb12[43].xy * v1.xy;
  r0.xy = max(float2(0,0), r0.xy);
  r1.x = min(cb12[44].z, r0.x);
  r1.y = min(cb12[43].y, r0.y);
  r0.x = t0.Sample(s0_s, r1.xy).x;
  r0.x = cb2[0].y * r0.x + cb2[0].z;
  r0.x = cb2[0].x / r0.x;
  r0.x = max(0, r0.x);
  o0.x = min(3.402823466e+38, r0.x); // Originally, o0.x = min(cb2[0].w, r0.x);
  return;
}