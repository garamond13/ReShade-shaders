#ifndef __SMOOTH_SATURATE_HLSLI__
#define __SMOOTH_SATURATE_HLSLI__

// x - input
// k - peak
// p - compression strength, should be in range (0, +inf)
//
// Returns input in range [0, peak).
#define TEMPLATE_SMOOTH_SATURATE(T)                      \
T smooth_saturate(T x, float k, float p)                 \
{                                                        \
	x = max(0.0, x);                                     \
	return k * x * pow(pow(k, p) + pow(x, p), -1.0 / p); \
}                                                        \

// Generate functions.
TEMPLATE_SMOOTH_SATURATE(float)
TEMPLATE_SMOOTH_SATURATE(float3)

#undef TEMPLATE_SMOOTH_SATURATE

// x - input
// k - peak
// w - white/clip point
// p - compression strength, should be in range (0, +inf)
//
// Returns input in range [0, peak].
#define TEMPLATE_SMOOTH_SATURATE(T)                                                            \
T smooth_saturate(T x, float k, float w, float p)                                              \
{                                                                                              \
	x = max(0.0, x);                                                                           \
	const float k_to_p = pow(k, p);                                                            \
	const float w_to_p = pow(w, p);                                                            \
	return min(k * w * x * pow(k_to_p * w_to_p + (w_to_p - k_to_p) * pow(x, p), -1.0 / p), k); \
}                                                                                              \

// Generate functions.
TEMPLATE_SMOOTH_SATURATE(float)
TEMPLATE_SMOOTH_SATURATE(float3)

#undef TEMPLATE_SMOOTH_SATURATE

// x - input
// k - peak
// w - white/clip point
// m - mid/anchor point
// p - compression strength, should be in range (1, +inf)
//
// Returns input in range [0, peak].
#define TEMPLATE_SMOOTH_SATURATE(T)                                                                                                                                             \
T smooth_saturate(T x, float k, float w, float m, float p)                                                                                                                      \
{                                                                                                                                                                               \
	x = max(0.0, x);                                                                                                                                                            \
	const T x_minus_m = x - m;                                                                                                                                                  \
	const float k_minus_m = k - m;                                                                                                                                              \
	const float w_minus_m = w - m;                                                                                                                                              \
	const float k_minus_m_to_p = pow(k_minus_m, p);                                                                                                                             \
	const float w_minus_m_to_p = pow(w_minus_m, p);                                                                                                                             \
	return x <= m ? x : min(m + k_minus_m * w_minus_m * x_minus_m * pow(k_minus_m_to_p * w_minus_m_to_p + (w_minus_m_to_p - k_minus_m_to_p) * pow(x_minus_m, p), -1.0 / p), k); \
}                                                                                                                                                                               \

// Generate functions.
TEMPLATE_SMOOTH_SATURATE(float)
TEMPLATE_SMOOTH_SATURATE(float3)

#undef TEMPLATE_SMOOTH_SATURATE

// Inverse functions.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// x - input
// k - peak
// p - compression strength, should be in range (0, +inf)
//
// Returns input in range [0, +inf).
#define TEMPLATE_INV_SMOOTH_SATURATE(T)                  \
T inv_smooth_saturate(T x, float k, float p)             \
{                                                        \
	x = clamp(x, 0.0, k - 1e-6);                         \
	return k * x * pow(pow(k, p) - pow(x, p), -1.0 / p); \
}                                                        \

// Generate functions.
TEMPLATE_INV_SMOOTH_SATURATE(float)
TEMPLATE_INV_SMOOTH_SATURATE(float3)

#undef TEMPLATE_INV_SMOOTH_SATURATE

// x - input
// k - peak
// w - white/clip point
// p - compression strength, should be in range (0, +inf)
//
// Returns input in range [0, white/clip point].
#define TEMPLATE_INV_SMOOTH_SATURATE(T)                                                \
T inv_smooth_saturate(T x, float k, float w, float p)                                  \
{                                                                                      \
	x = clamp(x, 0.0, k);                                                              \
	const float k_to_p = pow(k, p);                                                    \
	const float w_to_p = pow(w, p);                                                    \
	return k * w * x * pow(k_to_p * w_to_p - (w_to_p - k_to_p) * pow(x, p), -1.0 / p); \
}                                                                                      \

// Generate functions.
TEMPLATE_INV_SMOOTH_SATURATE(float)
TEMPLATE_INV_SMOOTH_SATURATE(float3)

#undef TEMPLATE_INV_SMOOTH_SATURATE

// x - input
// k - peak
// w - white/clip point
// m - mid/anchor point
// p - compression strength, should be in range (1, +inf)
//
// Returns input in range [0, white/clip point].
#define TEMPLATE_INV_SMOOTH_SATURATE(T)                                                                                                                                 \
T inv_smooth_saturate(T x, float k, float w, float m, float p)                                                                                                          \
{                                                                                                                                                                       \
	x = clamp(x, 0.0, k);                                                                                                                                               \
	const T x_minus_m = x - m;                                                                                                                                          \
	const float k_minus_m = k - m;                                                                                                                                      \
	const float w_minus_m = w - m;                                                                                                                                      \
	const float k_minus_m_to_p = pow(k_minus_m, p);                                                                                                                     \
	const float w_minus_m_to_p = pow(w_minus_m, p);                                                                                                                     \
	return x <= m ? x : m + k_minus_m * w_minus_m * x_minus_m * pow(k_minus_m_to_p * w_minus_m_to_p - (w_minus_m_to_p - k_minus_m_to_p) * pow(x_minus_m, p), -1.0 / p); \
}                                                                                                                                                                       \

// Generate functions.
TEMPLATE_INV_SMOOTH_SATURATE(float)
TEMPLATE_INV_SMOOTH_SATURATE(float3)

#undef TEMPLATE_INV_SMOOTH_SATURATE

#endif // __SMOOTH_SATURATE_HLSLI__