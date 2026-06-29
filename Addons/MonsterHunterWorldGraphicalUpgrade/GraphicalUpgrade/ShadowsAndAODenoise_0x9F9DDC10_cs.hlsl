groupshared struct { float val[2]; } g1[400];
groupshared struct { float val[1]; } g0[400];

cbuffer CBViewProjection : register(b0)
{
  row_major float4x4 fViewProj : packoffset(c0);
  row_major float4x4 fView : packoffset(c4);
  row_major float4x4 fProj : packoffset(c8);
  row_major float4x4 fViewI : packoffset(c12);
  row_major float4x4 fProjI : packoffset(c16);
  row_major float4x4 fViewProjI : packoffset(c20);
  float3 fCameraPos : packoffset(c24);
  float3 fCameraDir : packoffset(c25);
  float3 fZToLinear : packoffset(c26);
  float fCameraNearClip : packoffset(c26.w);
  float fCameraFarClip : packoffset(c27);
  float fCameraTargetDist : packoffset(c27.y);
  float4 fPassThrough : packoffset(c28);
  float3 fLODBasePos : packoffset(c29);
  row_major float4x4 fViewProjPF : packoffset(c30);
  row_major float4x4 fViewProjIPF : packoffset(c34);
  row_major float4x4 fViewPF : packoffset(c38);
  row_major float4x4 fProjPF : packoffset(c42);
  row_major float4x4 fViewProjIViewProjPF : packoffset(c46);
  row_major float4x4 fNoJitterProj : packoffset(c50);
  row_major float4x4 fNoJitterViewProj : packoffset(c54);
  row_major float4x4 fNoJitterViewProjI : packoffset(c58);
  row_major float4x4 fNoJitterViewProjIViewProjPF : packoffset(c62);
  float2 fPassThroughCorrect : packoffset(c66);
  bool bWideMonitor : packoffset(c66.z);
}

cbuffer CBScreen : register(b1)
{
  float2 fScreenOffset : packoffset(c0) = {1,-1};
  float2 fScreenScale : packoffset(c0.z) = {0.5,-0.5};
  float2 fScreenSize : packoffset(c1) = {1280,720};
  float2 fScreenInverseSize : packoffset(c1.z) = {0.000781250012,0.00138888892};
  uint2 iViewOffset : packoffset(c2);
  uint2 iViewSize : packoffset(c2.z);
  float2 fViewOffset : packoffset(c3);
  float2 fViewSize : packoffset(c3.z);
  float2 fViewInverseSize : packoffset(c4);
  float fContentScale : packoffset(c4.z);
  float fContentScalePF : packoffset(c4.w);
  float fContentScaleBase : packoffset(c5);
  float fContentScaleActual : packoffset(c5.y);
  float fContentScaleInverse : packoffset(c5.z);
  float fContentScaleBaseInverse : packoffset(c5.w);
  float fContentScaleActualInverse : packoffset(c6);
  float fContentScalePassScreen : packoffset(c6.y);
  bool bCheckerboard : packoffset(c6.z);
}

cbuffer CBGaussian : register(b3)
{
  float4 fOffset0 : packoffset(c0);
  float4 fOffset1 : packoffset(c1);
  float4 fWeight0 : packoffset(c2);
  float4 fWeight1 : packoffset(c3);
  float fEdgeSharpness : packoffset(c4) = {1};
}

Texture2D<float> tDepthMap : register(t0);
Texture2D<float> tPrimaryShadow : register(t1);
Texture2D<float> tSSAO : register(t2);
RWTexture2D<unorm float2> tSSShadowBlurMap : register(u0);

#define SHARPENING_MUL 15.0

// 3Dmigoto declarations
#define cmp -

