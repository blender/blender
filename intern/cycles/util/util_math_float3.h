/*
 * Copyright 2011-2017 Blender Foundation
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

#ifndef __UTIL_MATH_FLOAT3_H__
#define __UTIL_MATH_FLOAT3_H__

#ifndef __UTIL_MATH_H__
#  error "Do not include this file directly, include util_types.h instead."
#endif

CCL_NAMESPACE_BEGIN

/*******************************************************************************
 * Declaration.
 */

#ifndef __KERNEL_OPENCL__
ccl_device_inline float3 operator-(const float3& a);
ccl_device_inline float3 operator*(const float3& a, const float3& b);
ccl_device_inline float3 operator*(const float3& a, const float f);
ccl_device_inline float3 operator*(const float f, const float3& a);
ccl_device_inline float3 operator/(const float f, const float3& a);
ccl_device_inline float3 operator/(const float3& a, const float f);
ccl_device_inline float3 operator/(const float3& a, const float3& b);
ccl_device_inline float3 operator+(const float3& a, const float3& b);
ccl_device_inline float3 operator-(const float3& a, const float3& b);
ccl_device_inline float3 operator+=(float3& a, const float3& b);
ccl_device_inline float3 operator-=(float3& a, const float3& b);
ccl_device_inline float3 operator*=(float3& a, const float3& b);
ccl_device_inline float3 operator*=(float3& a, float f);
ccl_device_inline float3 operator/=(float3& a, const float3& b);
ccl_device_inline float3 operator/=(float3& a, float f);

ccl_device_inline bool operator==(const float3& a, const float3& b);
ccl_device_inline bool operator!=(const float3& a, const float3& b);

ccl_device_inline float dot(const float3& a, const float3& b);
ccl_device_inline float dot_xy(const float3& a, const float3& b);
ccl_device_inline float3 cross(const float3& a, const float3& b);
ccl_device_inline float3 normalize(const float3& a);
ccl_device_inline float3 min(const float3& a, const float3& b);
ccl_device_inline float3 max(const float3& a, const float3& b);
ccl_device_inline float3 clamp(const float3& a, const float3& mn, const float3& mx);
ccl_device_inline float3 fabs(const float3& a);
ccl_device_inline float3 mix(const float3& a, const float3& b, float t);
ccl_device_inline float3 rcp(const float3& a);
ccl_device_inline float3 sqrt(const float3& a);
#endif  /* !__KERNEL_OPENCL__ */

ccl_device_inline float min3(float3 a);
ccl_device_inline float max3(float3 a);
ccl_device_inline float len(const float3 a);
ccl_device_inline float len_squared(const float3 a);

ccl_device_inline float3 saturate3(float3 a);
ccl_device_inline float3 safe_normalize(const float3 a);
ccl_device_inline float3 normalize_len(const float3 a, float *t);
ccl_device_inline float3 safe_normalize_len(const float3 a, float *t);
ccl_device_inline float3 interp(float3 a, float3 b, float t);

ccl_device_inline bool is_zero(const float3 a);
ccl_device_inline float reduce_add(const float3 a);
ccl_device_inline float average(const float3 a);
ccl_device_inline bool isequal_float3(const float3 a, const float3 b);

/*******************************************************************************
 * Definition.
 */

#ifndef __KERNEL_OPENCL__
ccl_device_inline float3 operator-(const float3& a)
{
#ifdef __KERNEL_SSE__
	return float3(_mm_xor_ps(a.m128, _mm_castsi128_ps(_mm_set1_epi32(0x80000000))));
#else
	return make_float3(-a.x, -a.y, -a.z);
#endif
}

ccl_device_inline float3 operator*(const float3& a, const float3& b)
{
#ifdef __KERNEL_SSE__
	return float3(_mm_mul_ps(a.m128,b.m128));
#else
	return make_float3(a.x*b.x, a.y*b.y, a.z*b.z);
#endif
}

ccl_device_inline float3 operator*(const float3& a, const float f)
{
#ifdef __KERNEL_SSE__
	return float3(_mm_mul_ps(a.m128,_mm_set1_ps(f)));
#else
	return make_float3(a.x*f, a.y*f, a.z*f);
#endif
}

ccl_device_inline float3 operator*(const float f, const float3& a)
{
#if defined(__KERNEL_SSE__)
	return float3(_mm_mul_ps(_mm_set1_ps(f), a.m128));
#else
	return make_float3(a.x*f, a.y*f, a.z*f);
#endif
}

