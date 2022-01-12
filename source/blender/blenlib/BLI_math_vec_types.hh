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
 *
 * Copyright 2022, Blender Foundation.
 */

#pragma once

/** \file
 * \ingroup bli
 */

#include <array>
#include <cmath>
#include <iostream>
#include <type_traits>

#include "BLI_math_vector.hh"
#include "BLI_utildefines.h"

namespace blender {

/* clang-format off */
template<typename T>
using as_uint_type = std::conditional_t<sizeof(T) == sizeof(uint8_t), uint8_t,
                     std::conditional_t<sizeof(T) == sizeof(uint16_t), uint16_t,
                     std::conditional_t<sizeof(T) == sizeof(uint32_t), uint32_t,
                     std::conditional_t<sizeof(T) == sizeof(uint64_t), uint64_t, void>>>>;
/* clang-format on */

template<typename T, int Size> struct vec_struct_base {
  std::array<T, Size> values;
};

template<typename T> struct vec_struct_base<T, 2> {
  T x, y;
};

template<typename T> struct vec_struct_base<T, 3> {
  T x, y, z;
};

template<typename T> struct vec_struct_base<T, 4> {
  T x, y, z, w;
};

template<typename T, int Size> struct vec_base : public vec_struct_base<T, Size> {

  static constexpr int type_length = Size;

  using base_type = T;
  using uint_type = vec_base<as_uint_type<T>, Size>;

  vec_base() = default;

  explicit vec_base(uint value)
  {
    for (int i = 0; i < Size; i++) {
      (*this)[i] = static_cast<T>(value);
    }
  }

  explicit vec_base(int value)
  {
    for (int i = 0; i < Size; i++) {
      (*this)[i] = static_cast<T>(value);
    }
  }

  explicit vec_base(float value)
  {
    for (int i = 0; i < Size; i++) {
      (*this)[i] = static_cast<T>(value);
    }
  }

  explicit vec_base(double value)
  {
    for (int i = 0; i < Size; i++) {
      (*this)[i] = static_cast<T>(value);
    }
  }

/* Workaround issue with template BLI_ENABLE_IF((Size == 2)) not working. */
#define BLI_ENABLE_IF_VEC(_size, _test) int S = _size, BLI_ENABLE_IF((S _test))

  template<BLI_ENABLE_IF_VEC(Size, == 2)> vec_base(T _x, T _y)
  {
    (*this)[0] = _x;
    (*this)[1] = _y;
  }

  template<BLI_ENABLE_IF_VEC(Size, == 3)> vec_base(T _x, T _y, T _z)
  {
    (*this)[0] = _x;
    (*this)[1] = _y;
    (*this)[2] = _z;
  }

  template<BLI_ENABLE_IF_VEC(Size, == 4)> vec_base(T _x, T _y, T _z, T _w)
  {
    (*this)[0] = _x;
    (*this)[1] = _y;
    (*this)[2] = _z;
    (*this)[3] = _w;
  }

  /** Mixed scalar-vector constructors. */

  template<typename U, BLI_ENABLE_IF_VEC(Size, == 3)>
  constexpr vec_base(const vec_base<U, 2> &xy, T z)
      : vec_base(static_cast<T>(xy.x), static_cast<T>(xy.y), z)
  {
  }

  template<typename U, BLI_ENABLE_IF_VEC(Size, == 3)>
  constexpr vec_base(T x, const vec_base<U, 2> &yz)
      : vec_base(x, static_cast<T>(yz.x), static_cast<T>(yz.y))
  {
  }

  template<typename U, BLI_ENABLE_IF_VEC(Size, == 4)>
  vec_base(vec_base<U, 3> xyz, T w)
      : vec_base(
            static_cast<T>(xyz.x), static_cast<T>(xyz.y), static_cast<T>(xyz.z), static_cast<T>(w))
  {
  }

  template<typename U, BLI_ENABLE_IF_VEC(Size, == 4)>
  vec_base(T x, vec_base<U, 3> yzw)
      : vec_base(
            static_cast<T>(x), static_cast<T>(yzw.x), static_cast<T>(yzw.y), static_cast<T>(yzw.z))
  {
  }

  template<typename U, typename V, BLI_ENABLE_IF_VEC(Size, == 4)>
  vec_base(vec_base<U, 2> xy, vec_base<V, 2> zw)
      : vec_base(
            static_cast<T>(xy.x), static_cast<T>(xy.y), static_cast<T>(zw.x), static_cast<T>(zw.y))
  {
  }

  template<typename U, BLI_ENABLE_IF_VEC(Size, == 4)>
  vec_base(vec_base<U, 2> xy, T z, T w)
      : vec_base(static_cast<T>(xy.x), static_cast<T>(xy.y), static_cast<T>(z), static_cast<T>(w))
  {
  }

  template<typename U, BLI_ENABLE_IF_VEC(Size, == 4)>
  vec_base(T x, vec_base<U, 2> yz, T w)
      : vec_base(static_cast<T>(x), static_cast<T>(yz.x), static_cast<T>(yz.y), static_cast<T>(w))
  {
  }

  template<typename U, BLI_ENABLE_IF_VEC(Size, == 4)>
  vec_base(T x, T y, vec_base<U, 2> zw)
      : vec_base(static_cast<T>(x), static_cast<T>(y), static_cast<T>(zw.x), static_cast<T>(zw.y))
  {
  }

  /** Masking. */

  template<typename U, int OtherSize, BLI_ENABLE_IF(OtherSize > Size)>
  explicit vec_base(const vec_base<U, OtherSize> &other)
  {
    for (int i = 0; i < Size; i++) {
      (*this)[i] = static_cast<T>(other[i]);
    }
  }

#undef BLI_ENABLE_IF_VEC

  /** Conversion from pointers (from C-style vectors). */

  vec_base(const T *ptr)
  {
    for (int i = 0; i < Size; i++) {
      (*this)[i] = ptr[i];
    }
  }

  vec_base(const T (*ptr)[Size]) : vec_base(static_cast<const T *>(ptr[0]))
  {
  }

  /** Conversion from other vector types. */

  template<typename U> explicit vec_base(const vec_base<U, Size> &vec)
  {
    for (int i = 0; i < Size; i++) {
      (*this)[i] = static_cast<T>(vec[i]);
    }
  }

  /** C-style pointer dereference. */

  operator const T *() const
  {
    return reinterpret_cast<const T *>(this);
  }

  operator T *()
  {
    return reinterpret_cast<T *>(this);
  }

  /** Array access. */

  const T &operator[](int index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < Size);
    return reinterpret_cast<const T *>(this)[index];
  }

