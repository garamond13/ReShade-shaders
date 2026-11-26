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
    return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
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

uint4 pcg4d(uint4 v)
{
    v = v * 1664525u + 1013904223u;
    v.x += v.y * v.w;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v.w += v.y * v.z;
    v ^= v >> 16u;
    v.x += v.y * v.w;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v.w += v.y * v.z;
    return v;
}

//

// Dithering with triangularly distributed noise.
float3 dither(float3 color, uint3 seed, uint bits)
{
    const float bitstep = exp2(bits) - 1.0;
    const uint3 noise = pcg3d(seed);
    const float3 tri_noise = float3((noise & 0xFFFFu) + (noise >> 16u)) / 65535.0 - 1.0;
    return color + tri_noise / bitstep;
}

float3 film_grain(float3 color, uint4 seed, float amount)
{
    // Approximated Gaussian noise in range [-0.5, 0.5].
    const uint4 noise = pcg4d(seed);
    const uint4 sum1 = (noise & 0xFFFFu) + (noise >> 16u);
    const float gaus_noise = float(sum1.x + sum1.y + sum1.z + sum1.w) * (0.125 / 65535.0) - 0.5;

    // AMD FFX LFGA
    // Source: https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/54fbaafdc34716811751bea5032700e78f5a0f33/sdk/include/FidelityFX/gpu/fsr1/ffx_fsr1.h
    return color + gaus_noise * amount * min(1.0 - color, color);
}

#endif // __NOISE_HLSLI__