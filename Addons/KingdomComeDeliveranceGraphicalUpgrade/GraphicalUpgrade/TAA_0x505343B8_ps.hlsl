cbuffer PER_BATCH : register(b0)
{
  float4 PS_NearFarClipDist : packoffset(c0);
  float4 PS_ScreenSize : packoffset(c1);
  row_major float4x4 mReprojection : packoffset(c2);
}

cbuffer CBStaticPerInst : register(b8)
{
  struct
  {
    row_major float3x4 SPIObjWorldMat;
    float4 SPIBendInfo;
    float4 SPIRainLayerParams;
    float4 SPIVertexAO;
    float4 SPIAlphaTest;
  } SPI[128] : packoffset(c0);
}

SamplerState _tex3_s : register(s3);
SamplerState _tex4_s : register(s4);
SamplerState _tex5_s : register(s5);
Texture2D<float4> _tex3 : register(t3);
Texture2D<float4> _tex4 : register(t4);
Texture2D<float4> _tex5 : register(t5);
Texture2D<float4> PostAA_DepthTex : register(t16);

// 3Dmigoto declarations
#define cmp -

void main(
  float4 v0 : SV_Position0,
  float4 v1 : TEXCOORD0,
  float4 v2 : TEXCOORD1,
  out float2 o0 : SV_Target0) // Originally o0 is float4.
{
  float4 r0,r1,r2,r3,r4,r5,r6,r7,r8;
  uint4 bitmask, uiDest;
  float4 fDest;

  // Generate camera MVs and add them to dynamic objects MVs.
  r0.xy = (int2)v0.xy;
  r0.zw = float2(0,0);
  r0.x = PostAA_DepthTex.Load(r0.xyz).x;
  r0.yzw = mReprojection._m10_m11_m13 * v1.yyy;
  r0.yzw = v1.xxx * mReprojection._m00_m01_m03 + r0.yzw;
  r0.xyz = r0.xxx * mReprojection._m20_m21_m23 + r0.yzw;
  r0.xyz = mReprojection._m30_m31_m33 + r0.xyz;
  r0.xy = r0.xy / r0.zz;
  r0.zw = _tex3.Sample(_tex3_s, v1.xy).xy;
  r1.x = cmp(r0.z == 0.000000);
  r0.xy = -v1.xy + r0.xy;
  r0.zw = float2(-0.498039216,-0.498039216) + r0.zw;
  r1.yz = r0.zw + r0.zw;
  r1.yz = r1.yz * r1.yz;
  r0.zw = cmp(float2(0,0) < r0.zw);
  r0.zw = r0.zw ? float2(1,1) : float2(-1,-1);
  r0.zw = r1.yz * r0.zw;
  r0.xy = r1.xx ? r0.xy : r0.zw;
  
  // Output MVs.
  o0.xy = r0.xy;

  // Rest of the original TAA.
  /*
  r1.xyz = _tex4.Sample(_tex4_s, v1.xy).xyz;
  r2.xyzw = PS_ScreenSize.zwzw * float4(-2,-2,2,-2) + v1.xyxy;
  r3.xyz = _tex4.Sample(_tex4_s, r2.xy).xyz;
  r2.xyz = _tex4.Sample(_tex4_s, r2.zw).xyz;
  r0.zw = PS_ScreenSize.zw * float2(-2,2) + v1.xy;
  r4.xyz = _tex4.Sample(_tex4_s, r0.zw).xyz;
  r0.zw = PS_ScreenSize.zw * float2(2,2) + v1.xy;
  r5.xyz = _tex4.Sample(_tex4_s, r0.zw).xyz;
  r0.xy = v1.xy + r0.xy;
  r6.xyz = _tex5.Sample(_tex5_s, r0.xy).xyz;
  r0.zw = r0.xy * float2(2,2) + float2(-1,-1);
  r0.z = max(abs(r0.z), abs(r0.w));
  r0.z = cmp(r0.z < 1);
  if (r0.z != 0) {
    r7.xyz = min(r3.xyz, r2.xyz);
    r7.xyz = min(r7.xyz, r4.xyz);
    r7.xyz = min(r7.xyz, r5.xyz);
    r7.xyz = min(r7.xyz, r1.xyz);
    r2.xyz = max(r3.xyz, r2.xyz);
    r2.xyz = max(r2.xyz, r4.xyz);
    r2.xyz = max(r2.xyz, r5.xyz);
    r2.xyz = max(r2.xyz, r1.xyz);
    r3.xyz = r2.xyz + r7.xyz;
    r4.xyz = -r3.xyz * float3(0.5,0.5,0.5) + r2.xyz;
    r5.xyz = -r6.xyz + r1.xyz;
    r3.xyz = -r3.xyz * float3(0.5,0.5,0.5) + r6.xyz;
    r0.z = dot(r5.xyz, r5.xyz);
    r0.z = sqrt(r0.z);
    r0.z = cmp(r0.z >= 9.99999997e-007);
    r5.xyz = rcp(r5.xyz);
    r8.xyz = r4.xyz + -r3.xyz;
    r8.xyz = r8.xyz * r5.xyz;
    r3.xyz = -r4.xyz + -r3.xyz;
    r3.xyz = r3.xyz * r5.xyz;
    r3.xyz = min(r8.xyz, r3.xyz);
    r0.w = max(r3.x, r3.y);
    r0.w = saturate(max(r0.w, r3.z));
    r0.z = r0.z ? r0.w : 1;
    r3.xyzw = PS_ScreenSize.zwzw * float4(-2,-2,2,-2) + r0.xyxy;
    r4.xyz = _tex5.SampleLevel(_tex5_s, r3.xy, 0).xyz;
    r3.xyz = _tex5.SampleLevel(_tex5_s, r3.zw, 0).xyz;
    r5.xy = PS_ScreenSize.zw * float2(-2,2) + r0.xy;
    r5.xyz = _tex5.SampleLevel(_tex5_s, r5.xy, 0).xyz;
    r0.xy = PS_ScreenSize.zw * float2(2,2) + r0.xy;
    r0.xyw = _tex5.SampleLevel(_tex5_s, r0.xy, 0).xyz;
    r8.xyz = max(r4.xyz, r7.xyz);
    r8.xyz = min(r8.xyz, r2.xyz);
    r4.xyz = r8.xyz + -r4.xyz;
    r1.w = dot(r4.xyz, r4.xyz);
    r1.w = sqrt(r1.w);
    r4.xyz = max(r3.xyz, r7.xyz);
    r4.xyz = min(r4.xyz, r2.xyz);
    r3.xyz = r4.xyz + -r3.xyz;
    r2.w = dot(r3.xyz, r3.xyz);
    r2.w = sqrt(r2.w);
    r1.w = r2.w + r1.w;
    r3.xyz = max(r5.xyz, r7.xyz);
    r3.xyz = min(r3.xyz, r2.xyz);
    r3.xyz = r3.xyz + -r5.xyz;
    r2.w = dot(r3.xyz, r3.xyz);
    r2.w = sqrt(r2.w);
    r1.w = r2.w + r1.w;
    r3.xyz = max(r0.xyw, r7.xyz);
    r2.xyz = min(r3.xyz, r2.xyz);
    r0.xyw = r2.xyz + -r0.xyw;
    r0.x = dot(r0.xyw, r0.xyw);
    r0.x = sqrt(r0.x);
    r0.x = r1.w + r0.x;
    r0.x = cmp(r0.x < 0.0199999996);
    r0.x = r0.x ? 0 : r0.z;
  } else {
    r0.x = 1;
  }
  r0.yzw = r6.xyz + -r1.xyz;
  r0.y = dot(r0.yzw, r0.yzw);
  r0.y = sqrt(r0.y);
  r0.y = 10 * r0.y;
  r0.y = min(1, r0.y);
  r2.xyz = -r6.xyz + r1.xyz;
  r0.xzw = r0.xxx * r2.xyz + r6.xyz;
  r0.y = r0.y * 2.5 + 2.5;
  r0.y = rcp(r0.y);
  r1.xyz = r1.xyz + -r0.xzw;
  r0.xyz = r0.yyy * r1.xyz + r0.xzw;
  r1.xyz = cmp(r0.xyz < float3(0.00313080009,0.00313080009,0.00313080009));
  r2.xyz = float3(12.9200001,12.9200001,12.9200001) * r0.xyz;
  r0.xyz = log2(r0.xyz);
  r0.xyz = float3(0.416666657,0.416666657,0.416666657) * r0.xyz;
  r0.xyz = exp2(r0.xyz);
  r0.xyz = r0.xyz * float3(1.05499995,1.05499995,1.05499995) + float3(-0.0549999997,-0.0549999997,-0.0549999997);
  o0.xyz = r1.xyz ? r2.xyz : r0.xyz;
  o0.w = 0;
  return;
  */
}