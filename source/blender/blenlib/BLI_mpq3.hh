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

#ifdef WITH_GMP

#  include <iostream>

#  include "BLI_math.h"
#  include "BLI_math_mpq.hh"
#  include "BLI_span.hh"

namespace blender {

struct mpq3 {
  mpq_class x, y, z;

  mpq3() = default;

  mpq3(const mpq_class *ptr) : x{ptr[0]}, y{ptr[1]}, z{ptr[2]}
  {
  }

  mpq3(const mpq_class (*ptr)[3]) : mpq3((const mpq_class *)ptr)
  {
  }

  explicit mpq3(mpq_class value) : x(value), y(value), z(value)
  {
  }

  explicit mpq3(int value) : x(value), y(value), z(value)
  {
  }

  mpq3(mpq_class x, mpq_class y, mpq_class z) : x{x}, y{y}, z{z}
  {
  }

  operator const mpq_class *() const
  {
    return &x;
  }

  operator mpq_class *()
  {
    return &x;
  }

  /* Cannot do this exactly in rational arithmetic!
   * Approximate by going in and out of doubles.
   */
  mpq_class normalize_and_get_length()
  {
    double dv[3] = {x.get_d(), y.get_d(), z.get_d()};
    double len = normalize_v3_db(dv);
    this->x = mpq_class(dv[0]);
    this->y = mpq_class(dv[1]);
    this->z = mpq_class(dv[2]);
    return len;
  }

  mpq3 normalized() const
  {
    double dv[3] = {x.get_d(), y.get_d(), z.get_d()};
    double dr[3];
    normalize_v3_v3_db(dr, dv);
    return mpq3(mpq_class(dr[0]), mpq_class(dr[1]), mpq_class(dr[2]));
  }

  /* Cannot do this exactly in rational arithmetic!
   * Approximate by going in and out of double.
   */
  mpq_class length() const
  {
    mpq_class lsquared = this->length_squared();
    double dsquared = lsquared.get_d();
    double d = sqrt(dsquared);
    return mpq_class(d);
  }

  mpq_class length_squared() const
  {
    return x * x + y * y + z * z;
  }

  void reflect(const mpq3 &normal)
  {
    *this = this->reflected(normal);
  }

  mpq3 reflected(const mpq3 &normal) const
  {
    mpq3 result;
    const mpq_class dot2 = 2 * dot(*this, normal);
    result.x = this->x - (dot2 * normal.x);
    result.y = this->y - (dot2 * normal.y);
    result.z = this->z - (dot2 * normal.z);
    return result;
  }

  static mpq3 safe_divide(const mpq3 &a, const mpq3 &b)
  {
    mpq3 result;
    result.x = (b.x == 0) ? mpq_class(0) : a.x / b.x;
    result.y = (b.y == 0) ? mpq_class(0) : a.y / b.y;
    result.z = (b.z == 0) ? mpq_class(0) : a.z / b.z;
    return result;
  }

  void invert()
  {
    x = -x;
    y = -y;
    z = -z;
  }

  friend mpq3 operator+(const mpq3 &a, const mpq3 &b)
  {
    return mpq3(a.x + b.x, a.y + b.y, a.z + b.z);
  }

  void operator+=(const mpq3 &b)
  {
    this->x += b.x;
    this->y += b.y;
    this->z += b.z;
  }

  friend mpq3 operator-(const mpq3 &a, const mpq3 &b)
  {
    return mpq3(a.x - b.x, a.y - b.y, a.z - b.z);
  }

  friend mpq3 operator-(const mpq3 &a)
  {
    return mpq3(-a.x, -a.y, -a.z);
  }

  void operator-=(const mpq3 &b)
  {
    this->x -= b.x;
    this->y -= b.y;
    this->z -= b.z;
  }

  void operator*=(mpq_class scalar)
  {
    this->x *= scalar;
    this->y *= scalar;
    this->z *= scalar;
  }

  void operator*=(const mpq3 &other)
  {
    this->x *= other.x;
    this->y *= other.y;
    this->z *= other.z;
  }

