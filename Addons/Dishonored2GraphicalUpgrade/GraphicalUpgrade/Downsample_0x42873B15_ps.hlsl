struct postfx_luminance_autoexposure_t
{
    float EngineLuminanceFactor;   // Offset:    0
    float LuminanceFactor;         // Offset:    4
    float MinLuminanceLDR;         // Offset:    8
    float MaxLuminanceLDR;         // Offset:   12
    float MiddleGreyLuminanceLDR;  // Offset:   16
    float EV;                      // Offset:   20
    float Fstop;                   // Offset:   24
    uint PeakHistogramValue;       // Offset:   28
};

cbuffer PerInstanceCB : register(b2)
{
  float4 cb_positiontoviewtexture : packoffset(c0);
  uint2 cb_postfx_luminance_exposureindex : packoffset(c1);
  float cb_view_white_level : packoffset(c1.z);
}

cbuffer PerViewCB : register(b1)
{
  float4 cb_alwaystweak : packoffset(c0);
  float4 cb_viewrandom : packoffset(c1);
  float4x4 cb_viewprojectionmatrix : packoffset(c2);
  float4x4 cb_viewmatrix : packoffset(c6);
  float4 cb_subpixeloffset : packoffset(c10);
  float4x4 cb_projectionmatrix : packoffset(c11);
  float4x4 cb_previousviewprojectionmatrix : packoffset(c15);
  float4x4 cb_previousviewmatrix : packoffset(c19);
  float4x4 cb_previousprojectionmatrix : packoffset(c23);
  float4 cb_mousecursorposition : packoffset(c27);
  float4 cb_mousebuttonsdown : packoffset(c28);
  float4 cb_jittervectors : packoffset(c29);
  float4x4 cb_inverseviewprojectionmatrix : packoffset(c30);
  float4x4 cb_inverseviewmatrix : packoffset(c34);
  float4x4 cb_inverseprojectionmatrix : packoffset(c38);
  float4 cb_globalviewinfos : packoffset(c42);
  float3 cb_wscamforwarddir : packoffset(c43);
  uint cb_alwaysone : packoffset(c43.w);
  float3 cb_wscamupdir : packoffset(c44);
  uint cb_usecompressedhdrbuffers : packoffset(c44.w);
  float3 cb_wscampos : packoffset(c45);
  float cb_time : packoffset(c45.w);
  float3 cb_wscamleftdir : packoffset(c46);
  float cb_systime : packoffset(c46.w);
  float2 cb_jitterrelativetopreviousframe : packoffset(c47);
  float2 cb_worldtime : packoffset(c47.z);
  float2 cb_shadowmapatlasslicedimensions : packoffset(c48);
  float2 cb_resolutionscale : packoffset(c48.z);
  float2 cb_parallelshadowmapslicedimensions : packoffset(c49);
  float cb_framenumber : packoffset(c49.z);
  uint cb_alwayszero : packoffset(c49.w);
}

SamplerState smp_linearclamp_s : register(s0);
Texture2D<float4> ro_viewcolormap : register(t0);
StructuredBuffer<postfx_luminance_autoexposure_t> ro_postfx_luminance_buffautoexposure : register(t1);


// 3Dmigoto declarations
#define cmp -

#if 0
void main(
  float4 v0 : SV_POSITION0,
  out float4 o0 : SV_TARGET0)
{
  float4 r0,r1;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.x = ro_postfx_luminance_buffautoexposure[cb_postfx_luminance_exposureindex.y].EngineLuminanceFactor;
  r0.x = cb_view_white_level * r0.x;
  r0.yz = cb_positiontoviewtexture.zw * v0.xy;
  r0.yzw = ro_viewcolormap.Sample(smp_linearclamp_s, r0.yz).xyz;
  r1.xyz = r0.yzw * r0.xxx;
  r0.xyz = cb_usecompressedhdrbuffers ? r1.xyz : r0.yzw;
  r0.xyz = max(float3(0,0,0), r0.xyz);
  o0.w = dot(r0.xyz, float3(0.212599993,0.715200007,0.0722000003));
  o0.xyz = r0.xyz;
  return;
}
#else
void main(
  float4 v0 : SV_POSITION0,
  out float4 o0 : SV_TARGET0)
{
  float4 r0,r1,r2,r3;
  uint4 bitmask, uiDest;
  float4 fDest;

  // From subsequent downsample passes 0xAB0EAF9D.
  // They all share the same vertex shader.
  r0.xy = float2(0.5,0.5) * cb_positiontoviewtexture.zw;
  r0.xy = v0.xy * cb_positiontoviewtexture.zw + -r0.xy;
  r1.xw = cb_positiontoviewtexture.zw;
  r1.yz = float2(0,0);
  r0.zw = r1.xy + r0.xy;
  r2.xyzw = ro_viewcolormap.Sample(smp_linearclamp_s, r0.xy).xyzw;
  r0.xy = r0.zw + r1.zw;
  r3.xyzw = ro_viewcolormap.Sample(smp_linearclamp_s, r0.zw).xyzw;
  r2.xyzw = r3.xyzw + r2.xyzw;
  r0.zw = r0.xy + -r1.xy;
  r1.xyzw = ro_viewcolormap.Sample(smp_linearclamp_s, r0.xy).xyzw;
  r0.xyzw = ro_viewcolormap.Sample(smp_linearclamp_s, r0.zw).xyzw;
  r0.xyzw = r2.xyzw + r0.xyzw;
  r0.xyzw = r0.xyzw + r1.xyzw;
  //o0.xyzw = float4(0.25,0.25,0.25,0.25) * r0.xyzw;
  
  r0.yzw = r0.xyz * 0.25;

  // From the original shader.
  r0.x = ro_postfx_luminance_buffautoexposure[cb_postfx_luminance_exposureindex.y].EngineLuminanceFactor;
  r0.x = cb_view_white_level * r0.x;
  //r0.yz = cb_positiontoviewtexture.zw * v0.xy;
  //r0.yzw = ro_viewcolormap.Sample(smp_linearclamp_s, r0.yz).xyz;
  r1.xyz = r0.yzw * r0.xxx;
  r0.xyz = cb_usecompressedhdrbuffers ? r1.xyz : r0.yzw;
  r0.xyz = max(float3(0,0,0), r0.xyz);
  o0.w = dot(r0.xyz, float3(0.212599993,0.715200007,0.0722000003));
  o0.xyz = r0.xyz;
  return;
}
#endif