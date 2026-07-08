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

cbuffer CBPostAA : register(b0)
{
  struct
  {
    row_major float4x4 matReprojection;
    float4 params;
    float4 screenSize;
    float4 worldViewPos;
    float4 fxaaParams;
  } cbPostAA : packoffset(c0);
}

SamplerState ssPostAALinear_s : register(s0);
Texture2D<float4> PostAA_CurrentSceneTex : register(t0);
Texture2D<float4> PostAA_PreviousSceneTex : register(t1);
Texture2D<float2> PostAA_VelocityObjectsTex : register(t3);
Texture2D<float> PostAA_DeviceDepthTex : register(t16);

// 3Dmigoto declarations
#define cmp -

void main(
  float4 v0 : SV_Position0,
  float4 v1 : TEXCOORD0,
  float2 v2 : TEXCOORD1,
  out float2 o0 : SV_Target0) // Originally, out float4 o0 : SV_Target0
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7,r8,r9;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = (int2)v0.xy;
  r0.zw = float2(0,0);
  r1.z = PostAA_DeviceDepthTex.Load(r0.xyw).x;
  r1.xy = v1.xy;
  r1.w = 1;
  r2.x = dot(cbPostAA.matReprojection._m00_m01_m02_m03, r1.xyzw);
  r2.y = dot(cbPostAA.matReprojection._m10_m11_m12_m13, r1.xyzw);
  r1.x = dot(cbPostAA.matReprojection._m30_m31_m32_m33, r1.xyzw);
  r1.xy = r2.xy / r1.xx;
  r1.xy = -v1.xy + r1.xy;
  r0.xy = PostAA_VelocityObjectsTex.Load(r0.xyz).xy;
  r0.z = r0.x != 0.0 || r0.y != 0.0; // Originally, r0.z = cmp(r0.x != 0.000000);
  r0.xy = r0.zz ? r0.xy : r1.xy;

  o0.xy = r0.xy;

  // Rest of TAA.
  /*
  r0.zw = CV_HPosScale.xy * v1.xy;
  r0.xy = v1.xy + r0.xy;
  r1.xy = CV_HPosScale.zw * r0.xy;
  r0.zw = max(float2(0,0), r0.zw);
  r0.zw = min(CV_HPosClamp.xy, r0.zw);
  r2.xyz = PostAA_CurrentSceneTex.SampleLevel(ssPostAALinear_s, r0.zw, 0).xyz;
  r0.zw = max(float2(0,0), r1.xy);
  r0.zw = min(CV_HPosClamp.zw, r0.zw);
  r3.xyz = PostAA_PreviousSceneTex.SampleLevel(ssPostAALinear_s, r0.zw, 0).xyz;
  r0.zw = r1.xy * float2(2,2) + float2(-1,-1);
  r0.z = max(abs(r0.z), abs(r0.w));
  r0.z = cmp(r0.z < 1);
  if (r0.z != 0) {
    r0.zw = v1.xy * CV_HPosScale.xy + -cbPostAA.screenSize.zw;
    r0.zw = max(float2(0,0), r0.zw);
    r0.zw = min(CV_HPosClamp.xy, r0.zw);
    r1.xyz = PostAA_CurrentSceneTex.SampleLevel(ssPostAALinear_s, r0.zw, 0).xyz;
    r4.xyzw = float4(1,-1,-1,1) * cbPostAA.screenSize.zwzw;
    r5.xyzw = v1.xyxy * CV_HPosScale.xyxy + r4.xyzw;
    r5.xyzw = max(float4(0,0,0,0), r5.xyzw);
    r5.xyzw = min(CV_HPosClamp.xyxy, r5.xyzw);
    r6.xyz = PostAA_CurrentSceneTex.SampleLevel(ssPostAALinear_s, r5.xy, 0).xyz;
    r5.xyz = PostAA_CurrentSceneTex.SampleLevel(ssPostAALinear_s, r5.zw, 0).xyz;
    r0.zw = v1.xy * CV_HPosScale.xy + cbPostAA.screenSize.zw;
    r0.zw = max(float2(0,0), r0.zw);
    r0.zw = min(CV_HPosClamp.xy, r0.zw);
    r7.xyz = PostAA_CurrentSceneTex.SampleLevel(ssPostAALinear_s, r0.zw, 0).xyz;
    r8.xyz = min(r6.xyz, r5.xyz);
    r8.xyz = min(r8.xyz, r1.xyz);
    r9.xyz = min(r7.xyz, r2.xyz);
    r8.xyz = min(r9.xyz, r8.xyz);
    r5.xyz = max(r6.xyz, r5.xyz);
    r1.xyz = max(r5.xyz, r1.xyz);
    r5.xyz = max(r7.xyz, r2.xyz);
    r1.xyz = max(r5.xyz, r1.xyz);
    r5.xyz = r1.xyz + r8.xyz;
    r6.xyz = -r5.xyz * float3(0.5,0.5,0.5) + r1.xyz;
    r7.xyz = -r3.xyz + r2.xyz;
    r5.xyz = -r5.xyz * float3(0.5,0.5,0.5) + r3.xyz;
    r0.z = dot(r7.xyz, r7.xyz);
    r0.z = sqrt(r0.z);
    r0.z = cmp(r0.z >= 9.99999997e-007);
    r7.xyz = rcp(r7.xyz);
    r9.xyz = r6.xyz + -r5.xyz;
    r9.xyz = r9.xyz * r7.xyz;
    r5.xyz = -r6.xyz + -r5.xyz;
    r5.xyz = r5.xyz * r7.xyz;
    r5.xyz = min(r9.xyz, r5.xyz);
    r0.w = max(r5.x, r5.y);
    r0.w = saturate(max(r0.w, r5.z));
    r0.z = r0.z ? r0.w : 1;
    r5.xy = r0.xy * CV_HPosScale.zw + -cbPostAA.screenSize.zw;
    r5.xy = max(float2(0,0), r5.xy);
    r5.xy = min(CV_HPosClamp.zw, r5.xy);
    r5.xyz = PostAA_PreviousSceneTex.SampleLevel(ssPostAALinear_s, r5.xy, 0).xyz;
    r4.xyzw = r0.xyxy * CV_HPosScale.zwzw + r4.xyzw;
    r4.xyzw = max(float4(0,0,0,0), r4.xyzw);
    r4.xyzw = min(CV_HPosClamp.zwzw, r4.xyzw);
    r6.xyz = PostAA_PreviousSceneTex.SampleLevel(ssPostAALinear_s, r4.xy, 0).xyz;
    r4.xyz = PostAA_PreviousSceneTex.SampleLevel(ssPostAALinear_s, r4.zw, 0).xyz;
    r0.xy = r0.xy * CV_HPosScale.zw + cbPostAA.screenSize.zw;
    r0.xy = max(float2(0,0), r0.xy);
    r0.xy = min(CV_HPosClamp.zw, r0.xy);
    r0.xyw = PostAA_PreviousSceneTex.SampleLevel(ssPostAALinear_s, r0.xy, 0).xyz;
    r7.xyz = max(r5.xyz, r8.xyz);
    r7.xyz = min(r7.xyz, r1.xyz);
    r5.xyz = r7.xyz + -r5.xyz;
    r1.w = dot(r5.xyz, r5.xyz);
    r1.w = sqrt(r1.w);
    r5.xyz = max(r6.xyz, r8.xyz);
    r5.xyz = min(r5.xyz, r1.xyz);
    r5.xyz = r5.xyz + -r6.xyz;
    r2.w = dot(r5.xyz, r5.xyz);
    r2.w = sqrt(r2.w);
    r1.w = r2.w + r1.w;
    r5.xyz = max(r4.xyz, r8.xyz);
    r5.xyz = min(r5.xyz, r1.xyz);
    r4.xyz = r5.xyz + -r4.xyz;
    r2.w = dot(r4.xyz, r4.xyz);
    r2.w = sqrt(r2.w);
    r1.w = r2.w + r1.w;
    r4.xyz = max(r0.xyw, r8.xyz);
    r1.xyz = min(r4.xyz, r1.xyz);
    r0.xyw = r1.xyz + -r0.xyw;
    r0.x = dot(r0.xyw, r0.xyw);
    r0.x = sqrt(r0.x);
    r0.x = r1.w + r0.x;
    r0.x = cmp(r0.x < 0.0199999996);
    r0.x = r0.x ? 0 : r0.z;
  } else {
    r0.x = 1;
  }
  r0.yzw = r3.xyz + -r2.xyz;
  r0.y = dot(r0.yzw, r0.yzw);
  r0.y = sqrt(r0.y);
  r0.y = 10 * r0.y;
  r0.y = min(1, r0.y);
  r1.xyz = -r3.xyz + r2.xyz;
  r0.xzw = r0.xxx * r1.xyz + r3.xyz;
  r0.y = r0.y * 2.5 + 2.5;
  r0.y = rcp(r0.y);
  r1.xyz = r2.xyz + -r0.xzw;
  o0.xyz = r0.yyy * r1.xyz + r0.xzw;
  o0.w = 0;
  return;
  */
}