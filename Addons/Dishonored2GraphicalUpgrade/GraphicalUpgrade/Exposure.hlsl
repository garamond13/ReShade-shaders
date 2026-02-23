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

cbuffer PerInstanceCB : register(b0)
{
  float4 cb_positiontoviewtexture : packoffset(c0);
  float4 cb_taatexsize : packoffset(c1);
  float4 cb_taaditherandviewportsize : packoffset(c2);
  float4 cb_postfx_tonemapping_tonemappingparms : packoffset(c3);
  float4 cb_postfx_tonemapping_tonemappingcoeffsinverse1 : packoffset(c4);
  float4 cb_postfx_tonemapping_tonemappingcoeffsinverse0 : packoffset(c5);
  float4 cb_postfx_tonemapping_tonemappingcoeffs1 : packoffset(c6);
  float4 cb_postfx_tonemapping_tonemappingcoeffs0 : packoffset(c7);
  uint2 cb_postfx_luminance_exposureindex : packoffset(c8);
  float2 cb_prevresolutionscale : packoffset(c8.z);
  float cb_env_tonemapping_white_level : packoffset(c9);
  float cb_view_white_level : packoffset(c9.y);
  float cb_taaamount : packoffset(c9.z);
  float cb_postfx_luminance_customevbias : packoffset(c9.w);
}

StructuredBuffer<postfx_luminance_autoexposure_t> ro_postfx_luminance_buffautoexposure : register(t0);
RWTexture2D<float> uav : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	uav[uint2(0, 0)] = ro_postfx_luminance_buffautoexposure[cb_postfx_luminance_exposureindex.y].EV;
}