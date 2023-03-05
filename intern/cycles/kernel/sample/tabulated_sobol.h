/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "kernel/sample/util.h"
#include "util/hash.h"

#pragma once
CCL_NAMESPACE_BEGIN

ccl_device uint tabulated_sobol_shuffled_sample_index(KernelGlobals kg,
                                                      uint sample,
                                                      uint dimension,
                                                      uint seed)
{
  const uint sample_count = kernel_data.integrator.tabulated_sobol_sequence_size;

  /* Shuffle the pattern order and sample index to decorrelate
   * dimensions and make the most of the finite patterns we have.
   * The funky sample mask stuff is to ensure that we only shuffle
   * *within* the current sample pattern, which is necessary to avoid
   * early repeat pattern use. */
  const uint pattern_i = hash_shuffle_uint(dimension, NUM_TAB_SOBOL_PATTERNS, seed);
  /* sample_count should always be a power of two, so this results in a mask. */
  const uint sample_mask = sample_count - 1;
  const uint sample_shuffled = nested_uniform_scramble(sample,
                                                       hash_wang_seeded_uint(dimension, seed));
  sample = (sample & ~sample_mask) | (sample_shuffled & sample_mask);

  return ((pattern_i * sample_count) + sample) % (sample_count * NUM_TAB_SOBOL_PATTERNS);
}

ccl_device float tabulated_sobol_sample_1D(KernelGlobals kg,
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

  /* Fetch the sample. */
  const uint index = tabulated_sobol_shuffled_sample_index(kg, sample, dimension, seed);
  float x = kernel_data_fetch(sample_pattern_lut, index * NUM_TAB_SOBOL_DIMENSIONS);

  /* Do limited Cranley-Patterson rotation when using scrambling distance. */
  if (kernel_data.integrator.scrambling_distance < 1.0f) {
    const float jitter_x = hash_wang_seeded_float(dimension, rng_hash) *
                           kernel_data.integrator.scrambling_distance;
    x += jitter_x;
    x -= floorf(x);
  }

  return x;
}

ccl_device float2 tabulated_sobol_sample_2D(KernelGlobals kg,
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

  /* Fetch the sample. */
  const uint index = tabulated_sobol_shuffled_sample_index(kg, sample, dimension, seed);
  float x = kernel_data_fetch(sample_pattern_lut, index * NUM_TAB_SOBOL_DIMENSIONS);
  float y = kernel_data_fetch(sample_pattern_lut, index * NUM_TAB_SOBOL_DIMENSIONS + 1);

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

ccl_device float3 tabulated_sobol_sample_3D(KernelGlobals kg,
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

  /* Fetch the sample. */
  const uint index = tabulated_sobol_shuffled_sample_index(kg, sample, dimension, seed);
  float x = kernel_data_fetch(sample_pattern_lut, index * NUM_TAB_SOBOL_DIMENSIONS);
  float y = kernel_data_fetch(sample_pattern_lut, index * NUM_TAB_SOBOL_DIMENSIONS + 1);
  float z = kernel_data_fetch(sample_pattern_lut, index * NUM_TAB_SOBOL_DIMENSIONS + 2);

  /* Do limited Cranley-Patterson rotation when using scrambling distance. */
  if (kernel_data.integrator.scrambling_distance < 1.0f) {
    const float jitter_x = hash_wang_seeded_float(dimension, rng_hash) *
                           kernel_data.integrator.scrambling_distance;
    const float jitter_y = hash_wang_seeded_float(dimension, rng_hash ^ 0xca0e1151) *
                           kernel_data.integrator.scrambling_distance;
    const float jitter_z = hash_wang_seeded_float(dimension, rng_hash ^ 0xbf604c5a) *
                           kernel_data.integrator.scrambling_distance;
    x += jitter_x;
    y += jitter_y;
    z += jitter_z;
    x -= floorf(x);
    y -= floorf(y);
    z -= floorf(z);
  }

  return make_float3(x, y, z);
}

ccl_device float4 tabulated_sobol_sample_4D(KernelGlobals kg,
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

  /* Fetch the sample. */
  const uint index = tabulated_sobol_shuffled_sample_index(kg, sample, dimension, seed);
  float x = kernel_data_fetch(sample_pattern_lut, index * NUM_TAB_SOBOL_DIMENSIONS);
  float y = kernel_data_fetch(sample_pattern_lut, index * NUM_TAB_SOBOL_DIMENSIONS + 1);
  float z = kernel_data_fetch(sample_pattern_lut, index * NUM_TAB_SOBOL_DIMENSIONS + 2);
  float w = kernel_data_fetch(sample_pattern_lut, index * NUM_TAB_SOBOL_DIMENSIONS + 3);

  /* Do limited Cranley-Patterson rotation when using scrambling distance. */
  if (kernel_data.integrator.scrambling_distance < 1.0f) {
    const float jitter_x = hash_wang_seeded_float(dimension, rng_hash) *
                           kernel_data.integrator.scrambling_distance;
    const float jitter_y = hash_wang_seeded_float(dimension, rng_hash ^ 0xca0e1151) *
                           kernel_data.integrator.scrambling_distance;
    const float jitter_z = hash_wang_seeded_float(dimension, rng_hash ^ 0xbf604c5a) *
                           kernel_data.integrator.scrambling_distance;
    const float jitter_w = hash_wang_seeded_float(dimension, rng_hash ^ 0x99634d1d) *
                           kernel_data.integrator.scrambling_distance;
    x += jitter_x;
    y += jitter_y;
    z += jitter_z;
    w += jitter_w;
    x -= floorf(x);
    y -= floorf(y);
    z -= floorf(z);
    w -= floorf(w);
  }

  return make_float4(x, y, z, w);
}

CCL_NAMESPACE_END
