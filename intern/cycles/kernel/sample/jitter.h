/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "kernel/sample/util.h"
#include "util/hash.h"

#pragma once
CCL_NAMESPACE_BEGIN

ccl_device_inline uint32_t nested_uniform_scramble(uint32_t x, uint32_t seed)
{
  x = reverse_integer_bits(x);
  x = laine_karras_permutation(x, seed);
  x = reverse_integer_bits(x);

  return x;
}

ccl_device float pmj_sample_1D(KernelGlobals kg, uint sample, uint rng_hash, uint dimension)
{
  uint hash = rng_hash;
  float jitter_x = 0.0f;
  if (kernel_data.integrator.scrambling_distance < 1.0f) {
    hash = kernel_data.integrator.seed;

    jitter_x = hash_wang_seeded_float(dimension, rng_hash) *
               kernel_data.integrator.scrambling_distance;
  }

  /* Perform Owen shuffle of the sample number to reorder the samples. */
  const uint rv = hash_cmj_seeded_uint(dimension, hash);
#ifdef _XOR_SHUFFLE_
#  warning "Using XOR shuffle."
  const uint s = sample ^ rv;
#else /* Use _OWEN_SHUFFLE_ for reordering. */
  const uint s = nested_uniform_scramble(sample, rv);
#endif

  /* Based on the sample number a sample pattern is selected and offset by the dimension. */
  const uint sample_set = s / NUM_PMJ_SAMPLES;
  const uint d = (dimension + sample_set);
  const uint dim = d % NUM_PMJ_PATTERNS;

  /* The PMJ sample sets contain a sample with (x,y) with NUM_PMJ_SAMPLES so for 1D
   *  the x part is used for even dims and the y for odd. */
  int index = 2 * ((dim >> 1) * NUM_PMJ_SAMPLES + (s % NUM_PMJ_SAMPLES)) + (dim & 1);

  float fx = kernel_data_fetch(sample_pattern_lut, index);

#ifndef _NO_CRANLEY_PATTERSON_ROTATION_
  /* Use Cranley-Patterson rotation to displace the sample pattern. */
  float dx = hash_cmj_seeded_float(d, hash);
  /* Jitter sample locations and map back into [0 1]. */
  fx = fx + dx + jitter_x;
  fx = fx - floorf(fx);
#else
#  warning "Not using Cranley-Patterson Rotation."
#endif

  return fx;
}

ccl_device void pmj_sample_2D(KernelGlobals kg,
                              uint sample,
                              uint rng_hash,
                              uint dimension,
                              ccl_private float *x,
                              ccl_private float *y)
{
  uint hash = rng_hash;
  float jitter_x = 0.0f;
  float jitter_y = 0.0f;
  if (kernel_data.integrator.scrambling_distance < 1.0f) {
    hash = kernel_data.integrator.seed;

    jitter_x = hash_wang_seeded_float(dimension, rng_hash) *
               kernel_data.integrator.scrambling_distance;
    jitter_y = hash_wang_seeded_float(dimension + 1, rng_hash) *
               kernel_data.integrator.scrambling_distance;
  }

  /* Perform a shuffle on the sample number to reorder the samples. */
  const uint rv = hash_cmj_seeded_uint(dimension, hash);
#ifdef _XOR_SHUFFLE_
#  warning "Using XOR shuffle."
  const uint s = sample ^ rv;
#else /* Use _OWEN_SHUFFLE_ for reordering. */
  const uint s = nested_uniform_scramble(sample, rv);
#endif

  /* Based on the sample number a sample pattern is selected and offset by the dimension. */
  const uint sample_set = s / NUM_PMJ_SAMPLES;
  const uint d = dimension + sample_set;
  uint dim = d % NUM_PMJ_PATTERNS;
  int index = 2 * (dim * NUM_PMJ_SAMPLES + (s % NUM_PMJ_SAMPLES));

  float fx = kernel_data_fetch(sample_pattern_lut, index);
  float fy = kernel_data_fetch(sample_pattern_lut, index + 1);

#ifndef _NO_CRANLEY_PATTERSON_ROTATION_
  /* Use Cranley-Patterson rotation to displace the sample pattern. */
  float dx = hash_cmj_seeded_float(d, hash);
  float dy = hash_cmj_seeded_float(d + 1, hash);
  /* Jitter sample locations and map back to the unit square [0 1]x[0 1]. */
  float sx = fx + dx + jitter_x;
  float sy = fy + dy + jitter_y;
  sx = sx - floorf(sx);
  sy = sy - floorf(sy);
#else
#  warning "Not using Cranley Patterson Rotation."
#endif

  (*x) = sx;
  (*y) = sy;
}

CCL_NAMESPACE_END
