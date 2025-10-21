// SGSR1
//
// Minor refactoring and optimizations.
//
// Source: https://github.com/SnapdragonStudios/snapdragon-game-plugins-for-unreal-engine/tree/engine/5.5

Texture2D tex :register(t0);
SamplerState smp : register(s0);

// MUST BE DEFINED!
//

#ifndef INPUT_SIZE_IN_PIXELS_X
#define INPUT_SIZE_IN_PIXELS_X 0.0
#endif

#ifndef INPUT_SIZE_IN_PIXELS_Y
#define INPUT_SIZE_IN_PIXELS_Y 0.0
#endif

//

// User configurable.
#if !defined(SGSR_TARGET_HIGH_QUALITY) && !defined(SGSR_TARGET_MOBILE)
#define SGSR_TARGET_HIGH_QUALITY
#endif

#define con1 float4(              \
	1.0 / INPUT_SIZE_IN_PIXELS_X, \
	1.0 / INPUT_SIZE_IN_PIXELS_Y, \
	INPUT_SIZE_IN_PIXELS_X,       \
	INPUT_SIZE_IN_PIXELS_Y        \
)

#ifdef SGSR_TARGET_HIGH_QUALITY
float4 textureGatherMode(float2 coord)
{
	float Kr = 0.299;
	float Kg = 0.587;
	float Kb = 0.114;
	return tex.GatherRed(smp, coord) * Kr + tex.GatherGreen(smp, coord) * Kg + tex.GatherBlue(smp, coord) * Kb;
}

float2 calDirection(float4 left, float4 right)
{
	float RxLz = right.x - left.z;
	float RwLy = right.w - left.y;
	float2 delta = float2(RxLz + RwLy, RxLz - RwLy);
	return delta * rsqrt(dot(delta, delta) + 3.075740e-05);
}

float fastLanczos2(float x)
{
	float wA = x - 4.0;
	float wB = x * wA - wA;
	wA *= wA;
	return wB * wA;
}

float4 weightY(float dx, float dy, float c, float2 dir, float std)
{
	float dxy2 = dx * dx + dy * dy;
	float c2std = c * c * std;

	float dc = exp(c2std);
	float ds = sqrt(dxy2);

	float edgeDis = dx * dir.y + dy * dir.x;
	edgeDis *= edgeDis;

	float x1 = dxy2 + edgeDis * (saturate(c2std * -2.8) * 0.7 - 1.0);
	float w1 = fastLanczos2(x1);

	float x2 = ds * 0.54 + 0.36 - dc * 0.36;
	float w2 = fastLanczos2(x2);

	return float4(w1 * c, w1, w2 * c, w2);
}

float2 sgsrPass(float4 left, float4 right, float4 upDown, float2 pl)
{
	float sum2 = dot(left, left) + dot(right, right) + dot(upDown, upDown);
	float std2 = -6.0 * rcp(sum2);

	float2 dir = calDirection(left, right);

	float4 aYW = weightY(pl.x, pl.y + 1.0, upDown.x, dir, std2);
	aYW += weightY(pl.x - 1.0, pl.y + 1.0, upDown.y, dir, std2);
	aYW += weightY(pl.x - 1.0, pl.y - 2.0, upDown.z, dir, std2);
	aYW += weightY(pl.x, pl.y - 2.0, upDown.w, dir, std2);

	aYW += weightY(pl.x + 1.0, pl.y - 1.0, left.x, dir, std2);
	aYW += weightY(pl.x + 1.0, pl.y, left.w, dir, std2);
	aYW += weightY(pl.x - 2.0, pl.y - 1.0, right.y, dir, std2);
	aYW += weightY(pl.x - 2.0, pl.y, right.z, dir, std2);

	aYW += weightY(pl.x, pl.y - 1.0, left.y, dir, std2);
	aYW += weightY(pl.x, pl.y, left.z, dir, std2);
	aYW += weightY(pl.x - 1.0, pl.y - 1.0, right.x, dir, std2);
	aYW += weightY(pl.x - 1.0, pl.y, right.w, dir, std2);	

	return float2(aYW.x * rcp(aYW.y), aYW.z * rcp(aYW.w));
}

