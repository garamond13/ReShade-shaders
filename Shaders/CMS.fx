// Color Management System

#include "ReShade.fxh"
#include "TriDither.fxh"

#ifndef LUT_SIZE
#define LUT_SIZE 65
#endif

#ifndef TETRAHEDRAL_INTERPOLATION
#define TETRAHEDRAL_INTERPOLATION 0
#endif

#ifndef DITHERING
#define DITHERING 1
#endif

#ifndef CUBE_LUT
#define CUBE_LUT 0
#endif

#if CUBE_LUT
#define TEX_SOURCE "CMSLUT.cube"
#define TEX_FORMAT RGBA32F
#else
#define TEX_SOURCE "CMSLUT.dds"
#define TEX_FORMAT RGBA8
#endif

texture3D LUTTex < source = TEX_SOURCE; >
{
	Width = LUT_SIZE;
	Height = LUT_SIZE;
	Depth = LUT_SIZE;
	Format = TEX_FORMAT;
};

sampler3D smpLUT
{
	Texture = LUTTex;
};

float4 sample_trilinear(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
	float4 color = tex2D(ReShade::BackBuffer, texcoord);
	const float scale = float(LUT_SIZE - 1) / float(LUT_SIZE);
	const float offset = 1.0 / float(LUT_SIZE * 2);
	color.rgb = tex3D(smpLUT, saturate(color.rgb) * scale + offset).rgb;

	#if DITHERING
	color.rgb = color.rgb + TriDither(color.rgb, texcoord, BUFFER_COLOR_BIT_DEPTH);
	#endif

	return float4(color.rgb, color.a);
}

float4 sample_tetrahedral(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
	float4 color = tex2D(ReShade::BackBuffer, texcoord);
	const float3 coord = saturate(color.rgb) * float(LUT_SIZE - 1);
	
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
	
	const int3 base = floor(coord);
	const float3 v0 = tex3Dfetch(smpLUT, base, 0).rgb * bary.x;
	const float3 v1 = tex3Dfetch(smpLUT, base + 1, 0).rgb * bary.y;
	const float3 v2 = tex3Dfetch(smpLUT, base + vert2, 0).rgb * bary.z;
	const float3 v3 = tex3Dfetch(smpLUT, base + vert3, 0).rgb * bary.w;
	color.rgb = v0 + v1 + v2 + v3;
	
	#if DITHERING
	color.rgb = color.rgb + TriDither(color.rgb, texcoord, BUFFER_COLOR_BIT_DEPTH);
	#endif

	return float4(color.rgb, color.a);
}

technique CMS < ui_label = "Color Management System"; >
{
	pass
	{
		VertexShader = PostProcessVS;

		#if TETRAHEDRAL_INTERPOLATION
		PixelShader = sample_tetrahedral;
		#else
		PixelShader = sample_trilinear;
		#endif
	}
}