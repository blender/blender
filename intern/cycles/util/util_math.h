/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __UTIL_MATH_H__
#define __UTIL_MATH_H__

/* Math
 *
 * Basic math functions on scalar and vector types. This header is used by
 * both the kernel code when compiled as C++, and other C++ non-kernel code. */

#ifndef __KERNEL_OPENCL__

#define _USE_MATH_DEFINES

#include <float.h>
#include <math.h>
#include <stdio.h>

#endif

#include "util_types.h"

CCL_NAMESPACE_BEGIN

/* Float Pi variations */

#ifndef M_PI_F
#define M_PI_F		((float)3.14159265358979323846264338327950288)
#endif
#ifndef M_PI_2_F
#define M_PI_2_F	((float)1.57079632679489661923132169163975144)
#endif
#ifndef M_PI_4_F
#define M_PI_4_F	((float)0.785398163397448309615660845819875721)
#endif
#ifndef M_1_PI_F
#define M_1_PI_F	((float)0.318309886183790671537767526745028724)
#endif
#ifndef M_2_PI_F
#define M_2_PI_F	((float)0.636619772367581343075535053490057448)
#endif

/* Scalar */

#ifdef _WIN32

#ifndef __KERNEL_GPU__

#if(!defined(FREE_WINDOWS))
#define copysignf(x, y) ((float)_copysign(x, y))
#define hypotf(x, y) _hypotf(x, y)
#define isnan(x) _isnan(x)
#define isfinite(x) _finite(x)
#endif

#endif

#ifndef __KERNEL_OPENCL__

__device_inline float fmaxf(float a, float b)
{
	return (a > b)? a: b;
}

__device_inline float fminf(float a, float b)
{
	return (a < b)? a: b;
}

#endif

#endif

#ifndef __KERNEL_GPU__

__device_inline int max(int a, int b)
{
	return (a > b)? a: b;
}

__device_inline int min(int a, int b)
{
	return (a < b)? a: b;
}

__device_inline float max(float a, float b)
{
	return (a > b)? a: b;
}

__device_inline float min(float a, float b)
{
	return (a < b)? a: b;
}

__device_inline double max(double a, double b)
{
	return (a > b)? a: b;
}

__device_inline double min(double a, double b)
{
	return (a < b)? a: b;
}

#endif

__device_inline float min4(float a, float b, float c, float d)
{
	return min(min(a, b), min(c, d));
}

__device_inline float max4(float a, float b, float c, float d)
{
	return max(max(a, b), max(c, d));
}

#ifndef __KERNEL_OPENCL__

__device_inline int clamp(int a, int mn, int mx)
{
	return min(max(a, mn), mx);
}

__device_inline float clamp(float a, float mn, float mx)
{
	return min(max(a, mn), mx);
}

#endif

__device_inline float signf(float f)
{
	return (f < 0.0f)? -1.0f: 1.0f;
}

__device_inline float nonzerof(float f, float eps)
{
	if(fabsf(f) < eps)
		return signf(f)*eps;
	else
		return f;
}

/* Float2 Vector */

#ifndef __KERNEL_OPENCL__

__device_inline bool is_zero(const float2 a)
{
	return (a.x == 0.0f && a.y == 0.0f);
}

#endif

#ifndef __KERNEL_OPENCL__

__device_inline float average(const float2 a)
{
	return (a.x + a.y)*(1.0f/2.0f);
}

#endif

#ifndef __KERNEL_OPENCL__

__device_inline float2 operator-(const float2 a)
{
	return make_float2(-a.x, -a.y);
}

__device_inline float2 operator*(const float2 a, const float2 b)
{
	return make_float2(a.x*b.x, a.y*b.y);
}

__device_inline float2 operator*(const float2 a, float f)
{
	return make_float2(a.x*f, a.y*f);
}

__device_inline float2 operator*(float f, const float2 a)
{
	return make_float2(a.x*f, a.y*f);
}

__device_inline float2 operator/(float f, const float2 a)
{
	return make_float2(f/a.x, f/a.y);
}

__device_inline float2 operator/(const float2 a, float f)
{
	float invf = 1.0f/f;
	return make_float2(a.x*invf, a.y*invf);
}

__device_inline float2 operator/(const float2 a, const float2 b)
{
	return make_float2(a.x/b.x, a.y/b.y);
}

