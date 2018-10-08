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

#ifndef __UTIL_MATH_FLOAT4_H__
#define __UTIL_MATH_FLOAT4_H__

#ifndef __UTIL_MATH_H__
#  error "Do not include this file directly, include util_types.h instead."
#endif

CCL_NAMESPACE_BEGIN

/*******************************************************************************
 * Declaration.
 */

#ifndef __KERNEL_OPENCL__
ccl_device_inline float4 operator-(const float4& a);
ccl_device_inline float4 operator*(const float4& a, const float4& b);
ccl_device_inline float4 operator*(const float4& a, float f);
ccl_device_inline float4 operator*(float f, const float4& a);
ccl_device_inline float4 operator/(const float4& a, float f);
ccl_device_inline float4 operator/(const float4& a, const float4& b);
ccl_device_inline float4 operator+(const float4& a, const float4& b);
ccl_device_inline float4 operator-(const float4& a, const float4& b);
ccl_device_inline float4 operator+=(float4& a, const float4& b);
ccl_device_inline float4 operator*=(float4& a, const float4& b);
ccl_device_inline float4 operator*=(float4& a, float f);
ccl_device_inline float4 operator/=(float4& a, float f);

ccl_device_inline int4 operator<(const float4& a, const float4& b);
ccl_device_inline int4 operator>=(const float4& a, const float4& b);
ccl_device_inline int4 operator<=(const float4& a, const float4& b);
ccl_device_inline bool operator==(const float4& a, const float4& b);

ccl_device_inline float dot(const float4& a, const float4& b);
ccl_device_inline float len_squared(const float4& a);
ccl_device_inline float4 rcp(const float4& a);
ccl_device_inline float4 sqrt(const float4& a);
ccl_device_inline float4 sqr(const float4& a);
ccl_device_inline float4 cross(const float4& a, const float4& b);
ccl_device_inline bool is_zero(const float4& a);
ccl_device_inline float average(const float4& a);
ccl_device_inline float len(const float4& a);
ccl_device_inline float4 normalize(const float4& a);
ccl_device_inline float4 safe_normalize(const float4& a);
ccl_device_inline float4 min(const float4& a, const float4& b);
ccl_device_inline float4 max(const float4& a, const float4& b);
ccl_device_inline float4 clamp(const float4& a, const float4& mn, const float4& mx);
ccl_device_inline float4 fabs(const float4& a);
#endif  /* !__KERNEL_OPENCL__*/

#ifdef __KERNEL_SSE__
template<size_t index_0, size_t index_1, size_t index_2, size_t index_3>
__forceinline const float4 shuffle(const float4& b);
template<size_t index_0, size_t index_1, size_t index_2, size_t index_3>
__forceinline const float4 shuffle(const float4& a, const float4& b);

template<> __forceinline const float4 shuffle<0, 1, 0, 1>(const float4& b);

template<> __forceinline const float4 shuffle<0, 1, 0, 1>(const float4& a, const float4& b);
template<> __forceinline const float4 shuffle<2, 3, 2, 3>(const float4& a, const float4& b);

#  ifdef __KERNEL_SSE3__
template<> __forceinline const float4 shuffle<0, 0, 2, 2>(const float4& b);
template<> __forceinline const float4 shuffle<1, 1, 3, 3>(const float4& b);
#  endif
#endif  /* __KERNEL_SSE__ */

#ifndef __KERNEL_GPU__
ccl_device_inline float4 select(const int4& mask,
                                const float4& a,
                                const float4& b);
ccl_device_inline float4 reduce_min(const float4& a);
ccl_device_inline float4 reduce_max(const float4& a);
ccl_device_inline float4 reduce_add(const float4& a);
#endif  /* !__KERNEL_GPU__ */

/*******************************************************************************
 * Definition.
 */

#ifndef __KERNEL_OPENCL__
ccl_device_inline float4 operator-(const float4& a)
{
#ifdef __KERNEL_SSE__
	__m128 mask = _mm_castsi128_ps(_mm_set1_epi32(0x80000000));
	return float4(_mm_xor_ps(a.m128, mask));
#else
	return make_float4(-a.x, -a.y, -a.z, -a.w);
#endif
}

ccl_device_inline float4 operator*(const float4& a, const float4& b)
{
#ifdef __KERNEL_SSE__
	return float4(_mm_mul_ps(a.m128, b.m128));
#else
	return make_float4(a.x*b.x, a.y*b.y, a.z*b.z, a.w*b.w);
#endif
}

ccl_device_inline float4 operator*(const float4& a, float f)
{
#if defined(__KERNEL_SSE__)
	return a * make_float4(f);
#else
	return make_float4(a.x*f, a.y*f, a.z*f, a.w*f);
#endif
}

ccl_device_inline float4 operator*(float f, const float4& a)
{
	return a * f;
}

ccl_device_inline float4 operator/(const float4& a, float f)
{
	return a * (1.0f/f);
}

ccl_device_inline float4 operator/(const float4& a, const float4& b)
{
#ifdef __KERNEL_SSE__
	return float4(_mm_div_ps(a.m128, b.m128));
#else
	return make_float4(a.x/b.x, a.y/b.y, a.z/b.z, a.w/b.w);
#endif

}

