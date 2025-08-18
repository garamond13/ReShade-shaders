#include "ReShade.fxh"

#ifndef OVERLAY_TEXTURE
	#define OVERLAY_TEXTURE "overlay.png"
#endif

texture overlay_tex < source = OVERLAY_TEXTURE; >
{
	Width = BUFFER_WIDTH;
	Height = BUFFER_HEIGHT;
	Format = RGBA8;
};

sampler2D overlay_smp
{
	Texture = overlay_tex;
};

float4 draw_overlay(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
	const float4 color = tex2D(ReShade::BackBuffer, texcoord);
	const float4 overlay = tex2D(overlay_smp, texcoord);
	return lerp(color, overlay, overlay.a);
}

technique draw_overlay < ui_label = "Draw Overlay"; >
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = draw_overlay;
	}
}