__device_inline float2 operator+(const float2 a, const float2 b)
{
	return make_float2(a.x+b.x, a.y+b.y);
}

__device_inline float2 operator-(const float2 a, const float2 b)
{
	return make_float2(a.x-b.x, a.y-b.y);
}

__device_inline float2 operator+=(float2& a, const float2 b)
{
	return a = a + b;
}

__device_inline float2 operator*=(float2& a, const float2 b)
{
	return a = a * b;
}

__device_inline float2 operator*=(float2& a, float f)
{
	return a = a * f;
}

__device_inline float2 operator/=(float2& a, const float2 b)
{
	return a = a / b;
}

__device_inline float2 operator/=(float2& a, float f)
{
	float invf = 1.0f/f;
	return a = a * invf;
}


__device_inline float dot(const float2 a, const float2 b)
{
	return a.x*b.x + a.y*b.y;
}

__device_inline float cross(const float2 a, const float2 b)
{
	return (a.x*b.y - a.y*b.x);
}

#endif

#ifndef __KERNEL_OPENCL__

__device_inline float len(const float2 a)
{
	return sqrtf(dot(a, a));
}

__device_inline float2 normalize(const float2 a)
{
	return a/len(a);
}

__device_inline float2 normalize_len(const float2 a, float *t)
{
	*t = len(a);
	return a/(*t);
}

__device_inline bool operator==(const float2 a, const float2 b)
{
	return (a.x == b.x && a.y == b.y);
}

__device_inline bool operator!=(const float2 a, const float2 b)
{
	return !(a == b);
}

__device_inline float2 min(float2 a, float2 b)
{
	return make_float2(min(a.x, b.x), min(a.y, b.y));
}

__device_inline float2 max(float2 a, float2 b)
{
	return make_float2(max(a.x, b.x), max(a.y, b.y));
}

__device_inline float2 clamp(float2 a, float2 mn, float2 mx)
{
	return min(max(a, mn), mx);
}

__device_inline float2 fabs(float2 a)
{
	return make_float2(fabsf(a.x), fabsf(a.y));
}

__device_inline float2 as_float2(const float4 a)
{
	return make_float2(a.x, a.y);
}

#endif

#ifndef __KERNEL_GPU__

__device_inline void print_float2(const char *label, const float2& a)
{
	printf("%s: %.8f %.8f\n", label, a.x, a.y);
}

#endif

#ifndef __KERNEL_OPENCL__

__device_inline float2 interp(float2 a, float2 b, float t)
{
	return a + t*(b - a);
}

#endif

/* Float3 Vector */

#ifndef __KERNEL_OPENCL__

__device_inline float3 operator-(const float3 a)
{
	return make_float3(-a.x, -a.y, -a.z);
}

__device_inline float3 operator*(const float3 a, const float3 b)
{
	return make_float3(a.x*b.x, a.y*b.y, a.z*b.z);
}

__device_inline float3 operator*(const float3 a, float f)
{
	return make_float3(a.x*f, a.y*f, a.z*f);
}

__device_inline float3 operator*(float f, const float3 a)
{
	return make_float3(a.x*f, a.y*f, a.z*f);
}

__device_inline float3 operator/(float f, const float3 a)
{
	return make_float3(f/a.x, f/a.y, f/a.z);
}

__device_inline float3 operator/(const float3 a, float f)
{
	float invf = 1.0f/f;
	return make_float3(a.x*invf, a.y*invf, a.z*invf);
}

__device_inline float3 operator/(const float3 a, const float3 b)
{
	return make_float3(a.x/b.x, a.y/b.y, a.z/b.z);
}

__device_inline float3 operator+(const float3 a, const float3 b)
{
	return make_float3(a.x+b.x, a.y+b.y, a.z+b.z);
}

__device_inline float3 operator-(const float3 a, const float3 b)
{
	return make_float3(a.x-b.x, a.y-b.y, a.z-b.z);
}

__device_inline float3 operator+=(float3& a, const float3 b)
{
	return a = a + b;
}

__device_inline float3 operator*=(float3& a, const float3 b)
{
	return a = a * b;
}

__device_inline float3 operator*=(float3& a, float f)
{
	return a = a * f;
}

__device_inline float3 operator/=(float3& a, const float3 b)
{
	return a = a / b;
}

