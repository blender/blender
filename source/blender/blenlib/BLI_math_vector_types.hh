/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

#pragma once

/** \file
 * \ingroup bli
 */

#include <array>
#include <cmath>
#include <iostream>
#include <type_traits>

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

template<class Fn, size_t... I> void unroll_impl(Fn fn, std::index_sequence<I...> /*indices*/)
{
  (fn(I), ...);
}

/**
 * Variadic templates are used to unroll loops manually. This helps GCC avoid branching during math
 * operations and makes the code generation more explicit and predictable. Unrolling should always
 * be worth it because the vector size is expected to be small.
 */
template<int N, class Fn> void unroll(Fn fn)
{
  unroll_impl(fn, std::make_index_sequence<N>());
}

namespace math {

template<typename T> uint64_t vector_hash(const T &vec)
{
  BLI_STATIC_ASSERT(T::type_length <= 4, "Longer types need to implement vector_hash themself.");
  const typename T::uint_type &uvec = *reinterpret_cast<const typename T::uint_type *>(&vec);
  uint64_t result;
  result = uvec[0] * uint64_t(435109);
  if constexpr (T::type_length > 1) {
    result ^= uvec[1] * uint64_t(380867);
  }
  if constexpr (T::type_length > 2) {
    result ^= uvec[2] * uint64_t(1059217);
  }
  if constexpr (T::type_length > 3) {
    result ^= uvec[3] * uint64_t(2002613);
  }
  return result;
}

}  // namespace math

