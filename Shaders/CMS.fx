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

texture3D LUTTex < source = "CMSLUT.dds"; >
{
	Width = LUT_SIZE;
	Height = LUT_SIZE;
	Depth = LUT_SIZE;
	Format = RGBA8;
};

sampler3D smpLUT
{
	Texture = LUTTex;
};

float3 trilinear_interpolation(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
	float3 color = saturate(tex2D(ReShade::BackBuffer, texcoord).rgb);
	const float scale = float(LUT_SIZE - 1) / float(LUT_SIZE);
	const float offset = 1.0 / float(LUT_SIZE * 2);
	color = tex3D(smpLUT, color * scale + offset).rgb;

	#if DITHERING
	return color + TriDither(color, texcoord, BUFFER_COLOR_BIT_DEPTH);
	#else
	return color;
	#endif
}

float3 tetrahedral_interpolation(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
	float3 color = tex2D(ReShade::BackBuffer, texcoord).rgb;
	const float3 index = saturate(color) * float(LUT_SIZE - 1);
	
	// Get barycentric weights.
	// See https://doi.org/10.2312/egp.20211031
	//
	
	const float3 r = frac(index);
	const bool3 c = r.xyz >= r.yzx;
	const bool c_xy = c.x;
	const bool c_yz = c.y;
	const bool c_zx = c.z;
	const bool c_yx =!c.x;
	const bool c_zy =!c.y;
	const bool c_xz =!c.z;
	bool cond;
	float3 s = 0.0;
	float3 vert2 = 0.0;
	float3 vert3 = 1.0;

	#define order(x,y,z) \
	cond = c_ ## x ## y && c_ ## y ## z; \
	s = cond ? r.x ## y ## z : s; \
	vert2.x = cond ? 1.0 : vert2.x; \
	vert3.z = cond ? 0.0 : vert3.z;
	
	order(x, y, z)
	order(x, z, y)
	order(z, x, y)
	order(z, y, x)
	order(y, z, x)
	order(y, x, z)
	
	//
	
	const float3 base = floor(index) + 0.5;
	color = tex3D(smpLUT, base / float(LUT_SIZE)).rgb * (1.0 - s.x) + tex3D(smpLUT, (base + 1.0) / float(LUT_SIZE)).rgb * s.z + tex3D(smpLUT, (base + vert2) / float(LUT_SIZE)).rgb * (s.x - s.y) + tex3D(smpLUT, (base + vert3) / float(LUT_SIZE)).rgb * (s.y - s.z);
	
	#if DITHERING
	return color + TriDither(color, texcoord, BUFFER_COLOR_BIT_DEPTH);
	#else
	return color;
	#endif
}

technique CMS < ui_label = "Color Management System"; >
{
	pass
	{
		VertexShader = PostProcessVS;

		#if TETRAHEDRAL_INTERPOLATION
		PixelShader = tetrahedral_interpolation;
		#else
		PixelShader = trilinear_interpolation;
		#endif
	}
}