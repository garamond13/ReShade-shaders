#ifndef __COMMON_HLSLI__
#define __COMMON_HLSLI__

void fullscreen_triangle(uint vertex_id, out float4 position, out float2 texcoord)
{
	texcoord = float2((vertex_id << 1) & 2, vertex_id & 2);
	position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

float3 linear_to_srgb(float3 rgb)
{
	return rgb < 0.0031308 ? 12.92 * rgb : 1.055 * pow(rgb, 1.0 / 2.4) - 0.055;
}

float3 srgb_to_linear(float3 rgb)
{
	return rgb < 0.04045 ? rgb / 12.92 : pow((rgb + 0.055) / 1.055, 2.4);
}

float3 linear_to_gamma(float3 rgb, float gamma)
{
	return pow(rgb, 1.0 / gamma);
}

float3 gamma_to_linear(float3 rgb, float gamma)
{
	return pow(rgb, gamma);
}

float get_luma(float3 color)
{
	return dot(color, float3(0.2126, 0.7152, 0.0722));
}

// `p` should be in range (0, +inf).
float3 smooth_saturate(float3 color, float p)
{
	color = max(0.0, color);
	return color / pow(1.0 + pow(color, p), 1.0 / p);
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

// Bicubic upsampling in 4 texture fetches.
//
// f(x) = (4 + 3 * |x|^3 – 6 * |x|^2) / 6 for 0 <= |x| <= 1
// f(x) = (2 – |x|)^3 / 6 for 1 < |x| <= 2
// f(x) = 0 otherwise
//
// Source: https://www.researchgate.net/publication/220494113_Efficient_GPU-Based_Texture_Interpolation_using_Uniform_B-Splines
//
// smp has to be linear clamp!
float4 sample_bicubic(Texture2D tex, SamplerState smp, float2 texcoord)
{
	float2 src_size;
	tex.GetDimensions(src_size.x, src_size.y);
	float2 inv_src_size = 1.0 / src_size;

	// transform the coordinate from [0,extent] to [-0.5, extent-0.5]
	float2 coord_grid = texcoord * src_size - 0.5;
	float2 index = floor(coord_grid);
	float2 fraction = coord_grid - index;
	float2 one_frac = 1.0 - fraction;
	float2 one_frac2 = one_frac * one_frac;
	float2 fraction2 = fraction * fraction;
	float2 w0 = 1.0 / 6.0 * one_frac2 * one_frac;
	float2 w1 = 2.0 / 3.0 - 0.5 * fraction2 * (2.0 - fraction);
	float2 w2 = 2.0 / 3.0 - 0.5 * one_frac2 * (2.0 - one_frac);
	float2 w3 = 1.0 / 6.0 * fraction2 * fraction;
	float2 g0 = w0 + w1;
	float2 g1 = w2 + w3;

	// h0 = w1/g0 - 1, move from [-0.5, extent-0.5] to [0, extent]
	float2 h0 = ((w1 / g0) - 0.5 + index) * inv_src_size;
	float2 h1 = ((w3 / g1) + 1.5 + index) * inv_src_size;

	// fetch the four linear interpolations
	float4 tex00 = tex.SampleLevel(smp, float2(h0.x, h0.y), 0.0);
	float4 tex10 = tex.SampleLevel(smp, float2(h1.x, h0.y), 0.0);
	float4 tex01 = tex.SampleLevel(smp, float2(h0.x, h1.y), 0.0);
	float4 tex11 = tex.SampleLevel(smp, float2(h1.x, h1.y), 0.0);

	// weigh along the y-direction
	tex00 = lerp(tex01, tex00, g0.y);
	tex10 = lerp(tex11, tex10, g0.y);

	// weigh along the x-direction
	return lerp(tex10, tex00, g0.x);
}

// In range [0, 1].
// Source: http://byteblacksmith.com/improvements-to-the-canonical-one-liner-glsl-rand-for-opengl-es-2-0/
float white_noise(float2 uv)
{
	return frac(sin(fmod(dot(uv, float2(12.9898, 78.233)), 3.14)) * 43758.5453);
}

// Source: https://github.com/ashima/webgl-noise/blob/6abed1e77ed1e18b181627c35f688eb30c9fe75e/src/noise3D.glsl
//

float3 mod289(float3 x)
{
  	return x - floor(x * (1.0 / 289.0)) * 289.0;
}

float4 mod289(float4 x)
{
  	return x - floor(x * (1.0 / 289.0)) * 289.0;
}

float4 permute(float4 x)
{
  	return mod289(((x * 34.0) + 10.0) * x);
}

float4 taylorInvSqrt(float4 r)
{
  	return 1.79284291400159 - 0.85373472095314 * r;
}

float3 fade(float3 t) {
  	return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

// In range [-1, 1].
float perlin_noise(float3 P)
{
	float3 Pi0 = floor(P); // Integer part for indexing
	float3 Pi1 = Pi0 + 1.0; // Integer part + 1
	Pi0 = mod289(Pi0);
	Pi1 = mod289(Pi1);
	float3 Pf0 = frac(P); // Fractional part for interpolation
	float3 Pf1 = Pf0 - 1.0; // Fractional part - 1.0
	float4 ix = float4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
	float4 iy = float4(Pi0.yy, Pi1.yy);
	float4 iz0 = Pi0.zzzz;
	float4 iz1 = Pi1.zzzz;

	float4 ixy = permute(permute(ix) + iy);
	float4 ixy0 = permute(ixy + iz0);
	float4 ixy1 = permute(ixy + iz1);

	float4 gx0 = ixy0 * (1.0 / 7.0);
	float4 gy0 = frac(floor(gx0) * (1.0 / 7.0)) - 0.5;
	gx0 = frac(gx0);
	float4 gz0 = 0.5 - abs(gx0) - abs(gy0);
	float4 sz0 = step(gz0, 0.0);
	gx0 -= sz0 * (step(0.0, gx0) - 0.5);
	gy0 -= sz0 * (step(0.0, gy0) - 0.5);

	float4 gx1 = ixy1 * (1.0 / 7.0);
	float4 gy1 = frac(floor(gx1) * (1.0 / 7.0)) - 0.5;
	gx1 = frac(gx1);
	float4 gz1 = 0.5 - abs(gx1) - abs(gy1);
	float4 sz1 = step(gz1, 0.0);
	gx1 -= sz1 * (step(0.0, gx1) - 0.5);
	gy1 -= sz1 * (step(0.0, gy1) - 0.5);

	float3 g000 = float3(gx0.x, gy0.x, gz0.x);
	float3 g100 = float3(gx0.y, gy0.y, gz0.y);
	float3 g010 = float3(gx0.z, gy0.z, gz0.z);
	float3 g110 = float3(gx0.w, gy0.w, gz0.w);
	float3 g001 = float3(gx1.x, gy1.x, gz1.x);
	float3 g101 = float3(gx1.y, gy1.y, gz1.y);
	float3 g011 = float3(gx1.z, gy1.z, gz1.z);
	float3 g111 = float3(gx1.w, gy1.w, gz1.w);

	float4 norm0 = taylorInvSqrt(float4(dot(g000, g000), dot(g010, g010), dot(g100, g100), dot(g110, g110)));
	float4 norm1 = taylorInvSqrt(float4(dot(g001, g001), dot(g011, g011), dot(g101, g101), dot(g111, g111)));

	float n000 = norm0.x * dot(g000, Pf0);
	float n010 = norm0.y * dot(g010, float3(Pf0.x, Pf1.y, Pf0.z));
	float n100 = norm0.z * dot(g100, float3(Pf1.x, Pf0.yz));
	float n110 = norm0.w * dot(g110, float3(Pf1.xy, Pf0.z));
	float n001 = norm1.x * dot(g001, float3(Pf0.xy, Pf1.z));
	float n011 = norm1.y * dot(g011, float3(Pf0.x, Pf1.yz));
	float n101 = norm1.z * dot(g101, float3(Pf1.x, Pf0.y, Pf1.z));
	float n111 = norm1.w * dot(g111, Pf1);

	float3 fade_xyz = fade(Pf0);
	float4 n_z = lerp(float4(n000, n100, n010, n110), float4(n001, n101, n011, n111), fade_xyz.z);
	float2 n_yz = lerp(n_z.xy, n_z.zw, fade_xyz.y);
	float n_xyz = lerp(n_yz.x, n_yz.y, fade_xyz.x); 
	return 2.2 * n_xyz;
}

//


#endif // __COMMON_HLSLI__