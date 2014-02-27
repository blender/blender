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

#ifndef __UTIL_HALF_H__
#define __UTIL_HALF_H__

#include "util_types.h"

#ifdef __KERNEL_SSE2__
#include "util_simd.h"
#endif

CCL_NAMESPACE_BEGIN

/* Half Floats */

#ifdef __KERNEL_OPENCL__

#define float4_store_half(h, f, scale) vstore_half4(f * (scale), 0, h);

#else

typedef unsigned short half;
struct half4 { half x, y, z, w; };

#ifdef __KERNEL_CUDA__

ccl_device_inline void float4_store_half(half *h, float4 f, float scale)
{
	h[0] = __float2half_rn(f.x * scale);
	h[1] = __float2half_rn(f.y * scale);
	h[2] = __float2half_rn(f.z * scale);
	h[3] = __float2half_rn(f.w * scale);
}

#else

ccl_device_inline void float4_store_half(half *h, float4 f, float scale)
{
#ifndef __KERNEL_SSE2__
	for(int i = 0; i < 4; i++) {
		/* optimized float to half for pixels:
		 * assumes no negative, no nan, no inf, and sets denormal to 0 */
		union { uint i; float f; } in;
		float fscale = f[i] * scale;
		in.f = (fscale > 0.0f)? ((fscale < 65500.0f)? fscale: 65500.0f): 0.0f;
		int x = in.i;

		int absolute = x & 0x7FFFFFFF;
		int Z = absolute + 0xC8000000;
		int result = (absolute < 0x38800000)? 0: Z;
		int rshift = (result >> 13);

		h[i] = (rshift & 0x7FFF);
	}
#else
	/* same as above with SSE */
	const __m128 mm_scale = _mm_set_ps1(scale);
	const __m128i mm_38800000 = _mm_set1_epi32(0x38800000);
	const __m128i mm_7FFF = _mm_set1_epi32(0x7FFF);
	const __m128i mm_7FFFFFFF = _mm_set1_epi32(0x7FFFFFFF);
	const __m128i mm_C8000000 = _mm_set1_epi32(0xC8000000);

	__m128 mm_fscale = _mm_mul_ps(load_m128(f), mm_scale);
	__m128i x = _mm_castps_si128(_mm_min_ps(_mm_max_ps(mm_fscale, _mm_set_ps1(0.0f)), _mm_set_ps1(65500.0f)));
	__m128i absolute = _mm_and_si128(x, mm_7FFFFFFF);
	__m128i Z = _mm_add_epi32(absolute, mm_C8000000);
	__m128i result = _mm_andnot_si128(_mm_cmplt_epi32(absolute, mm_38800000), Z); 
	__m128i rh = _mm_and_si128(_mm_srai_epi32(result, 13), mm_7FFF);

	_mm_storel_pi((__m64*)h, _mm_castsi128_ps(_mm_packs_epi32(rh, rh)));
#endif
}

#endif

#endif

CCL_NAMESPACE_END

#endif /* __UTIL_HALF_H__ */

