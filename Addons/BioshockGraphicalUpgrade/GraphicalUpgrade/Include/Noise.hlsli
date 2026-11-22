#ifndef __NOISE_HLSLI__
#define __NOISE_HLSLI__

// uint32 hash to float in range [0, 1).
#define NOISE_TEMPLATE(T, U)                                            \
T hash_to_01_exclusive(U hash)                                          \
{                                                                       \
    /* float(hash >> (32 - FLT_MANT_DIG)) / float(1 << FLT_MANT_DIG) */ \
    return T(hash >> 8u) / float(1u << 24u);                            \
}                                                                       \

// Generate functions.
NOISE_TEMPLATE(float, uint)
NOISE_TEMPLATE(float2, uint2)
NOISE_TEMPLATE(float3, uint3)
NOISE_TEMPLATE(float4, uint4)

#undef NOISE_TEMPLATE

// uint32 hash to float in range [0, 1].
// Should have negligible bias if optimized into mul.
#define NOISE_TEMPLATE(T, U)                                                  \
T hash_to_01_inclusive(U hash)                                                \
{                                                                             \
    /* float(hash >> (32 - FLT_MANT_DIG)) / float((1 << FLT_MANT_DIG) - 1) */ \
    return T(hash >> 8u) / float((1u << 24u) - 1u);                           \
}                                                                             \

// Generate functions.
NOISE_TEMPLATE(float, uint)
NOISE_TEMPLATE(float2, uint2)
NOISE_TEMPLATE(float3, uint3)
NOISE_TEMPLATE(float4, uint4)

#undef NOISE_TEMPLATE

// Seed should be in range [0, 1].
// Output is in range [0, 1).
float rand21(float2 p)
{
    return frac(sin(dot(p, float2(12.9898, 78.233))) * 43757.5453);
}

// https://www.pcg-random.org/
// http://www.jcgt.org/published/0009/03/02/
//

uint pcg(uint v)
{
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

uint pcg21(uint2 v)
{
    return pcg(v.x + pcg(v.y));
}

uint pcg31(uint3 v)
{
    return pcg(v.x + pcg(v.y + pcg(v.z)));
}

uint2 pcg2d(uint2 v)
{
    v = v * 1664525u + 1013904223u;
    v.x += v.y * 1664525u;
    v.y += v.x * 1664525u;
    v ^= v >> 16u;
    v.x += v.y * 1664525u;
    v.y += v.x * 1664525u;
    v ^= v >> 16u;
    return v;
}

uint3 pcg3d(uint3 v)
{
    v = v * 1664525u + 1013904223u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v ^= v >> 16u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    return v;
}

//

// Source: https://github.com/stegu/webgl-noise/blob/22434e04d7753f7e949e8d724ab3da2864c17a0f/src/classicnoise3D.glsl
//

#define NOISE_TEMPLATE(T)                        \
T mod289(T x)                                    \
{                                                \
    return x - floor(x * (1.0 / 289.0)) * 289.0; \
}                                                \

// Generate functions.
NOISE_TEMPLATE(float3)
NOISE_TEMPLATE(float4)

#undef NOISE_TEMPLATE

float4 permute(float4 x)
{
    return mod289((x * 34.0 + 10.0) * x);
}

float4 taylorInvSqrt(float4 r)
{
    return 1.79284291400159 - 0.85373472095314 * r;
}

float3 fade(float3 t) {
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

// Output should be in range [-1, 1].
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

// Input color should be in [0, 1] range.
// Output color may excede [0, 1] range.
//
// Based on Triangular Dither by The Sandvich Maker.
float3 dither(float3 color, uint3 seed, float bits)
{
    const float bitstep = exp2(bits) - 1.0;

    const uint3 noise1 = pcg3d(seed);
    const uint3 noise2 = pcg3d(noise1);
    const float3 fnoise1 = hash_to_01_exclusive(noise1);
    const float3 fnoise2 = hash_to_01_exclusive(noise2);
    const float3 uni = fnoise1 - 0.5;
    const float3 tri = fnoise1 - fnoise2;

    const float3 s = saturate(min(color, 1.0 - color) * 2.0 * bitstep);
    return color + lerp(uni, tri, s) / bitstep;
}

#endif // __NOISE_HLSLI__