__device_inline float3 operator/=(float3& a, float f)
{
	float invf = 1.0f/f;
	return a = a * invf;
}

__device_inline float dot(const float3 a, const float3 b)
{
	return a.x*b.x + a.y*b.y + a.z*b.z;
}

__device_inline float3 cross(const float3 a, const float3 b)
{
	float3 r = make_float3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
	return r;
}

#endif

__device_inline float len(const float3 a)
{
	return sqrtf(dot(a, a));
}

#ifndef __KERNEL_OPENCL__

__device_inline float3 normalize(const float3 a)
{
	return a/len(a);
}

#endif

__device_inline float3 normalize_len(const float3 a, float *t)
{
	*t = len(a);
	return a/(*t);
}

#ifndef __KERNEL_OPENCL__

__device_inline bool operator==(const float3 a, const float3 b)
{
#ifdef __KERNEL_SSE__
	return (_mm_movemask_ps(_mm_cmpeq_ps(a.m128, b.m128)) & 7) == 7;
#else
	return (a.x == b.x && a.y == b.y && a.z == b.z);
#endif
}

__device_inline bool operator!=(const float3 a, const float3 b)
{
	return !(a == b);
}

__device_inline float3 min(float3 a, float3 b)
{
#ifdef __KERNEL_SSE__
	return _mm_min_ps(a.m128, b.m128);
#else
	return make_float3(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z));
#endif
}

__device_inline float3 max(float3 a, float3 b)
{
#ifdef __KERNEL_SSE__
	return _mm_max_ps(a.m128, b.m128);
#else
	return make_float3(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z));
#endif
}

__device_inline float3 clamp(float3 a, float3 mn, float3 mx)
{
	return min(max(a, mn), mx);
}

__device_inline float3 fabs(float3 a)
{
#ifdef __KERNEL_SSE__
	__m128 mask = _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff));
	return _mm_and_ps(a.m128, mask);
#else
	return make_float3(fabsf(a.x), fabsf(a.y), fabsf(a.z));
#endif
}

#endif

__device_inline float3 float4_to_float3(const float4 a)
{
	return make_float3(a.x, a.y, a.z);
}

__device_inline float4 float3_to_float4(const float3 a)
{
	return make_float4(a.x, a.y, a.z, 1.0f);
}

#ifndef __KERNEL_GPU__

__device_inline void print_float3(const char *label, const float3& a)
{
	printf("%s: %.8f %.8f %.8f\n", label, a.x, a.y, a.z);
}

__device_inline float3 rcp(const float3& a)
{
#ifdef __KERNEL_SSE__
	float4 r = _mm_rcp_ps(a.m128);
	return _mm_sub_ps(_mm_add_ps(r, r), _mm_mul_ps(_mm_mul_ps(r, r), a));
#else
	return make_float3(1.0f/a.x, 1.0f/a.y, 1.0f/a.z);
#endif
}

#endif

__device_inline float3 interp(float3 a, float3 b, float t)
{
	return a + t*(b - a);
}

__device_inline bool is_zero(const float3 a)
{
#ifdef __KERNEL_SSE__
	return a == make_float3(0.0f);
#else
	return (a.x == 0.0f && a.y == 0.0f && a.z == 0.0f);
#endif
}

__device_inline float reduce_add(const float3& a)
{
#ifdef __KERNEL_SSE__
	return (a.x + a.y + a.z);
#else
	return (a.x + a.y + a.z);
#endif
}

__device_inline float average(const float3 a)
{
	return reduce_add(a)*(1.0f/3.0f);
}

/* Float4 Vector */

#ifdef __KERNEL_SSE__

template<size_t index_0, size_t index_1, size_t index_2, size_t index_3> __forceinline const float4 shuffle(const float4& b)
{
	return _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(b), _MM_SHUFFLE(index_3, index_2, index_1, index_0)));
}

template<> __forceinline const float4 shuffle<0, 0, 2, 2>(const float4& b)
{
	return _mm_moveldup_ps(b);
}

template<> __forceinline const float4 shuffle<1, 1, 3, 3>(const float4& b)
{
	return _mm_movehdup_ps(b);
}

template<> __forceinline const float4 shuffle<0, 1, 0, 1>(const float4& b)
{
	return _mm_castpd_ps(_mm_movedup_pd(_mm_castps_pd(b)));
}