  T &operator[](int index)
  {
    BLI_assert(index >= 0);
    BLI_assert(index < Size);
    return reinterpret_cast<T *>(this)[index];
  }

  /** Internal Operators Macro. */

#define BLI_INT_OP(_T) template<typename U = _T, BLI_ENABLE_IF((std::is_integral_v<U>))>

#define BLI_VEC_OP_IMPL(_result, _i, _op) \
  vec_base _result; \
  for (int _i = 0; _i < Size; _i++) { \
    _op; \
  } \
  return _result;

#define BLI_VEC_OP_IMPL_SELF(_i, _op) \
  for (int _i = 0; _i < Size; _i++) { \
    _op; \
  } \
  return *this;

  /** Arithmetic operators. */

  friend vec_base operator+(const vec_base &a, const vec_base &b)
  {
    BLI_VEC_OP_IMPL(ret, i, ret[i] = a[i] + b[i]);
  }

  friend vec_base operator+(const vec_base &a, const T &b)
  {
    BLI_VEC_OP_IMPL(ret, i, ret[i] = a[i] + b);
  }

  friend vec_base operator+(const T &a, const vec_base &b)
  {
    return b + a;
  }

  vec_base &operator+=(const vec_base &b)
  {
    BLI_VEC_OP_IMPL_SELF(i, (*this)[i] += b[i]);
  }

  vec_base &operator+=(const T &b)
  {
    BLI_VEC_OP_IMPL_SELF(i, (*this)[i] += b);
  }

  friend vec_base operator-(const vec_base &a)
  {
    BLI_VEC_OP_IMPL(ret, i, ret[i] = -a[i]);
  }

  friend vec_base operator-(const vec_base &a, const vec_base &b)
  {
    BLI_VEC_OP_IMPL(ret, i, ret[i] = a[i] - b[i]);
  }

  friend vec_base operator-(const vec_base &a, const T &b)
  {
    BLI_VEC_OP_IMPL(ret, i, ret[i] = a[i] - b);
  }

  friend vec_base operator-(const T &a, const vec_base &b)
  {
    BLI_VEC_OP_IMPL(ret, i, ret[i] = a - b[i]);
  }

