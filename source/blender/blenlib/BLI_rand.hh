/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup bli
 */

#ifndef __BLI_RAND_HH__
#define __BLI_RAND_HH__

#include "BLI_float2.hh"
#include "BLI_float3.hh"
#include "BLI_math.h"
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
    x_ = (((uint64_t)seed) << 16) | lowseed;
  }

  void seed_random(uint32_t seed);

  uint32_t get_uint32()
  {
    this->step();
    return (uint32_t)(x_ >> 17);
  }

  int32_t get_int32()
  {
    this->step();
    return (int32_t)(x_ >> 17);
  }

  /**
   * \return Random value (0..1), but never 1.0.
   */
  double get_double()
  {
    return (double)this->get_int32() / 0x80000000;
  }

  /**
   * \return Random value (0..1), but never 1.0.
   */
  float get_float()
  {
    return (float)this->get_int32() / 0x80000000;
  }

  float2 get_unit_float2();
  float3 get_unit_float3();
  float2 get_triangle_sample(float2 v1, float2 v2, float2 v3);
  void get_bytes(MutableSpan<char> r_bytes);

  /**
   * Simulate getting \a n random values.
   */
  void skip(uint n)
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

#endif /* __BLI_RAND_HH__ */