ccl_device_inline float3 operator/(const float f, const float3& a)
{
#if defined(__KERNEL_SSE__)
	return float3(_mm_div_ps(_mm_set1_ps(f), a.m128));
#else
	return make_float3(f / a.x, f / a.y, f / a.z);
#endif
}

ccl_device_inline float3 operator/(const float3& a, const float f)
{
	float invf = 1.0f/f;
	return a * invf;
}

ccl_device_inline float3 operator/(const float3& a, const float3& b)
{
#if defined(__KERNEL_SSE__)
	return float3(_mm_div_ps(a.m128, b.m128));
#else
	return make_float3(a.x / b.x, a.y / b.y, a.z / b.z);
#endif
}

ccl_device_inline float3 operator+(const float3& a, const float3& b)
{
#ifdef __KERNEL_SSE__
	return float3(_mm_add_ps(a.m128, b.m128));
#else
	return make_float3(a.x + b.x, a.y + b.y, a.z + b.z);
#endif
}

ccl_device_inline float3 operator-(const float3& a, const float3& b)
{
#ifdef __KERNEL_SSE__
	return float3(_mm_sub_ps(a.m128, b.m128));
#else
	return make_float3(a.x - b.x, a.y - b.y, a.z - b.z);
#endif
}

ccl_device_inline float3 operator+=(float3& a, const float3& b)
{
	return a = a + b;
}

ccl_device_inline float3 operator-=(float3& a, const float3& b)
{
	return a = a - b;
}

ccl_device_inline float3 operator*=(float3& a, const float3& b)
{
	return a = a * b;
}

ccl_device_inline float3 operator*=(float3& a, float f)
{
	return a = a * f;
}

ccl_device_inline float3 operator/=(float3& a, const float3& b)
{
	return a = a / b;
}

ccl_device_inline float3 operator/=(float3& a, float f)
{
	float invf = 1.0f/f;
	return a = a * invf;
}

ccl_device_inline bool operator==(const float3& a, const float3& b)
{
#ifdef __KERNEL_SSE__
	return (_mm_movemask_ps(_mm_cmpeq_ps(a.m128, b.m128)) & 7) == 7;
#else
	return (a.x == b.x && a.y == b.y && a.z == b.z);
#endif
}

ccl_device_inline bool operator!=(const float3& a, const float3& b)
{
	return !(a == b);
}

ccl_device_inline float dot(const float3& a, const float3& b)
{
#if defined(__KERNEL_SSE41__) && defined(__KERNEL_SSE__)
	return _mm_cvtss_f32(_mm_dp_ps(a, b, 0x7F));
#else
	return a.x*b.x + a.y*b.y + a.z*b.z;
#endif
}

ccl_device_inline float dot_xy(const float3& a, const float3& b)
{
#if defined(__KERNEL_SSE41__) && defined(__KERNEL_SSE__)
	return _mm_cvtss_f32(_mm_hadd_ps(_mm_mul_ps(a,b),b));
#else
	return a.x*b.x + a.y*b.y;
#endif
}

ccl_device_inline float3 cross(const float3& a, const float3& b)
{
	float3 r = make_float3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
	return r;
}

ccl_device_inline float3 normalize(const float3& a)
{
#if defined(__KERNEL_SSE41__) && defined(__KERNEL_SSE__)
	__m128 norm = _mm_sqrt_ps(_mm_dp_ps(a.m128, a.m128, 0x7F));
	return float3(_mm_div_ps(a.m128, norm));
#else
	return a/len(a);
#endif
}

ccl_device_inline float3 min(const float3& a, const float3& b)
{
#ifdef __KERNEL_SSE__
	return float3(_mm_min_ps(a.m128, b.m128));
#else
	return make_float3(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z));
#endif
}

ccl_device_inline float3 max(const float3& a, const float3& b)
{
#ifdef __KERNEL_SSE__
	return float3(_mm_max_ps(a.m128, b.m128));
#else
	return make_float3(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z));
#endif
}

ccl_device_inline float3 clamp(const float3& a, const float3& mn, const float3& mx)
{
	return min(max(a, mn), mx);
}

ccl_device_inline float3 fabs(const float3& a)
{
#ifdef __KERNEL_SSE__
	__m128 mask = _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff));
	return float3(_mm_and_ps(a.m128, mask));
#else
	return make_float3(fabsf(a.x), fabsf(a.y), fabsf(a.z));
#endif
}

