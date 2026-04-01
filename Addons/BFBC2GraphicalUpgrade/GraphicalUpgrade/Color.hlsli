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

#endif // __COLOR_HLSLI__