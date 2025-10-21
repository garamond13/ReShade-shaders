// AMD FFX FSR1 EASU
//
// NON-PACKED 32-BIT VERSION
//
// Sources:
// https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/tree/v1.1.4
// https://github.com/GPUOpen-Effects/FidelityFX-FSR

Texture2D tex0 : register(t0);
SamplerState smp : register(s0);
RWTexture2D<unorm float4> tex1 : register(u0);

// MUST BE DEFINED!
//

#ifndef INPUT_VIEWPORT_IN_PIXELS_X
#define INPUT_VIEWPORT_IN_PIXELS_X 0.0
#endif

#ifndef INPUT_VIEWPORT_IN_PIXELS_Y
#define INPUT_VIEWPORT_IN_PIXELS_Y 0.0
#endif

#ifndef INPUT_SIZE_IN_PIXELS_X
#define INPUT_SIZE_IN_PIXELS_X 0.0
#endif

#ifndef INPUT_SIZE_IN_PIXELS_Y
#define INPUT_SIZE_IN_PIXELS_Y 0.0
#endif

#ifndef OUTPUT_SIZE_IN_PIXELS_X
#define OUTPUT_SIZE_IN_PIXELS_X 0.0
#endif

#ifndef OUTPUT_SIZE_IN_PIXELS_Y
#define OUTPUT_SIZE_IN_PIXELS_Y 0.0
#endif

//

// Output integer position to a pixel position in viewport.
#define con0 uint4(                                                           \
	asuint(INPUT_VIEWPORT_IN_PIXELS_X / OUTPUT_SIZE_IN_PIXELS_X),             \
	asuint(INPUT_VIEWPORT_IN_PIXELS_Y / OUTPUT_SIZE_IN_PIXELS_Y),             \
	asuint(0.5 * INPUT_VIEWPORT_IN_PIXELS_X / OUTPUT_SIZE_IN_PIXELS_X - 0.5), \
	asuint(0.5 * INPUT_VIEWPORT_IN_PIXELS_Y / OUTPUT_SIZE_IN_PIXELS_Y - 0.5)  \
)

#define con1 uint4(                                                \
	/* Viewport pixel position to normalized image space. */       \
	/* This is used to get upper-left of 'F' tap. */               \
	asuint(1.0 / INPUT_SIZE_IN_PIXELS_X),                          \
	asuint(1.0 / INPUT_SIZE_IN_PIXELS_Y),                          \
                                                                   \
	/* Centers of gather4, first offset from upper-left of 'F'. */ \
	/*      +---+---+      */                                      \
	/*      |   |   |      */                                      \
	/*      +--(0)--+      */                                      \
	/*      | b | c |      */                                      \
	/*  +---F---+---+---+  */                                      \
	/*  | e | f | g | h |  */                                      \
	/*  +--(1)--+--(2)--+  */                                      \
	/*  | i | j | k | l |  */                                      \
	/*  +---+---+---+---+  */                                      \
	/*      | n | o |      */                                      \
	/*      +--(3)--+      */                                      \
	/*      |   |   |      */                                      \
	/*      +---+---+      */                                      \
	asuint(1.0 / INPUT_SIZE_IN_PIXELS_X),                          \
	asuint(-1.0 / INPUT_SIZE_IN_PIXELS_Y)                          \
)

// These are from (0) instead of 'F'.
//

#define con2 uint4(                        \
	asuint(-1.0 / INPUT_SIZE_IN_PIXELS_X), \
	asuint(2.0 / INPUT_SIZE_IN_PIXELS_Y),  \
	asuint(1.0 / INPUT_SIZE_IN_PIXELS_X),  \
	asuint(2.0 / INPUT_SIZE_IN_PIXELS_Y)   \
)

#define con3 uint4(                       \
	asuint(0.0 / INPUT_SIZE_IN_PIXELS_X), \
	asuint(4.0 / INPUT_SIZE_IN_PIXELS_Y), \
	0,                                    \
	0                                     \
)

//

#define min3(x,y,z) min(x, min(y, z))
#define max3(x,y,z) max(x, max(y, z))

float ApproximateReciprocal(float value)
{
	return asfloat(0x7ef07ebbu - asuint(value));
}

float ApproximateReciprocalSquareRoot(float value)
{
	return asfloat(0x5f347d74u - (asuint(value) >> 1u));
}

