// The last shader before UI
//
// Optionaly (user configurable) disable lens flare.

cbuffer _Globals : register(b0)
{
  float2 invPixelSize : packoffset(c0);
  float mipLevelSource : packoffset(c0.z);
  float4 depthFactors : packoffset(c1);
  float2 fadeParams : packoffset(c2);
  float4 color : packoffset(c3);
  float4 colorMatrix0 : packoffset(c4);
  float4 colorMatrix1 : packoffset(c5);
  float4 colorMatrix2 : packoffset(c6);
  float4 halfResZPixelOffset : packoffset(c7);
  float4x4 shadowTransform0 : packoffset(c8);
  float4x4 shadowTransform1 : packoffset(c12);
  float4x4 shadowTransform2 : packoffset(c16);
  float4x4 shadowTransform3 : packoffset(c20);
  float4x4 viewTransform : packoffset(c24);
  float shadowFarPlane0 : packoffset(c28);
  float shadowFarPlane1 : packoffset(c28.y);
  float shadowFarPlane2 : packoffset(c28.z);
  float shadowFarPlane3 : packoffset(c28.w);
  float shaderTime : packoffset(c29);
  float4 pixelSize : packoffset(c30);
  float4 combineTextureWeights[2] : packoffset(c31);
  int wrecklessBlurIteration : packoffset(c33);
  float4 colorScale : packoffset(c34);
  float2 invTexelSize : packoffset(c35);
  int numSamples : packoffset(c35.z);
  float filterWidth : packoffset(c35.w);
  float4 seperableKernelWeights[8] : packoffset(c36);
  float4 radialBlurScales : packoffset(c44);
  float2 radialBlurCenter : packoffset(c45);
  float motionBlurAmount : packoffset(c45.z);
  float3 filmGrainColorScale : packoffset(c46);
  float4 filmGrainTextureScaleAndOffset : packoffset(c47);
  float4 effectColorMatrix0 : packoffset(c48);
  float4 effectColorMatrix1 : packoffset(c49);
  float4 effectColorMatrix2 : packoffset(c50);
  float4 effectScreenPosAndDistance : packoffset(c51);
  float effectFallof : packoffset(c52);
  float2 depthScaleFactors : packoffset(c52.y);
  float4 dofParams : packoffset(c53);
  float3 bloomScale : packoffset(c54);
  float3 invGamma : packoffset(c55);
  float adaptLuminanceFactor : packoffset(c55.w);
  float3 luminanceVector : packoffset(c56);
  float3 vignetteParams : packoffset(c57);
  float4 vignetteColor : packoffset(c58);
  float4 chromostereopsisParams : packoffset(c59);
  float exposureColorScale : packoffset(c60);
  float middleGray : packoffset(c60.y);
  float exposureFactor : packoffset(c60.z);
  float minExposure : packoffset(c60.w);
  float maxExposure : packoffset(c61);
  bool downsampleLogAverageEnable : packoffset(c61.y);
  float3 ghostStrengths : packoffset(c62);
  float2 ghost0Displacement : packoffset(c63);
  float2 ghost1Displacement : packoffset(c63.z);
  float2 ghost2Displacement : packoffset(c64);
  float3 ghostSubValues : packoffset(c65);
  float4 aberrationColor0 : packoffset(c66);
  float4 aberrationColor1 : packoffset(c67);
  float4 aberrationColor2 : packoffset(c68);
  float2 aberrationDisplacement1 : packoffset(c69);
  float2 aberrationDisplacement2 : packoffset(c69.z);
  float2 radialBlendDistanceCoefficients : packoffset(c70);
  float2 radialBlendCenter : packoffset(c70.z);
  float2 radialBlendScale : packoffset(c71);
}

SamplerState mainTexture_s : register(s0);
SamplerState tonemapBloomTexture_s : register(s1);
SamplerState flareTexture_s : register(s2);
SamplerState exposureTexture_s : register(s3);
Texture2D<float4> mainTexture : register(t0);
Texture2D<float4> tonemapBloomTexture : register(t1);
Texture2D<float4> flareTexture : register(t2);
Texture2D<float4> exposureTexture : register(t3);

// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float4 v1 : COLOR0,
  float2 v2 : TEXCOORD0,
  out float4 o0 : SV_Target0)
{
  float4 r0,r1,r2,r3;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5,-0.5) + v2.xy;
  r0.xy = vignetteParams.xy * r0.xy;
  r0.x = dot(r0.xy, r0.xy);
  r0.x = log2(r0.x);
  r0.x = vignetteParams.z * r0.x;
  r0.x = exp2(r0.x);
  r0.x = vignetteColor.w * r0.x;
  r0.yzw = float3(-1,-1,-1) + vignetteColor.xyz;
  r0.xyz = r0.xxx * r0.yzw + float3(1,1,1);
  r0.w = exposureTexture.Sample(exposureTexture_s, float2(0.5,0.5)).x;
  r1.xyz = colorScale.xyz * r0.www;
  r2.xyz = tonemapBloomTexture.Sample(tonemapBloomTexture_s, v2.xy).xyz;
  r3.xyzw = mainTexture.Sample(mainTexture_s, v2.xy).xyzw;
  r2.xyz = r2.xyz * bloomScale.xyz + r3.xyz;
  o0.w = r3.w;
  r1.xyz = r2.xyz * r1.xyz + float3(-0.00400000019,-0.00400000019,-0.00400000019);
  r1.xyz = max(float3(0,0,0), r1.xyz);
  r2.xyz = r1.xyz * float3(6.19999981,6.19999981,6.19999981) + float3(0.5,0.5,0.5);
  r2.xyz = r2.xyz * r1.xyz;
  r3.xyz = r1.xyz * float3(6.19999981,6.19999981,6.19999981) + float3(1.70000005,1.70000005,1.70000005);
  r1.xyz = r1.xyz * r3.xyz + float3(0.0599999987,0.0599999987,0.0599999987);
  r1.xyz = r2.xyz / r1.xyz;
  r1.xyz = min(float3(1,1,1), r1.xyz);
  
  #ifndef DISABLE_LENS_FLARE
  r2.xyz = flareTexture.Sample(flareTexture_s, v2.xy).xyz;
  r1.xyz = r2.xyz + r1.xyz;
  #endif
  
  r0.xyz = r1.xyz * r0.xyz;
  r0.w = 1;
  o0.x = dot(r0.xyzw, colorMatrix0.xyzw);
  o0.y = dot(r0.xyzw, colorMatrix1.xyzw);
  o0.z = dot(r0.xyzw, colorMatrix2.xyzw);
  return;
}