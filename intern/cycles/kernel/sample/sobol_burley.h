/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

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

#include "kernel/tables.h"

#include "kernel/sample/util.h"

#include "util/hash.h"

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
ccl_device_forceinline float sobol_burley(uint rev_bit_index,
                                          const uint dimension,
                                          const uint scramble_seed)
{
  uint result = 0;

  if (dimension == 0) {
    /* Fast-path for dimension 0, which is just Van der corput.
     * This makes a notable difference in performance since we reuse
     * dimensions for padding, and dimension 0 is reused the most. */
    result = reverse_integer_bits(rev_bit_index);
  }
  else {
    uint i = 0;
    while (rev_bit_index != 0) {
      const uint j = count_leading_zeros(rev_bit_index);
      result ^= sobol_burley_table[dimension][i + j];
      i += j + 1;

      /* We can't do "<<= j + 1" because that can overflow the shift
       * operator, which doesn't do what we need on at least x86. */
      rev_bit_index <<= j;
      rev_bit_index <<= 1;
    }
  }

  /* Apply Owen scrambling. */
  result = reverse_integer_bits(reversed_bit_owen(result, scramble_seed));

  return uint_to_float_excl(result);
}

/*
 * NOTE: the functions below intentionally produce samples that are
 * uncorrelated between functions.  For example, a 1D sample and 2D
 * sample produced with the same index, dimension, and seed are
 * uncorrelated with each other.  This allows more care-free usage
 * of the functions together, without having to worry about
 * e.g. 1D and 2D samples being accidentally correlated with each
 * other.
 */

/*
 * Computes a 1D Owen-scrambled and shuffled Sobol sample.
 *
 * `index` is the index of the sample in the sequence.
 *
 * `dimension` is which dimensions of the sample you want to fetch.  Note
 * that different 1D dimensions are uncorrelated.  For samples with > 1D
 * stratification, use the multi-dimensional sampling methods below.
 *
 * `seed`: different seeds produce statistically independent,
 * uncorrelated sequences.
 *
 * `shuffled_index_mask` limits the sample sequence length, improving
 * performance. It must be a string of binary 1 bits followed by a
 * string of binary 0 bits (e.g. 0xffff0000) for the sampler to operate
 * correctly. In general, `reverse_integer_bits(shuffled_index_mask)`
 * should be >= the maximum number of samples expected to be taken. A safe
 * default (but least performant) is 0xffffffff, for maximum sequence
 * length.
 */
ccl_device float sobol_burley_sample_1D(uint index,
                                        const uint dimension,
                                        uint seed,
                                        const uint shuffled_index_mask)
{
  /* Include the dimension in the seed, so we get decorrelated
   * sequences for different dimensions via shuffling. */
  seed ^= hash_hp_uint(dimension);

  /* Shuffle and mask.  The masking is just for better
   * performance at low sample counts. */
  index = reversed_bit_owen(reverse_integer_bits(index), seed ^ 0xbff95bfe);
  index &= shuffled_index_mask;

  return sobol_burley(index, 0, seed ^ 0x635c77bd);
}

/*
 * Computes a 2D Owen-scrambled and shuffled Sobol sample.
 *
 * `dimension_set` is which two dimensions of the sample you want to
 * fetch.  For example, 0 is the first two, 1 is the second two, etc.
 * The dimensions within a single set are stratified, but different sets
 * are uncorrelated.
 *
 * See sobol_burley_sample_1D for further usage details.
 */
ccl_device float2 sobol_burley_sample_2D(uint index,
                                         const uint dimension_set,
                                         uint seed,
                                         const uint shuffled_index_mask)
{
  /* Include the dimension set in the seed, so we get decorrelated
   * sequences for different dimension sets via shuffling. */
  seed ^= hash_hp_uint(dimension_set);

  /* Shuffle and mask.  The masking is just for better
   * performance at low sample counts. */
  index = reversed_bit_owen(reverse_integer_bits(index), seed ^ 0xf8ade99a);
  index &= shuffled_index_mask;

  return make_float2(sobol_burley(index, 0, seed ^ 0xe0aaaf76),
                     sobol_burley(index, 1, seed ^ 0x94964d4e));
}

/*
 * Computes a 3D Owen-scrambled and shuffled Sobol sample.
 *
 * `dimension_set` is which three dimensions of the sample you want to
 * fetch.  For example, 0 is the first three, 1 is the second three, etc.
 * The dimensions within a single set are stratified, but different sets
 * are uncorrelated.
 *
 * See sobol_burley_sample_1D for further usage details.
 */
ccl_device float3 sobol_burley_sample_3D(uint index,
                                         const uint dimension_set,
                                         uint seed,
                                         const uint shuffled_index_mask)
{
  /* Include the dimension set in the seed, so we get decorrelated
   * sequences for different dimension sets via shuffling. */
  seed ^= hash_hp_uint(dimension_set);

  /* Shuffle and mask.  The masking is just for better
   * performance at low sample counts. */
  index = reversed_bit_owen(reverse_integer_bits(index), seed ^ 0xcaa726ac);
  index &= shuffled_index_mask;

  return make_float3(sobol_burley(index, 0, seed ^ 0x9e78e391),
                     sobol_burley(index, 1, seed ^ 0x67c33241),
                     sobol_burley(index, 2, seed ^ 0x78c395c5));
}

/*
 * Computes a 4D Owen-scrambled and shuffled Sobol sample.
 *
 * `dimension_set` is which four dimensions of the sample you want to
 * fetch.  For example, 0 is the first four, 1 is the second four, etc.
 * The dimensions within a single set are stratified, but different sets
 * are uncorrelated.
 *
 * See sobol_burley_sample_1D for further usage details.
 */
ccl_device float4 sobol_burley_sample_4D(uint index,
                                         const uint dimension_set,
                                         uint seed,
                                         const uint shuffled_index_mask)
{
  /* Include the dimension set in the seed, so we get decorrelated
   * sequences for different dimension sets via shuffling. */
  seed ^= hash_hp_uint(dimension_set);

  /* Shuffle and mask.  The masking is just for better
   * performance at low sample counts. */
  index = reversed_bit_owen(reverse_integer_bits(index), seed ^ 0xc2c1a055);
  index &= shuffled_index_mask;

  return make_float4(sobol_burley(index, 0, seed ^ 0x39468210),
                     sobol_burley(index, 1, seed ^ 0xe9d8a845),
                     sobol_burley(index, 2, seed ^ 0x5f32b482),
                     sobol_burley(index, 3, seed ^ 0x1524cc56));
}

CCL_NAMESPACE_END
