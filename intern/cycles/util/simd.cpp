/* SPDX-FileCopyrightText: 2011-2013 Intel Corporation
 * SPDX-FileCopyrightText: 2014-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#if (defined(WITH_KERNEL_SSE2)) || (defined(WITH_KERNEL_NATIVE) && defined(__SSE2__))

#  define __KERNEL_SSE2__
#  include "util/simd.h"

CCL_NAMESPACE_BEGIN

const __m128 _mm_lookupmask_ps[16] = {_mm_castsi128_ps(_mm_set_epi32(0, 0, 0, 0)),
                                      _mm_castsi128_ps(_mm_set_epi32(0, 0, 0, -1)),
                                      _mm_castsi128_ps(_mm_set_epi32(0, 0, -1, 0)),
                                      _mm_castsi128_ps(_mm_set_epi32(0, 0, -1, -1)),
                                      _mm_castsi128_ps(_mm_set_epi32(0, -1, 0, 0)),
                                      _mm_castsi128_ps(_mm_set_epi32(0, -1, 0, -1)),
                                      _mm_castsi128_ps(_mm_set_epi32(0, -1, -1, 0)),
                                      _mm_castsi128_ps(_mm_set_epi32(0, -1, -1, -1)),
                                      _mm_castsi128_ps(_mm_set_epi32(-1, 0, 0, 0)),
                                      _mm_castsi128_ps(_mm_set_epi32(-1, 0, 0, -1)),
                                      _mm_castsi128_ps(_mm_set_epi32(-1, 0, -1, 0)),
                                      _mm_castsi128_ps(_mm_set_epi32(-1, 0, -1, -1)),
                                      _mm_castsi128_ps(_mm_set_epi32(-1, -1, 0, 0)),
                                      _mm_castsi128_ps(_mm_set_epi32(-1, -1, 0, -1)),
                                      _mm_castsi128_ps(_mm_set_epi32(-1, -1, -1, 0)),
                                      _mm_castsi128_ps(_mm_set_epi32(-1, -1, -1, -1))};

CCL_NAMESPACE_END

#endif  // WITH_KERNEL_SSE2
