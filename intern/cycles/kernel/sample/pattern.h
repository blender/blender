/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "kernel/sample/jitter.h"
#include "kernel/sample/sobol_burley.h"
#include "util/hash.h"

CCL_NAMESPACE_BEGIN

/* Pseudo random numbers, uncomment this for debugging correlations. Only run
 * this single threaded on a CPU for repeatable results. */
//#define __DEBUG_CORRELATION__

ccl_device_forceinline float path_rng_1D(KernelGlobals kg,
                                         uint rng_hash,
                                         int sample,
                                         int dimension)
{
#ifdef __DEBUG_CORRELATION__
  return (float)drand48();
#endif

  if (kernel_data.integrator.sampling_pattern == SAMPLING_PATTERN_SOBOL_BURLEY) {
    return sobol_burley_sample_1D(sample, dimension, rng_hash);
  }
  else {
    return pmj_sample_1D(kg, sample, rng_hash, dimension);
  }
}

ccl_device_forceinline float2 path_rng_2D(KernelGlobals kg,
                                          uint rng_hash,
                                          int sample,
                                          int dimension)
{
#ifdef __DEBUG_CORRELATION__
  return make_float2((float)drand48(), (float)drand48());
#endif

  if (kernel_data.integrator.sampling_pattern == SAMPLING_PATTERN_SOBOL_BURLEY) {
    return sobol_burley_sample_2D(sample, dimension, rng_hash);
  }
  else {
    return pmj_sample_2D(kg, sample, rng_hash, dimension);
  }
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

ccl_device_inline bool sample_is_even(int pattern, int sample)
{
  if (pattern == SAMPLING_PATTERN_PMJ) {
    /* See Section 10.2.1, "Progressive Multi-Jittered Sample Sequences", Christensen et al.
     * We can use this to get divide sample sequence into two classes for easier variance
     * estimation. */
    return popcount(uint(sample) & 0xaaaaaaaa) & 1;
  }
  else {
    /* TODO(Stefan): Are there reliable ways of dividing Sobol-Burley into two classes? */
    return sample & 0x1;
  }
}

CCL_NAMESPACE_END
