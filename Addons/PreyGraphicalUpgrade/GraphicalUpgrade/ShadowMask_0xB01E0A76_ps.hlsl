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

cbuffer CBPerFrame : register(b6)
{
  row_major float4x4 CF_ShadowSampling_TexGen0 : packoffset(c0);
  row_major float4x4 CF_ShadowSampling_TexGen1 : packoffset(c4);
  row_major float4x4 CF_ShadowSampling_TexGen2 : packoffset(c8);
  row_major float4x4 CF_ShadowSampling_TexGen3 : packoffset(c12);
  float4 CF_ShadowSampling_InvShadowMapSize : packoffset(c16);
  float4 CF_ShadowSampling_DepthTestBias : packoffset(c17);
  float4 CF_ShadowSampling_OneDivFarDist : packoffset(c18);
  float4 CF_ShadowSampling_KernelRadius : packoffset(c19);
  float4 CF_VolumetricFogParams : packoffset(c20);
  float4 CF_VolumetricFogRampParams : packoffset(c21);
  float4 CF_VolumetricFogSunDir : packoffset(c22);
  float4 CF_FogColGradColBase : packoffset(c23);
  float4 CF_FogColGradColDelta : packoffset(c24);
  float4 CF_FogColGradParams : packoffset(c25);
  float4 CF_FogColGradRadial : packoffset(c26);
  float4 CF_VolumetricFogSamplingParams : packoffset(c27);
  float4 CF_VolumetricFogDistributionParams : packoffset(c28);
  float4 CF_VolumetricFogScatteringParams : packoffset(c29);
  float4 CF_VolumetricFogScatteringBlendParams : packoffset(c30);
  float4 CF_VolumetricFogScatteringColor : packoffset(c31);
  float4 CF_VolumetricFogScatteringSecondaryColor : packoffset(c32);
  float4 CF_VolumetricFogHeightDensityParams : packoffset(c33);
  float4 CF_VolumetricFogHeightDensityRampParams : packoffset(c34);
  float4 CF_VolumetricFogDistanceParams : packoffset(c35);
  float4 CF_VolumetricFogGlobalEnvProbe0 : packoffset(c36);
  float4 CF_VolumetricFogGlobalEnvProbe1 : packoffset(c37);
  float4 CF_CloudShadingColorSun : packoffset(c38);
  float4 CF_CloudShadingColorSky : packoffset(c39);
  float CF_SSDOAmountDirect : packoffset(c40);
  float3 __padding0 : packoffset(c40.y);
  float4 CF_Timers[4] : packoffset(c41);
  float4 CF_RandomNumbers : packoffset(c45);
  float4 CF_irreg_kernel_2d[8] : packoffset(c46);
}

cbuffer CBShadowMask : register(b0)
{
  struct
  {
    row_major float4x4 unitMeshTransform;
    float4 lightVolumeSphereAdjust;
    row_major float4x4 lightShadowProj;
    float4 params;
    float3 vLightPos;
    float LG_SceneSelector;
  } cbShadowMaskConstants : packoffset(c0);
}

SamplerState ssShadowPointWrap_s : register(s7);
SamplerComparisonState ssShadowComparison_s : register(s3);
Texture2D<float4> sceneDepthTex : register(t0);
Texture2D<float4> ShadowMap : register(t1);
Texture2D<float4> shadowNoiseTex : register(t7);

// 3Dmigoto declarations
#define cmp -