template<typename T, int Size> struct VecBase : public vec_struct_base<T, Size> {

  static constexpr int type_length = Size;

  using base_type = T;
  using uint_type = VecBase<as_uint_type<T>, Size>;

  VecBase() = default;

  explicit VecBase(T value)
  {
    for (int i = 0; i < Size; i++) {
      (*this)[i] = value;
    }
  }

  template<typename U, BLI_ENABLE_IF((std::is_convertible_v<U, T>))>
  explicit VecBase(U value) : VecBase(T(value))
  {
  }

/* Workaround issue with template BLI_ENABLE_IF((Size == 2)) not working. */
#define BLI_ENABLE_IF_VEC(_size, _test) int S = _size, BLI_ENABLE_IF((S _test))

  template<BLI_ENABLE_IF_VEC(Size, == 2)> VecBase(T _x, T _y)
  {
    (*this)[0] = _x;
    (*this)[1] = _y;
  }

  template<BLI_ENABLE_IF_VEC(Size, == 3)> VecBase(T _x, T _y, T _z)
  {
    (*this)[0] = _x;
    (*this)[1] = _y;
    (*this)[2] = _z;
  }

  template<BLI_ENABLE_IF_VEC(Size, == 4)> VecBase(T _x, T _y, T _z, T _w)
  {
    (*this)[0] = _x;
    (*this)[1] = _y;
    (*this)[2] = _z;
    (*this)[3] = _w;
  }

  /** Mixed scalar-vector constructors. */

  template<typename U, BLI_ENABLE_IF_VEC(Size, == 3)>
  constexpr VecBase(const VecBase<U, 2> &xy, T z) : VecBase(T(xy.x), T(xy.y), z)
  {
  }

  template<typename U, BLI_ENABLE_IF_VEC(Size, == 3)>
  constexpr VecBase(T x, const VecBase<U, 2> &yz) : VecBase(x, T(yz.x), T(yz.y))
  {
  }

  template<typename U, BLI_ENABLE_IF_VEC(Size, == 4)>
  VecBase(VecBase<U, 3> xyz, T w) : VecBase(T(xyz.x), T(xyz.y), T(xyz.z), T(w))
  {
  }

  template<typename U, BLI_ENABLE_IF_VEC(Size, == 4)>
  VecBase(T x, VecBase<U, 3> yzw) : VecBase(T(x), T(yzw.x), T(yzw.y), T(yzw.z))
  {
  }

  template<typename U, typename V, BLI_ENABLE_IF_VEC(Size, == 4)>
  VecBase(VecBase<U, 2> xy, VecBase<V, 2> zw) : VecBase(T(xy.x), T(xy.y), T(zw.x), T(zw.y))
  {
  }

  template<typename U, BLI_ENABLE_IF_VEC(Size, == 4)>
  VecBase(VecBase<U, 2> xy, T z, T w) : VecBase(T(xy.x), T(xy.y), T(z), T(w))
  {
  }

  template<typename U, BLI_ENABLE_IF_VEC(Size, == 4)>
  VecBase(T x, VecBase<U, 2> yz, T w) : VecBase(T(x), T(yz.x), T(yz.y), T(w))
  {
  }

  template<typename U, BLI_ENABLE_IF_VEC(Size, == 4)>
  VecBase(T x, T y, VecBase<U, 2> zw) : VecBase(T(x), T(y), T(zw.x), T(zw.y))
  {
  }

  /** Masking. */

  template<typename U, int OtherSize, BLI_ENABLE_IF(OtherSize > Size)>
  explicit VecBase(const VecBase<U, OtherSize> &other)
  {
    for (int i = 0; i < Size; i++) {
      (*this)[i] = T(other[i]);
    }
  }

  /** Swizzling. */

  template<BLI_ENABLE_IF_VEC(Size, >= 3)> VecBase<T, 2> xy() const
  {
    return *reinterpret_cast<const VecBase<T, 2> *>(this);
  }

  template<BLI_ENABLE_IF_VEC(Size, >= 4)> VecBase<T, 3> xyz() const
  {
    return *reinterpret_cast<const VecBase<T, 3> *>(this);
  }

#undef BLI_ENABLE_IF_VEC

  /** Conversion from pointers (from C-style vectors). */

  VecBase(const T *ptr)
  {
    unroll<Size>([&](auto i) { (*this)[i] = ptr[i]; });
  }

  template<typename U, BLI_ENABLE_IF((std::is_convertible_v<U, T>))> explicit VecBase(const U *ptr)
  {
    unroll<Size>([&](auto i) { (*this)[i] = ptr[i]; });
  }

  VecBase(const T (*ptr)[Size]) : VecBase(static_cast<const T *>(ptr[0]))
  {
  }

  /** Conversion from other vector types. */

  template<typename U> explicit VecBase(const VecBase<U, Size> &vec)
  {
    unroll<Size>([&](auto i) { (*this)[i] = T(vec[i]); });
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

  /** Arithmetic operators. */

  friend VecBase operator+(const VecBase &a, const VecBase &b)
  {
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = a[i] + b[i]; });
    return result;
  }

  friend VecBase operator+(const VecBase &a, const T &b)
  {
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = a[i] + b; });
    return result;
  }

  friend VecBase operator+(const T &a, const VecBase &b)
  {
    return b + a;
  }

  VecBase &operator+=(const VecBase &b)
  {
    unroll<Size>([&](auto i) { (*this)[i] += b[i]; });
    return *this;
  }

  VecBase &operator+=(const T &b)
  {
    unroll<Size>([&](auto i) { (*this)[i] += b; });
    return *this;
  }

  friend VecBase operator-(const VecBase &a)
  {
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = -a[i]; });
    return result;
  }

  friend VecBase operator-(const VecBase &a, const VecBase &b)
  {
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = a[i] - b[i]; });
    return result;
  }

  friend VecBase operator-(const VecBase &a, const T &b)
  {
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = a[i] - b; });
    return result;
  }

  friend VecBase operator-(const T &a, const VecBase &b)
  {
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = a - b[i]; });
    return result;
  }

  VecBase &operator-=(const VecBase &b)
  {
    unroll<Size>([&](auto i) { (*this)[i] -= b[i]; });
    return *this;
  }

  VecBase &operator-=(const T &b)
  {
    unroll<Size>([&](auto i) { (*this)[i] -= b; });
    return *this;
  }

  friend VecBase operator*(const VecBase &a, const VecBase &b)
  {
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = a[i] * b[i]; });
    return result;
  }

  template<typename FactorT> friend VecBase operator*(const VecBase &a, FactorT b)
  {
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = a[i] * b; });
    return result;
  }

  friend VecBase operator*(T a, const VecBase &b)
  {
    return b * a;
  }

  VecBase &operator*=(T b)
  {
    unroll<Size>([&](auto i) { (*this)[i] *= b; });
    return *this;
  }

  VecBase &operator*=(const VecBase &b)
  {
    unroll<Size>([&](auto i) { (*this)[i] *= b[i]; });
    return *this;
  }

  friend VecBase operator/(const VecBase &a, const VecBase &b)
  {
    for (int i = 0; i < Size; i++) {
      BLI_assert(b[i] != T(0));
    }
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = a[i] / b[i]; });
    return result;
  }

  friend VecBase operator/(const VecBase &a, T b)
  {
    BLI_assert(b != T(0));
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = a[i] / b; });
    return result;
  }

  friend VecBase operator/(T a, const VecBase &b)
  {
    for (int i = 0; i < Size; i++) {
      BLI_assert(b[i] != T(0));
    }
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = a / b[i]; });
    return result;
  }

  VecBase &operator/=(T b)
  {
    BLI_assert(b != T(0));
    unroll<Size>([&](auto i) { (*this)[i] /= b; });
    return *this;
  }

  VecBase &operator/=(const VecBase &b)
  {
    BLI_assert(b != T(0));
    unroll<Size>([&](auto i) { (*this)[i] /= b[i]; });
    return *this;
  }

  /** Binary operators. */

  BLI_INT_OP(T) friend VecBase operator&(const VecBase &a, const VecBase &b)
  {
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = a[i] & b[i]; });
    return result;
  }

  BLI_INT_OP(T) friend VecBase operator&(const VecBase &a, T b)
  {
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = a[i] & b; });
    return result;
  }

  BLI_INT_OP(T) friend VecBase operator&(T a, const VecBase &b)
  {
    return b & a;
  }

  BLI_INT_OP(T) VecBase &operator&=(T b)
  {
    unroll<Size>([&](auto i) { (*this)[i] &= b; });
    return *this;
  }

  BLI_INT_OP(T) VecBase &operator&=(const VecBase &b)
  {
    unroll<Size>([&](auto i) { (*this)[i] &= b[i]; });
    return *this;
  }

  BLI_INT_OP(T) friend VecBase operator|(const VecBase &a, const VecBase &b)
  {
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = a[i] | b[i]; });
    return result;
  }

  BLI_INT_OP(T) friend VecBase operator|(const VecBase &a, T b)
  {
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = a[i] | b; });
    return result;
  }

  BLI_INT_OP(T) friend VecBase operator|(T a, const VecBase &b)
  {
    return b | a;
  }

  BLI_INT_OP(T) VecBase &operator|=(T b)
  {
    unroll<Size>([&](auto i) { (*this)[i] |= b; });
    return *this;
  }

  BLI_INT_OP(T) VecBase &operator|=(const VecBase &b)
  {
    unroll<Size>([&](auto i) { (*this)[i] |= b[i]; });
    return *this;
  }

  BLI_INT_OP(T) friend VecBase operator^(const VecBase &a, const VecBase &b)
  {
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = a[i] ^ b[i]; });
    return result;
  }

  BLI_INT_OP(T) friend VecBase operator^(const VecBase &a, T b)
  {
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = a[i] ^ b; });
    return result;
  }

  BLI_INT_OP(T) friend VecBase operator^(T a, const VecBase &b)
  {
    return b ^ a;
  }

  BLI_INT_OP(T) VecBase &operator^=(T b)
  {
    unroll<Size>([&](auto i) { (*this)[i] ^= b; });
    return *this;
  }

  BLI_INT_OP(T) VecBase &operator^=(const VecBase &b)
  {
    unroll<Size>([&](auto i) { (*this)[i] ^= b[i]; });
    return *this;
  }

  BLI_INT_OP(T) friend VecBase operator~(const VecBase &a)
  {
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = ~a[i]; });
    return result;
  }

  /** Bit-shift operators. */

  BLI_INT_OP(T) friend VecBase operator<<(const VecBase &a, const VecBase &b)
  {
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = a[i] << b[i]; });
    return result;
  }

  BLI_INT_OP(T) friend VecBase operator<<(const VecBase &a, T b)
  {
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = a[i] << b; });
    return result;
  }

  BLI_INT_OP(T) VecBase &operator<<=(T b)
  {
    unroll<Size>([&](auto i) { (*this)[i] <<= b; });
    return *this;
  }

  BLI_INT_OP(T) VecBase &operator<<=(const VecBase &b)
  {
    unroll<Size>([&](auto i) { (*this)[i] <<= b[i]; });
    return *this;
  }

  BLI_INT_OP(T) friend VecBase operator>>(const VecBase &a, const VecBase &b)
  {
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = a[i] >> b[i]; });
    return result;
  }

  BLI_INT_OP(T) friend VecBase operator>>(const VecBase &a, T b)
  {
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = a[i] >> b; });
    return result;
  }

  BLI_INT_OP(T) VecBase &operator>>=(T b)
  {
    unroll<Size>([&](auto i) { (*this)[i] >>= b; });
    return *this;
  }

  BLI_INT_OP(T) VecBase &operator>>=(const VecBase &b)
  {
    unroll<Size>([&](auto i) { (*this)[i] >>= b[i]; });
    return *this;
  }

  /** Modulo operators. */

  BLI_INT_OP(T) friend VecBase operator%(const VecBase &a, const VecBase &b)
  {
    for (int i = 0; i < Size; i++) {
      BLI_assert(b[i] != T(0));
    }
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = a[i] % b[i]; });
    return result;
  }

  BLI_INT_OP(T) friend VecBase operator%(const VecBase &a, T b)
  {
    BLI_assert(b != 0);
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = a[i] % b; });
    return result;
  }

  BLI_INT_OP(T) friend VecBase operator%(T a, const VecBase &b)
  {
    BLI_assert(b != T(0));
    VecBase result;
    unroll<Size>([&](auto i) { result[i] = a % b[i]; });
    return result;
  }