  friend mpq3 operator*(const mpq3 &a, const mpq3 &b)
  {
    return {a.x * b.x, a.y * b.y, a.z * b.z};
  }

  friend mpq3 operator*(const mpq3 &a, const mpq_class &b)
  {
    return mpq3(a.x * b, a.y * b, a.z * b);
  }

  friend mpq3 operator*(const mpq_class &a, const mpq3 &b)
  {
    return mpq3(a * b.x, a * b.y, a * b.z);
  }

  friend mpq3 operator/(const mpq3 &a, const mpq_class &b)
  {
    BLI_assert(b != 0);
    return mpq3(a.x / b, a.y / b, a.z / b);
  }

  friend bool operator==(const mpq3 &a, const mpq3 &b)
  {
    return a.x == b.x && a.y == b.y && a.z == b.z;
  }

  friend bool operator!=(const mpq3 &a, const mpq3 &b)
  {
    return a.x != b.x || a.y != b.y || a.z != b.z;
  }

  friend std::ostream &operator<<(std::ostream &stream, const mpq3 &v)
  {
    stream << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    return stream;
  }

  static mpq_class dot(const mpq3 &a, const mpq3 &b)
  {
    return a.x * b.x + a.y * b.y + a.z * b.z;
  }

  static mpq_class dot_with_buffer(const mpq3 &a, const mpq3 &b, mpq3 &buffer)
  {
    buffer = a;
    buffer *= b;
    buffer.x += buffer.y;
    buffer.x += buffer.z;
    return buffer.x;
  }

  static mpq3 cross(const mpq3 &a, const mpq3 &b)
  {
    return mpq3(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]);
  }

  static mpq3 cross_high_precision(const mpq3 &a, const mpq3 &b)
  {
    return cross(a, b);
  }

  static mpq3 project(const mpq3 &a, const mpq3 &b)
  {
    const mpq_class mul = mpq3::dot(a, b) / mpq3::dot(b, b);
    return mpq3(mul * b[0], mul * b[1], mul * b[2]);
  }

  static mpq_class distance(const mpq3 &a, const mpq3 &b)
  {
    mpq3 diff(a.x - b.x, a.y - b.y, a.z - b.z);
    return diff.length();
  }

  static mpq_class distance_squared(const mpq3 &a, const mpq3 &b)
  {
    mpq3 diff(a.x - b.x, a.y - b.y, a.z - b.z);
    return mpq3::dot(diff, diff);
  }

  static mpq_class distance_squared_with_buffer(const mpq3 &a, const mpq3 &b, mpq3 &buffer)
  {
    buffer = a;
    buffer -= b;
    return mpq3::dot(buffer, buffer);
  }

  static mpq3 interpolate(const mpq3 &a, const mpq3 &b, mpq_class t)
  {
    mpq_class s = 1 - t;
    return mpq3(a.x * s + b.x * t, a.y * s + b.y * t, a.z * s + b.z * t);
  }

  static mpq3 abs(const mpq3 &a)
  {
    mpq_class abs_x = (a.x >= 0) ? a.x : -a.x;
    mpq_class abs_y = (a.y >= 0) ? a.y : -a.y;
    mpq_class abs_z = (a.z >= 0) ? a.z : -a.z;
    return mpq3(abs_x, abs_y, abs_z);
  }

  static int dominant_axis(const mpq3 &a)
  {
    mpq_class x = (a.x >= 0) ? a.x : -a.x;
    mpq_class y = (a.y >= 0) ? a.y : -a.y;
    mpq_class z = (a.z >= 0) ? a.z : -a.z;
    return ((x > y) ? ((x > z) ? 0 : 2) : ((y > z) ? 1 : 2));
  }

  static mpq3 cross_poly(Span<mpq3> poly);

  /** There is a sensible use for hashing on exact arithmetic types. */
  uint64_t hash() const;
};

uint64_t hash_mpq_class(const mpq_class &value);

}  // namespace blender

#endif /* WITH_GMP */
