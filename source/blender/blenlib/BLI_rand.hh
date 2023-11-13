/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#pragma once

#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_utildefines.h"

namespace blender {

class RandomNumberGenerator {
 private:
  uint64_t x_;

 public:
  RandomNumberGenerator(uint32_t seed = 0)
  {
    this->seed(seed);
  }

  /**
   * Set the seed for future random numbers.
   */
  void seed(uint32_t seed)
  {
    constexpr uint64_t lowseed = 0x330E;
    x_ = (uint64_t(seed) << 16) | lowseed;
  }

  /**
   * Set a randomized hash of the value as seed.
   */
  void seed_random(uint32_t seed);

  uint32_t get_uint32()
  {
    this->step();
    return uint32_t(x_ >> 17);
  }

  int32_t get_int32()
  {
    this->step();
    return int32_t(x_ >> 17);
  }

  /**
   * \return Random value (0..N), but never N.
   */
  int32_t get_int32(int32_t max_exclusive)
  {
    BLI_assert(max_exclusive > 0);
    return this->get_int32() % max_exclusive;
  }

  /**
   * \return Random value (0..1), but never 1.0.
   */
  double get_double()
  {
    return double(this->get_int32()) / 0x80000000;
  }

  /**
   * \return Random value (0..1), but never 1.0.
   */
  float get_float()
  {
    return (float)this->get_int32() / 0x80000000;
  }

  template<typename T> void shuffle(MutableSpan<T> values)
  {
    /* Cannot shuffle arrays of this size yet. */
    BLI_assert(values.size() <= INT32_MAX);

    for (int i = values.size() - 1; i >= 2; i--) {
      int j = this->get_int32(i);
      if (i != j) {
        std::swap(values[i], values[j]);
      }
    }
  }

  /**
   * Compute uniformly distributed barycentric coordinates.
   */
  float3 get_barycentric_coordinates()
  {
    float rand1 = this->get_float();
    float rand2 = this->get_float();

    if (rand1 + rand2 > 1.0f) {
      rand1 = 1.0f - rand1;
      rand2 = 1.0f - rand2;
    }

    return float3(rand1, rand2, 1.0f - rand1 - rand2);
  }

  /**
   * Round value to the next integer randomly.
   * 4.9f is more likely to round to 5 than 4.6f.
   */
  int round_probabilistic(float x);

  float2 get_unit_float2();
  float3 get_unit_float3();
  /**
   * Generate a random point inside the given triangle.
   */
  float2 get_triangle_sample(float2 v1, float2 v2, float2 v3);
  float3 get_triangle_sample_3d(float3 v1, float3 v2, float3 v3);
  void get_bytes(MutableSpan<char> r_bytes);

  /**
   * Simulate getting \a n random values.
   */
  void skip(int64_t n)
  {
    while (n--) {
      this->step();
    }
  }

 private:
  void step()
  {
    constexpr uint64_t multiplier = 0x5DEECE66Dll;
    constexpr uint64_t addend = 0xB;
    constexpr uint64_t mask = 0x0000FFFFFFFFFFFFll;

    x_ = (multiplier * x_ + addend) & mask;
  }
};

}  // namespace blender
