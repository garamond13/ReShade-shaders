Texture2D tex : register(t0);

// We expect linear sampler.
SamplerState smp : register(s1);

float4 main(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
	return tex.SampleLevel(smp, texcoord, 0.0);
}