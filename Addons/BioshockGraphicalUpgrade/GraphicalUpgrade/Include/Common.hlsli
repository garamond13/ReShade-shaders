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

#endif // __COMMON_HLSLI__