/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

/** \file
 * \ingroup mikktspace
 */

#pragma once

#include <cmath>

namespace mikk {

struct float3 {
  float x, y, z;

  float3() = default;

  float3(const float *ptr) : x{ptr[0]}, y{ptr[1]}, z{ptr[2]} {}

  float3(const float (*ptr)[3]) : float3((const float *)ptr) {}

  explicit float3(float value) : x(value), y(value), z(value) {}

  explicit float3(int value) : x((float)value), y((float)value), z((float)value) {}

  float3(float x_, float y_, float z_) : x{x_}, y{y_}, z{z_} {}

  static float3 zero()
  {
    return {0.0f, 0.0f, 0.0f};
  }

  friend float3 operator*(const float3 &a, float b)
  {
    return {a.x * b, a.y * b, a.z * b};
  }

  friend float3 operator*(float b, const float3 &a)
  {
    return {a.x * b, a.y * b, a.z * b};
  }

  friend float3 operator-(const float3 &a, const float3 &b)
  {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
  }

  friend float3 operator-(const float3 &a)
  {
    return {-a.x, -a.y, -a.z};
  }

  friend bool operator==(const float3 &a, const float3 &b)
  {
    return a.x == b.x && a.y == b.y && a.z == b.z;
  }

  float length_squared() const
  {
    return x * x + y * y + z * z;
  }

  float length() const
  {
    return sqrtf(length_squared());
  }

  static float distance(const float3 &a, const float3 &b)
  {
    return (a - b).length();
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

  friend float3 operator*(const float3 &a, const float3 &b)
  {
    return {a.x * b.x, a.y * b.y, a.z * b.z};
  }

  float3 normalize() const
  {
    const float len = length();
    return (len != 0.0f) ? *this * (1.0f / len) : *this;
  }

  float reduce_add() const
  {
    return x + y + z;
  }
};

inline float dot(const float3 &a, const float3 &b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float distance(const float3 &a, const float3 &b)
{
  return float3::distance(a, b);
}

/* Projects v onto the surface with normal n. */
inline float3 project(const float3 &n, const float3 &v)
{
  return (v - n * dot(n, v)).normalize();
}

}  // namespace mikk