ccl_device_inline float4 operator+(const float4& a, const float4& b)
{
#ifdef __KERNEL_SSE__
	return float4(_mm_add_ps(a.m128, b.m128));
#else
	return make_float4(a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w);
#endif
}

ccl_device_inline float4 operator-(const float4& a, const float4& b)
{
#ifdef __KERNEL_SSE__
	return float4(_mm_sub_ps(a.m128, b.m128));
#else
	return make_float4(a.x-b.x, a.y-b.y, a.z-b.z, a.w-b.w);
#endif
}

ccl_device_inline float4 operator+=(float4& a, const float4& b)
{
	return a = a + b;
}

ccl_device_inline float4 operator*=(float4& a, const float4& b)
{
	return a = a * b;
}

ccl_device_inline float4 operator*=(float4& a, float f)
{
	return a = a * f;
}

ccl_device_inline float4 operator/=(float4& a, float f)
{
	return a = a / f;
}

ccl_device_inline int4 operator<(const float4& a, const float4& b)
{
#ifdef __KERNEL_SSE__
	return int4(_mm_castps_si128(_mm_cmplt_ps(a.m128, b.m128)));
#else
	return make_int4(a.x < b.x, a.y < b.y, a.z < b.z, a.w < b.w);
#endif
}

ccl_device_inline int4 operator>=(const float4& a, const float4& b)
{
#ifdef __KERNEL_SSE__
	return int4(_mm_castps_si128(_mm_cmpge_ps(a.m128, b.m128)));
#else
	return make_int4(a.x >= b.x, a.y >= b.y, a.z >= b.z, a.w >= b.w);
#endif
}

ccl_device_inline int4 operator<=(const float4& a, const float4& b)
{
#ifdef __KERNEL_SSE__
	return int4(_mm_castps_si128(_mm_cmple_ps(a.m128, b.m128)));
#else
	return make_int4(a.x <= b.x, a.y <= b.y, a.z <= b.z, a.w <= b.w);
#endif
}

ccl_device_inline bool operator==(const float4& a, const float4& b)
{
#ifdef __KERNEL_SSE__
	return (_mm_movemask_ps(_mm_cmpeq_ps(a.m128, b.m128)) & 15) == 15;
#else
	return (a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w);
#endif
}

ccl_device_inline float dot(const float4& a, const float4& b)
{
#if defined(__KERNEL_SSE41__) && defined(__KERNEL_SSE__)
	return _mm_cvtss_f32(_mm_dp_ps(a, b, 0xFF));
#else
	return (a.x*b.x + a.y*b.y) + (a.z*b.z + a.w*b.w);
#endif
}

ccl_device_inline float len_squared(const float4& a)
{
	return dot(a, a);
}

ccl_device_inline float4 rcp(const float4& a)
{
#ifdef __KERNEL_SSE__
	/* Don't use _mm_rcp_ps due to poor precision. */
	return float4(_mm_div_ps(_mm_set_ps1(1.0f), a.m128));
#else
	return make_float4(1.0f/a.x, 1.0f/a.y, 1.0f/a.z, 1.0f/a.w);
#endif
}

ccl_device_inline float4 sqrt(const float4& a)
{
#ifdef __KERNEL_SSE__
	return float4(_mm_sqrt_ps(a.m128));
#else
	return make_float4(sqrtf(a.x),
	                   sqrtf(a.y),
	                   sqrtf(a.z),
	                   sqrtf(a.w));
#endif
}

ccl_device_inline float4 sqr(const float4& a)
{
	return a * a;
}

ccl_device_inline float4 cross(const float4& a, const float4& b)
{
#ifdef __KERNEL_SSE__
	return (shuffle<1,2,0,0>(a)*shuffle<2,0,1,0>(b)) -
	       (shuffle<2,0,1,0>(a)*shuffle<1,2,0,0>(b));
#else
	return make_float4(a.y*b.z - a.z*b.y,
	                   a.z*b.x - a.x*b.z,
	                   a.x*b.y - a.y*b.x,
	                   0.0f);
#endif
}

ccl_device_inline bool is_zero(const float4& a)
{
#ifdef __KERNEL_SSE__
	return a == make_float4(0.0f);
#else
	return (a.x == 0.0f && a.y == 0.0f && a.z == 0.0f && a.w == 0.0f);
#endif
}

ccl_device_inline float4 reduce_add(const float4& a)
{
#ifdef __KERNEL_SSE__
#  ifdef __KERNEL_SSE3__
    float4 h(_mm_hadd_ps(a.m128, a.m128));
    return float4( _mm_hadd_ps(h.m128, h.m128));
#  else
	float4 h(shuffle<1,0,3,2>(a) + a);
	return  shuffle<2,3,0,1>(h) + h;
#  endif
#else
	float sum = (a.x + a.y) + (a.z + a.w);
	return make_float4(sum, sum, sum, sum);
#endif
}

ccl_device_inline float average(const float4& a)
{
	return reduce_add(a).x * 0.25f;
}

