/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_array.hh"
#include "BLI_compression.hh"

namespace blender {

TEST(compression, filter_transpose_delta)
{
  constexpr int num = 5;
  constexpr int size = 3;
  uint8_t input[num * size] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 5, 4, 3};
  uint8_t filtered_exp[num * size] = {0, 2, 6, 26, 227, 1, 2, 10, 42, 205, 1, 4, 16, 68, 170};
  uint8_t filtered[num * size] = {};
  uint8_t unfiltered[num * size] = {};
  filter_transpose_delta(input, filtered, num, size);
  EXPECT_EQ_ARRAY(filtered_exp, filtered, num * size);
  unfilter_transpose_delta(filtered, unfiltered, num, size);
  EXPECT_EQ_ARRAY(input, unfiltered, num * size);
}

static uint32_t pcg_rand(uint32_t &rng_state)
{
  uint32_t state = rng_state;
  rng_state = rng_state * 747796405u + 2891336453u;
  uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
}

TEST(compression, filter_transpose_delta_stress)
{
  uint32_t rng = 1;

  constexpr int size = 64 * 1024;
  Array<uint8_t> input(size);
  Array<uint8_t> filtered(size);
  Array<uint8_t> unfiltered(size);
  for (uint8_t &val : input) {
    val = pcg_rand(rng);
  }

  const int strides[] = {1, 2, 3, 4, 5, 8, 13, 16, 25, 48, 64, 65, 101, 300, 512, 513, size};
  for (int stride : strides) {
    const int num = size / stride;
    filter_transpose_delta(input.data(), filtered.data(), num, stride);
    unfilter_transpose_delta(filtered.data(), unfiltered.data(), num, stride);
    EXPECT_EQ_ARRAY(input.data(), unfiltered.data(), num * stride);
  }
}

}  // namespace blender
