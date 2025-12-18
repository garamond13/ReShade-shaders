cbuffer _Globals : register(b0)
{
  float4 _UserClipPlane : packoffset(c0);
  float4x4 ModelViewProj : packoffset(c1);
  float4x4 Model : packoffset(c5);
  float4 Params : packoffset(c9);
}

cbuffer ViewportShaderParameterProvider : register(b1)
{
  float4x4 _ViewRotProjectionMatrix : packoffset(c0);
  float4x4 _ViewProjectionMatrix : packoffset(c4);
  float4x4 _ProjectionMatrix : packoffset(c8);
  float4x4 _ViewMatrix : packoffset(c12);
  float4x4 _InvProjectionMatrix : packoffset(c16);
  float4x4 _InvProjectionMatrixDepth : packoffset(c20);
  float4x4 _DepthTextureTransform : packoffset(c24);
  float4x4 _WaterReflectionTransform : packoffset(c28);
  float4x4 _GrassCylindricalBillboardMatrix : packoffset(c32);
  float4x4 _InvViewMatrix : packoffset(c36);
  float4 _CameraDistances : packoffset(c40);
  float4 _WaterReflectionPlane : packoffset(c41);
  float4 _WaterLevelAdjustment : packoffset(c42);
  float4 _ViewportSize : packoffset(c43);
  float4 _WaterReflectionColor : packoffset(c44);
  float4 _CameraPosition_DistanceScale : packoffset(c45);
  float3 _CameraDirection : packoffset(c46);
  float3 _ViewPoint : packoffset(c47);
  float3 _FogColorVector : packoffset(c48);
  float3 _FogColor : packoffset(c49);
  float3 _FogColorRange : packoffset(c50);
  float3 _FogValues : packoffset(c51);
  float4 _FogHeightValues : packoffset(c52);
  float3 _CameraRight : packoffset(c53);
  float3 _CameraUp : packoffset(c54);
  float4 _CameraNearPlaneSize : packoffset(c55);
  float3 _UncompressDepthWeights : packoffset(c56);
  float3 _UncompressDepthWeightsWS : packoffset(c57);
  float _BloomAdaptationFactor : packoffset(c57.w);
  float3 _CameraPositionFractions : packoffset(c58);
  float4 _CurvedHorizonFactors : packoffset(c59);
  float4 _DepthTextureRcpSize : packoffset(c60);
  float _ShadowProjDepthMinValue : packoffset(c61);
  float _SunOcclusionFactor : packoffset(c61.y);
  float4 _WaterReflectionTexCoordRange : packoffset(c62);
}

SamplerState CelestialBodySampler_s : register(s0);
SamplerState TimeOfDayColorSampler_s : register(s1);
Texture2D CelestialBodySampler : register(t0);
Texture2D TimeOfDayColorSampler : register(t1);

#ifndef MAX_INTENSITY
#define MAX_INTENSITY 1.0
#endif

// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  linear centroid float2 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.x = Params.x;
  r0.y = 0.5;
  r0 = TimeOfDayColorSampler.Sample(TimeOfDayColorSampler_s, r0.xy);
  r1.x = Params.z * r0.w;
  r2 = CelestialBodySampler.Sample(CelestialBodySampler_s, v1.xy);
  r1.y = -1 + r2.w;
  r2.xyzw = Params.yyyy * r2.xyzw;
  r0.xyzw = r2.xyzw * r0.xyzw;
  r1.y = Params.w * r1.y + 1;
  r1.x = r1.y * r1.x;
  r0.xyz = r1.xxx * r0.xyz;
  o0.w = r0.w;

  // Originaly RTV is _UNORM, so it's implicitly clamped.
  o0.xyz = min(MAX_INTENSITY, _BloomAdaptationFactor * r0.xyz);

  return;
}