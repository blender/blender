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

#ifndef __BLI_FLOAT2_HH__
#define __BLI_FLOAT2_HH__

#include "BLI_float3.hh"

namespace blender {

struct float2 {
  float x, y;

  float2() = default;

  float2(const float *ptr) : x{ptr[0]}, y{ptr[1]}
  {
  }

  float2(float x, float y) : x(x), y(y)
  {
  }

  float2(const float3 &other) : x(other.x), y(other.y)
  {
  }

  operator float *()
  {
    return &x;
  }

  operator const float *() const
  {
    return &x;
  }

  friend float2 operator+(const float2 &a, const float2 &b)
  {
    return {a.x + b.x, a.y + b.y};
  }

  friend float2 operator-(const float2 &a, const float2 &b)
  {
    return {a.x - b.x, a.y - b.y};
  }

  friend float2 operator*(const float2 &a, float b)
  {
    return {a.x * b, a.y * b};
  }

  friend float2 operator/(const float2 &a, float b)
  {
    BLI_assert(b != 0.0f);
    return {a.x / b, a.y / b};
  }

  friend float2 operator*(float a, const float2 &b)
  {
    return b * a;
  }

  friend std::ostream &operator<<(std::ostream &stream, const float2 &v)
  {
    stream << "(" << v.x << ", " << v.y << ")";
    return stream;
  }
};

}  // namespace blender

#endif /* __BLI_FLOAT2_HH__ */