[numthreads(16, 16, 1)]
void main(uint3 vThreadGroupID : SV_GroupID, uint3 vThreadIDInGroup : SV_GroupThreadID, uint3 vThreadID : SV_DispatchThreadID)
{
// Needs manual fix for instruction:
// unknown dcl_: dcl_uav_typed_texture2d (unorm,unorm,unorm,unorm) u0
  float4 r0,r1,r2,r3,r4,r5;
  uint4 bitmask, uiDest;
  float4 fDest;

// Needs manual fix for instruction:
// unknown dcl_: dcl_thread_group 16, 16, 1
  r0.x = mad((int)vThreadIDInGroup.y, 16, (int)vThreadIDInGroup.x);
  r0.yz = (uint2)vThreadGroupID.xy << int2(4,4);
  r0.yz = (int2)r0.yz + (int2)iViewOffset.xy;
  r0.yz = (int2)r0.yz + int2(-2,-2);
  r1.xy = (int2)iViewSize.xy + (int2)iViewOffset.xy;
  r1.xy = (int2)r1.xy + int2(-1,-1);
  uiDest.x = (uint)r0.x / 20;
  r3.x = (uint)r0.x % 20;
  r2.x = uiDest.x;
  r3.y = r2.x;
  r1.zw = (int2)r0.yz + (int2)r3.xy;
  r1.zw = max((int2)iViewOffset.xy, (int2)r1.zw);
  r2.xy = min((int2)r1.zw, (int2)r1.xy);
  r2.zw = float2(0,0);
  r1.z = tSSAO.Load(r2.xyw).x;
  r1.w = tPrimaryShadow.Load(r2.xyw).x;
  g1[r0.x].val[0/4] = r1.z;
  g1[r0.x].val[0/4+1] = r1.w;
  r0.w = tDepthMap.Load(r2.xyz).x;
  r0.w = fProj._m22 + r0.w;
  r0.w = 1 / r0.w;
  r0.w = fProj._m32 * r0.w;
  r0.w = -r0.w;
  g0[r0.x].val[0/4] = r0.w;
  r1.zw = cmp((uint2)r0.xx < int2(144,64));
  if (r1.z != 0) {
    r0.w = (int)r0.x + 256;
    uiDest.x = (uint)r0.w / 20;
    r3.x = (uint)r0.w % 20;
    r2.x = uiDest.x;
    r3.y = r2.x;
    r0.yz = (int2)r0.yz + (int2)r3.xy;
    r0.yz = max((int2)iViewOffset.xy, (int2)r0.yz);
    r2.xy = min((int2)r0.yz, (int2)r1.xy);
    r2.zw = float2(0,0);
    r0.y = tSSAO.Load(r2.xyw).x;
    r0.z = tPrimaryShadow.Load(r2.xyw).x;
    g1[r0.w].val[0/4] = r0.y;
    g1[r0.w].val[0/4+1] = r0.z;
    r0.y = tDepthMap.Load(r2.xyz).x;
    r0.y = fProj._m22 + r0.y;
    r0.y = 1 / r0.y;
    r0.y = fProj._m32 * r0.y;
    r0.y = -r0.y;
    g0[r0.w].val[0/4] = r0.y;
  }
  GroupMemoryBarrierWithGroupSync();
  r0.y = mad((int)vThreadIDInGroup.y, 20, (int)vThreadIDInGroup.x);
  r2.xyzw = (int4)r0.yyyy + int4(2,1,4,3);
  r0.z = g1[r2.x].val[0/4];
  r0.w = g1[r2.x].val[0/4+1];
  r1.x = g1[r0.y].val[0/4];
  r1.y = g1[r0.y].val[0/4+1];
  r3.x = g1[r2.y].val[0/4];
  r3.y = g1[r2.y].val[0/4+1];
  r3.z = g1[r2.z].val[0/4];
  r3.w = g1[r2.z].val[0/4+1];
  r4.x = g1[r2.w].val[0/4];
  r4.y = g1[r2.w].val[0/4+1];
  r1.z = g0[r2.x].val[0/4];
  r4.z = g0[r0.y].val[0/4];
  r2.y = g0[r2.y].val[0/4];
  r2.z = g0[r2.z].val[0/4];
  r2.w = g0[r2.w].val[0/4];
  r4.z = r4.z + -r1.z;
  r4.z = saturate(fEdgeSharpness * SHARPENING_MUL * abs(r4.z));
  r4.z = 1 + -r4.z;
  r4.w = fWeight0.x * r4.z;
  r2.yz = r2.yz + -r1.zz;
  r2.yz = saturate(fEdgeSharpness * SHARPENING_MUL * abs(r2.yz));
  r2.yz = float2(1,1) + -r2.yz;
  r5.xy = fWeight0.yx * r2.yz;
  r1.z = r2.w + -r1.z;
  r1.z = saturate(fEdgeSharpness * SHARPENING_MUL * abs(r1.z));
  r1.z = 1 + -r1.z;
  r2.w = fWeight0.y * r1.z;
  r1.xy = r4.ww * r1.xy;
  r0.zw = r0.zw * fWeight0.zz + r1.xy;
  r0.zw = r3.xy * r5.xx + r0.zw;
  r0.zw = r3.zw * r5.yy + r0.zw;
  r0.zw = r4.xy * r2.ww + r0.zw;
  r1.x = r4.z * fWeight0.x + fWeight0.z;
  r1.x = r2.y * fWeight0.y + r1.x;
  r1.x = r2.z * fWeight0.x + r1.x;
  r1.x = r1.z * fWeight0.y + r1.x;
  r1.x = 1.1920929e-007 + r1.x;
  r0.zw = r0.zw / r1.xx;
  if (r1.w != 0) {
    r3.xyzw = (int4)r0.yyyy + int4(322,320,321,324);
    r1.x = g1[r3.x].val[0/4];
    r1.y = g1[r3.x].val[0/4+1];
    r2.y = g1[r3.y].val[0/4];
    r2.z = g1[r3.y].val[0/4+1];
    r4.x = g1[r3.z].val[0/4];
    r4.y = g1[r3.z].val[0/4+1];
    r4.z = g1[r3.w].val[0/4];
    r4.w = g1[r3.w].val[0/4+1];
    r1.z = (int)r0.y + 323;
    r5.x = g1[r1.z].val[0/4];
    r5.y = g1[r1.z].val[0/4+1];
    r2.w = g0[r3.x].val[0/4];
    r3.x = g0[r3.y].val[0/4];
    r3.y = g0[r3.z].val[0/4];
    r3.z = g0[r3.w].val[0/4];
    r1.z = g0[r1.z].val[0/4];
    r3.xyz = r3.xyz + -r2.www;
    r3.xyz = saturate(fEdgeSharpness * SHARPENING_MUL * abs(r3.xyz));
    r3.xyz = float3(1,1,1) + -r3.xyz;
    r3.w = fWeight0.x * r3.x;
    r5.zw = fWeight0.yx * r3.yz;
    r1.z = r1.z + -r2.w;
    r1.z = saturate(fEdgeSharpness * SHARPENING_MUL * abs(r1.z));
    r1.z = 1 + -r1.z;
    r2.w = fWeight0.y * r1.z;
    r2.yz = r3.ww * r2.yz;
    r1.xy = r1.xy * fWeight0.zz + r2.yz;
    r1.xy = r4.xy * r5.zz + r1.xy;
    r1.xy = r4.zw * r5.ww + r1.xy;
    r1.xy = r5.xy * r2.ww + r1.xy;
    r2.y = r3.x * fWeight0.x + fWeight0.z;
    r2.y = r3.y * fWeight0.y + r2.y;
    r2.y = r3.z * fWeight0.x + r2.y;
    r1.z = r1.z * fWeight0.y + r2.y;
    r1.z = 1.1920929e-007 + r1.z;
    r1.xy = r1.xy / r1.zz;
  } else {
    r1.xy = float2(0,0);
  }
  GroupMemoryBarrierWithGroupSync();
  g1[r0.x].val[0/4] = r0.z;
  g1[r0.x].val[0/4+1] = r0.w;
  if (r1.w != 0) {
    r0.x = (int)r0.x + 256;
    g1[r0.x].val[0/4] = r1.x;
    g1[r0.x].val[0/4+1] = r1.y;
  }
  GroupMemoryBarrierWithGroupSync();
  r0.xz = cmp((uint2)vThreadID.xy < (uint2)iViewSize.xy);
  r0.x = r0.z ? r0.x : 0;
  if (r0.x != 0) {
    r0.x = (uint)vThreadIDInGroup.y << 4;
    r0.x = (int)r0.x + 32;
    r0.x = (int)r0.x + (int)vThreadIDInGroup.x;
    r1.xyzw = (int4)r0.yyyy + int4(42,22,82,62);
    r0.y = g1[r0.x].val[0/4];
    r0.z = g1[r0.x].val[0/4+1];
    r3.xyzw = (int4)r0.xxxx + int4(-32,-16,32,16);
    r0.x = g1[r3.x].val[0/4];
    r0.w = g1[r3.x].val[0/4+1];
    r2.y = g1[r3.y].val[0/4];
    r2.z = g1[r3.y].val[0/4+1];
    r3.x = g1[r3.z].val[0/4];
    r3.y = g1[r3.z].val[0/4+1];
    r3.z = g1[r3.w].val[0/4];
    r3.w = g1[r3.w].val[0/4+1];
    r1.x = g0[r1.x].val[0/4];
    r2.x = g0[r2.x].val[0/4];
    r1.y = g0[r1.y].val[0/4];
    r1.z = g0[r1.z].val[0/4];
    r1.w = g0[r1.w].val[0/4];
    r2.x = r2.x + -r1.x;
    r2.x = saturate(fEdgeSharpness * SHARPENING_MUL * abs(r2.x));
    r2.x = 1 + -r2.x;
    r2.w = fWeight1.x * r2.x;
    r1.yz = r1.yz + -r1.xx;
    r1.yz = saturate(fEdgeSharpness * SHARPENING_MUL * abs(r1.yz));
    r1.yz = float2(1,1) + -r1.yz;
    r4.xy = fWeight1.yx * r1.yz;
    r1.x = r1.w + -r1.x;
    r1.x = saturate(fEdgeSharpness * SHARPENING_MUL * abs(r1.x));
    r1.x = 1 + -r1.x;
    r1.w = fWeight1.y * r1.x;
    r5.xyzw = r2.wwww * r0.xwxx;
    r0.xyzw = r0.yzyy * fWeight1.zzzz + r5.xyzw;
    r0.xyzw = r2.yzyy * r4.xxxx + r0.xyzw;
    r0.xyzw = r3.xyxx * r4.yyyy + r0.xyzw;
    r0.xyzw = r3.zwzz * r1.wwww + r0.xyzw;
    r1.w = r2.x * fWeight1.x + fWeight1.z;
    r1.y = r1.y * fWeight1.y + r1.w;
    r1.y = r1.z * fWeight1.x + r1.y;
    r1.x = r1.x * fWeight1.y + r1.y;
    r1.x = 1.1920929e-007 + r1.x;
    r0.xyzw = r0.xyzw / r1.xxxx;
    r1.xyzw = (int4)vThreadID.xyyy + (int4)iViewOffset.xyyy;
  // No code for instruction (needs manual fix):
    //store_uav_typed u0.xyzw, r1.xyzw, r0.xyzw
    tSSShadowBlurMap[r1.xy] = r0.xy;
  }
  return;
}