// Filtering for a given tap for the scalar.
void fsrEasuTapFloat(inout float3 accumulatedColor, inout float accumulatedWeight, float2 pixelOffset, float2 gradientDirection, float2 length, float negativeLobeStrength, float clippingPoint, float3 color)
{
	// Rotate offset by direction.
	float2 rotatedOffset;
	rotatedOffset.x = pixelOffset.x * gradientDirection.x + pixelOffset.y * gradientDirection.y;
	rotatedOffset.y = pixelOffset.x * -gradientDirection.y + pixelOffset.y * gradientDirection.x;

	// Anisotropy.
	rotatedOffset *= length;

	// Compute distance^2.
	float distanceSquared = rotatedOffset.x * rotatedOffset.x + rotatedOffset.y * rotatedOffset.y;

	// Limit to the window as at corner, 2 taps can easily be outside.
	distanceSquared = min(distanceSquared, clippingPoint);

	// Approximation of lancos2 without sin() or rcp(), or sqrt() to get x.
	//  (25/16 * (2/5 * x^2 - 1)^2 - (25/16 - 1)) * (1/4 * x^2 - 1)^2
	//  |_______________________________________|   |_______________|
	//                   base                             window
	// The general form of the 'base' is,
	//  (a*(b*x^2-1)^2-(a-1))
	// Where 'a=1/(2*b-b^2)' and 'b' moves around the negative lobe.
	float weightB = 2.0 / 5.0 * distanceSquared - 1.0;
	float weightA = negativeLobeStrength * distanceSquared - 1.0;
	weightB *= weightB;
	weightA *= weightA;
	weightB = 25.0 / 16.0 * weightB - (25.0 / 16.0 - 1.0);
	float weight = weightB * weightA;

	// Do weighted average.
	accumulatedColor += color * weight;
	accumulatedWeight += weight;
}

// Accumulate direction and length.
void fsrEasuSetFloat(inout float2 direction, inout float length, float2 pp, bool biS, bool biT, bool biU, bool biV, float lA, float lB, float lC, float lD, float lE)
{
	// Compute bilinear weight, branches factor out as predicates are compiler time immediates.
	//  s t
	//  u v
	float weight = 0.0;
	if (biS) {
		weight = (1.0 - pp.x) * (1.0 - pp.y);
	}
	if (biT) {
		weight = pp.x * (1.0 - pp.y);
	}
	if (biU) {
		weight = (1.0 - pp.x) * pp.y;
	}
	if (biV) {
		weight = pp.x * pp.y;
	}

	// Direction is the '+' diff.
	//    a
	//  b c d
	//    e
	// Then takes magnitude from abs average of both sides of 'c'.
	// Length converts gradient reversal to 0, smoothly to non-reversal at 1, shaped, then adding horz and vert terms.
	float dc = lD - lC;
	float cb = lC - lB;
	float lengthX = max(abs(dc), abs(cb));
	lengthX = ApproximateReciprocal(lengthX);
	float directionX = lD - lB;
	direction.x += directionX * weight;
	lengthX = saturate(abs(directionX) * lengthX);
	lengthX *= lengthX;
	length += lengthX * weight;

	// Repeat for the y axis.
	float ec = lE - lC;
	float ca = lC - lA;
	float lengthY = max(abs(ec), abs(ca));
	lengthY = ApproximateReciprocal(lengthY);
	float directionY = lE - lA;
	direction.y += directionY * weight;
	lengthY = saturate(abs(directionY) * lengthY);
	lengthY *= lengthY;
	length += lengthY * weight;
}

