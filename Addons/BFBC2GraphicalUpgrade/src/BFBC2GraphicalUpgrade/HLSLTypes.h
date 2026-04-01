#pragma once

#include "Common.h"

template<typename T>
struct Vec2
{
	T x;
	T y;
};

template<typename T>
struct Vec3
{
	T x;
	T y;
	T z;
};

template<typename T>
struct Vec4
{
	T x;
	T y;
	T z;
	T w;
};

// float
typedef Vec2<float> float2;
typedef Vec3<float> float3;
typedef Vec4<float> float4;

// int
typedef Vec2<int32_t> int2;
typedef Vec3<int32_t> int3;
typedef Vec4<int32_t> int4;

// uint
typedef Vec2<uint32_t> uint2;
typedef Vec3<uint32_t> uint3;
typedef Vec4<uint32_t> uint4;

// bool has to be eider uint32_t or int32_t.