cbuffer _Globals : register(b0)
{
  bool bUseBlurSkinning : packoffset(c0);
  bool bDistanceFogEnabled : packoffset(c0.y);
  float3 InverseGamma : packoffset(c1);
  float4 MappingPolynomial : packoffset(c2);
  float FilmSlope : packoffset(c3);
  float FilmToe : packoffset(c3.y);
  float FilmShoulder : packoffset(c3.z);
  float FilmBlackClip : packoffset(c3.w);
  float FilmWhiteClip : packoffset(c4);
  float WhiteTemp : packoffset(c4.y);
  float WhiteTint : packoffset(c4.z);
  float BlueCorrection : packoffset(c4.w);
  float ExpandGamut : packoffset(c5);
  float PQContrastAdjustment : packoffset(c5.y);
  uint bCompensateForColorBlindness : packoffset(c5.z);
  float4x4 ColorBlindnessMatrix : packoffset(c6);
  uint OutputDevice : packoffset(c10);
  uint OutputGamut : packoffset(c10.y);
  uint4 iFramebufferDimensions : packoffset(c11);
  float4 FramebufferDimensions : packoffset(c12);
  float4 UIBufferSize : packoffset(c13);
  float4 ColorBufferSize : packoffset(c14);
  float4 UIViewport : packoffset(c15);
  float4 HDRRemappingConstants1 : packoffset(c16);
  float4 HDRRemappingConstants2 : packoffset(c17);
  uint OutputDeviceMode : packoffset(c18);
  float TonemapperBrightnessAdjustment : packoffset(c18.y);
}

cbuffer ViewConstants : register(b1)
{
  float4x4 _PRIVATE_ViewProjectionMatrix : packoffset(c0);
  float4x4 _PRIVATE_InvViewProjectionMatrix : packoffset(c4);
  float4x4 _PRIVATE_PreviousViewProjectionMatrix : packoffset(c8);
  float4x4 _PRIVATE_ClipToPrevClipMatrix : packoffset(c12);
  float4x4 _PRIVATE_ProjectionMatrix : packoffset(c16);
  float4x4 _PRIVATE_WorldToViewMatrix : packoffset(c20);
  float4x4 _PRIVATE_ViewToWorldMatrix : packoffset(c24);
  float4x4 _PRIVATE_InvProjectionMatrix : packoffset(c28);
  float4x4 _PRIVATE_PrevProjectionMatrix : packoffset(c32);
  float4x4 _PRIVATE_SceneToWorldMatrix : packoffset(c36);
  float4x4 _PRIVATE_ScreenToWorldMatrix : packoffset(c40);
  float4 _PRIVATE_ViewOrigin : packoffset(c44);
  float4 _PRIVATE_ScreenPositionScaleBias : packoffset(c45);
  float4 _PRIVATE_InvScreenPositionScaleBias : packoffset(c46);
  float4 _PRIVATE_TilePositionScaleBias : packoffset(c47);
  float4 _PRIVATE_WindowDimensions : packoffset(c48);
  float4 _PRIVATE_ScreenUVMinMax : packoffset(c49);
  float4 _PRIVATE_MinZ_MaxZRatio : packoffset(c50);
  float4 _PRIVATE_InvFocalLength : packoffset(c51);
  float4 _PRIVATE_DebugDirectIndirectEmissiveOverrides : packoffset(c52);
  float4 _PRIVATE_DebugDiffuseSpecularOverrides : packoffset(c53);
  float4 _PRIVATE_ExposureScales : packoffset(c54);

  struct
  {
    float4 Color;
    float4 DistanceDensity;
  } _PRIVATE_FogBandData[8] : packoffset(c55);

  float3 _PRIVATE_ViewDirection : packoffset(c71);
  float _PRIVATE_AdaptiveTessellationFactor : packoffset(c71.w);
  uint _PRIVATE_ActiveDitherFrame : packoffset(c72);
  uint _PRIVATE_bUseHigherQualityGBufferEncoding : packoffset(c72.y);
  float _PRIVATE_ProjectionScaleX : packoffset(c72.z);
  float _PRIVATE_ProjectionScaleY : packoffset(c72.w);
  float3 _PRIVATE_VolumetricFogRange : packoffset(c73);
  bool _PRIVATE_VolumetricFogEnabled : packoffset(c73.w);
  float2 _PRIVATE_LevelDesatAndFadeControls : packoffset(c74);
  float _PRIVATE_AmbientIntensity : packoffset(c74.z);
  float _PRIVATE_AmbientAlpha : packoffset(c74.w);
  bool _PRIVATE_bDynamicPlanarReflectionsEnabled : packoffset(c75);
  bool _PRIVATE_bDynamicScreenSpaceReflectionsEnabled : packoffset(c75.y);
  int _PRIVATE_TotalEnvironmentMapVolumeCount : packoffset(c75.z);
  float _PRIVATE_SpecularMipFactor : packoffset(c75.w);
  float2 _PRIVATE_DOFFocusRange : packoffset(c76);
  float _PRIVATE_EnvironmentIBLContributionIntensity : packoffset(c76.z);
  uint _PRIVATE_dummy1 : packoffset(c76.w);
  float _PRIVATE_DynamicResolutionScaleRatio : packoffset(c77);
  float _PRIVATE_InvDynamicResolutionScaleRatio : packoffset(c77.y);
  uint _PRIVATE_EnableTemporalDithering : packoffset(c77.z);
  uint _PRIVATE_const0_1 : packoffset(c77.w);
  uint _PRIVATE_const0_2 : packoffset(c78);
  uint _PRIVATE_const0_3 : packoffset(c78.y);
  uint _PRIVATE_const0_4 : packoffset(c78.z);
  uint _PRIVATE_const1_1 : packoffset(c78.w);
  uint _PRIVATE_const1_2 : packoffset(c79);
  uint _PRIVATE_const1_3 : packoffset(c79.y);
  uint _PRIVATE_const1_4 : packoffset(c79.z);
}

