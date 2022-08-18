/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

/*
 * A shuffled, Owen-scrambled Sobol sampler, implemented with the
 * techniques from the paper "Practical Hash-based Owen Scrambling"
 * by Brent Burley, 2020, Journal of Computer Graphics Techniques.
 *
 * Note that unlike a standard high-dimensional Sobol sequence, this
 * Sobol sampler uses padding to achieve higher dimensions, as described
 * in Burley's paper.
 */

#pragma once

#include "kernel/sample/util.h"
#include "util/hash.h"
#include "util/math.h"
#include "util/types.h"

CCL_NAMESPACE_BEGIN

/*
 * Computes a single dimension of a sample from an Owen-scrambled
 * Sobol sequence.  This is used in the main sampling functions,
 * sobol_burley_sample_#D(), below.
 *
 * - rev_bit_index: the sample index, with reversed order bits.
 * - dimension:     the sample dimension.
 * - scramble_seed: the Owen scrambling seed.
 *
 * Note that the seed must be well randomized before being
 * passed to this function.
 */
ccl_device_forceinline float sobol_burley(uint rev_bit_index, uint dimension, uint scramble_seed)
{
  uint result = 0;

  if (dimension == 0) {
    // Fast-path for dimension 0, which is just Van der corput.
    // This makes a notable difference in performance since we reuse
    // dimensions for padding, and dimension 0 is reused the most.
    result = reverse_integer_bits(rev_bit_index);
  }
  else {
    uint i = 0;
    while (rev_bit_index != 0) {
      uint j = count_leading_zeros(rev_bit_index);
      result ^= sobol_burley_table[dimension][i + j];
      i += j + 1;

      // We can't do "<<= j + 1" because that can overflow the shift
      // operator, which doesn't do what we need on at least x86.
      rev_bit_index <<= j;
      rev_bit_index <<= 1;
    }
  }

  // Apply Owen scrambling.
  result = reverse_integer_bits(reversed_bit_owen(result, scramble_seed));

  return uint_to_float_excl(result);
}

/*
 * Computes a 1D Owen-scrambled and shuffled Sobol sample.
 */
ccl_device float sobol_burley_sample_1D(uint index, uint dimension, uint seed)
{
  // Include the dimension in the seed, so we get decorrelated
  // sequences for different dimensions via shuffling.
  seed ^= hash_hp_uint(dimension);

  // Shuffle.
  index = reversed_bit_owen(reverse_integer_bits(index), seed ^ 0xbff95bfe);

  return sobol_burley(index, 0, seed ^ 0x635c77bd);
}

/*
 * Computes a 2D Owen-scrambled and shuffled Sobol sample.
 */
ccl_device void sobol_burley_sample_2D(
    uint index, uint dimension_set, uint seed, ccl_private float *x, ccl_private float *y)
{
  // Include the dimension set in the seed, so we get decorrelated
  // sequences for different dimension sets via shuffling.
  seed ^= hash_hp_uint(dimension_set);

  // Shuffle.
  index = reversed_bit_owen(reverse_integer_bits(index), seed ^ 0xf8ade99a);

  *x = sobol_burley(index, 0, seed ^ 0xe0aaaf76);
  *y = sobol_burley(index, 1, seed ^ 0x94964d4e);
}

/*
 * Computes a 3D Owen-scrambled and shuffled Sobol sample.
 */
ccl_device void sobol_burley_sample_3D(uint index,
                                       uint dimension_set,
                                       uint seed,
                                       ccl_private float *x,
                                       ccl_private float *y,
                                       ccl_private float *z)
{
  // Include the dimension set in the seed, so we get decorrelated
  // sequences for different dimension sets via shuffling.
  seed ^= hash_hp_uint(dimension_set);

  // Shuffle.
  index = reversed_bit_owen(reverse_integer_bits(index), seed ^ 0xcaa726ac);

  *x = sobol_burley(index, 0, seed ^ 0x9e78e391);
  *y = sobol_burley(index, 1, seed ^ 0x67c33241);
  *z = sobol_burley(index, 2, seed ^ 0x78c395c5);
}

/*
 * Computes a 4D Owen-scrambled and shuffled Sobol sample.
 */
ccl_device void sobol_burley_sample_4D(uint index,
                                       uint dimension_set,
                                       uint seed,
                                       ccl_private float *x,
                                       ccl_private float *y,
                                       ccl_private float *z,
                                       ccl_private float *w)
{
  // Include the dimension set in the seed, so we get decorrelated
  // sequences for different dimension sets via shuffling.
  seed ^= hash_hp_uint(dimension_set);

  // Shuffle.
  index = reversed_bit_owen(reverse_integer_bits(index), seed ^ 0xc2c1a055);

  *x = sobol_burley(index, 0, seed ^ 0x39468210);
  *y = sobol_burley(index, 1, seed ^ 0xe9d8a845);
  *z = sobol_burley(index, 2, seed ^ 0x5f32b482);
  *w = sobol_burley(index, 3, seed ^ 0x1524cc56);
}

CCL_NAMESPACE_END