void main(
  float4 v0 : SV_POSITION0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2,r3,r4;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = (int2)v0.xy;
  r0.zw = float2(0,0);
  r0.z = sceneDepthTex.Load(r0.xyz).x;
  r1.xy = trunc(v0.xy);
  r0.xy = r1.xy * r0.zz;
  r1.x = dot(CV_ScreenToWorldBasis._m00_m01_m02, r0.xyz);
  r1.y = dot(CV_ScreenToWorldBasis._m10_m11_m12, r0.xyz);
  r1.z = dot(CV_ScreenToWorldBasis._m20_m21_m22, r0.xyz);
  r1.w = 1;
  r0.x = dot(cbShadowMaskConstants.lightShadowProj._m00_m01_m02_m03, r1.xyzw);
  r0.y = dot(cbShadowMaskConstants.lightShadowProj._m10_m11_m12_m13, r1.xyzw);
  r0.z = dot(cbShadowMaskConstants.lightShadowProj._m30_m31_m32_m33, r1.xyzw);
  r0.w = dot(cbShadowMaskConstants.lightShadowProj._m20_m21_m22_m23, r1.xyzw);
  r0.w = -cbShadowMaskConstants.params.w + r0.w;
  r1.xyzw = r0.xyxy / r0.zzzz;

  // Multiply with 64.0, fixes pixelated shadows.
  // Source: https://github.com/Filoppi/Luma-Framework/blob/b5f7774a0a59ae08670188a3896379b4c2c3fad4/Shaders/Prey/DeferredShading_ShadowMaskGen_0xB01E0A76.ps_5_0.hlsl
  r0.xy = cbShadowMaskConstants.params.xx * 64.0 * r1.zw;

  r0.xy = float2(1000,1000) * r0.xy;
  r0.xy = shadowNoiseTex.Sample(ssShadowPointWrap_s, r0.xy).xy;
  r0.z = 0.001953125 * cbShadowMaskConstants.params.x;
  r2.xyz = r0.yxx * r0.zzz;
  r2.w = -r2.x;
  r3.xyzw = CF_irreg_kernel_2d[0].yyzz * r2.xzzw;
  r3.xyzw = r2.zwxz * CF_irreg_kernel_2d[0].xxww + r3.xyzw;
  r3.xyzw = r3.xyzw + r1.zwzw;
  r4.x = ShadowMap.SampleCmpLevelZero(ssShadowComparison_s, r3.xy, r0.w).x;
  r4.y = ShadowMap.SampleCmpLevelZero(ssShadowComparison_s, r3.zw, r0.w).x;
  r3.xyzw = CF_irreg_kernel_2d[1].yyzz * r2.xzzw;
  r3.xyzw = r2.zwxz * CF_irreg_kernel_2d[1].xxww + r3.xyzw;
  r3.xyzw = r3.xyzw + r1.zwzw;
  r4.z = ShadowMap.SampleCmpLevelZero(ssShadowComparison_s, r3.xy, r0.w).x;
  r4.w = ShadowMap.SampleCmpLevelZero(ssShadowComparison_s, r3.zw, r0.w).x;
  r0.x = dot(r4.xyzw, float4(0.0625,0.0625,0.0625,0.0625));
  r3.xyzw = CF_irreg_kernel_2d[2].yyzz * r2.xzzw;
  r3.xyzw = r2.zwxz * CF_irreg_kernel_2d[2].xxww + r3.xyzw;
  r3.xyzw = r3.xyzw + r1.zwzw;
  r4.x = ShadowMap.SampleCmpLevelZero(ssShadowComparison_s, r3.xy, r0.w).x;
  r4.y = ShadowMap.SampleCmpLevelZero(ssShadowComparison_s, r3.zw, r0.w).x;
  r3.xyzw = CF_irreg_kernel_2d[3].yyzz * r2.xzzw;
  r3.xyzw = r2.zwxz * CF_irreg_kernel_2d[3].xxww + r3.xyzw;
  r3.xyzw = r3.xyzw + r1.zwzw;
  r4.z = ShadowMap.SampleCmpLevelZero(ssShadowComparison_s, r3.xy, r0.w).x;
  r4.w = ShadowMap.SampleCmpLevelZero(ssShadowComparison_s, r3.zw, r0.w).x;
  r0.y = dot(r4.xyzw, float4(0.0625,0.0625,0.0625,0.0625));
  r0.x = r0.x + r0.y;
  r3.xyzw = CF_irreg_kernel_2d[4].yyzz * r2.xzzw;
  r3.xyzw = r2.zwxz * CF_irreg_kernel_2d[4].xxww + r3.xyzw;
  r3.xyzw = r3.xyzw + r1.zwzw;
  r4.x = ShadowMap.SampleCmpLevelZero(ssShadowComparison_s, r3.xy, r0.w).x;
  r4.y = ShadowMap.SampleCmpLevelZero(ssShadowComparison_s, r3.zw, r0.w).x;
  r3.xyzw = CF_irreg_kernel_2d[5].yyzz * r2.xzzw;
  r3.xyzw = r2.zwxz * CF_irreg_kernel_2d[5].xxww + r3.xyzw;
  r3.xyzw = r3.xyzw + r1.zwzw;
  r4.z = ShadowMap.SampleCmpLevelZero(ssShadowComparison_s, r3.xy, r0.w).x;
  r4.w = ShadowMap.SampleCmpLevelZero(ssShadowComparison_s, r3.zw, r0.w).x;
  r0.y = dot(r4.xyzw, float4(0.0625,0.0625,0.0625,0.0625));
  r0.x = r0.x + r0.y;
  r3.xyzw = CF_irreg_kernel_2d[6].yyzz * r2.xzzw;
  r3.xyzw = r2.zwxz * CF_irreg_kernel_2d[6].xxww + r3.xyzw;
  r3.xyzw = r3.xyzw + r1.zwzw;
  r4.x = ShadowMap.SampleCmpLevelZero(ssShadowComparison_s, r3.xy, r0.w).x;
  r4.y = ShadowMap.SampleCmpLevelZero(ssShadowComparison_s, r3.zw, r0.w).x;
  r3.xyzw = CF_irreg_kernel_2d[7].yyzz * r2.xyzw;
  r2.xyzw = r2.zwxz * CF_irreg_kernel_2d[7].xxww + r3.xyzw;
  r1.xyzw = r2.xyzw + r1.xyzw;
  r4.z = ShadowMap.SampleCmpLevelZero(ssShadowComparison_s, r1.xy, r0.w).x;
  r4.w = ShadowMap.SampleCmpLevelZero(ssShadowComparison_s, r1.zw, r0.w).x;
  r0.y = dot(r4.xyzw, float4(0.0625,0.0625,0.0625,0.0625));
  r0.x = r0.x + r0.y;
  o0.xyzw = float4(1,1,1,1) + -r0.xxxx;
  return;
}