  vec_base &operator-=(const vec_base &b)
  {
    BLI_VEC_OP_IMPL_SELF(i, (*this)[i] -= b[i]);
  }

  vec_base &operator-=(const T &b)
  {
    BLI_VEC_OP_IMPL_SELF(i, (*this)[i] -= b);
  }

  friend vec_base operator*(const vec_base &a, const vec_base &b)
  {
    BLI_VEC_OP_IMPL(ret, i, ret[i] = a[i] * b[i]);
  }

  friend vec_base operator*(const vec_base &a, T b)
  {
    BLI_VEC_OP_IMPL(ret, i, ret[i] = a[i] * b);
  }

  friend vec_base operator*(T a, const vec_base &b)
  {
    return b * a;
  }

  vec_base &operator*=(T b)
  {
    BLI_VEC_OP_IMPL_SELF(i, (*this)[i] *= b);
  }

  vec_base &operator*=(const vec_base &b)
  {
    BLI_VEC_OP_IMPL_SELF(i, (*this)[i] *= b[i]);
  }

  friend vec_base operator/(const vec_base &a, const vec_base &b)
  {
    BLI_assert(!math::is_any_zero(b));
    BLI_VEC_OP_IMPL(ret, i, ret[i] = a[i] / b[i]);
  }

  friend vec_base operator/(const vec_base &a, T b)
  {
    BLI_assert(b != T(0));
    BLI_VEC_OP_IMPL(ret, i, ret[i] = a[i] / b);
  }

  friend vec_base operator/(T a, const vec_base &b)
  {
    BLI_assert(!math::is_any_zero(b));
    BLI_VEC_OP_IMPL(ret, i, ret[i] = a / b[i]);
  }

  vec_base &operator/=(T b)
  {
    BLI_assert(b != T(0));
    BLI_VEC_OP_IMPL_SELF(i, (*this)[i] /= b);
  }

  vec_base &operator/=(const vec_base &b)
  {
    BLI_assert(!math::is_any_zero(b));
    BLI_VEC_OP_IMPL_SELF(i, (*this)[i] /= b[i]);
  }

  /** Binary operators. */

  BLI_INT_OP(T) friend vec_base operator&(const vec_base &a, const vec_base &b)
  {
    BLI_VEC_OP_IMPL(ret, i, ret[i] = a[i] & b[i]);
  }

  BLI_INT_OP(T) friend vec_base operator&(const vec_base &a, T b)
  {
    BLI_VEC_OP_IMPL(ret, i, ret[i] = a[i] & b);
  }

  BLI_INT_OP(T) friend vec_base operator&(T a, const vec_base &b)
  {
    return b & a;
  }

  BLI_INT_OP(T) vec_base &operator&=(T b)
  {
    BLI_VEC_OP_IMPL_SELF(i, (*this)[i] &= b);
  }

  BLI_INT_OP(T) vec_base &operator&=(const vec_base &b)
  {
    BLI_VEC_OP_IMPL_SELF(i, (*this)[i] &= b[i]);
  }

  BLI_INT_OP(T) friend vec_base operator|(const vec_base &a, const vec_base &b)
  {
    BLI_VEC_OP_IMPL(ret, i, ret[i] = a[i] | b[i]);
  }

  BLI_INT_OP(T) friend vec_base operator|(const vec_base &a, T b)
  {
    BLI_VEC_OP_IMPL(ret, i, ret[i] = a[i] | b);
  }

  BLI_INT_OP(T) friend vec_base operator|(T a, const vec_base &b)
  {
    return b | a;
  }

  BLI_INT_OP(T) vec_base &operator|=(T b)
  {
    BLI_VEC_OP_IMPL_SELF(i, (*this)[i] |= b);
  }

  BLI_INT_OP(T) vec_base &operator|=(const vec_base &b)
  {
    BLI_VEC_OP_IMPL_SELF(i, (*this)[i] |= b[i]);
  }

  BLI_INT_OP(T) friend vec_base operator^(const vec_base &a, const vec_base &b)
  {
    BLI_VEC_OP_IMPL(ret, i, ret[i] = a[i] ^ b[i]);
  }

  BLI_INT_OP(T) friend vec_base operator^(const vec_base &a, T b)
  {
    BLI_VEC_OP_IMPL(ret, i, ret[i] = a[i] ^ b);
  }

  BLI_INT_OP(T) friend vec_base operator^(T a, const vec_base &b)
  {
    return b ^ a;
  }

