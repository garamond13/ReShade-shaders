#ifndef __SAMPLING_HLSLI__
#define __SAMPLING_HLSLI__

// Hermite sampling in 1 texture fetch.
//
// f(x) = 2 * |x|^3 - 3 * |x|^2 + 1 for 0 <= |x| <= 1
// f(x) = 0 otherwise
//
// smp should be linear clamp.
#define SAMPLING_TEMPLATE(T)                                      \
T sample_hermite(Texture2D<T> tex, SamplerState smp, float2 uv)   \
{                                                                 \
    float2 src_size;                                              \
    tex.GetDimensions(src_size.x, src_size.y);                    \
    float2 inv_src_size = 1.0 / src_size;                         \
                                                                  \
    const float2 f = frac(uv * src_size - 0.5);                   \
    const float2 offset = smoothstep(0.0, 1.0, f) - f;            \
    return tex.SampleLevel(smp, uv + offset * inv_src_size, 0.0); \
}                                                                 \

// Generate functions.
SAMPLING_TEMPLATE(float)
SAMPLING_TEMPLATE(float2)
SAMPLING_TEMPLATE(float3)
SAMPLING_TEMPLATE(float4)

#undef SAMPLING_TEMPLATE

// Bicubic upsampling in 4 texture fetches.
//
// f(x) = (4 + 3 * |x|^3 – 6 * |x|^2) / 6 for 0 <= |x| <= 1
// f(x) = (2 – |x|)^3 / 6 for 1 < |x| <= 2
// f(x) = 0 otherwise
//
// Source: https://www.researchgate.net/publication/220494113_Efficient_GPU-Based_Texture_Interpolation_using_Uniform_B-Splines
//
// smp should be linear clamp.
#define SAMPLING_TEMPLATE(T)                                          \
T sample_bicubic(Texture2D<T> tex, SamplerState smp, float2 uv)       \
{                                                                     \
    float2 src_size;                                                  \
    tex.GetDimensions(src_size.x, src_size.y);                        \
    float2 inv_src_size = 1.0 / src_size;                             \
                                                                      \
    float2 coord_grid = uv * src_size - 0.5;                          \
    float2 index = floor(coord_grid);                                 \
    float2 fraction = coord_grid - index;                             \
    float2 one_frac = 1.0 - fraction;                                 \
    float2 one_frac2 = one_frac * one_frac;                           \
    float2 fraction2 = fraction * fraction;                           \
    float2 w0 = 1.0 / 6.0 * one_frac2 * one_frac;                     \
    float2 w1 = 2.0 / 3.0 - 0.5 * fraction2 * (2.0 - fraction);       \
    float2 w2 = 2.0 / 3.0 - 0.5 * one_frac2 * (2.0 - one_frac);       \
    float2 w3 = 1.0 / 6.0 * fraction2 * fraction;                     \
    float2 g0 = w0 + w1;                                              \
    float2 g1 = w2 + w3;                                              \
                                                                      \
    /* h0 = w1/g0 - 1, move from [-0.5, extent-0.5] to [0, extent] */ \
    float2 h0 = ((w1 / g0) - 0.5 + index) * inv_src_size;             \
    float2 h1 = ((w3 / g1) + 1.5 + index) * inv_src_size;             \
                                                                      \
    /* fetch the four linear interpolations */                        \
    T tex00 = tex.SampleLevel(smp, float2(h0.x, h0.y), 0.0);          \
    T tex10 = tex.SampleLevel(smp, float2(h1.x, h0.y), 0.0);          \
    T tex01 = tex.SampleLevel(smp, float2(h0.x, h1.y), 0.0);          \
    T tex11 = tex.SampleLevel(smp, float2(h1.x, h1.y), 0.0);          \
                                                                      \
    /* weigh along the y-direction */                                 \
    tex00 = lerp(tex01, tex00, g0.y);                                 \
    tex10 = lerp(tex11, tex10, g0.y);                                 \
                                                                      \
    /* weigh along the x-direction */                                 \
    return lerp(tex10, tex00, g0.x);                                  \
}                                                                     \

// Generate functions.
SAMPLING_TEMPLATE(float)
SAMPLING_TEMPLATE(float2)
SAMPLING_TEMPLATE(float3)
SAMPLING_TEMPLATE(float4)

#undef SAMPLING_TEMPLATE

#endif // __SAMPLING_HLSLI__