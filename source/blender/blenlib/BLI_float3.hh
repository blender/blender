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

#ifndef __BLI_FLOAT3_HH__
#define __BLI_FLOAT3_HH__

#include <iostream>

#include "BLI_math_vector.h"

namespace blender {

struct float3 {
  float x, y, z;

  float3() = default;

  float3(const float *ptr) : x{ptr[0]}, y{ptr[1]}, z{ptr[2]}
  {
  }

  float3(const float (*ptr)[3]) : float3((const float *)ptr)
  {
  }

  explicit float3(float value) : x(value), y(value), z(value)
  {
  }

  explicit float3(int value) : x(value), y(value), z(value)
  {
  }

  float3(float x, float y, float z) : x{x}, y{y}, z{z}
  {
  }

  operator const float *() const
  {
    return &x;
  }

  operator float *()
  {
    return &x;
  }

  float normalize_and_get_length()
  {
    return normalize_v3(*this);
  }

  float3 normalized() const
  {
    float3 result;
    normalize_v3_v3(result, *this);
    return result;
  }

  float length() const
  {
    return len_v3(*this);
  }

  float length_squared() const
  {
    return len_squared_v3(*this);
  }

  void reflect(const float3 &normal)
  {
    *this = this->reflected(normal);
  }

  float3 reflected(const float3 &normal) const
  {
    float3 result;
    reflect_v3_v3v3(result, *this, normal);
    return result;
  }

  static float3 safe_divide(const float3 &a, const float3 &b)
  {
    float3 result;
    result.x = (b.x == 0.0f) ? 0.0f : a.x / b.x;
    result.y = (b.y == 0.0f) ? 0.0f : a.y / b.y;
    result.z = (b.z == 0.0f) ? 0.0f : a.z / b.z;
    return result;
  }

  void invert()
  {
    x = -x;
    y = -y;
    z = -z;
  }

  friend float3 operator+(const float3 &a, const float3 &b)
  {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
  }

  void operator+=(const float3 &b)
  {
    this->x += b.x;
    this->y += b.y;
    this->z += b.z;
  }

  friend float3 operator-(const float3 &a, const float3 &b)
  {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
  }

  friend float3 operator-(const float3 &a)
  {
    return {-a.x, -a.y, -a.z};
  }

  void operator-=(const float3 &b)
  {
    this->x -= b.x;
    this->y -= b.y;
    this->z -= b.z;
  }

  void operator*=(float scalar)
  {
    this->x *= scalar;
    this->y *= scalar;
    this->z *= scalar;
  }

  void operator*=(const float3 &other)
  {
    this->x *= other.x;
    this->y *= other.y;
    this->z *= other.z;
  }

  friend float3 operator*(const float3 &a, const float3 &b)
  {
    return {a.x * b.x, a.y * b.y, a.z * b.z};
  }

  friend float3 operator*(const float3 &a, float b)
  {
    return {a.x * b, a.y * b, a.z * b};
  }

  friend float3 operator*(float a, const float3 &b)
  {
    return b * a;
  }

  friend float3 operator/(const float3 &a, float b)
  {
    BLI_assert(b != 0.0f);
    return {a.x / b, a.y / b, a.z / b};
  }

  friend std::ostream &operator<<(std::ostream &stream, const float3 &v)
  {
    stream << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    return stream;
  }

  static float dot(const float3 &a, const float3 &b)
  {
    return a.x * b.x + a.y * b.y + a.z * b.z;
  }

  static float3 cross_high_precision(const float3 &a, const float3 &b)
  {
    float3 result;
    cross_v3_v3v3_hi_prec(result, a, b);
    return result;
  }

  static float3 project(const float3 &a, const float3 &b)
  {
    float3 result;
    project_v3_v3v3(result, a, b);
    return result;
  }

  static float distance(const float3 &a, const float3 &b)
  {
    return (a - b).length();
  }

  static float distance_squared(const float3 &a, const float3 &b)
  {
    return float3::dot(a, b);
  }

  static float3 interpolate(const float3 &a, const float3 &b, float t)
  {
    return a * (1 - t) + b * t;
  }
};

}  // namespace blender

#endif /* __BLI_FLOAT3_HH__ */
