/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "util/types.h"

CCL_NAMESPACE_BEGIN

/*
 * Performs base-2 Owen scrambling on a reversed-bit integer.
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
 * Performs base-2 Owen scrambling on a reversed-bit integer.
 *
 * This is here for backwards-compatibility, and can be replaced
 * with reversed_bit_owen() above at some point.
 * See https://developer.blender.org/D15679#426304
 */
ccl_device_inline uint laine_karras_permutation(uint x, uint seed)
{
  x += seed;
  x ^= (x * 0x6c50b47cu);
  x ^= x * 0xb82f1e52u;
  x ^= x * 0xc7afe638u;
  x ^= x * 0x8d22f6e6u;

  return x;
}

CCL_NAMESPACE_END
