cbuffer cb11 : register(b11)
{
  float4 cb11[1]; // cb11[0].xy should be inverse resolution. 
}

SamplerState s0_s : register(s0);
Texture2D<float4> t0 : register(t0);

// 3Dmigoto declarations
#define cmp -

#if 0 // Original shader.
void main(
  float4 v0 : SV_POSITION0,
  float2 v1 : TEXCOORD0,
  out float4 o0 : SV_TARGET0)
{
  float4 r0,r1,r2;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyzw = cb11[0].xyxy * float4(-0.666666687,-1,0.666666687,-1) + v1.xyxy;
  r1.xyz = t0.Sample(s0_s, r0.xy).xyz;
  r0.xyz = t0.Sample(s0_s, r0.zw).xyz;
  r0.xyz = saturate(r0.xyz);
  r1.xyz = saturate(r1.xyz);
  r0.xyz = r1.xyz + r0.xyz;
  r1.xyzw = cb11[0].xyxy * float4(-0.666666687,0,0.666666687,0) + v1.xyxy;
  r2.xyz = t0.Sample(s0_s, r1.xy).xyz;
  r1.xyz = t0.Sample(s0_s, r1.zw).xyz;
  r1.xyz = saturate(r1.xyz);
  r2.xyz = saturate(r2.xyz);
  r0.xyz = r2.xyz + r0.xyz;
  r0.xyz = r0.xyz + r1.xyz;
  r1.xyzw = cb11[0].xyxy * float4(-0.666666687,1,0.666666687,1) + v1.xyxy;
  r2.xyz = t0.Sample(s0_s, r1.xy).xyz;
  r1.xyz = t0.Sample(s0_s, r1.zw).xyz;
  r1.xyz = saturate(r1.xyz);
  r2.xyz = saturate(r2.xyz);
  r0.xyz = r2.xyz + r0.xyz;
  r0.xyz = r0.xyz + r1.xyz;
  r1.xyzw = t0.Sample(s0_s, v1.xy).xyzw;
  r2.xyz = saturate(r1.xyz);
  r0.xyz = -r0.xyz * float3(0.166666672,0.166666672,0.166666672) + r2.xyz;
  r0.xyz = cb11[0].zzz * r0.xyz + r1.xyz;
  o0.w = r1.w;
  o0.xyz = max(float3(0,0,0), r0.xyz);
  return;
}
#else
// Modified AMD FFX CAS
//
// Rescaled the effect of sharpening, now sharpness = 0 means no sharpening and sharpness = 1 is same as before.
//
// Source: https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/54fbaafdc34716811751bea5032700e78f5a0f33/sdk/include/FidelityFX/gpu/cas/ffx_cas.h
//
// This is wrong, the CAS expects linear color.
// But the game renders everything in sRGB (or mixed), than at some point just squares color, than renders to unorm_srgb, than renders to unorm from unorm_srgb.
//

#ifndef SHARPNESS
#define SHARPNESS 0.4
#endif

#define min3(x,y,z) min(x, min(y, z))
#define max3(x,y,z) max(x, max(y, z))

void main(float4 v0 : SV_POSITION0, float2 v1 : TEXCOORD0, out float4 o0 : SV_TARGET0)
{
  // Load a collection of samples in a 3x3 neighorhood, where e is the current pixel.
  // a b c
  // d e f
  // g h i
  float3 sampleA = saturate(t0.Load(int3(v0.xy, 0), int2(-1, -1)).rgb);
  float3 sampleB = saturate(t0.Load(int3(v0.xy, 0), int2(0, -1)).rgb);
  float3 sampleC = saturate(t0.Load(int3(v0.xy, 0), int2(1, -1)).rgb);
  float3 sampleD = saturate(t0.Load(int3(v0.xy, 0), int2(-1, 0)).rgb);
  float4 sampleE = saturate(t0.Load(int3(v0.xy, 0)));
  float3 sampleF = saturate(t0.Load(int3(v0.xy, 0), int2(1, 0)).rgb);
  float3 sampleG = saturate(t0.Load(int3(v0.xy, 0), int2(-1, 1)).rgb); 
  float3 sampleH = saturate(t0.Load(int3(v0.xy, 0), int2(0, 1)).rgb);
  float3 sampleI = saturate(t0.Load(int3(v0.xy, 0), int2(1, 1)).rgb);

  // Soft min and max.
  //  a b c             b
  //  d e f * 0.5  +  d e f * 0.5
  //  g h i             h
  // These are 2.0x bigger (factored out the extra multiply).
  float3 minimumRGB = min3(min3(sampleD, sampleE.rgb, sampleF), sampleB, sampleH);
  float3 minimumRGB2 = min3(min3(minimumRGB, sampleA, sampleC), sampleG, sampleI);
  minimumRGB += minimumRGB2;
  float3 maximumRGB = max3(max3(sampleD, sampleE.rgb, sampleF), sampleB, sampleH);
  float3 maximumRGB2 = max3(max3(maximumRGB, sampleA, sampleC), sampleG, sampleI);
  maximumRGB += maximumRGB2;

  // Smooth minimum distance to signal limit divided by smooth max.
  float3 amplifyRGB = saturate(min(minimumRGB, 2.0 - maximumRGB) * rcp(maximumRGB));
  
  // Shaping amount of sharpening.
  amplifyRGB = sqrt(amplifyRGB);

  // Filter shape.
  //  0 w 0
  //  w 1 w
  //  0 w 0
  float peak = -0.2 * sqrt(saturate(SHARPNESS));
  float3 weight = amplifyRGB * peak;

  // Filter using green coef only, depending on dead code removal to strip out the extra overhead.
  sampleE.rgb = saturate((sampleB * weight.g + sampleD * weight.g + sampleF * weight.g + sampleH * weight.g + sampleE.rgb) * rcp(1.0 + 4.0 * weight.g));

  o0 = float4(sampleE.rgb, sampleE.a);
}
#endif