ccl_device_inline float len(const float4& a)
{
	return sqrtf(dot(a, a));
}

ccl_device_inline float4 normalize(const float4& a)
{
	return a/len(a);
}

ccl_device_inline float4 safe_normalize(const float4& a)
{
	float t = len(a);
	return (t != 0.0f)? a/t: a;
}

ccl_device_inline float4 min(const float4& a, const float4& b)
{
#ifdef __KERNEL_SSE__
	return float4(_mm_min_ps(a.m128, b.m128));
#else
	return make_float4(min(a.x, b.x),
	                   min(a.y, b.y),
	                   min(a.z, b.z),
	                   min(a.w, b.w));
#endif
}

ccl_device_inline float4 max(const float4& a, const float4& b)
{
#ifdef __KERNEL_SSE__
	return float4(_mm_max_ps(a.m128, b.m128));
#else
	return make_float4(max(a.x, b.x),
	                   max(a.y, b.y),
	                   max(a.z, b.z),
	                   max(a.w, b.w));
#endif
}

ccl_device_inline float4 clamp(const float4& a, const float4& mn, const float4& mx)
{
	return min(max(a, mn), mx);
}

ccl_device_inline float4 fabs(const float4& a)
{
#ifdef __KERNEL_SSE__
	return float4(_mm_and_ps(a.m128, _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff))));
#else
	return make_float4(fabsf(a.x),
	                   fabsf(a.y),
	                   fabsf(a.z),
	                   fabsf(a.w));
#endif
}
#endif  /* !__KERNEL_OPENCL__*/

#ifdef __KERNEL_SSE__
template<size_t index_0, size_t index_1, size_t index_2, size_t index_3>
__forceinline const float4 shuffle(const float4& b)
{
	return float4(_mm_castsi128_ps(
	        _mm_shuffle_epi32(_mm_castps_si128(b),
	                          _MM_SHUFFLE(index_3, index_2, index_1, index_0))));
}

template<size_t index_0, size_t index_1, size_t index_2, size_t index_3>
__forceinline const float4 shuffle(const float4& a, const float4& b)
{
	return float4(_mm_shuffle_ps(a.m128, b.m128,
	                             _MM_SHUFFLE(index_3, index_2, index_1, index_0)));
}

template<> __forceinline const float4 shuffle<0, 1, 0, 1>(const float4& b)
{
	return float4(_mm_castpd_ps(_mm_movedup_pd(_mm_castps_pd(b))));
}

template<> __forceinline const float4 shuffle<0, 1, 0, 1>(const float4& a, const float4& b)
{
	return float4(_mm_movelh_ps(a.m128, b.m128));
}

template<> __forceinline const float4 shuffle<2, 3, 2, 3>(const float4& a, const float4& b)
{
	return float4(_mm_movehl_ps(b.m128, a.m128));
}

#  ifdef __KERNEL_SSE3__
template<> __forceinline const float4 shuffle<0, 0, 2, 2>(const float4& b)
{
	return float4(_mm_moveldup_ps(b));
}

template<> __forceinline const float4 shuffle<1, 1, 3, 3>(const float4& b)
{
	return float4(_mm_movehdup_ps(b));
}
#  endif  /* __KERNEL_SSE3__ */
#endif  /* __KERNEL_SSE__ */

#ifndef __KERNEL_GPU__
ccl_device_inline float4 select(const int4& mask,
                                const float4& a,
                                const float4& b)
{
#ifdef __KERNEL_SSE__
	return float4(_mm_blendv_ps(b.m128, a.m128, _mm_castsi128_ps(mask.m128)));
#else
	return make_float4((mask.x)? a.x: b.x,
	                   (mask.y)? a.y: b.y,
	                   (mask.z)? a.z: b.z,
	                   (mask.w)? a.w: b.w);
#endif
}

ccl_device_inline float4 mask(const int4& mask,
                              const float4& a)
{
	/* Replace elements of x with zero where mask isn't set. */
	return select(mask, a, make_float4(0.0f));
}

ccl_device_inline float4 reduce_min(const float4& a)
{
#ifdef __KERNEL_SSE__
	float4 h = min(shuffle<1,0,3,2>(a), a);
	return min(shuffle<2,3,0,1>(h), h);
#else
	return make_float4(min(min(a.x, a.y), min(a.z, a.w)));
#endif
}

ccl_device_inline float4 reduce_max(const float4& a)
{
#ifdef __KERNEL_SSE__
	float4 h = max(shuffle<1,0,3,2>(a), a);
	return max(shuffle<2,3,0,1>(h), h);
#else
	return make_float4(max(max(a.x, a.y), max(a.z, a.w)));
#endif
}

ccl_device_inline float4 load_float4(const float *v)
{
#ifdef __KERNEL_SSE__
	return float4(_mm_loadu_ps(v));
#else
	return make_float4(v[0], v[1], v[2], v[3]);
#endif
}

#endif  /* !__KERNEL_GPU__ */

CCL_NAMESPACE_END

#endif /* __UTIL_MATH_FLOAT4_H__ */
