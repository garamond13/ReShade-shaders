#ifndef __COLOR_HLSLI__
#define __COLOR_HLSLI__

float3 linear_to_srgb(float3 rgb)
{
	return rgb < 0.0031308 ? 12.92 * rgb : 1.055 * pow(rgb, 1.0 / 2.4) - 0.055;
}

float3 srgb_to_linear(float3 rgb)
{
	return rgb < 0.04045 ? rgb / 12.92 : pow((rgb + 0.055) / 1.055, 2.4);
}

#endif // __COLOR_HLSLI__