#endif

#ifndef __KERNEL_OPENCL__

__device_inline float4 operator-(const float4& a)
{
#ifdef __KERNEL_SSE__
	__m128 mask = _mm_castsi128_ps(_mm_set1_epi32(0x80000000));
	return _mm_xor_ps(a.m128, mask);
#else
	return make_float4(-a.x, -a.y, -a.z, -a.w);
#endif
}

__device_inline float4 operator*(const float4& a, const float4& b)
{
#ifdef __KERNEL_SSE__
	return _mm_mul_ps(a.m128, b.m128);
#else
	return make_float4(a.x*b.x, a.y*b.y, a.z*b.z, a.w*b.w);
#endif
}

__device_inline float4 operator*(const float4& a, float f)
{
#ifdef __KERNEL_SSE__
	return a * make_float4(f);
#else
	return make_float4(a.x*f, a.y*f, a.z*f, a.w*f);
#endif
}

__device_inline float4 operator*(float f, const float4& a)
{
	return a * f;
}

__device_inline float4 rcp(const float4& a)
{
#ifdef __KERNEL_SSE__
	float4 r = _mm_rcp_ps(a.m128);
	return _mm_sub_ps(_mm_add_ps(r, r), _mm_mul_ps(_mm_mul_ps(r, r), a));
#else
	return make_float4(1.0f/a.x, 1.0f/a.y, 1.0f/a.z, 1.0f/a.w);
#endif
}

__device_inline float4 operator/(const float4& a, float f)
{
	return a * (1.0f/f);
}

__device_inline float4 operator/(const float4& a, const float4& b)
{
#ifdef __KERNEL_SSE__
	return a * rcp(b);
#else
	return make_float4(a.x/b.x, a.y/b.y, a.z/b.z, a.w/b.w);
#endif

}

__device_inline float4 operator+(const float4& a, const float4& b)
{
#ifdef __KERNEL_SSE__
	return _mm_add_ps(a.m128, b.m128);
#else
	return make_float4(a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w);
#endif
}

__device_inline float4 operator-(const float4& a, const float4& b)
{
#ifdef __KERNEL_SSE__
	return _mm_sub_ps(a.m128, b.m128);
#else
	return make_float4(a.x-b.x, a.y-b.y, a.z-b.z, a.w-b.w);
#endif
}

__device_inline float4 operator+=(float4& a, const float4& b)
{
	return a = a + b;
}

__device_inline float4 operator*=(float4& a, const float4& b)
{
	return a = a * b;
}

__device_inline float4 operator/=(float4& a, float f)
{
	return a = a / f;
}

__device_inline int4 operator<(const float4& a, const float4& b)
{
#ifdef __KERNEL_SSE__
	return _mm_cvtps_epi32(_mm_cmplt_ps(a.m128, b.m128)); /* todo: avoid cvt */
#else
	return make_int4(a.x < b.x, a.y < b.y, a.z < b.z, a.w < b.w);
#endif
}

__device_inline int4 operator>=(float4 a, float4 b)
{
#ifdef __KERNEL_SSE__
	return _mm_cvtps_epi32(_mm_cmpge_ps(a.m128, b.m128)); /* todo: avoid cvt */
#else
	return make_int4(a.x >= b.x, a.y >= b.y, a.z >= b.z, a.w >= b.w);
#endif
}

__device_inline int4 operator<=(const float4& a, const float4& b)
{
#ifdef __KERNEL_SSE__
	return _mm_cvtps_epi32(_mm_cmple_ps(a.m128, b.m128)); /* todo: avoid cvt */
#else
	return make_int4(a.x <= b.x, a.y <= b.y, a.z <= b.z, a.w <= b.w);
#endif
}

__device_inline bool operator==(const float4 a, const float4 b)
{
#ifdef __KERNEL_SSE__
	return (_mm_movemask_ps(_mm_cmpeq_ps(a.m128, b.m128)) & 15) == 15;
#else
	return (a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w);
#endif
}

__device_inline float4 cross(const float4& a, const float4& b)
{
#ifdef __KERNEL_SSE__
	return (shuffle<1,2,0,0>(a)*shuffle<2,0,1,0>(b)) - (shuffle<2,0,1,0>(a)*shuffle<1,2,0,0>(b));
#else
	return make_float4(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x, 0.0f);
#endif
}

