// Tone mapping

#include "Color.hlsli"

cbuffer _Globals : register(b0)
{
  float ImageSpace_hlsl_ToneMapPixelShader0000000000000000000000000000000b_4bits : packoffset(c0);
  float4 fogColor : packoffset(c1);
  float3 fogTransform : packoffset(c2);
  float2 fogLuminance : packoffset(c3);
  row_major float3x4 screenDataToCamera : packoffset(c4);
  float globalScale : packoffset(c7);
  float sceneDepthAlphaMask : packoffset(c7.y);
  float globalOpacity : packoffset(c7.z);
  float distortionBufferScale : packoffset(c7.w);
  float3 wToZScaleAndBias : packoffset(c8);
  float4 screenTransform[2] : packoffset(c9);
  float4 textureToPixel : packoffset(c11);
  float4 pixelToTexture : packoffset(c12);
  float maxScale : packoffset(c13);
  float bloomAlpha : packoffset(c13.y);
  float sceneBias : packoffset(c13.z);
  float3 gammaSettings : packoffset(c14);
  float exposure : packoffset(c14.w);
  float deltaExposure : packoffset(c15);
  float4 ColorFill : packoffset(c16);
  float2 LowResTextureDimensions : packoffset(c17);
  float2 DownsizeTextureDimensions : packoffset(c17.z);
}

SamplerState s_framebuffer_s : register(s0);
SamplerState s_bloom_s : register(s1);
SamplerState s_toneMapTable_s : register(s2);
Texture2D s_framebuffer : register(t0);
Texture2D s_bloom : register(t1);
Texture3D s_toneMapTable : register(t2);


// 3Dmigoto declarations
#define cmp -

#if 0 // The original shader.
void main(
  float4 v0 : SV_Position0,
  float2 v1 : TEXCOORD0,
  float2 w1 : TEXCOORD1,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0 = s_bloom.Sample(s_bloom_s, w1.xy);
  r1 = s_framebuffer.Sample(s_framebuffer_s, v1.xy);
  r0.xyz = r0.xyz * bloomAlpha + r1.xyz;
  r1.w = saturate(r1.w);
  r0.xyz = saturate(sceneBias * r0.xyz);

  // Delinearization, gamma 2.2.
  r0.xyz = log2(r0.xyz);
  r0.xyz = float3(0.454545468,0.454545468,0.454545468) * r0.xyz;
  r0.xyz = exp2(r0.xyz);

  // 3D LUT application, the LUT size is 16x16x16.
  // This gives look close to as if TRC for delinearization was sRGB,
  // was that the intent?
  r0 = s_toneMapTable.Sample(s_toneMapTable_s, r0.xyz);
  
  o0.xyz = r0.xyz;
  o0.w = r1.w;
  return;
}
#else // We will need both linear and delinearized output. Won't apply the 3D LUT later, insted we will use sRGB TRC as the default.
void main(float4 v0 : SV_Position0, float2 v1 : TEXCOORD0, float2 w1 : TEXCOORD1, out float4 o0 : SV_Target0, out float4 o1 : SV_Target1)
{
  float4 r0,r1;

  r0 = s_bloom.Sample(s_bloom_s, w1.xy);
  r1 = s_framebuffer.Sample(s_framebuffer_s, v1.xy);
  r0.xyz = r0.xyz * bloomAlpha + r1.xyz;
  r1.w = saturate(r1.w);
  r0.xyz = saturate(sceneBias * r0.xyz);

  // Linear output.
  o0 = float4(r0.xyz, r1.w);

  // Delinearization.
  #ifdef SRGB
	r0.xyz = linear_to_srgb(r0.xyz);
	#else
	r0.xyz = linear_to_gamma(r0.xyz);
	#endif

  // Delinearized output.
  o1 = float4(r0.xyz, r1.w);
}
#endif