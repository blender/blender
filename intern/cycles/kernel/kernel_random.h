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
#pragma once

#include "kernel/kernel_jitter.h"
#include "util/util_hash.h"

CCL_NAMESPACE_BEGIN

/* Pseudo random numbers, uncomment this for debugging correlations. Only run
 * this single threaded on a CPU for repeatable results. */
//#define __DEBUG_CORRELATION__

/* High Dimensional Sobol.
 *
 * Multidimensional sobol with generator matrices. Dimension 0 and 1 are equal
 * to classic Van der Corput and Sobol sequences. */

#ifdef __SOBOL__

/* Skip initial numbers that for some dimensions have clear patterns that
 * don't cover the entire sample space. Ideally we would have a better
 * progressive pattern that doesn't suffer from this problem, because even
 * with this offset some dimensions are quite poor.
 */
#  define SOBOL_SKIP 64

ccl_device uint sobol_dimension(KernelGlobals kg, int index, int dimension)
{
  uint result = 0;
  uint i = index + SOBOL_SKIP;
  for (int j = 0, x; (x = find_first_set(i)); i >>= x) {
    j += x;
    result ^= __float_as_uint(kernel_tex_fetch(__sample_pattern_lut, 32 * dimension + j - 1));
  }
  return result;
}

#endif /* __SOBOL__ */

ccl_device_forceinline float path_rng_1D(KernelGlobals kg,
                                         uint rng_hash,
                                         int sample,
                                         int dimension)
{
#ifdef __DEBUG_CORRELATION__
  return (float)drand48();
#endif

#ifdef __SOBOL__
  if (kernel_data.integrator.sampling_pattern == SAMPLING_PATTERN_PMJ)
#endif
  {
    return pmj_sample_1D(kg, sample, rng_hash, dimension);
  }

#ifdef __SOBOL__
  /* Sobol sequence value using direction vectors. */
  uint result = sobol_dimension(kg, sample, dimension);
  float r = (float)result * (1.0f / (float)0xFFFFFFFF);

  /* Cranly-Patterson rotation using rng seed */
  float shift;

  /* Hash rng with dimension to solve correlation issues.
   * See T38710, T50116.
   */
  uint tmp_rng = cmj_hash_simple(dimension, rng_hash);
  shift = tmp_rng * (1.0f / (float)0xFFFFFFFF);

  return r + shift - floorf(r + shift);
#endif
}

ccl_device_forceinline void path_rng_2D(KernelGlobals kg,
                                        uint rng_hash,
                                        int sample,
                                        int dimension,
                                        ccl_private float *fx,
                                        ccl_private float *fy)
{
#ifdef __DEBUG_CORRELATION__
  *fx = (float)drand48();
  *fy = (float)drand48();
  return;
#endif

#ifdef __SOBOL__
  if (kernel_data.integrator.sampling_pattern == SAMPLING_PATTERN_PMJ)
#endif
  {
    pmj_sample_2D(kg, sample, rng_hash, dimension, fx, fy);

    return;
  }

#ifdef __SOBOL__
  /* Sobol. */
  *fx = path_rng_1D(kg, rng_hash, sample, dimension);
  *fy = path_rng_1D(kg, rng_hash, sample, dimension + 1);
#endif
}

/**
 * 1D hash recommended from "Hash Functions for GPU Rendering" JCGT Vol. 9, No. 3, 2020
 * See https://www.shadertoy.com/view/4tXyWN and https://www.shadertoy.com/view/XlGcRh
 * http://www.jcgt.org/published/0009/03/02/paper.pdf
 */
ccl_device_inline uint hash_iqint1(uint n)
{
  n = (n << 13U) ^ n;
  n = n * (n * n * 15731U + 789221U) + 1376312589U;

  return n;
}

/**
 * 2D hash recommended from "Hash Functions for GPU Rendering" JCGT Vol. 9, No. 3, 2020
 * See https://www.shadertoy.com/view/4tXyWN and https://www.shadertoy.com/view/XlGcRh
 * http://www.jcgt.org/published/0009/03/02/paper.pdf
 */
ccl_device_inline uint hash_iqnt2d(const uint x, const uint y)
{
  const uint qx = 1103515245U * ((x >> 1U) ^ (y));
  const uint qy = 1103515245U * ((y >> 1U) ^ (x));
  const uint n = 1103515245U * ((qx) ^ (qy >> 3U));

  return n;
}

ccl_device_inline uint path_rng_hash_init(KernelGlobals kg,
                                          const int sample,
                                          const int x,
                                          const int y)
{
  const uint rng_hash = hash_iqnt2d(x, y) ^ kernel_data.integrator.seed;

#ifdef __DEBUG_CORRELATION__
  srand48(rng_hash + sample);
#else
  (void)sample;
#endif

  return rng_hash;
}

/* Linear Congruential Generator */

ccl_device uint lcg_step_uint(uint *rng)
{
  /* implicit mod 2^32 */
  *rng = (1103515245 * (*rng) + 12345);
  return *rng;
}

ccl_device float lcg_step_float(uint *rng)
{
  /* implicit mod 2^32 */
  *rng = (1103515245 * (*rng) + 12345);
  return (float)*rng * (1.0f / (float)0xFFFFFFFF);
}

ccl_device uint lcg_init(uint seed)
{
  uint rng = seed;
  lcg_step_uint(&rng);
  return rng;
}

ccl_device_inline uint lcg_state_init(const uint rng_hash,
                                      const uint rng_offset,
                                      const uint sample,
                                      const uint scramble)
{
  return lcg_init(rng_hash + rng_offset + sample * scramble);
}

ccl_device_inline bool sample_is_even(int pattern, int sample)
{
  if (pattern == SAMPLING_PATTERN_PMJ) {
    /* See Section 10.2.1, "Progressive Multi-Jittered Sample Sequences", Christensen et al.
     * We can use this to get divide sample sequence into two classes for easier variance
     * estimation. */
#if defined(__GNUC__) && !defined(__KERNEL_GPU__)
    return __builtin_popcount(sample & 0xaaaaaaaa) & 1;
#elif defined(__NVCC__)
    return __popc(sample & 0xaaaaaaaa) & 1;
#else
    /* TODO(Stefan): pop-count intrinsic for Windows with fallback for older CPUs. */
    int i = sample & 0xaaaaaaaa;
    i = i - ((i >> 1) & 0x55555555);
    i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
    i = (((i + (i >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
    return i & 1;
#endif
  }
  else {
    /* TODO(Stefan): Are there reliable ways of dividing CMJ and Sobol into two classes? */
    return sample & 0x1;
  }
}

CCL_NAMESPACE_END
