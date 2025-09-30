// Sample LUT
// Using tetrahedral interpolation.

Texture2D tex : register(t0);
Texture3D lut : register(t1);

#ifndef LUT_SIZE
#define LUT_SIZE 65
#endif

float4 main(float4 pos : SV_Position) : SV_Target
{
	float4 color = tex.Load(int3(pos.xy, 0));
	const float3 coord = saturate(color.rgb) * (float(LUT_SIZE) - 1.0);
	
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
	color.rgb = v0 + v1 + v2 + v3;

	return float4(color.rgb, color.a);
}