/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "kernel/sample/util.h"
#include "util/hash.h"

#pragma once
CCL_NAMESPACE_BEGIN

ccl_device float pmj_sample_1D(KernelGlobals kg,
                               uint sample,
                               const uint rng_hash,
                               const uint dimension)
{
  uint seed = rng_hash;

  /* Use the same sample sequence seed for all pixels when using
   * scrambling distance. */
  if (kernel_data.integrator.scrambling_distance < 1.0f) {
    seed = kernel_data.integrator.seed;
  }

  /* Shuffle the pattern order and sample index to better decorrelate
   * dimensions and make the most of the finite patterns we have.
   * The funky sample mask stuff is to ensure that we only shuffle
   * *within* the current sample pattern, which is necessary to avoid
   * early repeat pattern use. */
  const uint pattern_i = hash_shuffle_uint(dimension, NUM_PMJ_PATTERNS, seed);
  /* NUM_PMJ_SAMPLES should be a power of two, so this results in a mask. */
  const uint sample_mask = NUM_PMJ_SAMPLES - 1;
  const uint sample_shuffled = nested_uniform_scramble(sample,
                                                       hash_wang_seeded_uint(dimension, seed));
  sample = (sample & ~sample_mask) | (sample_shuffled & sample_mask);

  /* Fetch the sample. */
  const uint index = ((pattern_i * NUM_PMJ_SAMPLES) + sample) %
                     (NUM_PMJ_SAMPLES * NUM_PMJ_PATTERNS);
  float x = kernel_data_fetch(sample_pattern_lut, index * 2);

  /* Do limited Cranley-Patterson rotation when using scrambling distance. */
  if (kernel_data.integrator.scrambling_distance < 1.0f) {
    const float jitter_x = hash_wang_seeded_float(dimension, rng_hash) *
                           kernel_data.integrator.scrambling_distance;
    x += jitter_x;
    x -= floorf(x);
  }

  return x;
}

ccl_device float2 pmj_sample_2D(KernelGlobals kg,
                                uint sample,
                                const uint rng_hash,
                                const uint dimension)
{
  uint seed = rng_hash;

  /* Use the same sample sequence seed for all pixels when using
   * scrambling distance. */
  if (kernel_data.integrator.scrambling_distance < 1.0f) {
    seed = kernel_data.integrator.seed;
  }

  /* Shuffle the pattern order and sample index to better decorrelate
   * dimensions and make the most of the finite patterns we have.
   * The funky sample mask stuff is to ensure that we only shuffle
   * *within* the current sample pattern, which is necessary to avoid
   * early repeat pattern use. */
  const uint pattern_i = hash_shuffle_uint(dimension, NUM_PMJ_PATTERNS, seed);
  /* NUM_PMJ_SAMPLES should be a power of two, so this results in a mask. */
  const uint sample_mask = NUM_PMJ_SAMPLES - 1;
  const uint sample_shuffled = nested_uniform_scramble(sample,
                                                       hash_wang_seeded_uint(dimension, seed));
  sample = (sample & ~sample_mask) | (sample_shuffled & sample_mask);

  /* Fetch the sample. */
  const uint index = ((pattern_i * NUM_PMJ_SAMPLES) + sample) %
                     (NUM_PMJ_SAMPLES * NUM_PMJ_PATTERNS);
  float x = kernel_data_fetch(sample_pattern_lut, index * 2);
  float y = kernel_data_fetch(sample_pattern_lut, index * 2 + 1);

  /* Do limited Cranley-Patterson rotation when using scrambling distance. */
  if (kernel_data.integrator.scrambling_distance < 1.0f) {
    const float jitter_x = hash_wang_seeded_float(dimension, rng_hash) *
                           kernel_data.integrator.scrambling_distance;
    const float jitter_y = hash_wang_seeded_float(dimension, rng_hash ^ 0xca0e1151) *
                           kernel_data.integrator.scrambling_distance;
    x += jitter_x;
    y += jitter_y;
    x -= floorf(x);
    y -= floorf(y);
  }

  return make_float2(x, y);
}

CCL_NAMESPACE_END
