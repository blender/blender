/*
 * Copyright 2011-2013 Intel Corporation
 * Modifications Copyright 2014, Blender Foundation.
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

#ifdef WITH_KERNEL_SSE2

#define __KERNEL_SSE2__
#include "util_simd.h"

CCL_NAMESPACE_BEGIN

const __m128 _mm_lookupmask_ps[16] = {
	_mm_castsi128_ps(_mm_set_epi32( 0, 0, 0, 0)),
	_mm_castsi128_ps(_mm_set_epi32( 0, 0, 0,-1)),
	_mm_castsi128_ps(_mm_set_epi32( 0, 0,-1, 0)),
	_mm_castsi128_ps(_mm_set_epi32( 0, 0,-1,-1)),
	_mm_castsi128_ps(_mm_set_epi32( 0,-1, 0, 0)),
	_mm_castsi128_ps(_mm_set_epi32( 0,-1, 0,-1)),
	_mm_castsi128_ps(_mm_set_epi32( 0,-1,-1, 0)),
	_mm_castsi128_ps(_mm_set_epi32( 0,-1,-1,-1)),
	_mm_castsi128_ps(_mm_set_epi32(-1, 0, 0, 0)),
	_mm_castsi128_ps(_mm_set_epi32(-1, 0, 0,-1)),
	_mm_castsi128_ps(_mm_set_epi32(-1, 0,-1, 0)),
	_mm_castsi128_ps(_mm_set_epi32(-1, 0,-1,-1)),
	_mm_castsi128_ps(_mm_set_epi32(-1,-1, 0, 0)),
	_mm_castsi128_ps(_mm_set_epi32(-1,-1, 0,-1)),
	_mm_castsi128_ps(_mm_set_epi32(-1,-1,-1, 0)),
	_mm_castsi128_ps(_mm_set_epi32(-1,-1,-1,-1))
};


CCL_NAMESPACE_END

#endif  // WITH_KERNEL_SSE2
