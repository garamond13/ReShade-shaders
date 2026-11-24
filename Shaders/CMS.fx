// Color Management System

#include "ReShade.fxh"

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

uniform int framecount < source = "framecount"; >;

sampler2D smpColor
{
    Texture = ReShade::BackBufferTex;
    MagFilter = POINT;
    MinFilter = POINT;
    MipFilter = POINT;
};

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

// http://www.jcgt.org/published/0009/03/02/
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

// Dithering with triangularly distributed noise.
float3 dither(float3 color, uint3 seed, uint bits)
{
    const float bitstep = exp2(bits) - 1.0;
    const uint3 noise = pcg3d(seed);
    const float3 tri_noise = float3((noise & 0xFFFFu) + (noise >> 16u)) / 65535.0 - 1.0;
    return color + tri_noise / bitstep;
}

float4 sample_trilinear_ps(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
    float4 color = tex2Dfetch(smpColor, int2(pos.xy), 0);
    const float scale = float(LUT_SIZE - 1) / float(LUT_SIZE);
    const float offset = 1.0 / float(LUT_SIZE * 2);
    color.rgb = tex3D(smpLUT, saturate(color.rgb) * scale + offset).rgb;

    #if DITHERING
    color.rgb = dither(color.rgb, uint3(pos.xy, framecount & 63), BUFFER_COLOR_BIT_DEPTH);
    #endif

    return color;
}

float4 sample_tetrahedral_ps(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
    float4 color = tex2Dfetch(smpColor, int2(pos.xy), 0);
    color.rgb = saturate(color.rgb) * float(LUT_SIZE - 1);

    // See https://doi.org/10.2312/egp.20211031
    //

    const float3 r = frac(color.rgb);
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

    const int3 base = floor(color.rgb);
    const float3 v0 = tex3Dfetch(smpLUT, base, 0).rgb * bary.x;
    const float3 v1 = tex3Dfetch(smpLUT, base + 1, 0).rgb * bary.y;
    const float3 v2 = tex3Dfetch(smpLUT, base + vert2, 0).rgb * bary.z;
    const float3 v3 = tex3Dfetch(smpLUT, base + vert3, 0).rgb * bary.w;
    color.rgb = v0 + v1 + v2 + v3;

    #if DITHERING
    color.rgb = dither(color.rgb, uint3(pos.xy, framecount & 63), BUFFER_COLOR_BIT_DEPTH);
    #endif

    return color;
}

technique CMS < ui_label = "Color Management System"; >
{
    pass
    {
        VertexShader = PostProcessVS;

        #if TETRAHEDRAL_INTERPOLATION
        PixelShader = sample_tetrahedral_ps;
        #else
        PixelShader = sample_trilinear_ps;
        #endif
    }
}