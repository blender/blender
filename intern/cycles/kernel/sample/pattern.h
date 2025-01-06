/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"

#include "kernel/sample/sobol_burley.h"
#include "kernel/sample/tabulated_sobol.h"

#include "util/hash.h"

CCL_NAMESPACE_BEGIN

/* Pseudo random numbers, uncomment this for debugging correlations. Only run
 * this single threaded on a CPU for repeatable results. */
// #define __DEBUG_CORRELATION__

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

ccl_device_forceinline uint3 blue_noise_indexing(KernelGlobals kg,
                                                 uint pixel_index,
                                                 const uint sample)
{
  if (kernel_data.integrator.sampling_pattern == SAMPLING_PATTERN_SOBOL_BURLEY) {
    /* One sequence per pixel, using the length mask optimization. */
    return make_uint3(sample, pixel_index, kernel_data.integrator.sobol_index_mask);
  }
  if (kernel_data.integrator.sampling_pattern == SAMPLING_PATTERN_BLUE_NOISE_PURE) {
    /* For blue-noise samples, we use a single sequence (seed 0) with each pixel receiving
     * a section of it.
     * The total length is expected to get very large (effectively pixel count times sample count),
     * so we don't use the length mask optimization here. */
    pixel_index *= kernel_data.integrator.blue_noise_sequence_length;
    return make_uint3(sample + pixel_index, 0, 0xffffffff);
  }
  if (kernel_data.integrator.sampling_pattern == SAMPLING_PATTERN_BLUE_NOISE_FIRST) {
    /* The "first" pattern uses a 1SPP blue-noise sequence for the first sample, and a separate
     * N-1 SPP sequence for the remaining pixels. The purpose of this is to get blue-noise
     * properties during viewport navigation, which will generally use 1 SPP.
     * Unfortunately using just the first sample of a full blue-noise sequence doesn't give
     * its benefits, so we combine the two as a tradeoff between quality at 1 SPP and full SPP. */
    if (sample == 0) {
      return make_uint3(pixel_index, 0x0cd0519f, 0xffffffff);
    }
    pixel_index *= kernel_data.integrator.blue_noise_sequence_length;
    return make_uint3((sample - 1) + pixel_index, 0, 0xffffffff);
  }
  kernel_assert(false);
  return make_uint3(0, 0, 0);
}

ccl_device_forceinline float path_rng_1D(KernelGlobals kg,
                                         const uint rng_pixel,
                                         const uint sample,
                                         const int dimension)
{
#ifdef __DEBUG_CORRELATION__
  return (float)drand48();
#endif

  if (kernel_data.integrator.sampling_pattern == SAMPLING_PATTERN_TABULATED_SOBOL) {
    return tabulated_sobol_sample_1D(kg, sample, rng_pixel, dimension);
  }

  const uint3 index = blue_noise_indexing(kg, rng_pixel, sample);
  return sobol_burley_sample_1D(index.x, dimension, index.y, index.z);
}

ccl_device_forceinline float2 path_rng_2D(KernelGlobals kg,
                                          const uint rng_pixel,
                                          const int sample,
                                          const int dimension)
{
#ifdef __DEBUG_CORRELATION__
  return make_float2((float)drand48(), (float)drand48());
#endif

  if (kernel_data.integrator.sampling_pattern == SAMPLING_PATTERN_TABULATED_SOBOL) {
    return tabulated_sobol_sample_2D(kg, sample, rng_pixel, dimension);
  }

  const uint3 index = blue_noise_indexing(kg, rng_pixel, sample);
  return sobol_burley_sample_2D(index.x, dimension, index.y, index.z);
}

ccl_device_forceinline float3 path_rng_3D(KernelGlobals kg,
                                          const uint rng_pixel,
                                          const int sample,
                                          const int dimension)
{
#ifdef __DEBUG_CORRELATION__
  return make_float3((float)drand48(), (float)drand48(), (float)drand48());
#endif

  if (kernel_data.integrator.sampling_pattern == SAMPLING_PATTERN_TABULATED_SOBOL) {
    return tabulated_sobol_sample_3D(kg, sample, rng_pixel, dimension);
  }

  const uint3 index = blue_noise_indexing(kg, rng_pixel, sample);
  return sobol_burley_sample_3D(index.x, dimension, index.y, index.z);
}

ccl_device_forceinline float4 path_rng_4D(KernelGlobals kg,
                                          const uint rng_pixel,
                                          const int sample,
                                          const int dimension)
{
#ifdef __DEBUG_CORRELATION__
  return make_float4((float)drand48(), (float)drand48(), (float)drand48(), (float)drand48());
#endif

  if (kernel_data.integrator.sampling_pattern == SAMPLING_PATTERN_TABULATED_SOBOL) {
    return tabulated_sobol_sample_4D(kg, sample, rng_pixel, dimension);
  }

  const uint3 index = blue_noise_indexing(kg, rng_pixel, sample);
  return sobol_burley_sample_4D(index.x, dimension, index.y, index.z);
}

ccl_device_inline uint path_rng_pixel_init(KernelGlobals kg,
                                           const int sample,
                                           const int x,
                                           const int y)
{
  const uint pattern = kernel_data.integrator.sampling_pattern;
  if (pattern == SAMPLING_PATTERN_TABULATED_SOBOL || pattern == SAMPLING_PATTERN_SOBOL_BURLEY) {
#ifdef __DEBUG_CORRELATION__
    return srand48(rng_pixel + sample);
#else
    (void)sample;
#endif

    /* The white-noise samplers use a random per-pixel hash to generate independent sequences. */
    return hash_iqnt2d(x, y) ^ kernel_data.integrator.seed;
  }

  /* The blue-noise samplers use a single sequence for all pixels, but offset the index within
   * the sequence for each pixel. We use a hierarchically shuffled 2D morton curve to determine
   * each pixel's offset along the sequence.
   *
   * Based on:
   * https://psychopath.io/post/2022_07_24_owen_scrambling_based_dithered_blue_noise_sampling.
   *
   * TODO(lukas): Use a precomputed Hilbert curve to avoid directionality bias in the noise
   * distribution. We can just precompute a small-ish tile and repeat it in morton code order.
   */
  return nested_uniform_scramble_base4(morton2d(x, y), kernel_data.integrator.seed);
}

/**
 * Splits samples into two different classes, A and B, which can be
 * compared for variance estimation.
 */
ccl_device_inline bool sample_is_class_A(const int pattern, const int sample)
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