/// Apply edge-aware spatial upsampling using 32bit floating point precision calculations.
///
/// @param [out] outPixel               The computed color of a pixel.
/// @param [in]  integerPosition        Integer pixel position within the output.
void ffxFsrEasuFloat(out float3 pix, uint2 ip)
{
	// Get position of 'f'.
	float2 pp = float2(ip) * asfloat(con0.xy) + asfloat(con0.zw);
	float2 fp = floor(pp);
	pp -= fp;

	// 12-tap kernel.
	//    b c
	//  e f g h
	//  i j k l
	//    n o
	// Gather 4 ordering.
	//  a b
	//  r g
	// For packed FP16, need either {rg} or {ab} so using the following setup for gather in all versions,
	//    a b    <- unused (z)
	//    r g
	//  a b a b
	//  r g r g
	//    a b
	//    r g    <- unused (z)
	// Allowing dead-code removal to remove the 'z's.
	float2 p0 = fp * asfloat(con1.xy) + asfloat(con1.zw);

	// These are from p0 to avoid pulling two constants on pre-Navi hardware.
	float2 p1 = p0 + asfloat(con2.xy);
	float2 p2 = p0 + asfloat(con2.zw);
	float2 p3 = p0 + asfloat(con3.xy);
	float4 bczzR = tex0.GatherRed(smp, p0);
	float4 bczzG = tex0.GatherGreen(smp, p0);
	float4 bczzB = tex0.GatherBlue(smp, p0);
	float4 ijfeR = tex0.GatherRed(smp, p1);
	float4 ijfeG = tex0.GatherGreen(smp, p1);
	float4 ijfeB = tex0.GatherBlue(smp, p1);
	float4 klhgR = tex0.GatherRed(smp, p2);
	float4 klhgG = tex0.GatherGreen(smp, p2);
	float4 klhgB = tex0.GatherBlue(smp, p2);
	float4 zzonR = tex0.GatherRed(smp, p3);
	float4 zzonG = tex0.GatherGreen(smp, p3);
	float4 zzonB = tex0.GatherBlue(smp, p3);

	// Simplest multi-channel approximate luma possible (luma times 2, in 2 FMA/MAD).
	float4 bczzL = bczzB * 0.5 + (bczzR * 0.5 + bczzG);
	float4 ijfeL = ijfeB * 0.5 + (ijfeR * 0.5 + ijfeG);
	float4 klhgL = klhgB * 0.5 + (klhgR * 0.5 + klhgG);
	float4 zzonL = zzonB * 0.5 + (zzonR * 0.5 + zzonG);

	// Rename.
	float bL = bczzL.x;
	float cL = bczzL.y;
	float iL = ijfeL.x;
	float jL = ijfeL.y;
	float fL = ijfeL.z;
	float eL = ijfeL.w;
	float kL = klhgL.x;
	float lL = klhgL.y;
	float hL = klhgL.z;
	float gL = klhgL.w;
	float oL = zzonL.z;
	float nL = zzonL.w;

	// Accumulate for bilinear interpolation.
	float2 dir = 0.0;
	float len = 0.0;
	fsrEasuSetFloat(dir, len, pp, true,  false, false, false, bL, eL, fL, gL, jL);
	fsrEasuSetFloat(dir, len, pp, false, true,  false, false, cL, fL, gL, hL, kL);
	fsrEasuSetFloat(dir, len, pp, false, false, true,  false, fL, iL, jL, kL, nL);
	fsrEasuSetFloat(dir, len, pp, false, false, false, true,  gL, jL, kL, lL, oL);

	// Normalize with approximation, and cleanup close to zero.
	float2 dir2 = dir * dir;
	float dirR = dir2.x + dir2.y;
	bool zro  = dirR < 1.0 / 32768.0;
	dirR = ApproximateReciprocalSquareRoot(dirR);
	dirR = zro ? 1.0 : dirR;
	dir.x = zro ? 1.0 : dir.x;
	dir *= dirR;

	// Transform from {0 to 2} to {0 to 1} range, and shape with square.
	len = len * 0.5;
	len *= len;

	// Stretch kernel {1.0 vert|horz, to sqrt(2.0) on diagonal}.
	float stretch = (dir.x * dir.x + dir.y * dir.y) * ApproximateReciprocal(max(abs(dir.x), abs(dir.y)));

	// Anisotropic length after rotation,
	//  x := 1.0 lerp to 'stretch' on edges
	//  y := 1.0 lerp to 2x on edges
	float2 len2 = float2(1.0 + (stretch - 1.0) * len, 1.0 - 0.5 * len);

	// Based on the amount of 'edge',
	// the window shifts from +/-{sqrt(2.0) to slightly beyond 2.0}.
	float lob = 0.5 + ((1.0 / 4.0 - 0.04) - 0.5) * len;

	// Set distance^2 clipping point to the end of the adjustable window.
	float clp = ApproximateReciprocal(lob);

	// Accumulation mixed with min/max of 4 nearest.
	//    b c
	//  e f g h
	//  i j k l
	//    n o
	float3 min4 = min(min3(float3(ijfeR.z, ijfeG.z, ijfeB.z), float3(klhgR.w, klhgG.w, klhgB.w), float3(ijfeR.y, ijfeG.y, ijfeB.y)), float3(klhgR.x, klhgG.x, klhgB.x));
	float3 max4 = max(max3(float3(ijfeR.z, ijfeG.z, ijfeB.z), float3(klhgR.w, klhgG.w, klhgB.w), float3(ijfeR.y, ijfeG.y, ijfeB.y)), float3(klhgR.x, klhgG.x, klhgB.x));

	// Accumulation.
	float3 aC = 0.0;
	float aW = 0.0;
	fsrEasuTapFloat(aC, aW, float2(0.0, -1.0) - pp, dir, len2, lob, clp, float3(bczzR.x, bczzG.x, bczzB.x));  // b
	fsrEasuTapFloat(aC, aW, float2(1.0, -1.0) - pp, dir, len2, lob, clp, float3(bczzR.y, bczzG.y, bczzB.y));  // c
	fsrEasuTapFloat(aC, aW, float2(-1.0, 1.0) - pp, dir, len2, lob, clp, float3(ijfeR.x, ijfeG.x, ijfeB.x));  // i
	fsrEasuTapFloat(aC, aW, float2(0.0, 1.0) - pp, dir, len2, lob, clp, float3(ijfeR.y, ijfeG.y, ijfeB.y));   // j
	fsrEasuTapFloat(aC, aW, float2(0.0, 0.0) - pp, dir, len2, lob, clp, float3(ijfeR.z, ijfeG.z, ijfeB.z));   // f
	fsrEasuTapFloat(aC, aW, float2(-1.0, 0.0) - pp, dir, len2, lob, clp, float3(ijfeR.w, ijfeG.w, ijfeB.w));  // e
	fsrEasuTapFloat(aC, aW, float2(1.0, 1.0) - pp, dir, len2, lob, clp, float3(klhgR.x, klhgG.x, klhgB.x));   // k
	fsrEasuTapFloat(aC, aW, float2(2.0, 1.0) - pp, dir, len2, lob, clp, float3(klhgR.y, klhgG.y, klhgB.y));   // l
	fsrEasuTapFloat(aC, aW, float2(2.0, 0.0) - pp, dir, len2, lob, clp, float3(klhgR.z, klhgG.z, klhgB.z));   // h
	fsrEasuTapFloat(aC, aW, float2(1.0, 0.0) - pp, dir, len2, lob, clp, float3(klhgR.w, klhgG.w, klhgB.w));   // g
	fsrEasuTapFloat(aC, aW, float2(1.0, 2.0) - pp, dir, len2, lob, clp, float3(zzonR.z, zzonG.z, zzonB.z));   // o
	fsrEasuTapFloat(aC, aW, float2(0.0, 2.0) - pp, dir, len2, lob, clp, float3(zzonR.w, zzonG.w, zzonB.w));   // n

	// Normalize and dering.
	pix = min(max4, max(min4, aC * rcp(aW)));
}

