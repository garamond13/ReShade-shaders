#include "ReShade.fxh"

#ifndef GAMMA_VALUE
#define GAMMA_VALUE 2.2
#endif

// We are assuming that the SDR back buffer is sRGB!
// It may not be, it may be sRGB UI + 2.2 gamma scene, or whatever.

sampler2D smpColor
{
	Texture = ReShade::BackBufferTex;
	MagFilter = POINT;
	MinFilter = POINT;
	MipFilter = POINT;

	#if BUFFER_COLOR_BIT_DEPTH == 8
	SRGBTexture = true;
	#endif
};

float3 _linearize(float3 rgb)
{
	return rgb < 0.04045 ? rgb / 12.92 : pow((rgb + 0.055) / 1.055, 2.4);
}

#if BUFFER_COLOR_BIT_DEPTH == 8
#define linearize(x) (x)
#else
#define linearize(x) (_linearize(x))
#endif

float4 apply_gamma(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	#if BUFFER_COLOR_BIT_DEPTH == 8
	float4 color = tex2Dlod(smpColor, float4(texcoord, 0.0, 0.0));
	#else
	float4 color = tex2Dfetch(smpColor, int2(pos.xy), 0);
	#endif

	color.rgb = linearize(color.rgb);
	color.rgb = pow(max(0.0, color.rgb), 1.0 / GAMMA_VALUE);
	return color;
}

technique apply_gamma < ui_label = "Gamma"; >
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = apply_gamma;
	}
}
