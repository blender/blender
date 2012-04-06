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

__device_inline float fmaxf(float a, float b)
{
	return (a > b)? a: b;
}

__device_inline float fminf(float a, float b)
{
	return (a < b)? a: b;
}

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
	float2 r = {-a.x, -a.y};
	return r;
}

__device_inline float2 operator*(const float2 a, const float2 b)
{
	float2 r = {a.x*b.x, a.y*b.y};
	return r;
}

__device_inline float2 operator*(const float2 a, float f)
{
	float2 r = {a.x*f, a.y*f};
	return r;
}

__device_inline float2 operator*(float f, const float2 a)
{
	float2 r = {a.x*f, a.y*f};
	return r;
}

__device_inline float2 operator/(float f, const float2 a)
{
	float2 r = {f/a.x, f/a.y};
	return r;
}

__device_inline float2 operator/(const float2 a, float f)
{
	float invf = 1.0f/f;
	float2 r = {a.x*invf, a.y*invf};
	return r;
}

__device_inline float2 operator/(const float2 a, const float2 b)
{
	float2 r = {a.x/b.x, a.y/b.y};
	return r;
}

__device_inline float2 operator+(const float2 a, const float2 b)
{
	float2 r = {a.x+b.x, a.y+b.y};
	return r;
}

__device_inline float2 operator-(const float2 a, const float2 b)
{
	float2 r = {a.x-b.x, a.y-b.y};
	return r;
}

__device_inline float2 operator+=(float2& a, const float2 b)
{
	a.x += b.x;
	a.y += b.y;
	return a;
}

__device_inline float2 operator*=(float2& a, const float2 b)
{
	a.x *= b.x;
	a.y *= b.y;
	return a;
}

__device_inline float2 operator*=(float2& a, float f)
{
	a.x *= f;
	a.y *= f;
	return a;
}

__device_inline float2 operator/=(float2& a, const float2 b)
{
	a.x /= b.x;
	a.y /= b.y;
	return a;
}

__device_inline float2 operator/=(float2& a, float f)
{
	float invf = 1.0f/f;
	a.x *= invf;
	a.y *= invf;
	return a;
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
	float2 r = {min(a.x, b.x), min(a.y, b.y)};
	return r;
}

__device_inline float2 max(float2 a, float2 b)
{
	float2 r = {max(a.x, b.x), max(a.y, b.y)};
	return r;
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

__device_inline bool is_zero(const float3 a)
{
	return (a.x == 0.0f && a.y == 0.0f && a.z == 0.0f);
}

__device_inline float average(const float3 a)
{
	return (a.x + a.y + a.z)*(1.0f/3.0f);
}

#ifndef __KERNEL_OPENCL__

__device_inline float3 operator-(const float3 a)
{
	float3 r = make_float3(-a.x, -a.y, -a.z);
	return r;
}

__device_inline float3 operator*(const float3 a, const float3 b)
{
	float3 r = make_float3(a.x*b.x, a.y*b.y, a.z*b.z);
	return r;
}

__device_inline float3 operator*(const float3 a, float f)
{
	float3 r = make_float3(a.x*f, a.y*f, a.z*f);
	return r;
}

__device_inline float3 operator*(float f, const float3 a)
{
	float3 r = make_float3(a.x*f, a.y*f, a.z*f);
	return r;
}

__device_inline float3 operator/(float f, const float3 a)
{
	float3 r = make_float3(f/a.x, f/a.y, f/a.z);
	return r;
}

__device_inline float3 operator/(const float3 a, float f)
{
	float invf = 1.0f/f;
	float3 r = make_float3(a.x*invf, a.y*invf, a.z*invf);
	return r;
}

__device_inline float3 operator/(const float3 a, const float3 b)
{
	float3 r = make_float3(a.x/b.x, a.y/b.y, a.z/b.z);
	return r;
}

__device_inline float3 operator+(const float3 a, const float3 b)
{
	float3 r = make_float3(a.x+b.x, a.y+b.y, a.z+b.z);
	return r;
}

__device_inline float3 operator-(const float3 a, const float3 b)
{
	float3 r = make_float3(a.x-b.x, a.y-b.y, a.z-b.z);
	return r;
}

__device_inline float3 operator+=(float3& a, const float3 b)
{
	a.x += b.x;
	a.y += b.y;
	a.z += b.z;
	return a;
}

__device_inline float3 operator*=(float3& a, const float3 b)
{
	a.x *= b.x;
	a.y *= b.y;
	a.z *= b.z;
	return a;
}

__device_inline float3 operator*=(float3& a, float f)
{
	a.x *= f;
	a.y *= f;
	a.z *= f;
	return a;
}

__device_inline float3 operator/=(float3& a, const float3 b)
{
	a.x /= b.x;
	a.y /= b.y;
	a.z /= b.z;
	return a;
}

__device_inline float3 operator/=(float3& a, float f)
{
	float invf = 1.0f/f;
	a.x *= invf;
	a.y *= invf;
	a.z *= invf;
	return a;
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
	return (a.x == b.x && a.y == b.y && a.z == b.z);
}

__device_inline bool operator!=(const float3 a, const float3 b)
{
	return !(a == b);
}

__device_inline float3 min(float3 a, float3 b)
{
	float3 r = make_float3(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z));
	return r;
}

