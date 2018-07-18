/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __UTIL_MATH_H__
#define __UTIL_MATH_H__

/* Math
 *
 * Basic math functions on scalar and vector types. This header is used by
 * both the kernel code when compiled as C++, and other C++ non-kernel code. */

#ifndef __KERNEL_GPU__
#  include <cmath>
#endif


#ifndef __KERNEL_OPENCL__
#  include <float.h>
#  include <math.h>
#  include <stdio.h>
#endif  /* __KERNEL_OPENCL__ */

#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

/* Float Pi variations */

/* Division */
#ifndef M_PI_F
#  define M_PI_F    (3.1415926535897932f)  /* pi */
#endif
#ifndef M_PI_2_F
#  define M_PI_2_F  (1.5707963267948966f)  /* pi/2 */
#endif
#ifndef M_PI_4_F
#  define M_PI_4_F  (0.7853981633974830f)  /* pi/4 */
#endif
#ifndef M_1_PI_F
#  define M_1_PI_F  (0.3183098861837067f)  /* 1/pi */
#endif
#ifndef M_2_PI_F
#  define M_2_PI_F  (0.6366197723675813f)  /* 2/pi */
#endif
#ifndef M_1_2PI_F
#  define M_1_2PI_F (0.1591549430918953f)  /* 1/(2*pi) */
#endif
#ifndef M_SQRT_PI_8_F
#  define M_SQRT_PI_8_F (0.6266570686577501f) /* sqrt(pi/8) */
#endif
#ifndef M_LN_2PI_F
#  define M_LN_2PI_F (1.8378770664093454f) /* ln(2*pi) */
#endif

/* Multiplication */
#ifndef M_2PI_F
#  define M_2PI_F   (6.2831853071795864f)  /* 2*pi */
#endif
#ifndef M_4PI_F
#  define M_4PI_F   (12.566370614359172f)  /* 4*pi */
#endif

/* Float sqrt variations */
#ifndef M_SQRT2_F
#  define M_SQRT2_F (1.4142135623730950f)  /* sqrt(2) */
#endif
#ifndef M_LN2_F
#  define M_LN2_F   (0.6931471805599453f)  /* ln(2) */
#endif
#ifndef M_LN10_F
#  define M_LN10_F  (2.3025850929940457f)  /* ln(10) */
#endif

/* Scalar */

#ifdef _WIN32
#  ifndef __KERNEL_OPENCL__
ccl_device_inline float fmaxf(float a, float b)
{
	return (a > b)? a: b;
}

ccl_device_inline float fminf(float a, float b)
{
	return (a < b)? a: b;
}
#  endif  /* !__KERNEL_OPENCL__ */
#endif  /* _WIN32 */

#ifndef __KERNEL_GPU__
using std::isfinite;
using std::isnan;
using std::sqrt;

ccl_device_inline int abs(int x)
{
	return (x > 0)? x: -x;
}

ccl_device_inline int max(int a, int b)
{
	return (a > b)? a: b;
}

ccl_device_inline int min(int a, int b)
{
	return (a < b)? a: b;
}

ccl_device_inline float max(float a, float b)
{
	return (a > b)? a: b;
}

ccl_device_inline float min(float a, float b)
{
	return (a < b)? a: b;
}

ccl_device_inline double max(double a, double b)
{
	return (a > b)? a: b;
}

ccl_device_inline double min(double a, double b)
{
	return (a < b)? a: b;
}

/* These 2 guys are templated for usage with registers data.
 *
 * NOTE: Since this is CPU-only functions it is ok to use references here.
 * But for other devices we'll need to be careful about this.
 */

template<typename T>
ccl_device_inline T min4(const T& a, const T& b, const T& c, const T& d)
{
	return min(min(a,b),min(c,d));
}

template<typename T>
ccl_device_inline T max4(const T& a, const T& b, const T& c, const T& d)
{
	return max(max(a,b),max(c,d));
}
#endif /* __KERNEL_GPU__ */

ccl_device_inline float min4(float a, float b, float c, float d)
{
	return min(min(a, b), min(c, d));
}

