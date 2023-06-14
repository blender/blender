/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types.h"

CCL_NAMESPACE_BEGIN

/*
 * Performs base-2 Owen scrambling on a reversed-bit unsigned integer.
 *
 * This is equivalent to the Laine-Karras permutation, but much higher
 * quality.  See https://psychopath.io/post/2021_01_30_building_a_better_lk_hash
 */
ccl_device_inline uint reversed_bit_owen(uint n, uint seed)
{
  n ^= n * 0x3d20adea;
  n += seed;
  n *= (seed >> 16) | 1;
  n ^= n * 0x05526c56;
  n ^= n * 0x53a22864;

  return n;
}

/*
 * Performs base-2 Owen scrambling on an unsigned integer.
 */
ccl_device_inline uint nested_uniform_scramble(uint i, uint seed)
{
  return reverse_integer_bits(reversed_bit_owen(reverse_integer_bits(i), seed));
}

CCL_NAMESPACE_END
