SamplerState LinearTextureSampler_s : register(s0);
Texture2D<float4> LinearTextureSampler : register(t0);

// 3Dmigoto declarations
#define cmp -

#if 0 // original shader.
void main(
  float4 v0 : SV_Position0,
  linear centroid float2 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  o0 = LinearTextureSampler.Sample(LinearTextureSampler_s, v1.xy);
  return;
}
#else // 2x box downsample in 4 bilinear texture fatches.
void main(
  float4 v0 : SV_Position0,
  linear centroid float2 v1 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float x, y;
  LinearTextureSampler.GetDimensions(x, y);
  float2 LinearTextureSampler_inv_dims = 1.0 / float2(x, y);

  float4 color = 0.0;

  color += LinearTextureSampler.Sample(LinearTextureSampler_s, v1 + float2(-1.0, -1.0) * LinearTextureSampler_inv_dims);
  color += LinearTextureSampler.Sample(LinearTextureSampler_s, v1 + float2(1.0, -1.0) * LinearTextureSampler_inv_dims);
  color += LinearTextureSampler.Sample(LinearTextureSampler_s, v1 + float2(-1.0, 1.0) * LinearTextureSampler_inv_dims);
  color += LinearTextureSampler.Sample(LinearTextureSampler_s, v1 + float2(1.0, 1.0) * LinearTextureSampler_inv_dims);

  o0 = color / 4.0;
}
#endif