ccl_device_inline float max4(float a, float b, float c, float d)
{
	return max(max(a, b), max(c, d));
}

#ifndef __KERNEL_OPENCL__
/* Int/Float conversion */

ccl_device_inline int as_int(uint i)
{
	union { uint ui; int i; } u;
	u.ui = i;
	return u.i;
}

ccl_device_inline uint as_uint(int i)
{
	union { uint ui; int i; } u;
	u.i = i;
	return u.ui;
}

ccl_device_inline uint as_uint(float f)
{
	union { uint i; float f; } u;
	u.f = f;
	return u.i;
}

ccl_device_inline int __float_as_int(float f)
{
	union { int i; float f; } u;
	u.f = f;
	return u.i;
}

ccl_device_inline float __int_as_float(int i)
{
	union { int i; float f; } u;
	u.i = i;
	return u.f;
}

ccl_device_inline uint __float_as_uint(float f)
{
	union { uint i; float f; } u;
	u.f = f;
	return u.i;
}

ccl_device_inline float __uint_as_float(uint i)
{
	union { uint i; float f; } u;
	u.i = i;
	return u.f;
}
#endif /* __KERNEL_OPENCL__ */

/* Versions of functions which are safe for fast math. */
ccl_device_inline bool isnan_safe(float f)
{
	unsigned int x = __float_as_uint(f);
	return (x << 1) > 0xff000000u;
}

ccl_device_inline bool isfinite_safe(float f)
{
	/* By IEEE 754 rule, 2*Inf equals Inf */
	unsigned int x = __float_as_uint(f);
	return (f == f) && (x == 0 || x == (1u << 31) || (f != 2.0f*f)) && !((x << 1) > 0xff000000u);
}

ccl_device_inline float ensure_finite(float v)
{
	return isfinite_safe(v)? v : 0.0f;
}

#ifndef __KERNEL_OPENCL__
ccl_device_inline int clamp(int a, int mn, int mx)
{
	return min(max(a, mn), mx);
}

ccl_device_inline float clamp(float a, float mn, float mx)
{
	return min(max(a, mn), mx);
}

ccl_device_inline float mix(float a, float b, float t)
{
    return a + t*(b - a);
}
#endif  /* __KERNEL_OPENCL__ */

#ifndef __KERNEL_CUDA__
ccl_device_inline float saturate(float a)
{
	return clamp(a, 0.0f, 1.0f);
}
#endif  /* __KERNEL_CUDA__ */

ccl_device_inline int float_to_int(float f)
{
	return (int)f;
}

ccl_device_inline int floor_to_int(float f)
{
	return float_to_int(floorf(f));
}

ccl_device_inline int quick_floor_to_int(float x)
{
	return float_to_int(x) - ((x < 0) ? 1 : 0);
}

ccl_device_inline int ceil_to_int(float f)
{
	return float_to_int(ceilf(f));
}

ccl_device_inline float signf(float f)
{
	return (f < 0.0f)? -1.0f: 1.0f;
}

ccl_device_inline float nonzerof(float f, float eps)
{
	if(fabsf(f) < eps)
		return signf(f)*eps;
	else
		return f;
}

ccl_device_inline float smoothstepf(float f)
{
	float ff = f*f;
	return (3.0f*ff - 2.0f*ff*f);
}

ccl_device_inline int mod(int x, int m)
{
	return (x % m + m) % m;
}

ccl_device_inline float3 float2_to_float3(const float2 a)
{
	return make_float3(a.x, a.y, 0.0f);
}

ccl_device_inline float3 float4_to_float3(const float4 a)
{
	return make_float3(a.x, a.y, a.z);
}

ccl_device_inline float4 float3_to_float4(const float3 a)
{
	return make_float4(a.x, a.y, a.z, 1.0f);
}

ccl_device_inline float inverse_lerp(float a, float b, float x)
{
	return (x - a) / (b - a);
}

/* Cubic interpolation between b and c, a and d are the previous and next point. */
ccl_device_inline float cubic_interp(float a, float b, float c, float d, float x)
{
	return 0.5f*(((d + 3.0f*(b-c) - a)*x + (2.0f*a - 5.0f*b + 4.0f*c - d))*x + (c - a))*x + b;
}