#undef BLI_INT_OP

  /** Compare. */

  friend bool operator==(const VecBase &a, const VecBase &b)
  {
    for (int i = 0; i < Size; i++) {
      if (a[i] != b[i]) {
        return false;
      }
    }
    return true;
  }

  friend bool operator!=(const VecBase &a, const VecBase &b)
  {
    return !(a == b);
  }

  /** Misc. */

  uint64_t hash() const
  {
    return math::vector_hash(*this);
  }

  friend std::ostream &operator<<(std::ostream &stream, const VecBase &v)
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

namespace math {

template<typename T> struct AssertUnitEpsilon {
  /** \note Copy of BLI_ASSERT_UNIT_EPSILON_DB to avoid dragging the entire header. */
  static constexpr T value = T(0.0002);
};

}  // namespace math

using char3 = blender::VecBase<int8_t, 3>;

using uchar3 = blender::VecBase<uint8_t, 3>;
using uchar4 = blender::VecBase<uint8_t, 4>;

using int2 = VecBase<int32_t, 2>;
using int3 = VecBase<int32_t, 3>;
using int4 = VecBase<int32_t, 4>;

using uint2 = VecBase<uint32_t, 2>;
using uint3 = VecBase<uint32_t, 3>;
using uint4 = VecBase<uint32_t, 4>;

using short2 = blender::VecBase<int16_t, 2>;
using short3 = blender::VecBase<int16_t, 3>;

using ushort2 = VecBase<uint16_t, 2>;
using ushort3 = blender::VecBase<uint16_t, 3>;
using ushort4 = blender::VecBase<uint16_t, 4>;

using float2 = VecBase<float, 2>;
using float3 = VecBase<float, 3>;
using float4 = VecBase<float, 4>;

using double2 = VecBase<double, 2>;
using double3 = VecBase<double, 3>;
using double4 = VecBase<double, 4>;

}  // namespace blender
