// Tonemap

cbuffer _Globals : register(b0)
{
  float4 SceneShadowsAndDesaturation : packoffset(c0);
  float4 SceneInverseHighLights : packoffset(c1);
  float4 SceneMidTones : packoffset(c2);
  float4 SceneScaledLuminanceWeights : packoffset(c3);
  float4 GammaColorScaleAndInverse : packoffset(c4);
  float4 GammaOverlayColor : packoffset(c5);
  float4 RenderTargetExtent : packoffset(c6);
  float2 DownsampledDepthScale : packoffset(c7);
  float4 ImageAdjustment : packoffset(c8);
}

SamplerState SceneColorTexture_s : register(s0);
SamplerState DOFAndMotionBlurImage_s : register(s1);
SamplerState DOFAndMotionBlurInfoImage_s : register(s2);
SamplerState BloomOnlyImage_s : register(s3);
SamplerState LightShaftTexture_s : register(s4);
SamplerState ColorGradingLUT_s : register(s5);
Texture2D<float4> SceneColorTexture : register(t0);
Texture2D<float4> DOFAndMotionBlurImage : register(t1);
Texture2D<float4> DOFAndMotionBlurInfoImage : register(t2);
Texture2D<float4> BloomOnlyImage : register(t3);
Texture2D<float4> LightShaftTexture : register(t4);
Texture2D<float4> ColorGradingLUT : register(t5);


#ifndef BLOOM_INTENSITY
#define BLOOM_INTENSITY 1.0
#endif

// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : TEXCOORD0,
  float2 v1 : TEXCOORD1,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2,r3;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyz = SceneColorTexture.Sample(SceneColorTexture_s, v0.zw).xyz;
  r1.xyz = DOFAndMotionBlurImage.Sample(DOFAndMotionBlurImage_s, v1.xy).xyz;
  r0.w = DOFAndMotionBlurInfoImage.Sample(DOFAndMotionBlurInfoImage_s, v1.xy).z;
  r1.xyzw = r1.zzxy * r0.wwww;
  r0.w = 1 + -r0.w;
  r0.xyzw = r0.zzxy * r0.wwww + r1.xyzw;
  r1.xyz = BloomOnlyImage.Sample(BloomOnlyImage_s, v1.xy).xyz * BLOOM_INTENSITY;
  r0.xyzw = r1.zzxy + r0.xyzw;
  r1.x = dot(r0.zwy, float3(0.300000012,0.589999974,0.109999999));
  r1.x = -3 * r1.x;
  r1.x = exp2(r1.x);
  r1.x = min(1, r1.x);
  r2.xyzw = LightShaftTexture.Sample(LightShaftTexture_s, v0.zw).xyzw;
  r3.xyzw = float4(4,4,4,4) * r2.zzxy;
  r1.xyzw = r3.xyzw * r1.xxxx;
  r0.xyzw = r0.xyzw * r2.wwww + r1.xyzw;
  r1.xyzw = ImageAdjustment.xxxx * r0.xyzw;
  r0.xyzw = r0.yyzw * ImageAdjustment.xxxx + float4(0.187000006,0.187000006,0.187000006,0.187000006);
  r0.xyzw = r1.xyzw / abs(r0.xyzw);
  r0.xyzw = saturate(float4(1.03499997,1.03499997,1.03499997,1.03499997) * r0.xyzw);
  r1.xyw = float3(14.9998999,0.9375,0.05859375) * r0.xwz;
  r0.x = floor(r1.x);
  r1.x = r0.x * 0.0625 + r1.w;
  r1.xyzw = float4(0.001953125,0.03125,0.064453125,0.03125) + r1.xyxy;
  r0.x = 0.0625 * r0.x;
  r0.x = r0.y * 0.9375 + -r0.x;
  r0.x = 16 * r0.x;
  r2.xyzw = ColorGradingLUT.Sample(ColorGradingLUT_s, r1.zw).xyzw;
  r1.xyzw = ColorGradingLUT.Sample(ColorGradingLUT_s, r1.xy).xyzw;
  r2.xyzw = r2.xyzw + -r1.xyzw;
  o0.xyzw = r0.xxxx * r2.xyzw + r1.xyzw;
  return;
}