CCL_NAMESPACE_END

#include "util/util_math_int2.h"
#include "util/util_math_int3.h"
#include "util/util_math_int4.h"

#include "util/util_math_float2.h"
#include "util/util_math_float3.h"
#include "util/util_math_float4.h"

#include "util/util_rect.h"

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_OPENCL__
/* Interpolation */

template<class A, class B> A lerp(const A& a, const A& b, const B& t)
{
	return (A)(a * ((B)1 - t) + b * t);
}

#endif  /* __KERNEL_OPENCL__ */

/* Triangle */

#ifndef __KERNEL_OPENCL__
ccl_device_inline float triangle_area(const float3& v1,
                                      const float3& v2,
                                      const float3& v3)
#else
ccl_device_inline float triangle_area(const float3 v1,
                                      const float3 v2,
                                      const float3 v3)
#endif
{
	return len(cross(v3 - v2, v1 - v2))*0.5f;
}

/* Orthonormal vectors */

ccl_device_inline void make_orthonormals(const float3 N, float3 *a, float3 *b)
{
#if 0
	if(fabsf(N.y) >= 0.999f) {
		*a = make_float3(1, 0, 0);
		*b = make_float3(0, 0, 1);
		return;
	}
	if(fabsf(N.z) >= 0.999f) {
		*a = make_float3(1, 0, 0);
		*b = make_float3(0, 1, 0);
		return;
	}
#endif

	if(N.x != N.y || N.x != N.z)
		*a = make_float3(N.z-N.y, N.x-N.z, N.y-N.x);  //(1,1,1)x N
	else
		*a = make_float3(N.z-N.y, N.x+N.z, -N.y-N.x);  //(-1,1,1)x N

	*a = normalize(*a);
	*b = cross(N, *a);
}

/* Color division */

ccl_device_inline float3 safe_invert_color(float3 a)
{
	float x, y, z;

	x = (a.x != 0.0f)? 1.0f/a.x: 0.0f;
	y = (a.y != 0.0f)? 1.0f/a.y: 0.0f;
	z = (a.z != 0.0f)? 1.0f/a.z: 0.0f;

	return make_float3(x, y, z);
}

ccl_device_inline float3 safe_divide_color(float3 a, float3 b)
{
	float x, y, z;

	x = (b.x != 0.0f)? a.x/b.x: 0.0f;
	y = (b.y != 0.0f)? a.y/b.y: 0.0f;
	z = (b.z != 0.0f)? a.z/b.z: 0.0f;

	return make_float3(x, y, z);
}

ccl_device_inline float3 safe_divide_even_color(float3 a, float3 b)
{
	float x, y, z;

	x = (b.x != 0.0f)? a.x/b.x: 0.0f;
	y = (b.y != 0.0f)? a.y/b.y: 0.0f;
	z = (b.z != 0.0f)? a.z/b.z: 0.0f;

	/* try to get gray even if b is zero */
	if(b.x == 0.0f) {
		if(b.y == 0.0f) {
			x = z;
			y = z;
		}
		else if(b.z == 0.0f) {
			x = y;
			z = y;
		}
		else
			x = 0.5f*(y + z);
	}
	else if(b.y == 0.0f) {
		if(b.z == 0.0f) {
			y = x;
			z = x;
		}
		else
			y = 0.5f*(x + z);
	}
	else if(b.z == 0.0f) {
		z = 0.5f*(x + y);
	}

	return make_float3(x, y, z);
}

/* Rotation of point around axis and angle */