__device_inline float3 max(float3 a, float3 b)
{
	float3 r = make_float3(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z));
	return r;
}

__device_inline float3 clamp(float3 a, float3 mn, float3 mx)
{
	return min(max(a, mn), mx);
}

__device_inline float3 fabs(float3 a)
{
	return make_float3(fabsf(a.x), fabsf(a.y), fabsf(a.z));
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

#endif

__device_inline float3 interp(float3 a, float3 b, float t)
{
	return a + t*(b - a);
}

/* Float4 Vector */

#ifndef __KERNEL_OPENCL__

__device_inline bool is_zero(const float4& a)
{
	return (a.x == 0.0f && a.y == 0.0f && a.z == 0.0f && a.w == 0.0f);
}

__device_inline float average(const float4& a)
{
	return (a.x + a.y + a.z + a.w)*(1.0f/4.0f);
}

__device_inline float4 operator-(const float4& a)
{
	float4 r = {-a.x, -a.y, -a.z, -a.w};
	return r;
}

__device_inline float4 operator*(const float4& a, const float4& b)
{
	float4 r = {a.x*b.x, a.y*b.y, a.z*b.z, a.w*b.w};
	return r;
}

__device_inline float4 operator*(const float4& a, float f)
{
	float4 r = {a.x*f, a.y*f, a.z*f, a.w*f};
	return r;
}

__device_inline float4 operator*(float f, const float4& a)
{
	float4 r = {a.x*f, a.y*f, a.z*f, a.w*f};
	return r;
}

__device_inline float4 operator/(const float4& a, float f)
{
	float invf = 1.0f/f;
	float4 r = {a.x*invf, a.y*invf, a.z*invf, a.w*invf};
	return r;
}

__device_inline float4 operator/(const float4& a, const float4& b)
{
	float4 r = {a.x/b.x, a.y/b.y, a.z/b.z, a.w/b.w};
	return r;
}

__device_inline float4 operator+(const float4& a, const float4& b)
{
	float4 r = {a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w};
	return r;
}

__device_inline float4 operator-(const float4& a, const float4& b)
{
	float4 r = {a.x-b.x, a.y-b.y, a.z-b.z, a.w-b.w};
	return r;
}

__device_inline float4 operator+=(float4& a, const float4& b)
{
	a.x += b.x;
	a.y += b.y;
	a.z += b.z;
	a.w += b.w;
	return a;
}

__device_inline float4 operator*=(float4& a, const float4& b)
{
	a.x *= b.x;
	a.y *= b.y;
	a.z *= b.z;
	a.w *= b.w;
	return a;
}

__device_inline float4 operator/=(float4& a, float f)
{
	float invf = 1.0f/f;
	a.x *= invf;
	a.y *= invf;
	a.z *= invf;
	a.w *= invf;
	return a;
}

__device_inline float dot(const float4& a, const float4& b)
{
	return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
}

__device_inline float4 cross(const float4& a, const float4& b)
{
	float4 r = {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x, 0.0f};
	return r;
}

__device_inline float4 min(float4 a, float4 b)
{
	return make_float4(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z), min(a.w, b.w));
}

__device_inline float4 max(float4 a, float4 b)
{
	return make_float4(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z), max(a.w, b.w));
}

#endif

#ifndef __KERNEL_GPU__

__device_inline void print_float4(const char *label, const float4& a)
{
	printf("%s: %.8f %.8f %.8f %.8f\n", label, a.x, a.y, a.z, a.w);
}

#endif

/* Int3 */

#ifndef __KERNEL_OPENCL__

__device_inline int3 max(int3 a, int3 b)
{
	int3 r = {max(a.x, b.x), max(a.y, b.y), max(a.z, b.z)};
	return r;
}

__device_inline int3 clamp(const int3& a, int mn, int mx)
{
	int3 r = {clamp(a.x, mn, mx), clamp(a.y, mn, mx), clamp(a.z, mn, mx)};
	return r;
}

__device_inline int3 clamp(const int3& a, int3& mn, int mx)
{
	int3 r = {clamp(a.x, mn.x, mx), clamp(a.y, mn.y, mx), clamp(a.z, mn.z, mx)};
	return r;
}

#endif

#ifndef __KERNEL_GPU__

__device_inline void print_int3(const char *label, const int3& a)
{
	printf("%s: %d %d %d\n", label, a.x, a.y, a.z);
}

#endif

/* Int4 */

#ifndef __KERNEL_OPENCL__

__device_inline int4 operator>=(float4 a, float4 b)
{
	return make_int4(a.x >= b.x, a.y >= b.y, a.z >= b.z, a.w >= b.w);
}

#endif

#ifndef __KERNEL_GPU__

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