SamplerState BilinearClampedSamplerState_s : register(s0);
SamplerState Bilinear3DClampedSamplerState_s : register(s1);
Texture2D<float4> UIInputTexture : register(t0);
Texture2D<float3> ColorInputTexture : register(t1);
Texture3D<float4> TonemappingLUT : register(t2); // 32x32x32
RWTexture2D<unorm float4> OutputSurface : register(u0);

// 3Dmigoto declarations
#define cmp -

float3 sample_tetrahedral(float3 color, Texture3D lut, int lut_size)
{
  const float3 coord = saturate(color) * (float)(lut_size - 1);

  // See https://doi.org/10.2312/egp.20211031
  //

  const float3 r = frac(coord);
  bool cond;
  float3 s = 0.0;
  int3 vert2 = 0;
  int3 vert3 = 1;
  const bool3 c = r.xyz >= r.yzx;
  const bool c_xy = c.x;
  const bool c_yz = c.y;
  const bool c_zx = c.z;
  const bool c_yx = !c.x;
  const bool c_zy = !c.y;
  const bool c_xz = !c.z;

  #define order(x,y,z) \
  cond = c_ ## x ## y && c_ ## y ## z; \
  s = cond ? r.x ## y ## z : s; \
  vert2.x = cond ? 1 : vert2.x; \
  vert3.z = cond ? 0 : vert3.z;

  order(x, y, z)
  order(x, z, y)
  order(z, x, y)
  order(z, y, x)
  order(y, z, x)
  order(y, x, z)

  const float4 bary = float4(1.0 - s.x, s.z, s.x - s.y, s.y - s.z);

  //

  // Interpolate between 4 vertices using barycentric weights.
  const int3 base = floor(coord);
  const float3 v0 = lut.Load(int4(base, 0)).rgb * bary.x;
  const float3 v1 = lut.Load(int4(base + 1, 0)).rgb * bary.y;
  const float3 v2 = lut.Load(int4(base + vert2, 0)).rgb * bary.z;
  const float3 v3 = lut.Load(int4(base + vert3, 0)).rgb * bary.w;
  return v0 + v1 + v2 + v3;
}

