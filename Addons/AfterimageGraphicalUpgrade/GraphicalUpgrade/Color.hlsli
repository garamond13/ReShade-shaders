#ifndef __COLOR_HLSLI__
#define __COLOR_HLSLI__

#ifndef GAMMA
#define GAMMA 2.2
#endif

float3 linear_to_srgb(float3 rgb)
{
	return rgb < 0.0031308 ? 12.92 * rgb : 1.055 * pow(rgb, 1.0 / 2.4) - 0.055;
}

float3 srgb_to_linear(float3 rgb)
{
	return rgb < 0.04045 ? rgb / 12.92 : pow((rgb + 0.055) / 1.055, 2.4);
}

float3 linear_to_gamma(float3 rgb)
{
	return pow(rgb, 1.0 / GAMMA);
}

float3 gamma_to_linear(float3 rgb)
{
	return pow(rgb, GAMMA);
}

float3 sample_tetrahedral(float3 color, Texture3D lut, int lut_size)
{
	const float3 coord = saturate(color) * (float)(lut_size - 1);

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

	// Interpolate between 4 vertices using barycentric weights.
	const int3 base = floor(coord);
	const float3 v0 = lut.Load(int4(base, 0)).rgb * bary.x;
	const float3 v1 = lut.Load(int4(base + 1, 0)).rgb * bary.y;
	const float3 v2 = lut.Load(int4(base + vert2, 0)).rgb * bary.z;
	const float3 v3 = lut.Load(int4(base + vert3, 0)).rgb * bary.w;
	return v0 + v1 + v2 + v3;
}

#endif // __COLOR_HLSLI__