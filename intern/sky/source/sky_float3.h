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

#ifndef __SKY_FLOAT3_H__
#define __SKY_FLOAT3_H__

// minimal float3 + util_math.h implementation for nishita sky model

#include <math.h>

#ifndef M_PI_F
#  define M_PI_F (3.1415926535897932f) /* pi */
#endif
#ifndef M_PI_2_F
#  define M_PI_2_F (1.5707963267948966f) /* pi/2 */
#endif
#ifndef M_2PI_F
#  define M_2PI_F (6.2831853071795864f) /* 2*pi */
#endif

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

  float length_squared() const
  {
    return x * x + y * y + z * z;
  }

  float length() const
  {
    return sqrt(length_squared());
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
};

inline float sqr(float a)
{
  return a * a;
}

inline float3 make_float3(float x, float y, float z)
{
  return float3(x, y, z);
}

inline float dot(const float3 &a, const float3 &b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float distance(const float3 &a, const float3 &b)
{
  return float3::distance(a, b);
}

inline float len_squared(float3 f)
{
  return f.length_squared();
}

inline float len(float3 f)
{
  return f.length();
}

inline float reduce_add(float3 f)
{
  return f.x + f.y + f.z;
}

#endif /* __SKY_FLOAT3_H__ */
