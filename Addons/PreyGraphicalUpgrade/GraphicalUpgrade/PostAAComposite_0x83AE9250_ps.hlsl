cbuffer CBPerViewGlobal : register(b13)
{
  row_major float4x4 CV_ViewProjZeroMatr : packoffset(c0);
  float4 CV_AnimGenParams : packoffset(c4);
  row_major float4x4 CV_ViewProjMatr : packoffset(c5);
  row_major float4x4 CV_ViewProjNearestMatr : packoffset(c9);
  row_major float4x4 CV_InvViewProj : packoffset(c13);
  row_major float4x4 CV_PrevViewProjMatr : packoffset(c17);
  row_major float4x4 CV_PrevViewProjNearestMatr : packoffset(c21);
  row_major float3x4 CV_ScreenToWorldBasis : packoffset(c25);
  float4 CV_TessInfo : packoffset(c28);
  float4 CV_CameraRightVector : packoffset(c29);
  float4 CV_CameraFrontVector : packoffset(c30);
  float4 CV_CameraUpVector : packoffset(c31);
  float4 CV_ScreenSize : packoffset(c32);
  float4 CV_HPosScale : packoffset(c33);
  float4 CV_HPosClamp : packoffset(c34);
  float4 CV_ProjRatio : packoffset(c35);
  float4 CV_NearestScaled : packoffset(c36);
  float4 CV_NearFarClipDist : packoffset(c37);
  float4 CV_SunLightDir : packoffset(c38);
  float4 CV_SunColor : packoffset(c39);
  float4 CV_SkyColor : packoffset(c40);
  float4 CV_FogColor : packoffset(c41);
  float4 CV_TerrainInfo : packoffset(c42);
  float4 CV_DecalZFightingRemedy : packoffset(c43);
  row_major float4x4 CV_FrustumPlaneEquation : packoffset(c44);
  float4 CV_WindGridOffset : packoffset(c48);
  row_major float4x4 CV_ViewMatr : packoffset(c49);
  row_major float4x4 CV_InvViewMatr : packoffset(c53);
  float CV_LookingGlass_SunSelector : packoffset(c57);
  float CV_LookingGlass_DepthScalar : packoffset(c57.y);
  float CV_PADDING0 : packoffset(c57.z);
  float CV_PADDING1 : packoffset(c57.w);
}

cbuffer CBComposites : register(b0)
{
  struct
  {
    float Sharpening;
    float ChromaShift;
    float MinExposure;
    float MaxExposure;
    float FilmGrainAmount;
    float FilmGrainTime;
    float __padding;
    float VignetteFalloff;
    float4 VignetteBorder;
    float4 VignetteColor;
  } cbComposites : packoffset(c0);
}

SamplerState ssCompositeSource_s : register(s0);
SamplerState ssFilmGrain_s : register(s6);
SamplerState ssSceneLum_s : register(s7);
Texture2D<float4> compositeSourceTex : register(t0);
Texture3D<float4> filmGrainTex : register(t6);
Texture2D<float4> SceneLumTex : register(t7);

#ifndef VIGENETTE_STRENGTH
#define VIGENETTE_STRENGTH 1.0
#endif

// 3Dmigoto declarations
#define cmp -

void main(
  float4 v0 : SV_Position0,
  float4 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = CV_HPosScale.xy * v1.xy;
  r0.w = SceneLumTex.Sample(ssSceneLum_s, r0.xy).x;
  r1.x = 1 + r0.w;
  r1.x = log2(r1.x);
  r1.x = 2 + r1.x;
  r1.x = 2 / r1.x;
  r1.x = 1.02999997 + -r1.x;
  r0.w = r1.x / r0.w;
  r0.w = max(cbComposites.MinExposure, r0.w);
  r0.w = min(cbComposites.MaxExposure, r0.w);
  r0.w = sqrt(r0.w);
  r1.x = CV_ScreenSize.x / CV_ScreenSize.y;
  r1.y = 4;
  r0.z = 4 * r0.x;
  r1.xy = r1.xy * r0.zy;
  r2.xyzw = compositeSourceTex.Sample(ssCompositeSource_s, r0.xy).xyzw;
  r1.z = 3 * cbComposites.FilmGrainTime;
  r0.x = filmGrainTex.Sample(ssFilmGrain_s, r1.xyz).x;
  r0.x = -0.5 + r0.x;
  r0.x = cbComposites.FilmGrainAmount * r0.x + 0.5;
  r0.y = 0.5 + -r0.x;
  r0.x = r0.w * r0.y + r0.x;
  r0.zw = v1.xy * float2(2,2) + float2(-1,-1);
  r1.xy = cmp(r0.zw < float2(0,0));
  r1.xy = r1.xy ? cbComposites.VignetteBorder.xz : cbComposites.VignetteBorder.yw;
  r0.zw = r1.xy * r0.zw;
  r0.z = dot(r0.zw, r0.zw);
  r0.z = sqrt(r0.z);
  r0.z = cbComposites.VignetteFalloff * r0.z;
  r0.z = r0.z * r0.z + 1;
  r0.z = r0.z * r0.z;
  r0.z = rcp(r0.z);
  r0.yz = float2(1,1) + -r0.xz;
  r0.w = -cbComposites.VignetteColor.w * VIGENETTE_STRENGTH * r0.z + 1;
  r0.z = cbComposites.VignetteColor.w * VIGENETTE_STRENGTH * r0.z;
  r1.xyz = r2.xyz * r0.www;
  o0.w = r2.w;
  r1.xyz = cbComposites.VignetteColor.xyz * r0.zzz + r1.xyz;
  r2.xyz = float3(1,1,1) + -r1.xyz;
  r2.xyz = r2.xyz + r2.xyz;
  r0.yzw = -r2.xyz * r0.yyy + float3(1,1,1);
  r2.xyz = r1.xyz * r0.xxx;
  r1.xyz = cmp(r1.xyz >= float3(0.5,0.5,0.5));
  r1.xyz = r1.xyz ? float3(1,1,1) : 0;
  r0.xyz = -r2.xyz * float3(2,2,2) + r0.yzw;
  r2.xyz = r2.xyz + r2.xyz;
  o0.xyz = r1.xyz * r0.xyz + r2.xyz;
  return;
}