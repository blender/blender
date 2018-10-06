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

#ifndef __UTIL_MATH_INT4_H__
#define __UTIL_MATH_INT4_H__

#ifndef __UTIL_MATH_H__
#  error "Do not include this file directly, include util_types.h instead."
#endif

CCL_NAMESPACE_BEGIN

/*******************************************************************************
 * Declaration.
 */

#ifndef __KERNEL_GPU__
ccl_device_inline int4 operator+(const int4& a, const int4& b);
ccl_device_inline int4 operator+=(int4& a, const int4& b);
ccl_device_inline int4 operator>>(const int4& a, int i);
ccl_device_inline int4 operator<<(const int4& a, int i);
ccl_device_inline int4 operator<(const int4& a, const int4& b);
ccl_device_inline int4 operator>=(const int4& a, const int4& b);
ccl_device_inline int4 operator&(const int4& a, const int4& b);
ccl_device_inline int4 min(int4 a, int4 b);
ccl_device_inline int4 max(int4 a, int4 b);
ccl_device_inline int4 clamp(const int4& a, const int4& mn, const int4& mx);
ccl_device_inline int4 select(const int4& mask, const int4& a, const int4& b);
#endif  /* __KERNEL_GPU__ */

/*******************************************************************************
 * Definition.
 */

#ifndef __KERNEL_GPU__
ccl_device_inline int4 operator+(const int4& a, const int4& b)
{
#ifdef __KERNEL_SSE__
	return int4(_mm_add_epi32(a.m128, b.m128));
#else
	return make_int4(a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w);
#endif
}

ccl_device_inline int4 operator+=(int4& a, const int4& b)
{
	return a = a + b;
}

ccl_device_inline int4 operator>>(const int4& a, int i)
{
#ifdef __KERNEL_SSE__
	return int4(_mm_srai_epi32(a.m128, i));
#else
	return make_int4(a.x >> i, a.y >> i, a.z >> i, a.w >> i);
#endif
}

ccl_device_inline int4 operator<<(const int4& a, int i)
{
#ifdef __KERNEL_SSE__
	return int4(_mm_slli_epi32(a.m128, i));
#else
	return make_int4(a.x << i, a.y << i, a.z << i, a.w << i);
#endif
}

ccl_device_inline int4 operator<(const int4& a, const int4& b)
{
#ifdef __KERNEL_SSE__
	return int4(_mm_cmplt_epi32(a.m128, b.m128));
#else
	return make_int4(a.x < b.x, a.y < b.y, a.z < b.z, a.w < b.w);
#endif
}

ccl_device_inline int4 operator>=(const int4& a, const int4& b)
{
#ifdef __KERNEL_SSE__
	return int4(_mm_xor_si128(_mm_set1_epi32(0xffffffff), _mm_cmplt_epi32(a.m128, b.m128)));
#else
	return make_int4(a.x >= b.x, a.y >= b.y, a.z >= b.z, a.w >= b.w);
#endif
}

ccl_device_inline int4 operator&(const int4& a, const int4& b)
{
#ifdef __KERNEL_SSE__
	return int4(_mm_and_si128(a.m128, b.m128));
#else
	return make_int4(a.x & b.x, a.y & b.y, a.z & b.z, a.w & b.w);
#endif
}

ccl_device_inline int4 min(int4 a, int4 b)
{
#if defined(__KERNEL_SSE__) && defined(__KERNEL_SSE41__)
	return int4(_mm_min_epi32(a.m128, b.m128));
#else
	return make_int4(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z), min(a.w, b.w));
#endif
}

ccl_device_inline int4 max(int4 a, int4 b)
{
#if defined(__KERNEL_SSE__) && defined(__KERNEL_SSE41__)
	return int4(_mm_max_epi32(a.m128, b.m128));
#else
	return make_int4(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z), max(a.w, b.w));
#endif
}

ccl_device_inline int4 clamp(const int4& a, const int4& mn, const int4& mx)
{
	return min(max(a, mn), mx);
}

ccl_device_inline int4 select(const int4& mask, const int4& a, const int4& b)
{
#ifdef __KERNEL_SSE__
	const __m128 m = _mm_cvtepi32_ps(mask);
	/* TODO(sergey): avoid cvt. */
	return int4(_mm_castps_si128(
	        _mm_or_ps(_mm_and_ps(m, _mm_castsi128_ps(a)),
	                  _mm_andnot_ps(m, _mm_castsi128_ps(b)))));
#else
	return make_int4((mask.x)? a.x: b.x,
	                 (mask.y)? a.y: b.y,
	                 (mask.z)? a.z: b.z,
	                 (mask.w)? a.w: b.w);
#endif
}

ccl_device_inline int4 load_int4(const int *v)
{
#ifdef __KERNEL_SSE__
	return int4(_mm_loadu_si128((__m128i*)v));
#else
	return make_int4(v[0], v[1], v[2], v[3]);
#endif
}
#endif  /* __KERNEL_GPU__ */

CCL_NAMESPACE_END

#endif /* __UTIL_MATH_INT4_H__ */
