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

#pragma once

namespace blender {

struct float4 {
  float x, y, z, w;

  float4() = default;

  float4(const float *ptr) : x{ptr[0]}, y{ptr[1]}, z{ptr[2]}, w{ptr[3]}
  {
  }

  explicit float4(float value) : x(value), y(value), z(value), w(value)
  {
  }

  explicit float4(int value) : x(value), y(value), z(value), w(value)
  {
  }

  float4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w)
  {
  }

  operator float *()
  {
    return &x;
  }

  friend float4 operator+(const float4 &a, const float &b)
  {
    return {a.x + b, a.y + b, a.z + b, a.w + b};
  }

  operator const float *() const
  {
    return &x;
  }

  float4 &operator+=(const float4 &other)
  {
    x += other.x;
    y += other.y;
    z += other.z;
    w += other.w;
    return *this;
  }

  friend float4 operator-(const float4 &a, const float4 &b)
  {
    return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
  }

  friend float4 operator-(const float4 &a, const float &b)
  {
    return {a.x - b, a.y - b, a.z - b, a.w - b};
  }

  friend float4 operator+(const float4 &a, const float4 &b)
  {
    return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
  }

  friend float4 operator/(const float4 &a, float f)
  {
    BLI_assert(f != 0.0f);
    return a * (1.0f / f);
  }

  float4 &operator*=(float factor)
  {
    x *= factor;
    y *= factor;
    z *= factor;
    w *= factor;
    return *this;
  }

  friend float4 operator*(const float4 &a, float b)
  {
    return {a.x * b, a.y * b, a.z * b, a.w * b};
  }

  friend float4 operator*(float a, const float4 &b)
  {
    return b * a;
  }

  float length() const
  {
    return len_v4(*this);
  }

  static float distance(const float4 &a, const float4 &b)
  {
    return (a - b).length();
  }

  static float4 safe_divide(const float4 &a, const float b)
  {
    return (b != 0.0f) ? a / b : float4(0.0f);
  }

  static float4 interpolate(const float4 &a, const float4 &b, float t)
  {
    return a * (1 - t) + b * t;
  }

  static float4 floor(const float4 &a)
  {
    return float4(floorf(a.x), floorf(a.y), floorf(a.z), floorf(a.w));
  }

  static float4 normalize(const float4 &a)
  {
    const float t = len_v4(a);
    return (t != 0.0f) ? a / t : float4(0.0f);
  }
};

}  // namespace blender