ccl_device_inline float3 rotate_around_axis(float3 p, float3 axis, float angle)
{
	float costheta = cosf(angle);
	float sintheta = sinf(angle);
	float3 r;

	r.x = ((costheta + (1 - costheta) * axis.x * axis.x) * p.x) +
	      (((1 - costheta) * axis.x * axis.y - axis.z * sintheta) * p.y) +
	      (((1 - costheta) * axis.x * axis.z + axis.y * sintheta) * p.z);

	r.y = (((1 - costheta) * axis.x * axis.y + axis.z * sintheta) * p.x) +
	      ((costheta + (1 - costheta) * axis.y * axis.y) * p.y) +
	     (((1 - costheta) * axis.y * axis.z - axis.x * sintheta) * p.z);

	r.z = (((1 - costheta) * axis.x * axis.z - axis.y * sintheta) * p.x) +
	      (((1 - costheta) * axis.y * axis.z + axis.x * sintheta) * p.y) +
	      ((costheta + (1 - costheta) * axis.z * axis.z) * p.z);

	return r;
}

/* NaN-safe math ops */

ccl_device_inline float safe_sqrtf(float f)
{
	return sqrtf(max(f, 0.0f));
}

ccl_device float safe_asinf(float a)
{
	return asinf(clamp(a, -1.0f, 1.0f));
}

ccl_device float safe_acosf(float a)
{
	return acosf(clamp(a, -1.0f, 1.0f));
}

ccl_device float compatible_powf(float x, float y)
{
#ifdef __KERNEL_GPU__
	if(y == 0.0f) /* x^0 -> 1, including 0^0 */
		return 1.0f;

	/* GPU pow doesn't accept negative x, do manual checks here */
	if(x < 0.0f) {
		if(fmodf(-y, 2.0f) == 0.0f)
			return powf(-x, y);
		else
			return -powf(-x, y);
	}
	else if(x == 0.0f)
		return 0.0f;
#endif
	return powf(x, y);
}

ccl_device float safe_powf(float a, float b)
{
	if(UNLIKELY(a < 0.0f && b != float_to_int(b)))
		return 0.0f;

	return compatible_powf(a, b);
}

ccl_device float safe_divide(float a, float b)
{
	return (b != 0.0f)? a/b: 0.0f;
}

ccl_device float safe_logf(float a, float b)
{
	if(UNLIKELY(a <= 0.0f || b <= 0.0f))
		return 0.0f;

	return safe_divide(logf(a),logf(b));
}

ccl_device float safe_modulo(float a, float b)
{
	return (b != 0.0f)? fmodf(a, b): 0.0f;
}

ccl_device_inline float sqr(float a)
{
	return a * a;
}

ccl_device_inline float pow20(float a)
{
    return sqr(sqr(sqr(sqr(a))*a));
}

ccl_device_inline float pow22(float a)
{
    return sqr(a*sqr(sqr(sqr(a))*a));
}

ccl_device_inline float beta(float x, float y)
{
#ifndef __KERNEL_OPENCL__
	return expf(lgammaf(x) + lgammaf(y) - lgammaf(x+y));
#else
	return expf(lgamma(x) + lgamma(y) - lgamma(x+y));
#endif
}

ccl_device_inline float xor_signmask(float x, int y)
{
	return __int_as_float(__float_as_int(x) ^ y);
}

ccl_device float bits_to_01(uint bits)
{
	return bits * (1.0f/(float)0xFFFFFFFF);
}

/* projections */
ccl_device_inline float2 map_to_tube(const float3 co)
{
	float len, u, v;
	len = sqrtf(co.x * co.x + co.y * co.y);
	if(len > 0.0f) {
		u = (1.0f - (atan2f(co.x / len, co.y / len) / M_PI_F)) * 0.5f;
		v = (co.z + 1.0f) * 0.5f;
	}
	else {
		u = v = 0.0f;
	}
	return make_float2(u, v);
}

ccl_device_inline float2 map_to_sphere(const float3 co)
{
	float l = len(co);
	float u, v;
	if(l > 0.0f) {
		if(UNLIKELY(co.x == 0.0f && co.y == 0.0f)) {
			u = 0.0f;  /* othwise domain error */
		}
		else {
			u = (1.0f - atan2f(co.x, co.y) / M_PI_F) / 2.0f;
		}
		v = 1.0f - safe_acosf(co.z / l) / M_PI_F;
	}
	else {
		u = v = 0.0f;
	}
	return make_float2(u, v);
}

CCL_NAMESPACE_END

#endif /* __UTIL_MATH_H__ */
