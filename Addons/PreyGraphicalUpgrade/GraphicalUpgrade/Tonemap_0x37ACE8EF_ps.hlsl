cbuffer PER_BATCH : register(b0)
{
  float4 HDRColorBalance : packoffset(c0);
  float4 SunShafts_SunCol : packoffset(c1);
  float4 HDREyeAdaptation : packoffset(c2);
  float4 HDRFilmCurve : packoffset(c3);
  float4 HDRBloomColor : packoffset(c4);
}

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

SamplerState ssHdrLinearClamp_s : register(s0);
Texture2D<float4> hdrSourceTex : register(t0);
Texture2D<float2> adaptedLumTex : register(t1);
Texture2D<float4> bloomTex : register(t2);
Texture2D<float4> vignettingTex : register(t7);
Texture2D<float4> colorChartTex : register(t8);
Texture2D<float4> sunshaftsTex : register(t9);

#ifndef BLOOM_INTENSITY
#define BLOOM_INTENSITY 1.0
#endif

// 3Dmigoto declarations
#define cmp -

float3 load_lut_2d(Texture2D lut, int3 index, int lut_size)
{
  const int x = index.x + index.z * lut_size; // Slice offset in X.
  const int y = index.y;
  return lut.Load(int3(x, y, 0)).xyz;
}

float3 sample_tetrahedral(float3 color, Texture2D lut, int lut_size)
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
  const float3 v0 = load_lut_2d(lut, base, lut_size) * bary.x;
  const float3 v1 = load_lut_2d(lut, base + int3(1,1,1), lut_size) * bary.y;
  const float3 v2 = load_lut_2d(lut, base + vert2, lut_size) * bary.z;
  const float3 v3 = load_lut_2d(lut, base + vert3, lut_size) * bary.w;
  return v0 + v1 + v2 + v3;
}

void main(
  float4 v0 : SV_Position0,
  float4 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2,r3;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.x = adaptedLumTex.Sample(ssHdrLinearClamp_s, v1.xy).x;
  r0.y = 1 + r0.x;
  r0.y = log2(r0.y);
  r0.y = 2 + r0.y;
  r0.y = 2 / r0.y;
  r0.y = 1.02999997 + -r0.y;
  r0.x = r0.y / r0.x;
  r0.x = max(HDREyeAdaptation.y, r0.x);
  r0.x = min(HDREyeAdaptation.z, r0.x);
  r0.y = vignettingTex.Sample(ssHdrLinearClamp_s, v1.xy).x;
  r0.x = r0.y * r0.x;
  r0.yzw = saturate(HDRBloomColor.xyz);
  r1.xy = CV_HPosScale.xy * v1.xy;
  r1.xyz = bloomTex.Sample(ssHdrLinearClamp_s, r1.xy).xyz * BLOOM_INTENSITY;
  r2.xy = (int2)v0.xy;
  r2.zw = float2(0,0);
  r3.xyz = hdrSourceTex.Load(r2.xyw).xyz;
  r2.xyzw = (int4)r2.xyzw;
  r2.xyzw = SunShafts_SunCol.wwww * r2.xyzw;
  r2.xyzw = (int4)r2.xyzw;
  r2.xyz = sunshaftsTex.Load(r2.xyz).xyz;
  r2.xyz = SunShafts_SunCol.xyz * r2.xyz;
  r1.xyz = -r3.xyz + r1.xyz;
  r0.yzw = r0.yzw * r1.xyz + r3.xyz;
  r1.xyz = r0.xxx * r0.yzw;
  r1.x = dot(r1.xyz, float3(0.212599993,0.715200007,0.0722000003));
  r0.xyz = r0.xxx * r0.yzw + -r1.xxx;
  r0.xyz = HDRColorBalance.www * r0.xyz + r1.xxx;
  r0.xyz = HDRColorBalance.xyz * r0.xyz;
  r0.xyz = max(float3(0,0,0), r0.xyz);
  r1.xyz = r0.xyz * float3(0.879999995,0.879999995,0.879999995) + float3(0.0300000012,0.0300000012,0.0300000012);
  r1.xyz = r0.xyz * r1.xyz + float3(0.00200000009,0.00200000009,0.00200000009);
  r3.xyz = r0.xyz * float3(0.879999995,0.879999995,0.879999995) + float3(0.300000012,0.300000012,0.300000012);
  r0.xyz = r0.xyz * r3.xyz + float3(0.0599999987,0.0599999987,0.0599999987);
  r0.xyz = r1.xyz / r0.xyz;
  r0.xyz = float3(-0.0333333313,-0.0333333313,-0.0333333313) + r0.xyz;
  r0.xyz = saturate(float3(1.21417511,1.21417511,1.21417511) * r0.xyz);
  r1.xyz = log2(r0.xyz);
  r1.xyz = float3(0.416666657,0.416666657,0.416666657) * r1.xyz;
  r1.xyz = exp2(r1.xyz);
  r1.xyz = r1.xyz * float3(1.05499995,1.05499995,1.05499995) + float3(-0.0549999997,-0.0549999997,-0.0549999997);
  r3.xyz = cmp(r0.xyz < float3(0.00313080009,0.00313080009,0.00313080009));
  r0.xyz = float3(12.9200001,12.9200001,12.9200001) * r0.xyz;
  r0.xyz = r3.xyz ? r0.xyz : r1.xyz;
  r1.xyz = float3(1,1,1) + -r0.xyz;
  r0.xyz = saturate(r2.xyz * r1.xyz + r0.xyz);

  // Original trilinear LUT sampling.
  //r0.yzw = r0.xyz * float3(0.9375,0.9375,0.9375) + float3(0.03125,0.03125,0);
  //r1.x = 16 * r0.w;
  //r1.x = frac(r1.x);
  //r0.w = r0.w * 16 + -r1.x;
  //r0.y = r0.y + r0.w;
  //r0.x = 0.0625 * r0.y;
  //r0.yw = float2(0.0625,0) + r0.xz;
  //r1.yzw = colorChartTex.Sample(ssHdrLinearClamp_s, r0.xz).xyz;
  //r0.xyz = colorChartTex.Sample(ssHdrLinearClamp_s, r0.yw).xyz;
  //r0.xyz = r0.xyz + -r1.yzw;
  //r0.xyz = r0.xyz * r1.xxx + r1.yzw;

  // Tetrahedral LUT sampling.
  r0.xyz = sample_tetrahedral(r0.xyz, colorChartTex, 16);

  r1.xy = float2(0.57889998,0.57889998) + v1.xy;
  r0.w = dot(r1.xy, float2(34.4830017,89.637001));
  r0.w = sin(r0.w);
  r1.xyz = float3(29156.4766,38273.5625,47843.7539) * r0.www;
  r1.xyz = frac(r1.xyz);
  r0.w = dot(v1.xy, float2(34.4830017,89.637001));
  r0.w = sin(r0.w);
  r2.xyz = float3(29156.4766,38273.5625,47843.7539) * r0.www;
  r2.xyz = frac(r2.xyz);
  r1.xyz = r2.xyz + r1.xyz;
  r1.xyz = float3(-0.5,-0.5,-0.5) + r1.xyz;
  o0.xyz = r1.xyz * float3(0.00392156886,0.00392156886,0.00392156886) + r0.xyz;
  o0.w = 0;
  return;
}