uint ffxBitfieldInsertMask(uint src, uint ins, uint bits)
{
	uint mask = (1u << bits) - 1;
	return (ins & mask) | (src & (~mask));
}

uint ffxBitfieldExtract(uint src, uint off, uint bits)
{
	uint mask = (1u << bits) - 1;
	return (src >> off) & mask;
}

uint2 ffxRemapForQuad(uint a)
{
	return uint2(ffxBitfieldExtract(a, 1u, 3u), ffxBitfieldInsertMask(ffxBitfieldExtract(a, 3u, 3u), a, 1u));
}

void CurrFilter(int2 pos)
{
	float3 c;
	ffxFsrEasuFloat(c, pos);
	tex1[pos] = float4(c, 1.0);
}

[numthreads(64, 1, 1)]
void main(uint3 LocalThreadId : SV_GroupThreadID, uint3 WorkGroupId : SV_GroupID, uint3 Dtid : SV_DispatchThreadID)
{
	// Do remapping of local xy in workgroup for a more PS-like swizzle pattern.
	uint2 gxy = ffxRemapForQuad(LocalThreadId.x) + uint2(WorkGroupId.x << 4u, WorkGroupId.y << 4u);
	
	CurrFilter(gxy);
	gxy.x += 8u;
	CurrFilter(gxy);
	gxy.y += 8u;
	CurrFilter(gxy);
	gxy.x -= 8u;
	CurrFilter(gxy);
}

//