  BLI_INT_OP(T) vec_base &operator^=(T b)
  {
    BLI_VEC_OP_IMPL_SELF(i, (*this)[i] ^= b);
  }

  BLI_INT_OP(T) vec_base &operator^=(const vec_base &b)
  {
    BLI_VEC_OP_IMPL_SELF(i, (*this)[i] ^= b[i]);
  }

  BLI_INT_OP(T) friend vec_base operator~(const vec_base &a)
  {
    BLI_VEC_OP_IMPL(ret, i, ret[i] = ~a[i]);
  }

  /** Bit-shift operators. */

  BLI_INT_OP(T) friend vec_base operator<<(const vec_base &a, const vec_base &b)
  {
    BLI_VEC_OP_IMPL(ret, i, ret[i] = a[i] << b[i]);
  }

  BLI_INT_OP(T) friend vec_base operator<<(const vec_base &a, T b)
  {
    BLI_VEC_OP_IMPL(ret, i, ret[i] = a[i] << b);
  }

  BLI_INT_OP(T) vec_base &operator<<=(T b)
  {
    BLI_VEC_OP_IMPL_SELF(i, (*this)[i] <<= b);
  }

  BLI_INT_OP(T) vec_base &operator<<=(const vec_base &b)
  {
    BLI_VEC_OP_IMPL_SELF(i, (*this)[i] <<= b[i]);
  }

  BLI_INT_OP(T) friend vec_base operator>>(const vec_base &a, const vec_base &b)
  {
    BLI_VEC_OP_IMPL(ret, i, ret[i] = a[i] >> b[i]);
  }

  BLI_INT_OP(T) friend vec_base operator>>(const vec_base &a, T b)
  {
    BLI_VEC_OP_IMPL(ret, i, ret[i] = a[i] >> b);
  }

  BLI_INT_OP(T) vec_base &operator>>=(T b)
  {
    BLI_VEC_OP_IMPL_SELF(i, (*this)[i] >>= b);
  }

  BLI_INT_OP(T) vec_base &operator>>=(const vec_base &b)
  {
    BLI_VEC_OP_IMPL_SELF(i, (*this)[i] >>= b[i]);
  }

  /** Modulo operators. */

  BLI_INT_OP(T) friend vec_base operator%(const vec_base &a, const vec_base &b)
  {
    BLI_assert(!math::is_any_zero(b));
    BLI_VEC_OP_IMPL(ret, i, ret[i] = a[i] % b[i]);
  }

  BLI_INT_OP(T) friend vec_base operator%(const vec_base &a, T b)
  {
    BLI_assert(b != 0);
    BLI_VEC_OP_IMPL(ret, i, ret[i] = a[i] % b);
  }

  BLI_INT_OP(T) friend vec_base operator%(T a, const vec_base &b)
  {
    BLI_assert(!math::is_any_zero(b));
    BLI_VEC_OP_IMPL(ret, i, ret[i] = a % b[i]);
  }

#undef BLI_INT_OP
#undef BLI_VEC_OP_IMPL
#undef BLI_VEC_OP_IMPL_SELF

  /** Compare. */

  friend bool operator==(const vec_base &a, const vec_base &b)
  {
    for (int i = 0; i < Size; i++) {
      if (a[i] != b[i]) {
        return false;
      }
    }
    return true;
  }

  friend bool operator!=(const vec_base &a, const vec_base &b)
  {
    return !(a == b);
  }

  /** Misc. */

  uint64_t hash() const
  {
    return math::vector_hash(*this);
  }

  friend std::ostream &operator<<(std::ostream &stream, const vec_base &v)
  {
    stream << "(";
    for (int i = 0; i < Size; i++) {
      stream << v[i];
      if (i != Size - 1) {
        stream << ", ";
      }
    }
    stream << ")";
    return stream;
  }
};

using int2 = vec_base<int32_t, 2>;
using int3 = vec_base<int32_t, 3>;
using int4 = vec_base<int32_t, 4>;

using uint2 = vec_base<uint32_t, 2>;
using uint3 = vec_base<uint32_t, 3>;
using uint4 = vec_base<uint32_t, 4>;

using float2 = vec_base<float, 2>;
using float3 = vec_base<float, 3>;
using float4 = vec_base<float, 4>;

using double2 = vec_base<double, 2>;
using double3 = vec_base<double, 3>;
using double4 = vec_base<double, 4>;

}  // namespace blender
