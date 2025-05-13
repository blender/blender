/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_utildefines_lib.glsl"

namespace occupancy {

struct Bits {
  uint bits[8];
};

int bit_index_from_depth(float depth, int bit_count)
{
  /* We want the occupancy at the center of each range a bit covers.
   * So we round the depth to the nearest bit. */
  return int(depth * float(bit_count) + 0.5f);
}

/**
 * Example with for 16bits per layer and 2 layers.
 * 0    Layer0   15  16   Layer1   31  < Bits index from LSB to MSB (left to right)
 * |--------------|  |--------------|
 * 0000000001111111  1111111111111111  < Surf0
 * 0000000000000000  0000001111111111  < Surf1
 * 0000000001111111  1111110000000000  < Result occupancy at each froxel depths
 * \a depth in [0..1] range.
 * \a bit_count in [1..256] range.
 */
Bits bits_from_depth(float depth, int bit_count)
{
  /* We want the occupancy at the center of each range a bit covers.
   * So we round the depth to the nearest bit. */
  int depth_bit_index = bit_index_from_depth(depth, bit_count);

  Bits occupancy;
  for (int i = 0; i < 8; i++) {
    int shift = clamp(depth_bit_index - i * 32, 0, 32);
    /* Cannot bit shift more than 31 positions. */
    occupancy.bits[i] = (shift == 32) ? 0x0u : (~0x0u << uint(shift));
  }
  return occupancy;
}

/**
 * Returns an empty structure, cleared to 0.
 */
Bits occupancy_new()
{
  Bits occupancy;
  for (int i = 0; i < 8; i++) {
    occupancy.bits[i] = 0x0u;
  }
  return occupancy;
}

/**
 * Example with for 16bits per layer and 2 layers.
 * 0    Layer0   15  16   Layer1   31  < Bits index from LSB to MSB (left to right)
 * |--------------|  |--------------|
 * 0000000001000010  0001000000000000  < Surf entry points
 * 0000000000000000  0100001000000000  < Surf exit points
 * 0000000001111111  1001110000000000  < Result occupancy at each froxel depths after resolve
 * \a depth in [0..1] range.
 * \a bit_count in [1..256] range.
 */
Bits bit_from_depth(float depth, int bit_count)
{
  /* We want the occupancy at the center of each range a bit covers.
   * So we round the depth to the nearest bit. */
  int depth_bit_index = bit_index_from_depth(depth, bit_count);

  Bits occupancy;
  for (int i = 0; i < 8; i++) {
    int shift = depth_bit_index - i * 32;
    /* Cannot bit shift more than 31 positions. */
    occupancy.bits[i] = (shift >= 0 && shift < 32) ? (0x1u << uint(shift)) : 0x0u;
  }
  return occupancy;
}

/**
 * Same as binary OR but for the whole Bits structure.
 */
Bits bitwise_or(Bits a, Bits b)
{
  Bits occupancy;
  for (int i = 0; i < 8; i++) {
    occupancy.bits[i] = a.bits[i] | b.bits[i];
  }
  return occupancy;
}

/**
 * Set a series of bits high inside the given Bits.
 */
Bits set_bits_high(Bits occupancy, int bit_start, int bit_count)
{
  for (int i = 0; i < bit_count; i++) {
    int bit = bit_start + i;
    occupancy.bits[bit / 32] |= 1u << uint(bit % 32);
  }
  return occupancy;
}

/**
 * Same as findLSB but for the whole Bits structure.
 */
int find_lsb(Bits occupancy)
{
  for (int i = 0; i < 8; i++) {
    if (occupancy.bits[i] != 0) {
      return findLSB(occupancy.bits[i]) + i * 32;
    }
  }
  return -1;
}

/**
 * Converts the first four occupancy words to a uint4.
 */
uint4 to_uint4(Bits occupancy)
{
  return uint4(occupancy.bits[0], occupancy.bits[1], occupancy.bits[2], occupancy.bits[3]);
}

/**
 * From a entry and exit occupancy tuple, returns if a specific bit is inside the volume.
 */
bool bit_resolve(Bits entry, Bits exit, int bit_n, int bit_count)
{
  int first_exit = find_lsb(exit);
  int first_entry = find_lsb(entry);
  first_exit = (first_exit == -1) ? 99999 : first_exit;
  /* Check if the first surface is an exit. If it is, initialize as inside the volume. */
  bool inside_volume = first_exit < first_entry;
  for (int j = 0; j <= bit_n / 32; j++) {
    uint entry_word = entry.bits[j];
    uint exit_word = exit.bits[j];
    /* TODO(fclem): Could use fewer iteration using findLSB and/or other intrinsics. */
    for (uint i = 0; i < 32; i++) {
      if (flag_test(exit_word, 1u << i) && flag_test(entry_word, 1u << i)) {
        /* Do nothing. */
      }
      else {
        if (flag_test(exit_word, 1u << i)) {
          inside_volume = false;
        }
        if (flag_test(entry_word, 1u << i)) {
          inside_volume = true;
        }
      }
      if (i + j * 32 == uint(bit_n)) {
        return inside_volume;
      }
    }
  }
  return inside_volume;
}

/**
 * From a entry and exit occupancy tuple, returns the full occupancy map.
 */
Bits resolve(Bits entry, Bits exit, int bit_count)
{
  Bits occupancy;
  for (int j = 0; j < 8; j++) {
    for (int i = 0; i < 32; i++) {
      bool test = false;
      if (i < bit_count - j * 32) {
        test = bit_resolve(entry, exit, i + j * 32, bit_count);
      }
      set_flag_from_test(occupancy.bits[j], test, 1u << uint(i));
    }
  }
  return occupancy;
}

}  // namespace occupancy
