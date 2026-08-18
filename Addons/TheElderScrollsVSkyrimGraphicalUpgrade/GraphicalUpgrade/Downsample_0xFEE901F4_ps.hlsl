// This is always used for 4x downscale?

Texture2D<float4> t0 : register(t0);
SamplerState s0_s : register(s0);

cbuffer cb2 : register(b2)
{
  float4 cb2[12];
}

cbuffer cb12 : register(b12)
{
  float4 cb12[45];
}

// 3Dmigoto declarations
#define cmp -

#if 0 // Original shader
void main(
  float4 v0 : SV_POSITION0,
  float2 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2,r3;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.x = cmp(0.5 < cb2[0].x);
  r0.yzw = float3(0,0,0);
  r1.x = 0;
  while (true) {
    r1.y = cmp((int)r1.x >= 4);
    if (r1.y != 0) break;
    r1.yz = cb2[r1.x+7].xy * cb2[6].xy + v1.xy;
    if (r0.x != 0) {
      r2.xy = cb12[43].xy * r1.yz;
      r2.xy = max(float2(0,0), r2.xy);
      r3.x = min(cb12[44].z, r2.x);
      r3.y = min(cb12[43].y, r2.y);
      r2.xyz = t0.Sample(s0_s, r3.xy).xyz;
    } else {
      r2.xyz = t0.Sample(s0_s, r1.yz).xyz;
    }
    r0.yzw = r2.xyz * cb2[r1.x+7].zzz + r0.yzw;
    r1.x = (int)r1.x + 1;
  }
  o0.xyz = r0.yzw;
  o0.w = cb2[6].z;
  return;
}
#else // 4x box downsample in 16 bilinear texture fetches.
void main(float4 v0 : SV_POSITION0, float2 v1 : TEXCOORD0, out float4 o0 : SV_Target0)
{
  float x, y;
  t0.GetDimensions(x, y);
  float2 t0_inv_dims = 1.0 / float2(x, y);

  float3 csum = 0.0;
  float3 color;

  color = t0.Sample(s0_s, v1 + float2(-3.0, -3.0) * t0_inv_dims).xyz;
  color = isfinite(color) ? color : 0;
  csum += color;
  color = t0.Sample(s0_s, v1 + float2(-3.0, -1.0) * t0_inv_dims).xyz;
  color = isfinite(color) ? color : 0;
  csum += color;
  color = t0.Sample(s0_s, v1 + float2(-3.0, 1.0) * t0_inv_dims).xyz;
  color = isfinite(color) ? color : 0;
  csum += color;
  color = t0.Sample(s0_s, v1 + float2(-3.0, 3.0) * t0_inv_dims).xyz;
  color = isfinite(color) ? color : 0;
  csum += color;
  color = t0.Sample(s0_s, v1 + float2(-1.0, -3.0) * t0_inv_dims).xyz;
  color = isfinite(color) ? color : 0;
  csum += color;
  color = t0.Sample(s0_s, v1 + float2(-1.0, -1.0) * t0_inv_dims).xyz;
  color = isfinite(color) ? color : 0;
  csum += color;
  color = t0.Sample(s0_s, v1 + float2(-1.0, 1.0) * t0_inv_dims).xyz;
  color = isfinite(color) ? color : 0;
  csum += color;
  color = t0.Sample(s0_s, v1 + float2(-1.0, 3.0) * t0_inv_dims).xyz;
  color = isfinite(color) ? color : 0;
  csum += color;
  color = t0.Sample(s0_s, v1 + float2(1.0, -3.0) * t0_inv_dims).xyz;
  color = isfinite(color) ? color : 0;
  csum += color;
  color = t0.Sample(s0_s, v1 + float2(1.0, -1.0) * t0_inv_dims).xyz;
  color = isfinite(color) ? color : 0;
  csum += color;
  color = t0.Sample(s0_s, v1 + float2(1.0, 1.0) * t0_inv_dims).xyz;
  color = isfinite(color) ? color : 0;
  csum += color;
  color = t0.Sample(s0_s, v1 + float2(1.0, 3.0) * t0_inv_dims).xyz;
  color = isfinite(color) ? color : 0;
  csum += color;
  color = t0.Sample(s0_s, v1 + float2(3.0, -3.0) * t0_inv_dims).xyz;
  color = isfinite(color) ? color : 0;
  csum += color;
  color = t0.Sample(s0_s, v1 + float2(3.0, -1.0) * t0_inv_dims).xyz;
  color = isfinite(color) ? color : 0;
  csum += color;
  color = t0.Sample(s0_s, v1 + float2(3.0, 1.0) * t0_inv_dims).xyz;
  color = isfinite(color) ? color : 0;
  csum += color;
  color = t0.Sample(s0_s, v1 + float2(3.0, 3.0) * t0_inv_dims).xyz;
  color = isfinite(color) ? color : 0;
  csum += color;

  o0.xyz = csum / 16.0;
  o0.w = cb2[7].z;
}
#endif