void SgsrYuvH(out float4 pix, float2 uv)
{
	float edgeThreshold = 4.0 / 255.0;
	float edgeSharpness = 2.0;

	pix.xyz = tex.SampleLevel(smp, uv, 0.0).xyz;
	
	float Kr = 0.299;
	float Kg = 0.587;
	float Kb = 0.114;
	float2 imgCoord = uv * con1.zw + float2(-0.5, 0.5);
	float2 imgCoordPixel = floor(imgCoord);
	float2 coord = imgCoordPixel * con1.xy;
	float2 pl = imgCoord - imgCoordPixel;

	float4 left = textureGatherMode(coord);

	pix.w = pix.x * Kr + pix.y * Kg + pix.z * Kb;

	float edgeVote = abs(left.z - left.y) + abs(pix.w - left.y) + abs(pix.w - left.z);
	if (edgeVote > edgeThreshold) {
		coord.x += con1.x;

		float4 right = textureGatherMode(coord + float2(con1.x, 0.0));
		float4 upDown;
		upDown.xy = textureGatherMode(coord + float2(0.0, -con1.y)).wz;
		upDown.zw = textureGatherMode(coord + float2(0.0, con1.y)).yx;

		float mean = (left.y + left.z + right.x + right.w) * 0.25;
		left -= mean;
		right -= mean;
		upDown -= mean;
		pix.w -= mean;

		float2 y2 = sgsrPass(left, right, upDown, pl);

		float maxY = max(max(left.y, left.z), max(right.x, right.w));
		float minY = min(min(left.y, left.z), min(right.x, right.w));

		float edgeY = clamp(edgeSharpness * y2.x, minY, maxY);
		float smoothY = clamp(edgeSharpness * y2.y, minY, maxY);

		float deltaY = (smoothY * 0.4 - pix.w) + edgeY * 0.6;
		pix.xyz = saturate(pix.xyz + deltaY);
	}

	pix.w = 1.0; // assume alpha channel is not used
}
#endif // SGSR_TARGET_HIGH_QUALITY

#ifdef SGSR_TARGET_MOBILE
float fastLanczos2(float x)
{
	float wA = x - 4.0;
	float wB = x * wA - wA;
	wA *= wA;
	return wB * wA;
}

float2 weightY(float dx, float dy, float c, float std)
{
	float x = (dx * dx + dy * dy) * 0.5 + saturate(abs(c) * std);
	float w = fastLanczos2(x);
	return float2(w, w * c);
}

void SgsrYuvH(out float4 pix, float2 uv)
{
	float edgeThreshold = 8.0 / 255.0;
	float edgeSharpness = 2.0;

	pix.xyz = tex.SampleLevel(smp, uv, 0.0).xyz;

	float2 imgCoord = uv.xy * con1.zw + float2(-0.5, 0.5);
	float2 imgCoordPixel = floor(imgCoord);
	float2 coord = imgCoordPixel * con1.xy;
	float2 pl = imgCoord - imgCoordPixel;
	float4 left = tex.GatherGreen(smp, coord);
		
	float pixLum = pix[1]; // luminance approximation
	float edgeVote = abs(left.z - left.y) + abs(pixLum - left.y) + abs(pixLum - left.z);
	if (edgeVote > edgeThreshold) {
		coord.x += con1.x;

		float4 right = tex.GatherGreen(smp, coord + float2(con1.x, 0.0));
		float4 upDown;
		upDown.xy = tex.GatherGreen(smp, coord + float2(0.0, -con1.y)).wz;
		upDown.zw = tex.GatherGreen(smp, coord + float2(0.0, con1.y)).yx;

		float mean = (left.y + left.z + right.x + right.w) * 0.25;
		left = left - mean;
		right = right - mean;
		upDown = upDown - mean;
		pix.w = pixLum - mean;

		float sum = abs(left.x) + abs(left.y) + abs(left.z) + abs(left.w) + abs(right.x) + abs(right.y) + abs(right.z) + abs(right.w) + abs(upDown.x) + abs(upDown.y) + abs(upDown.z) + abs(upDown.w);
		float std = 2.181818 * rcp(sum);

		float2 aWY = weightY(pl.x, pl.y + 1.0, upDown.x, std);
		aWY += weightY(pl.x - 1.0, pl.y + 1.0, upDown.y, std);
		aWY += weightY(pl.x - 1.0, pl.y - 2.0, upDown.z, std);
		aWY += weightY(pl.x, pl.y - 2.0, upDown.w, std);
		aWY += weightY(pl.x + 1.0, pl.y - 1.0, left.x, std);
		aWY += weightY(pl.x, pl.y - 1.0, left.y, std);
		aWY += weightY(pl.x, pl.y, left.z, std);
		aWY += weightY(pl.x + 1.0, pl.y, left.w, std);
		aWY += weightY(pl.x - 1.0, pl.y - 1.0, right.x, std);
		aWY += weightY(pl.x - 2.0, pl.y - 1.0, right.y, std);
		aWY += weightY(pl.x - 2.0, pl.y, right.z, std);
		aWY += weightY(pl.x - 1.0, pl.y, right.w, std);

		float finalY = aWY.y * rcp(aWY.x);

		float max4 = max(max(left.y, left.z), max(right.x, right.w));
		float min4 = min(min(left.y, left.z), min(right.x, right.w));
		finalY = clamp(edgeSharpness * finalY, min4, max4);

		float deltaY = finalY - pix.w;
		
		pix.xyz = saturate(pix.xyz + deltaY);
	}

	pix.w = 1.0; // assume alpha channel is not used
}
#endif // SGSR_TARGET_MOBILE

// Implementation.
float4 main(float4 pos : SV_Position, noperspective float2 texcoord : TEXCOORD) : SV_Target
{
	float4 c;
	SgsrYuvH(c, texcoord);
	return c;
}