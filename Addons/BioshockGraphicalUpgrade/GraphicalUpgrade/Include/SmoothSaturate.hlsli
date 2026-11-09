#ifndef __SMOOTH_SATURATE_HLSLI__
#define __SMOOTH_SATURATE_HLSLI__

// x - input
// k - peak, should be in range (0, +inf)
// m - mid/anchor point, should be in range [0, k)
// p - compression strength, for m = 0 should be in range (0, +inf) and for m > 0 should be in range (1, +inf)
//
// Returns input in range [0, k).
#define SMOOTH_SATURATE_TEMPLATE(T)                                                                      \
T smooth_saturate(T x, float k, float m, float p)                                                        \
{                                                                                                        \
	x = max(0.0, x);                                                                                     \
	const T x_minus_m = x - m;                                                                           \
	const float k_minus_m = k - m;                                                                       \
	return x < m ? x : m + k_minus_m * x_minus_m * pow(pow(k_minus_m, p) + pow(x_minus_m, p), -1.0 / p); \
}                                                                                                        \

// Generate functions.
SMOOTH_SATURATE_TEMPLATE(float)
SMOOTH_SATURATE_TEMPLATE(float3)

#undef SMOOTH_SATURATE_TEMPLATE

// x - input
// k - peak, should be in range (0, +inf)
// m - mid/anchor point, should be in range [0, k)
// w - white/clip point, should be in range [k, +inf)
// p - compression strength, for m = 0 should be in range (0, +inf) and for m > 0 should be in range (1, +inf)
//
// Returns input in range [0, k].
#define SMOOTH_SATURATE_TEMPLATE(T)                                                                                                                                            \
T smooth_saturate(T x, float k, float m, float w, float p)                                                                                                                     \
{                                                                                                                                                                              \
	x = max(0.0, x);                                                                                                                                                           \
	const T x_minus_m = x - m;                                                                                                                                                 \
	const float k_minus_m = k - m;                                                                                                                                             \
	const float w_minus_m = w - m;                                                                                                                                             \
	const float k_minus_m_to_p = pow(k_minus_m, p);                                                                                                                            \
	const float w_minus_m_to_p = pow(w_minus_m, p);                                                                                                                            \
	return x < m ? x : min(m + k_minus_m * w_minus_m * x_minus_m * pow(k_minus_m_to_p * w_minus_m_to_p + (w_minus_m_to_p - k_minus_m_to_p) * pow(x_minus_m, p), -1.0 / p), k); \
}                                                                                                                                                                              \

// Generate functions.
SMOOTH_SATURATE_TEMPLATE(float)
SMOOTH_SATURATE_TEMPLATE(float3)

#undef SMOOTH_SATURATE_TEMPLATE

// Inverse functions.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// x - input
// k - peak
// m - mid/anchor point
// p - compression strength
//
// Returns input in range [0, +inf).
#define SMOOTH_SATURATE_TEMPLATE(T)                                                                      \
T inv_smooth_saturate(T x, float k, float m, float p)                                                    \
{                                                                                                        \
	x = clamp(x, 0.0, k - 1e-6);                                                                         \
	const T x_minus_m = x - m;                                                                           \
	const float k_minus_m = k - m;                                                                       \
	return x < m ? x : m + k_minus_m * x_minus_m * pow(pow(k_minus_m, p) - pow(x_minus_m, p), -1.0 / p); \
}                                                                                                        \

// Generate functions.
SMOOTH_SATURATE_TEMPLATE(float)
SMOOTH_SATURATE_TEMPLATE(float3)

#undef SMOOTH_SATURATE_TEMPLATE

// x - input
// k - peak
// m - mid/anchor point
// w - white/clip point
// p - compression strength
//
// Returns input in range [0, w].
#define SMOOTH_SATURATE_TEMPLATE(T)                                                                                                                                    \
T inv_clipped_smooth_saturate(T x, float k, float m, float w, float p)                                                                                                 \
{                                                                                                                                                                      \
	x = clamp(x, 0.0, k);                                                                                                                                              \
	const T x_minus_m = x - m;                                                                                                                                         \
	const float k_minus_m = k - m;                                                                                                                                     \
	const float w_minus_m = w - m;                                                                                                                                     \
	const float k_minus_m_to_p = pow(k_minus_m, p);                                                                                                                    \
	const float w_minus_m_to_p = pow(w_minus_m, p);                                                                                                                    \
	return x < m ? x : m + k_minus_m * w_minus_m * x_minus_m * pow(k_minus_m_to_p * w_minus_m_to_p - (w_minus_m_to_p - k_minus_m_to_p) * pow(x_minus_m, p), -1.0 / p); \
}                                                                                                                                                                      \

// Generate functions.
SMOOTH_SATURATE_TEMPLATE(float)
SMOOTH_SATURATE_TEMPLATE(float3)

#undef SMOOTH_SATURATE_TEMPLATE

#endif // __SMOOTH_SATURATE_HLSLI__