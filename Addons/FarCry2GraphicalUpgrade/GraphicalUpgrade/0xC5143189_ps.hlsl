cbuffer _Globals : register(b0)
{
  float4 _UserClipPlane : packoffset(c0);
  float4 QuadParams : packoffset(c1);
  float2 TexCoordScale : packoffset(c2);
  float4 SampleOffsets[13] : packoffset(c3);
  float4 SampleWeights[13] : packoffset(c16);
  float ps3RegisterCount : packoffset(c29);
  float Brightness : packoffset(c29.y);
  float2 BloomParams : packoffset(c29.z);
  float2 LuminanceBlendFactors : packoffset(c30);
  float2 LuminanceRange : packoffset(c30.z);
  float Saturation : packoffset(c31);
  float SinCityEffectIntensity : packoffset(c31.y);
  float3 ColorRemapData : packoffset(c32);
  float3 ContrastData : packoffset(c33);
  float2 LuminanceAdaptationRange : packoffset(c34);
  float2 VignetteTextureAverage : packoffset(c34.z);
}

SamplerState VignetteSampler_s : register(s0);
SamplerState LinearTextureSampler_s : register(s1);
SamplerState AdaptedLuminanceSampler_s : register(s2);
Texture2D LinearTextureSampler : register(t0);
Texture2D AdaptedLuminanceSampler : register(t1);
Texture2D VignetteSampler : register(t2);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  linear centroid float4 v1 : TEXCOORD0,
  linear centroid float4 v2 : TEXCOORD1,
  linear centroid float2 v3 : TEXCOORD2,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.x = AdaptedLuminanceSampler.Sample(AdaptedLuminanceSampler_s, float2(0.5,0.5)).x;
  r0.y = max(LuminanceRange.x, r0.x);
  r0.y = min(LuminanceRange.y, r0.y);
  r0.x = r0.y / r0.x;
  r0.x = max(LuminanceAdaptationRange.x, r0.x);
  r0.x = min(LuminanceAdaptationRange.y, r0.x);
  r1.xyz = LinearTextureSampler.Sample(LinearTextureSampler_s, v1.zw).xyz;
  r0.yzw = r1.xyz * r0.xxx;
  r1.x = dot(r1.xyz, float3(0.298900008,0.587000012,0.114));
  r1.y = dot(r0.yzw, float3(0.298900008,0.587000012,0.114));
  r1.z = -BloomParams.y + r1.y;
  r1.y = saturate(r1.z / r1.y);
  r0.yzw = r1.yyy * r0.yzw;
  r2.xyz = LinearTextureSampler.Sample(LinearTextureSampler_s, v1.xy).xyz;
  r1.yzw = r2.xyz * r0.xxx;
  r2.x = dot(r2.xyz, float3(0.298900008,0.587000012,0.114));
  r1.x = r2.x + r1.x;
  r2.x = dot(r1.yzw, float3(0.298900008,0.587000012,0.114));
  r2.y = -BloomParams.y + r2.x;
  r2.x = saturate(r2.y / r2.x);
  r0.yzw = r1.yzw * r2.xxx + r0.yzw;
  r2.xyz = LinearTextureSampler.Sample(LinearTextureSampler_s, v2.xy).xyz;
  r1.yzw = r2.xyz * r0.xxx;
  r2.x = dot(r2.xyz, float3(0.298900008,0.587000012,0.114));
  r1.x = r2.x + r1.x;
  r2.x = dot(r1.yzw, float3(0.298900008,0.587000012,0.114));
  r2.y = -BloomParams.y + r2.x;
  r2.x = saturate(r2.y / r2.x);
  r0.yzw = r1.yzw * r2.xxx + r0.yzw;
  r2.xyz = LinearTextureSampler.Sample(LinearTextureSampler_s, v2.zw).xyz;
  r1.yzw = r2.xyz * r0.xxx;
  r0.x = dot(r2.xyz, float3(0.298900008,0.587000012,0.114));
  r2.w = r0.x + r1.x;
  r0.x = dot(r1.yzw, float3(0.298900008,0.587000012,0.114));
  r1.x = -BloomParams.y + r0.x;
  r0.x = saturate(r1.x / r0.x);
  r2.xyz = r1.yzw * r0.xxx + r0.yzw;
  r0.xy = (int2)r2.xz & int2(0x7fffffff,0x7fffffff);
  r0.xy = cmp((int2)r0.xy == int2(0x7f800000,0x7f800000));
  r0.x = (int)r0.y | (int)r0.x;
  r0.xyzw = r0.xxxx ? float4(0,0,0,0) : r2.xyzw;

  // Clamp NaNs.
  // Fixes the black sqaure bug.
  r0 = max(0.0, r0);

  r1.x = 0.25 * r0.w;
  r2.x = VignetteSampler.Sample(VignetteSampler_s, v3.xy).x;
  r1.y = VignetteTextureAverage.y * r2.x;
  r0.w = r1.y * r1.x;
  o0.xyzw = float4(0.25,0.25,0.25,1) * r0.xyzw;
  return;
}