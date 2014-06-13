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
	const ssef mm_scale = ssef(scale);
	const ssei mm_38800000 = ssei(0x38800000);
	const ssei mm_7FFF = ssei(0x7FFF);
	const ssei mm_7FFFFFFF = ssei(0x7FFFFFFF);
	const ssei mm_C8000000 = ssei(0xC8000000);

	ssef mm_fscale = load4f(f) * mm_scale;
	ssei x = cast(min(max(mm_fscale, ssef(0.0f)), ssef(65500.0f)));
	ssei absolute = x & mm_7FFFFFFF;
	ssei Z = absolute + mm_C8000000;
	ssei result = andnot(absolute < mm_38800000, Z); 
	ssei rh = (result >> 13) & mm_7FFF;

	_mm_storel_pi((__m64*)h, _mm_castsi128_ps(_mm_packs_epi32(rh, rh)));
#endif
}

#endif

#endif

CCL_NAMESPACE_END

#endif /* __UTIL_HALF_H__ */

