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
 * limitations under the License
 */

#ifndef __UTIL_SIMD_H__
#define __UTIL_SIMD_H__

CCL_NAMESPACE_BEGIN

#ifdef __KERNEL_SSE2__

/* SSE shuffle utility functions */

#ifdef __KERNEL_SSSE3__

/* faster version for SSSE3 */
typedef __m128i shuffle_swap_t;

ccl_device_inline const shuffle_swap_t shuffle_swap_identity(void)
{
	return _mm_set_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
}

ccl_device_inline const shuffle_swap_t shuffle_swap_swap(void)
{
	return _mm_set_epi8(7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8);
}

ccl_device_inline const __m128 shuffle_swap(const __m128& a, const shuffle_swap_t& shuf)
{
	return _mm_castsi128_ps(_mm_shuffle_epi8(_mm_castps_si128(a), shuf));
}

#else

/* somewhat slower version for SSE2 */
typedef int shuffle_swap_t;

ccl_device_inline const shuffle_swap_t shuffle_swap_identity(void)
{
	return 0;
}

ccl_device_inline const shuffle_swap_t shuffle_swap_swap(void)
{
	return 1;
}

ccl_device_inline const __m128 shuffle_swap(const __m128& a, shuffle_swap_t shuf)
{
	/* shuffle value must be a constant, so we need to branch */
	if(shuf)
		return _mm_shuffle_ps(a, a, _MM_SHUFFLE(1, 0, 3, 2));
	else
		return _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 2, 1, 0));
}

#endif

template<size_t i0, size_t i1, size_t i2, size_t i3> ccl_device_inline const __m128 shuffle(const __m128& a, const __m128& b)
{
	return _mm_shuffle_ps(a, b, _MM_SHUFFLE(i3, i2, i1, i0));
}

template<size_t i0, size_t i1, size_t i2, size_t i3> ccl_device_inline const __m128 shuffle(const __m128& b)
{
	return _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(b), _MM_SHUFFLE(i3, i2, i1, i0)));
}

/* Blend 2 vectors based on mask: (a[i] & mask[i]) | (b[i] & ~mask[i]) */
#ifdef __KERNEL_SSE41__
ccl_device_inline const __m128 blend(const __m128& mask, const __m128& a, const __m128& b)
{
	return _mm_blendv_ps(b, a, mask);
}
#else
ccl_device_inline const __m128 blend(const __m128& mask, const __m128& a, const __m128& b)
{
	return _mm_or_ps(_mm_and_ps(mask, a), _mm_andnot_ps(mask, b));
}
#endif

/* calculate a*b+c (replacement for fused multiply-add on SSE CPUs) */
ccl_device_inline const __m128 fma(const __m128& a, const __m128& b, const __m128& c)
{
	return _mm_add_ps(_mm_mul_ps(a, b), c);
}

template<size_t N> ccl_device_inline const __m128 broadcast(const __m128& a)
{
	return _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(a), _MM_SHUFFLE(N, N, N, N)));
}

template<size_t N> ccl_device_inline const __m128i broadcast(const __m128i& a)
{
	return _mm_shuffle_epi32(a, _MM_SHUFFLE(N, N, N, N));
}

#endif /* __KERNEL_SSE2__ */

CCL_NAMESPACE_END

#endif /* __UTIL_SIMD_H__ */