__device_inline float4 min(float4 a, float4 b)
{
#ifdef __KERNEL_SSE__
	return _mm_min_ps(a.m128, b.m128);
#else
	return make_float4(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z), min(a.w, b.w));
#endif
}

__device_inline float4 max(float4 a, float4 b)
{
#ifdef __KERNEL_SSE__
	return _mm_max_ps(a.m128, b.m128);
#else
	return make_float4(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z), max(a.w, b.w));
#endif
}

#endif

#ifndef __KERNEL_GPU__

__device_inline float4 select(const int4& mask, const float4& a, const float4& b)
{
#ifdef __KERNEL_SSE__
	/* blendv is sse4, and apparently broken on vs2008 */
	return _mm_or_ps(_mm_and_ps(_mm_cvtepi32_ps(mask), a), _mm_andnot_ps(_mm_cvtepi32_ps(mask), b)); /* todo: avoid cvt */
#else
	return make_float4((mask.x)? a.x: b.x, (mask.y)? a.y: b.y, (mask.z)? a.z: b.z, (mask.w)? a.w: b.w);
#endif
}

__device_inline float4 reduce_min(const float4& a)
{
#ifdef __KERNEL_SSE__
	float4 h = min(shuffle<1,0,3,2>(a), a);
	return min(shuffle<2,3,0,1>(h), h);
#else
	return make_float4(min(min(a.x, a.y), min(a.z, a.w)));
#endif
}

__device_inline float4 reduce_max(const float4& a)
{
#ifdef __KERNEL_SSE__
	float4 h = max(shuffle<1,0,3,2>(a), a);
	return max(shuffle<2,3,0,1>(h), h);
#else
	return make_float4(max(max(a.x, a.y), max(a.z, a.w)));
#endif
}

#if 0
__device_inline float4 reduce_add(const float4& a)
{
#ifdef __KERNEL_SSE__
	float4 h = shuffle<1,0,3,2>(a) + a;
	return shuffle<2,3,0,1>(h) + h;
#else
	return make_float4((a.x + a.y) + (a.z + a.w));
#endif
}
#endif

__device_inline void print_float4(const char *label, const float4& a)
{
	printf("%s: %.8f %.8f %.8f %.8f\n", label, a.x, a.y, a.z, a.w);
}

#endif

#ifndef __KERNEL_OPENCL__

__device_inline bool is_zero(const float4& a)
{
#ifdef __KERNEL_SSE__
	return a == make_float4(0.0f);
#else
	return (a.x == 0.0f && a.y == 0.0f && a.z == 0.0f && a.w == 0.0f);
#endif
}

__device_inline float reduce_add(const float4& a)
{
#ifdef __KERNEL_SSE__
	float4 h = shuffle<1,0,3,2>(a) + a;
	return _mm_cvtss_f32(shuffle<2,3,0,1>(h) + h); /* todo: efficiency? */
#else
	return ((a.x + a.y) + (a.z + a.w));
#endif
}

__device_inline float average(const float4& a)
{
	return reduce_add(a) * 0.25f;
}

__device_inline float dot(const float4& a, const float4& b)
{
	return reduce_add(a * b);
}

#endif

/* Int3 */

#ifndef __KERNEL_OPENCL__

__device_inline int3 min(int3 a, int3 b)
{
#ifdef __KERNEL_SSE__
	return _mm_min_epi32(a.m128, b.m128);
#else
	return make_int3(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z));
#endif
}

__device_inline int3 max(int3 a, int3 b)
{
#ifdef __KERNEL_SSE__
	return _mm_max_epi32(a.m128, b.m128);
#else
	return make_int3(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z));
#endif
}

__device_inline int3 clamp(const int3& a, int mn, int mx)
{
#ifdef __KERNEL_SSE__
	return min(max(a, make_int3(mn)), make_int3(mx));
#else
	return make_int3(clamp(a.x, mn, mx), clamp(a.y, mn, mx), clamp(a.z, mn, mx));
#endif
}

__device_inline int3 clamp(const int3& a, int3& mn, int mx)
{
#ifdef __KERNEL_SSE__
	return min(max(a, mn), make_int3(mx));
#else
	return make_int3(clamp(a.x, mn.x, mx), clamp(a.y, mn.y, mx), clamp(a.z, mn.z, mx));
#endif
}

