/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/math.h"
#include "util/types.h"

CCL_NAMESPACE_BEGIN

/*
 * Performs base-2 Owen scrambling on a reversed-bit unsigned integer.
 *
 * This is equivalent to the Laine-Karras permutation, but much higher
 * quality.  See https://psychopath.io/post/2021_01_30_building_a_better_lk_hash
 */
ccl_device_inline uint reversed_bit_owen(uint n, const uint seed)
{
  n ^= n * 0x3d20adea;
  n += seed;
  n *= (seed >> 16) | 1;
  n ^= n * 0x05526c56;
  n ^= n * 0x53a22864;

  return n;
}

/*
 * Performs base-4 Owen scrambling on a reversed-bit unsigned integer.
 *
 * See https://psychopath.io/post/2022_08_14_a_fast_hash_for_base_4_owen_scrambling
 */

ccl_device_inline uint reversed_bit_owen_base4(uint n, const uint seed)
{
  n ^= n * 0x3d20adea;
  n ^= (n >> 1) & (n << 1) & 0x55555555;
  n += seed;
  n *= (seed >> 16) | 1;
  n ^= (n >> 1) & (n << 1) & 0x55555555;
  n ^= n * 0x05526c56;
  n ^= n * 0x53a22864;

  return n;
}

/*
 * Performs base-2 Owen scrambling on an unsigned integer.
 */
ccl_device_inline uint nested_uniform_scramble(const uint i, const uint seed)
{
  return reverse_integer_bits(reversed_bit_owen(reverse_integer_bits(i), seed));
}

/*
 * Performs base-4 Owen scrambling on an unsigned integer.
 */
ccl_device_inline uint nested_uniform_scramble_base4(const uint i, const uint seed)
{
  return reverse_integer_bits(reversed_bit_owen_base4(reverse_integer_bits(i), seed));
}

ccl_device_inline uint expand_bits(uint x)
{
  x &= 0x0000ffff;
  x = (x ^ (x << 8)) & 0x00ff00ff;
  x = (x ^ (x << 4)) & 0x0f0f0f0f;
  x = (x ^ (x << 2)) & 0x33333333;
  x = (x ^ (x << 1)) & 0x55555555;
  return x;
}

ccl_device_inline uint morton2d(const uint x, const uint y)
{
  return (expand_bits(x) << 1) | expand_bits(y);
}

CCL_NAMESPACE_END
