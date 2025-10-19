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

float3 draw_overlay(float4 pos : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{
	const float3 color = tex2Dfetch(ReShade::BackBuffer, int2(pos.xy), 0).rgb;
	const float4 overlay = tex2Dlod(overlay_smp, float4(texcoord, 0.0, 0.0));
	return lerp(color, overlay.rgb, overlay.a);
}

technique draw_overlay < ui_label = "Draw Overlay"; >
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = draw_overlay;
	}
}
