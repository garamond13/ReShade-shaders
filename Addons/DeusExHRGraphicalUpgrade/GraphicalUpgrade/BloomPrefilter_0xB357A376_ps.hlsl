cbuffer DrawableBuffer : register(b1)
{
  float4 FogColor : packoffset(c0);
  float4 DebugColor : packoffset(c1);
  float MaterialOpacity : packoffset(c2);
  float AlphaThreshold : packoffset(c3);
}

cbuffer SceneBuffer : register(b2)
{
  row_major float4x4 View : packoffset(c0);
  row_major float4x4 ScreenMatrix : packoffset(c4);
  float2 DepthExportScale : packoffset(c8);
  float2 FogScaleOffset : packoffset(c9);
  float3 CameraPosition : packoffset(c10);
  float3 CameraDirection : packoffset(c11);
  float3 DepthFactors : packoffset(c12);
  float2 ShadowDepthBias : packoffset(c13);
  float4 SubframeViewport : packoffset(c14);
  row_major float3x4 DepthToWorld : packoffset(c15);
  float4 DepthToView : packoffset(c18);
  float4 OneOverDepthToView : packoffset(c19);
  float4 DepthToW : packoffset(c20);
  float4 ClipPlane : packoffset(c21);
  float2 ViewportDepthScaleOffset : packoffset(c22);
  float2 ColorDOFDepthScaleOffset : packoffset(c23);
  float2 TimeVector : packoffset(c24);
  float3 HeightFogParams : packoffset(c25);
  float3 GlobalAmbient : packoffset(c26);
  float4 GlobalParams[16] : packoffset(c27);
  float DX3_SSAOScale : packoffset(c43);
  float4 ScreenExtents : packoffset(c44);
  float2 ScreenResolution : packoffset(c45);
  float4 PSSMToMap1Lin : packoffset(c46);
  float4 PSSMToMap1Const : packoffset(c47);
  float4 PSSMToMap2Lin : packoffset(c48);
  float4 PSSMToMap2Const : packoffset(c49);
  float4 PSSMToMap3Lin : packoffset(c50);
  float4 PSSMToMap3Const : packoffset(c51);
  float4 PSSMDistances : packoffset(c52);
  row_major float4x4 WorldToPSSM0 : packoffset(c53);
}

cbuffer MaterialBuffer : register(b3)
{
  float4 MaterialParams[32] : packoffset(c0);
}

SamplerState p_default_Material_0B464C7419133480_Param_sampler_s : register(s0);
Texture2D<float4> p_default_Material_0B464C7419133480_Param_texture : register(t0);

// 3Dmigoto declarations
#define cmp -

#if 0 // Original shader.
void main(
  float4 v0 : SV_POSITION0,
  out float4 o0 : SV_Target0)
{
  float4 r0;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = v0.xy * ScreenExtents.zw + ScreenExtents.xy;
  r0.xyzw = p_default_Material_0B464C7419133480_Param_texture.Sample(p_default_Material_0B464C7419133480_Param_sampler_s, r0.xy).xyzw;
  r0.w = saturate(MaterialParams[0].x * r0.w);
  o0.xyz = r0.www * r0.xyz;
  o0.w = MaterialOpacity;
  return;
}
#else

// We are doing manual linear downsampling instead of the hardware one which would cause aliasing.
// We only expect 2x downscale.

float2 get_linear_weight(float2 xy)
{
  return 1.0 - abs(xy);
}

void main(float4 v0 : SV_POSITION0, out float4 o0 : SV_Target0)
{
  float4 r0;
  uint4 bitmask, uiDest;
  float4 fDest;

  // Get source size.
  float2 src_size;
  p_default_Material_0B464C7419133480_Param_texture.GetDimensions(src_size.x, src_size.y);
  const float2 inv_src_size = 1.0 / src_size;

  // Calculate fractional part and texel center.
  const float2 uv = (v0.xy * ScreenExtents.zw + ScreenExtents.xy);
  const float2 f = frac(uv * src_size - 0.5);
  const float2 tc = uv - f * inv_src_size;

  float4 csum = 0.0;
  float wsum = 0.0;

  // The kernel radius is 1 * 2.
  [unroll]
  for (float y = -1.0; y <= 2.0; ++y) {
    [unroll]
    for (float x = -1.0; x <= 2.0; ++x) {
      const float4 color = p_default_Material_0B464C7419133480_Param_texture.Sample(p_default_Material_0B464C7419133480_Param_sampler_s, tc + float2(x, y) * inv_src_size).xyzw;
      const float2 weight = get_linear_weight((float2(x, y) - f) / 2.0);
      const float final_weight = weight.x * weight.y;
      csum += color * final_weight;
      wsum += final_weight;
    }
  }

  // Normalize.
  csum /= wsum;

  // As done by the original shader.
  csum.w = saturate(MaterialParams[0].x * csum.w);
  o0.xyz = csum.www * csum.xyz;
  o0.w = MaterialOpacity;

  return;
}
#endif