#endif

#ifndef __KERNEL_GPU__

__device_inline void print_int3(const char *label, const int3& a)
{
	printf("%s: %d %d %d\n", label, a.x, a.y, a.z);
}

#endif

/* Int4 */

#ifndef __KERNEL_GPU__

__device_inline int4 operator+(const int4& a, const int4& b)
{
#ifdef __KERNEL_SSE__
	return _mm_add_epi32(a.m128, b.m128);
#else
	return make_int4(a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w);
#endif
}

__device_inline int4 operator+=(int4& a, const int4& b)
{
	return a = a + b;
}

__device_inline int4 operator>>(const int4& a, int i)
{
#ifdef __KERNEL_SSE__
	return _mm_srai_epi32(a.m128, i);
#else
	return make_int4(a.x >> i, a.y >> i, a.z >> i, a.w >> i);
#endif
}

__device_inline int4 min(int4 a, int4 b)
{
#ifdef __KERNEL_SSE__
	return _mm_min_epi32(a.m128, b.m128);
#else
	return make_int4(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z), min(a.w, b.w));
#endif
}

__device_inline int4 max(int4 a, int4 b)
{
#ifdef __KERNEL_SSE__
	return _mm_max_epi32(a.m128, b.m128);
#else
	return make_int4(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z), max(a.w, b.w));
#endif
}

__device_inline int4 clamp(const int4& a, const int4& mn, const int4& mx)
{
	return min(max(a, mn), mx);
}

__device_inline int4 select(const int4& mask, const int4& a, const int4& b)
{
#ifdef __KERNEL_SSE__
	__m128 m = _mm_cvtepi32_ps(mask);
	return _mm_castps_si128(_mm_or_ps(_mm_and_ps(m, _mm_castsi128_ps(a)), _mm_andnot_ps(m, _mm_castsi128_ps(b)))); /* todo: avoid cvt */
#else
	return make_int4((mask.x)? a.x: b.x, (mask.y)? a.y: b.y, (mask.z)? a.z: b.z, (mask.w)? a.w: b.w);
#endif
}

__device_inline void print_int4(const char *label, const int4& a)
{
	printf("%s: %d %d %d %d\n", label, a.x, a.y, a.z, a.w);
}

#endif

/* Int/Float conversion */

#ifndef __KERNEL_OPENCL__

__device_inline unsigned int as_uint(float f)
{
	union { unsigned int i; float f; } u;
	u.f = f;
	return u.i;
}

__device_inline int __float_as_int(float f)
{
	union { int i; float f; } u;
	u.f = f;
	return u.i;
}

__device_inline float __int_as_float(int i)
{
	union { int i; float f; } u;
	u.i = i;
	return u.f;
}

__device_inline uint __float_as_uint(float f)
{
	union { uint i; float f; } u;
	u.f = f;
	return u.i;
}

__device_inline float __uint_as_float(uint i)
{
	union { uint i; float f; } u;
	u.i = i;
	return u.f;
}

/* Interpolation */

template<class A, class B> A lerp(const A& a, const A& b, const B& t)
{
	return (A)(a * ((B)1 - t) + b * t);
}

/* Triangle */

__device_inline float triangle_area(const float3 v1, const float3 v2, const float3 v3)
{
	return len(cross(v3 - v2, v1 - v2))*0.5f;
}

#endif

/* Orthonormal vectors */

__device_inline void make_orthonormals(const float3 N, float3 *a, float3 *b)
{
	if(N.x != N.y || N.x != N.z)
		*a = make_float3(N.z-N.y, N.x-N.z, N.y-N.x);  //(1,1,1)x N
	else
		*a = make_float3(N.z-N.y, N.x+N.z, -N.y-N.x);  //(-1,1,1)x N

	*a = normalize(*a);
	*b = cross(N, *a);
}

/* Color division */

__device_inline float3 safe_divide_color(float3 a, float3 b)
{
	float x, y, z;

	x = (b.x != 0.0f)? a.x/b.x: 0.0f;
	y = (b.y != 0.0f)? a.y/b.y: 0.0f;
	z = (b.z != 0.0f)? a.z/b.z: 0.0f;

	return make_float3(x, y, z);
}

CCL_NAMESPACE_END

#endif /* __UTIL_MATH_H__ */