ccl_device_inline float3 sqrt(const float3& a)
{
#ifdef __KERNEL_SSE__
	return float3(_mm_sqrt_ps(a));
#else
	return make_float3(sqrtf(a.x), sqrtf(a.y), sqrtf(a.z));
#endif
}

ccl_device_inline float3 mix(const float3& a, const float3& b, float t)
{
	return a + t*(b - a);
}

ccl_device_inline float3 rcp(const float3& a)
{
#ifdef __KERNEL_SSE__
	/* Don't use _mm_rcp_ps due to poor precision. */
	return float3(_mm_div_ps(_mm_set_ps1(1.0f), a.m128));
#else
	return make_float3(1.0f/a.x, 1.0f/a.y, 1.0f/a.z);
#endif
}
#endif  /* !__KERNEL_OPENCL__ */

ccl_device_inline float min3(float3 a)
{
	return min(min(a.x, a.y), a.z);
}

ccl_device_inline float max3(float3 a)
{
	return max(max(a.x, a.y), a.z);
}

ccl_device_inline float len(const float3 a)
{
#if defined(__KERNEL_SSE41__) && defined(__KERNEL_SSE__)
	return _mm_cvtss_f32(_mm_sqrt_ss(_mm_dp_ps(a.m128, a.m128, 0x7F)));
#else
	return sqrtf(dot(a, a));
#endif
}

ccl_device_inline float len_squared(const float3 a)
{
	return dot(a, a);
}

ccl_device_inline float3 saturate3(float3 a)
{
	return make_float3(saturate(a.x), saturate(a.y), saturate(a.z));
}

ccl_device_inline float3 normalize_len(const float3 a, float *t)
{
	*t = len(a);
	float x = 1.0f / *t;
	return a*x;
}

ccl_device_inline float3 safe_normalize(const float3 a)
{
	float t = len(a);
	return (t != 0.0f)? a * (1.0f/t) : a;
}

ccl_device_inline float3 safe_normalize_len(const float3 a, float *t)
{
	*t = len(a);
	return (*t != 0.0f)? a/(*t): a;
}

ccl_device_inline float3 interp(float3 a, float3 b, float t)
{
	return a + t*(b - a);
}

ccl_device_inline bool is_zero(const float3 a)
{
#ifdef __KERNEL_SSE__
	return a == make_float3(0.0f);
#else
	return (a.x == 0.0f && a.y == 0.0f && a.z == 0.0f);
#endif
}

ccl_device_inline float reduce_add(const float3 a)
{
	return (a.x + a.y + a.z);
}

ccl_device_inline float average(const float3 a)
{
	return reduce_add(a)*(1.0f/3.0f);
}

ccl_device_inline bool isequal_float3(const float3 a, const float3 b)
{
#ifdef __KERNEL_OPENCL__
	return all(a == b);
#else
	return a == b;
#endif
}

ccl_device_inline float3 pow3(float3 v, float e)
{
	return make_float3(powf(v.x, e), powf(v.y, e), powf(v.z, e));
}

ccl_device_inline float3 exp3(float3 v)
{
	return make_float3(expf(v.x), expf(v.y), expf(v.z));
}

ccl_device_inline float3 log3(float3 v)
{
	return make_float3(logf(v.x), logf(v.y), logf(v.z));
}

ccl_device_inline int3 quick_floor_to_int3(const float3 a)
{
#ifdef __KERNEL_SSE__
	int3 b = int3(_mm_cvttps_epi32(a.m128));
	int3 isneg = int3(_mm_castps_si128(_mm_cmplt_ps(a.m128, _mm_set_ps1(0.0f))));
	/* Unsaturated add 0xffffffff is the same as subtract -1. */
	return b + isneg;
#else
	return make_int3(quick_floor_to_int(a.x), quick_floor_to_int(a.y), quick_floor_to_int(a.z));
#endif
}

ccl_device_inline bool isfinite3_safe(float3 v)
{
	return isfinite_safe(v.x) && isfinite_safe(v.y) && isfinite_safe(v.z);
}

ccl_device_inline float3 ensure_finite3(float3 v)
{
	if(!isfinite_safe(v.x)) v.x = 0.0f;
	if(!isfinite_safe(v.y)) v.y = 0.0f;
	if(!isfinite_safe(v.z)) v.z = 0.0f;
	return v;
}

CCL_NAMESPACE_END

#endif /* __UTIL_MATH_FLOAT3_H__ */
