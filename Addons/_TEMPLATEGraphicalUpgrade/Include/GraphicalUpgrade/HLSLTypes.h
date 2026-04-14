#pragma once

#include "Common.h"

// bool has to be eider uint32_t or int32_t!

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

template<typename T>
struct Mat1x1
{
	T _m00;
};

template<typename T>
struct Mat1x2
{
	T _m00, _m01;
};

template<typename T>
struct Mat1x3
{
	T _m00, _m01, _m02;
};

template<typename T>
struct Mat1x4
{
	T _m00, _m01, _m02, _m03;
};

template<typename T>
struct Mat2x1
{
	T _m00;
	T _m10;
};

template<typename T>
struct Mat2x2
{
	T _m00, _m01;
	T _m10, _m11;
};

template<typename T>
struct Mat2x3
{
	T _m00, _m01, _m02;
	T _m10, _m11, _m12;
};

template<typename T>
struct Mat2x4
{
	T _m00, _m01, _m02, _m03;
	T _m10, _m11, _m12, _m13;
};

template<typename T>
struct Mat3x1
{
	T _m00;
	T _m10;
	T _m20;
};

template<typename T>
struct Mat3x2
{
	T _m00, _m01;
	T _m10, _m11;
	T _m20, _m21;
};

template<typename T>
struct Mat3x3
{
	T _m00, _m01, _m02;
	T _m10, _m11, _m12;
	T _m20, _m21, _m22;
};

template<typename T>
struct Mat3x4
{
	T _m00, _m01, _m02, _m03;
	T _m10, _m11, _m12, _m13;
	T _m20, _m21, _m22, _m23;
};

template<typename T>
struct Mat4x1
{
	T _m00;
	T _m10;
	T _m20;
	T _m30;
};

template<typename T>
struct Mat4x2
{
	T _m00, _m01;
	T _m10, _m11;
	T _m20, _m21;
	T _m30, _m31;
};

template<typename T>
struct Mat4x3
{
	T _m00, _m01, _m02;
	T _m10, _m11, _m12;
	T _m20, _m21, _m22;
	T _m30, _m31, _m32;
};

template<typename T>
struct Mat4x4
{
	T _m00, _m01, _m02, _m03;
	T _m10, _m11, _m12, _m13;
	T _m20, _m21, _m22, _m23;
	T _m30, _m31, _m32, _m33;
};

// float
//

// Vectors.
typedef Vec2<float> float2;
typedef Vec3<float> float3;
typedef Vec4<float> float4;

// Matrices.
typedef Mat1x1<float> float1x1;
typedef Mat1x2<float> float1x2;
typedef Mat1x3<float> float1x3;
typedef Mat1x4<float> float1x4;
typedef Mat2x1<float> float2x1;
typedef Mat2x2<float> float2x2;
typedef Mat2x3<float> float2x3;
typedef Mat2x4<float> float2x4;
typedef Mat3x1<float> float3x1;
typedef Mat3x2<float> float3x2;
typedef Mat3x3<float> float3x3;
typedef Mat3x4<float> float3x4;
typedef Mat4x1<float> float4x1;
typedef Mat4x2<float> float4x2;
typedef Mat4x3<float> float4x3;
typedef Mat4x4<float> float4x4;

//

// int
//

// Vectors.
typedef Vec2<int32_t> int2;
typedef Vec3<int32_t> int3;
typedef Vec4<int32_t> int4;

// Matrices.
typedef Mat1x1<int32_t> int1x1;
typedef Mat1x2<int32_t> int1x2;
typedef Mat1x3<int32_t> int1x3;
typedef Mat1x4<int32_t> int1x4;
typedef Mat2x1<int32_t> int2x1;
typedef Mat2x2<int32_t> int2x2;
typedef Mat2x3<int32_t> int2x3;
typedef Mat2x4<int32_t> int2x4;
typedef Mat3x1<int32_t> int3x1;
typedef Mat3x2<int32_t> int3x2;
typedef Mat3x3<int32_t> int3x3;
typedef Mat3x4<int32_t> int3x4;
typedef Mat4x1<int32_t> int4x1;
typedef Mat4x2<int32_t> int4x2;
typedef Mat4x3<int32_t> int4x3;
typedef Mat4x4<int32_t> int4x4;

//

// uint
//

typedef uint32_t uint;

// Vectors.
typedef Vec2<uint32_t> uint2;
typedef Vec3<uint32_t> uint3;
typedef Vec4<uint32_t> uint4;

// Matrices.
typedef Mat1x1<uint32_t> uint1x1;
typedef Mat1x2<uint32_t> uint1x2;
typedef Mat1x3<uint32_t> uint1x3;
typedef Mat1x4<uint32_t> uint1x4;
typedef Mat2x1<uint32_t> uint2x1;
typedef Mat2x2<uint32_t> uint2x2;
typedef Mat2x3<uint32_t> uint2x3;
typedef Mat2x4<uint32_t> uint2x4;
typedef Mat3x1<uint32_t> uint3x1;
typedef Mat3x2<uint32_t> uint3x2;
typedef Mat3x3<uint32_t> uint3x3;
typedef Mat3x4<uint32_t> uint3x4;
typedef Mat4x1<uint32_t> uint4x1;
typedef Mat4x2<uint32_t> uint4x2;
typedef Mat4x3<uint32_t> uint4x3;
typedef Mat4x4<uint32_t> uint4x4;

//