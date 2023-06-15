/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/sample/sobol_burley.h"
#include "kernel/sample/tabulated_sobol.h"
#include "util/hash.h"

CCL_NAMESPACE_BEGIN

/* Pseudo random numbers, uncomment this for debugging correlations. Only run
 * this single threaded on a CPU for repeatable results. */
//#define __DEBUG_CORRELATION__

/*
 * The `path_rng_*()` functions below use a shuffled scrambled Sobol
 * sequence to generate their samples.  Sobol samplers have a property
 * that is worth being aware of when choosing how to use the sample
 * dimensions:
 *
 * 1. In general, earlier sets of dimensions are better stratified.  So
 *    prefer e.g. x,y over y,z over z,w for the things that are most
 *    important to sample well.
 * 2. As a rule of thumb, dimensions that are closer to each other are
 *    better stratified than dimensions that are far.  So prefer e.g.
 *    x,y over x,z.
 */

ccl_device_forceinline float path_rng_1D(KernelGlobals kg,
                                         uint rng_hash,
                                         int sample,
                                         int dimension)
{
#ifdef __DEBUG_CORRELATION__
  return (float)drand48();
#endif

  if (kernel_data.integrator.sampling_pattern == SAMPLING_PATTERN_SOBOL_BURLEY) {
    const uint index_mask = kernel_data.integrator.sobol_index_mask;
    return sobol_burley_sample_1D(sample, dimension, rng_hash, index_mask);
  }
  else {
    return tabulated_sobol_sample_1D(kg, sample, rng_hash, dimension);
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
    const uint index_mask = kernel_data.integrator.sobol_index_mask;
    return sobol_burley_sample_2D(sample, dimension, rng_hash, index_mask);
  }
  else {
    return tabulated_sobol_sample_2D(kg, sample, rng_hash, dimension);
  }
}

ccl_device_forceinline float3 path_rng_3D(KernelGlobals kg,
                                          uint rng_hash,
                                          int sample,
                                          int dimension)
{
#ifdef __DEBUG_CORRELATION__
  return make_float3((float)drand48(), (float)drand48(), (float)drand48());
#endif

  if (kernel_data.integrator.sampling_pattern == SAMPLING_PATTERN_SOBOL_BURLEY) {
    const uint index_mask = kernel_data.integrator.sobol_index_mask;
    return sobol_burley_sample_3D(sample, dimension, rng_hash, index_mask);
  }
  else {
    return tabulated_sobol_sample_3D(kg, sample, rng_hash, dimension);
  }
}

ccl_device_forceinline float4 path_rng_4D(KernelGlobals kg,
                                          uint rng_hash,
                                          int sample,
                                          int dimension)
{
#ifdef __DEBUG_CORRELATION__
  return make_float4((float)drand48(), (float)drand48(), (float)drand48(), (float)drand48());
#endif

  if (kernel_data.integrator.sampling_pattern == SAMPLING_PATTERN_SOBOL_BURLEY) {
    const uint index_mask = kernel_data.integrator.sobol_index_mask;
    return sobol_burley_sample_4D(sample, dimension, rng_hash, index_mask);
  }
  else {
    return tabulated_sobol_sample_4D(kg, sample, rng_hash, dimension);
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

/**
 * Splits samples into two different classes, A and B, which can be
 * compared for variance estimation.
 */
ccl_device_inline bool sample_is_class_A(int pattern, int sample)
{
#if 0
  if (!(pattern == SAMPLING_PATTERN_TABULATED_SOBOL || pattern == SAMPLING_PATTERN_SOBOL_BURLEY)) {
    /* Fallback: assign samples randomly.
     * This is guaranteed to work "okay" for any sampler, but isn't good.
     * (NOTE: the seed constant is just a random number to guard against
     * possible interactions with other uses of the hash. There's nothing
     * special about it.)
     */
    return hash_hp_seeded_uint(sample, 0xa771f873) & 1;
  }
#else
  (void)pattern;
#endif

  /* This follows the approach from section 10.2.1 of "Progressive
   * Multi-Jittered Sample Sequences" by Christensen et al., but
   * implemented with efficient bit-fiddling.
   *
   * This approach also turns out to work equally well with Owen
   * scrambled and shuffled Sobol (see https://developer.blender.org/D15746#429471).
   */
  return popcount(uint(sample) & 0xaaaaaaaa) & 1;
}
CCL_NAMESPACE_END
