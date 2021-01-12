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

/** \file
 * \ingroup bli
 */

#include <iostream>

#include "BLI_math_vector.h"
#include "BLI_span.hh"

namespace blender {

struct double3 {
  double x, y, z;

  double3() = default;

  double3(const double *ptr) : x{ptr[0]}, y{ptr[1]}, z{ptr[2]}
  {
  }

  double3(const double (*ptr)[3]) : double3((const double *)ptr)
  {
  }

  explicit double3(double value) : x(value), y(value), z(value)
  {
  }

  explicit double3(int value) : x(value), y(value), z(value)
  {
  }

  double3(double x, double y, double z) : x{x}, y{y}, z{z}
  {
  }

  operator const double *() const
  {
    return &x;
  }

  operator double *()
  {
    return &x;
  }

  double normalize_and_get_length()
  {
    return normalize_v3_db(*this);
  }

  double3 normalized() const
  {
    double3 result;
    normalize_v3_v3_db(result, *this);
    return result;
  }

  double length() const
  {
    return len_v3_db(*this);
  }

  double length_squared() const
  {
    return len_squared_v3_db(*this);
  }

  void reflect(const double3 &normal)
  {
    *this = this->reflected(normal);
  }

  double3 reflected(const double3 &normal) const
  {
    double3 result;
    reflect_v3_v3v3_db(result, *this, normal);
    return result;
  }

  static double3 safe_divide(const double3 &a, const double3 &b)
  {
    double3 result;
    result.x = (b.x == 0.0) ? 0.0 : a.x / b.x;
    result.y = (b.y == 0.0) ? 0.0 : a.y / b.y;
    result.z = (b.z == 0.0) ? 0.0 : a.z / b.z;
    return result;
  }

  void invert()
  {
    x = -x;
    y = -y;
    z = -z;
  }

  friend double3 operator+(const double3 &a, const double3 &b)
  {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
  }

  void operator+=(const double3 &b)
  {
    this->x += b.x;
    this->y += b.y;
    this->z += b.z;
  }

  friend double3 operator-(const double3 &a, const double3 &b)
  {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
  }

  friend double3 operator-(const double3 &a)
  {
    return {-a.x, -a.y, -a.z};
  }

  void operator-=(const double3 &b)
  {
    this->x -= b.x;
    this->y -= b.y;
    this->z -= b.z;
  }

  void operator*=(const double &scalar)
  {
    this->x *= scalar;
    this->y *= scalar;
    this->z *= scalar;
  }

  void operator*=(const double3 &other)
  {
    this->x *= other.x;
    this->y *= other.y;
    this->z *= other.z;
  }

  friend double3 operator*(const double3 &a, const double3 &b)
  {
    return {a.x * b.x, a.y * b.y, a.z * b.z};
  }

  friend double3 operator*(const double3 &a, const double &b)
  {
    return {a.x * b, a.y * b, a.z * b};
  }

  friend double3 operator*(const double &a, const double3 &b)
  {
    return b * a;
  }

  friend double3 operator/(const double3 &a, const double &b)
  {
    BLI_assert(b != 0.0);
    return {a.x / b, a.y / b, a.z / b};
  }

  friend bool operator==(const double3 &a, const double3 &b)
  {
    return a.x == b.x && a.y == b.y && a.z == b.z;
  }

  friend bool operator!=(const double3 &a, const double3 &b)
  {
    return a.x != b.x || a.y != b.y || a.z != b.z;
  }

  friend std::ostream &operator<<(std::ostream &stream, const double3 &v)
  {
    stream << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    return stream;
  }

  static double dot(const double3 &a, const double3 &b)
  {
    return a.x * b.x + a.y * b.y + a.z * b.z;
  }

  static double3 cross_high_precision(const double3 &a, const double3 &b)
  {
    double3 result;
    cross_v3_v3v3_db(result, a, b);
    return result;
  }

  static double3 project(const double3 &a, const double3 &b)
  {
    double3 result;
    project_v3_v3v3_db(result, a, b);
    return result;
  }

  static double distance(const double3 &a, const double3 &b)
  {
    return (a - b).length();
  }

  static double distance_squared(const double3 &a, const double3 &b)
  {
    double3 diff = a - b;
    return double3::dot(diff, diff);
  }

  static double3 interpolate(const double3 &a, const double3 &b, double t)
  {
    return a * (1 - t) + b * t;
  }

  static double3 abs(const double3 &a)
  {
    return double3(fabs(a.x), fabs(a.y), fabs(a.z));
  }

  static int dominant_axis(const double3 &a)
  {
    double x = (a.x >= 0) ? a.x : -a.x;
    double y = (a.y >= 0) ? a.y : -a.y;
    double z = (a.z >= 0) ? a.z : -a.z;
    return ((x > y) ? ((x > z) ? 0 : 2) : ((y > z) ? 1 : 2));
  }

  static double3 cross_poly(Span<double3> poly);
};

}  // namespace blender