[numthreads(8, 8, 1)]
void main(uint3 vThreadID : SV_DispatchThreadID)
{
  float4 r0,r1,r2,r3,r4,r5;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = min((uint4)iFramebufferDimensions.xyyy, (uint4)vThreadID.xyyy);
  r1.xy = (uint2)vThreadID.xy;
  r1.zw = cmp(r1.xy >= UIViewport.xy);
  r1.z = r1.w ? r1.z : 0;
  r2.xy = cmp(UIViewport.zw >= r1.xy);
  r1.z = r1.z ? r2.x : 0;
  r1.z = r2.y ? r1.z : 0;
  if (r1.z != 0) {
    r1.xy = -UIViewport.xy + r1.xy;
    r1.xy = float2(0.5,0.5) + r1.xy;
    r1.xy = UIBufferSize.zw * r1.xy;
    r1.xyzw = UIInputTexture.SampleLevel(BilinearClampedSamplerState_s, r1.xy, 0).xyzw;
  } else {
    OutputSurface[r0.xw] = float4(0,0,0,0);
    return;
  }
  r2.xy = (uint2)r0.xw;
  r2.xy = float2(0.5,0.5) + r2.xy;
  r2.xy = FramebufferDimensions.zw * r2.xy;
  r2.xy = _PRIVATE_DynamicResolutionScaleRatio * r2.xy;
  r2.xyz = ColorInputTexture.SampleLevel(BilinearClampedSamplerState_s, r2.xy, 0).xyz;
  r2.xyz = min(float3(0,0,0), -r2.xyz);
  r3.xy = max(r1.xz, r1.yw);
  r2.w = max(r3.x, r3.y);
  r2.w = cmp(r2.w == 0.000000);
  r3.x = dot(-r2.xyz, float3(0.298999995,0.587000012,0.114));
  r3.yz = cmp(float2(0.996999979,0) < r1.ww);
  r3.w = cmp(1 < r3.x);
  r3.z = r3.w ? r3.z : 0;
  r3.w = 1 + -r1.w;
  r3.w = r3.w * r3.w;
  r1.w = r3.x * r1.w;
  r1.w = 1 / r1.w;
  r1.w = saturate(r1.w);
  r1.w = r3.w * r1.w;
  r1.w = r3.z ? r1.w : r3.w;
  r1.w = r3.y ? 0 : r1.w;
  r3.xyz = -r2.xyz * r1.www;
  r2.xyz = r2.www ? -r2.xyz : r3.xyz;
  if (r2.w == 0) {
    r3.xyz = float3(0.0773993805,0.0773993805,0.0773993805) * r1.xyz;
    r4.xyz = float3(0.0549999997,0.0549999997,0.0549999997) + r1.xyz;
    r4.xyz = float3(0.947867334,0.947867334,0.947867334) * r4.xyz;
    r4.xyz = log2(abs(r4.xyz));
    r4.xyz = float3(2.4000001,2.4000001,2.4000001) * r4.xyz;
    r4.xyz = exp2(r4.xyz);
    r1.xyz = cmp(float3(0.0404499993,0.0404499993,0.0404499993) >= r1.xyz);
    r1.xyz = r1.xyz ? r3.xyz : r4.xyz;
    r3.xyz = float3(0.00266771927,0.00266771927,0.00266771927) + r2.xyz;
    r3.xyz = log2(r3.xyz);
    r3.xyz = r3.xyz * float3(0.0714285746,0.0714285746,0.0714285746) + float3(0.610726953,0.610726953,0.610726953);

    // Original LUT sampling.
    //r3.xyz = min(float3(1,1,1), r3.xyz);
    //r3.xyz = r3.xyz * float3(0.96875,0.96875,0.96875) + float3(0.015625,0.015625,0.015625);
    //r3.xyz = TonemappingLUT.SampleLevel(Bilinear3DClampedSamplerState_s, r3.xyz, 0).xyz;

    // Tetrahedral LUT sampling.
    r3.xyz = sample_tetrahedral(r3.xyz, TonemappingLUT, 32);

    r4.xyz = float3(12.9200001,12.9200001,12.9200001) * r1.xyz;
    r5.xyz = log2(abs(r1.xyz));
    r5.xyz = float3(0.416666657,0.416666657,0.416666657) * r5.xyz;
    r5.xyz = exp2(r5.xyz);
    r5.xyz = r5.xyz * float3(1.05499995,1.05499995,1.05499995) + float3(-0.0549999997,-0.0549999997,-0.0549999997);
    r1.xyz = cmp(float3(0.00313080009,0.00313080009,0.00313080009) >= r1.xyz);
    r1.xyz = r1.xyz ? r4.xyz : r5.xyz;
    r1.xyz = r3.xyz * float3(1.04999995,1.04999995,1.04999995) + r1.xyz;
    r1.w = 1;
    OutputSurface[r0.xw] = r1.xyzw;
  } else {
    r1.xyz = float3(0.00266771927,0.00266771927,0.00266771927) + r2.xyz;
    r1.xyz = log2(r1.xyz);
    r1.xyz = r1.xyz * float3(0.0714285746,0.0714285746,0.0714285746) + float3(0.610726953,0.610726953,0.610726953);

    // Original LUT sampling.
    //r1.xyz = min(float3(1,1,1), r1.xyz);
    //r1.xyz = r1.xyz * float3(0.96875,0.96875,0.96875) + float3(0.015625,0.015625,0.015625);
    //r1.xyz = TonemappingLUT.SampleLevel(Bilinear3DClampedSamplerState_s, r1.xyz, 0).xyz;

    // Tetrahedral LUT sampling.
    r1.xyz = sample_tetrahedral(r1.xyz, TonemappingLUT, 32);

    r1.xyz = float3(1.04999995,1.04999995,1.04999995) * r1.xyz;
    r1.w = 1;
    OutputSurface[r0.xy] = r1.xyzw;
  }
  return;
}