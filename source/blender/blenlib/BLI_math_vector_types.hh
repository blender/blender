/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include <array>
#include <ostream>
#include <type_traits>

#include "BLI_build_config.h"
#include "BLI_math_vector_swizzle.hh"
#include "BLI_math_vector_unroll.hh"
#include "BLI_utildefines.h"

namespace blender {

/* clang-format off */
template<typename T>
using as_uint_type = std::conditional_t<sizeof(T) == sizeof(uint8_t), uint8_t,
                     std::conditional_t<sizeof(T) == sizeof(uint16_t), uint16_t,
                     std::conditional_t<sizeof(T) == sizeof(uint32_t), uint32_t,
                     std::conditional_t<sizeof(T) == sizeof(uint64_t), uint64_t, void>>>>;
/* clang-format on */

template<typename T, int Size, bool is_trivial_type> struct vec_struct_base {
  std::array<T, Size> values;
};

template<typename T> struct vec_struct_base<T, 2, false> : VecSwizzleFunc<T, 2> {
  T x, y;
};

template<typename T> struct vec_struct_base<T, 3, false> : VecSwizzleFunc<T, 3> {
  T x, y, z;
};

template<typename T> struct vec_struct_base<T, 4, false> : VecSwizzleFunc<T, 4> {
  T x, y, z, w;
};

/**
 * Avoid warning caused by anonymous struct in unions.
 */
#if COMPILER_GCC
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wpedantic"
#elif COMPILER_CLANG
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
#  pragma clang diagnostic ignored "-Wnested-anon-types"
#elif COMPILER_MSVC
#  pragma warning(push)
#  pragma warning(disable : 4201)  // nonstandard extension used : nameless struct/union
#endif

template<typename T> struct vec_struct_base<T, 2, true> {
  union {
#ifndef NDEBUG
    /* Easier to read inside a debugger. */
    std::array<T, 2> debug_values_;
#endif
    struct {
      T x;
      union {
        struct {
          /* This still needs to be wrapped in a struct to avoid a MSVC 16.5+ bug. */
          T y;
        };
        Y_SWIZZLES;
      };
    };
    /* Nesting to avoid readability issues inside a debugger. */
    union {
      X_SWIZZLES;
      XY_SWIZZLES;
    };
  };
};

template<typename T> struct vec_struct_base<T, 3, true> {
  union {
#ifndef NDEBUG
    /* Easier to read inside a debugger. */
    std::array<T, 3> debug_values_;
#endif
    struct {
      T x;
      union {
        struct {
          T y;
          union {
            struct {
              /* This still needs to be wrapped in a struct to avoid a MSVC 16.5+ bug. */
              T z;
            };
            Z_SWIZZLES;
          };
        };
        Y_SWIZZLES;
        YZ_SWIZZLES;
      };
    };
    /* Nesting to avoid readability issues inside a debugger. */
    union {
      X_SWIZZLES;
      XY_SWIZZLES;
      XYZ_SWIZZLES;
    };
  };
};

template<typename T> struct vec_struct_base<T, 4, true> {
  union {
#ifndef NDEBUG
    /* Useful for debugging. */
    std::array<T, 4> debug_values_;
#endif
    struct {
      T x;
      union {
        struct {
          T y;
          union {
            struct {
              T z;
              union {
                struct {
                  /* This still needs to be wrapped in a struct to avoid a MSVC 16.5+ bug. */
                  T w;
                };
                W_SWIZZLES;
              };
            };
            Z_SWIZZLES;
            ZW_SWIZZLES;
          };
        };
        Y_SWIZZLES;
        YZ_SWIZZLES;
        YZW_SWIZZLES;
      };
    };
    /* Nesting to avoid readability issues inside a debugger. */
    union {
      X_SWIZZLES;
      XY_SWIZZLES;
      XYZ_SWIZZLES;
      XYZW_SWIZZLES;
    };
  };
};

#if COMPILER_GCC
#  pragma GCC diagnostic pop
#elif COMPILER_CLANG
#  pragma clang diagnostic pop
#elif COMPILER_MSVC
#  pragma warning(pop)
#endif

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

template<typename T, int Size>
struct VecBase : public vec_struct_base<T, Size, std::is_trivial_v<T>> {

  BLI_STATIC_ASSERT(alignof(T) <= sizeof(T),
                    "VecBase is not compatible with aligned type for now.");

/* Workaround issue with template BLI_ENABLE_IF((Size == 2)) not working. */
#define BLI_ENABLE_IF_VEC(_size, _test) int S = _size, BLI_ENABLE_IF((S _test))

  static constexpr int type_length = Size;

  using base_type = T;
  using uint_type = VecBase<as_uint_type<T>, Size>;

  VecBase() = default;

#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-copy"
#endif

  /** Make assignment on swizzle result an error. */
  VecBase &operator=(const VecBase &) & = default;

#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

  template<BLI_ENABLE_IF_VEC(Size, > 1)> explicit VecBase(T value)
  {
    for (int i = 0; i < Size; i++) {
      (*this)[i] = value;
    }
  }

  template<typename U, BLI_ENABLE_IF((std::is_convertible_v<U, T>))>
  explicit VecBase(U value) : VecBase(T(value))
  {
  }

  template<BLI_ENABLE_IF_VEC(Size, == 1)> constexpr VecBase(T _x)
  {
    this->x = _x;
  }

  template<BLI_ENABLE_IF_VEC(Size, == 2)> constexpr VecBase(T _x, T _y)
  {
    this->x = _x;
    this->y = _y;
  }

  template<BLI_ENABLE_IF_VEC(Size, == 3)> constexpr VecBase(T _x, T _y, T _z)
  {
    this->x = _x;
    this->y = _y;
    this->z = _z;
  }

  template<BLI_ENABLE_IF_VEC(Size, == 4)> constexpr VecBase(T _x, T _y, T _z, T _w)
  {
    this->x = _x;
    this->y = _y;
    this->z = _z;
    this->w = _w;
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

  /**
   * Prevent up-cast of dimensions (creating a bigger vector initialized with data
   * from a smaller one) by deleting all copy constructors accepting smaller vectors
   * as source.
   */
  template<typename U, int OtherSize, BLI_ENABLE_IF(OtherSize < Size)>
  VecBase(const VecBase<U, OtherSize> &other) = delete;

  /** Masking. */

  template<typename U, int OtherSize, BLI_ENABLE_IF(OtherSize > Size)>
  explicit VecBase(const VecBase<U, OtherSize> &other)
  {
    for (int i = 0; i < Size; i++) {
      (*this)[i] = T(other[i]);
    }
  }

#undef BLI_ENABLE_IF_VEC

  /** Conversion from pointers (from C-style vectors). */

  /* False positive warning with GCC: it sees array access like [3] but
   * input is only a 3-element array. But it fails to realize that the
   * [3] access is within "if constexpr (Size == 4)" check already. */
#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Warray-bounds"
#endif

  VecBase(const T *ptr)
  {
    BLI_UNROLL_MATH_VEC_OP_INIT_INDEX(ptr);
  }

  template<typename U, BLI_ENABLE_IF((std::is_convertible_v<U, T>))> explicit VecBase(const U *ptr)
  {
    BLI_UNROLL_MATH_VEC_OP_INIT_INDEX(ptr);
  }

  VecBase(const T (*ptr)[Size]) : VecBase(static_cast<const T *>(ptr[0])) {}

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

  /** Conversion from other vector types. */

  template<typename U> explicit VecBase(const VecBase<U, Size> &vec)
  {
    BLI_UNROLL_MATH_VEC_OP_INIT_VECTOR(vec);
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
    BLI_UNROLL_MATH_VEC_OP_VEC_VEC(+, a, b);
  }

  friend VecBase operator+(const VecBase &a, const T &b)
  {
    BLI_UNROLL_MATH_VEC_OP_VEC_SCALAR(+, a, b);
  }

  friend VecBase operator+(const T &a, const VecBase &b)
  {
    BLI_UNROLL_MATH_VEC_OP_SCALAR_VEC(+, a, b);
  }

  VecBase &operator+=(const VecBase &b) &
  {
    BLI_UNROLL_MATH_VEC_OP_ASSIGN_VEC(+=, b);
  }

  VecBase &operator+=(const T &b) &
  {
    BLI_UNROLL_MATH_VEC_OP_ASSIGN_SCALAR(+=, b);
  }

  friend VecBase operator-(const VecBase &a)
  {
    BLI_UNROLL_MATH_VEC_OP_VEC(-, a);
  }

  friend VecBase operator-(const VecBase &a, const VecBase &b)
  {
    BLI_UNROLL_MATH_VEC_OP_VEC_VEC(-, a, b);
  }

  friend VecBase operator-(const VecBase &a, const T &b)
  {
    BLI_UNROLL_MATH_VEC_OP_VEC_SCALAR(-, a, b);
  }

  friend VecBase operator-(const T &a, const VecBase &b)
  {
    BLI_UNROLL_MATH_VEC_OP_SCALAR_VEC(-, a, b);
  }

  VecBase &operator-=(const VecBase &b) &
  {
    BLI_UNROLL_MATH_VEC_OP_ASSIGN_VEC(-=, b);
  }

  VecBase &operator-=(const T &b) &
  {
    BLI_UNROLL_MATH_VEC_OP_ASSIGN_SCALAR(-=, b);
  }

  friend VecBase operator*(const VecBase &a, const VecBase &b)
  {
    BLI_UNROLL_MATH_VEC_OP_VEC_VEC(*, a, b);
  }

  template<typename FactorT> friend VecBase operator*(const VecBase &a, FactorT b)
  {
    BLI_UNROLL_MATH_VEC_OP_VEC_SCALAR(*, a, b);
  }

  friend VecBase operator*(T a, const VecBase &b)
  {
    BLI_UNROLL_MATH_VEC_OP_SCALAR_VEC(*, a, b);
  }

  VecBase &operator*=(T b) &
  {
    BLI_UNROLL_MATH_VEC_OP_ASSIGN_SCALAR(*=, b);
  }

  VecBase &operator*=(const VecBase &b) &
  {
    BLI_UNROLL_MATH_VEC_OP_ASSIGN_VEC(*=, b);
  }

  friend VecBase operator/(const VecBase &a, const VecBase &b)
  {
    for (int i = 0; i < Size; i++) {
      BLI_assert(b[i] != T(0));
    }
    BLI_UNROLL_MATH_VEC_OP_VEC_VEC(/, a, b);
  }

  friend VecBase operator/(const VecBase &a, T b)
  {
    BLI_assert(b != T(0));
    BLI_UNROLL_MATH_VEC_OP_VEC_SCALAR(/, a, b);
  }

  friend VecBase operator/(T a, const VecBase &b)
  {
    for (int i = 0; i < Size; i++) {
      BLI_assert(b[i] != T(0));
    }
    BLI_UNROLL_MATH_VEC_OP_SCALAR_VEC(/, a, b);
  }

  VecBase &operator/=(T b) &
  {
    BLI_assert(b != T(0));
    BLI_UNROLL_MATH_VEC_OP_ASSIGN_SCALAR(/=, b);
  }

  VecBase &operator/=(const VecBase &b) &
  {
    for (int i = 0; i < Size; i++) {
      BLI_assert(b[i] != T(0));
    }
    BLI_UNROLL_MATH_VEC_OP_ASSIGN_VEC(/=, b);
  }

  /** Binary operators. */

  BLI_INT_OP(T) friend VecBase operator&(const VecBase &a, const VecBase &b)
  {
    BLI_UNROLL_MATH_VEC_OP_VEC_VEC(&, a, b);
  }

  BLI_INT_OP(T) friend VecBase operator&(const VecBase &a, T b)
  {
    BLI_UNROLL_MATH_VEC_OP_VEC_SCALAR(&, a, b);
  }

  BLI_INT_OP(T) friend VecBase operator&(T a, const VecBase &b)
  {
    BLI_UNROLL_MATH_VEC_OP_SCALAR_VEC(&, a, b);
  }

  BLI_INT_OP(T) VecBase &operator&=(T b) &
  {
    BLI_UNROLL_MATH_VEC_OP_ASSIGN_SCALAR(&=, b);
  }

  BLI_INT_OP(T) VecBase &operator&=(const VecBase &b) &
  {
    BLI_UNROLL_MATH_VEC_OP_ASSIGN_VEC(&=, b);
  }

  BLI_INT_OP(T) friend VecBase operator|(const VecBase &a, const VecBase &b)
  {
    BLI_UNROLL_MATH_VEC_OP_VEC_VEC(|, a, b);
  }

  BLI_INT_OP(T) friend VecBase operator|(const VecBase &a, T b)
  {
    BLI_UNROLL_MATH_VEC_OP_VEC_SCALAR(|, a, b);
  }

  BLI_INT_OP(T) friend VecBase operator|(T a, const VecBase &b)
  {
    BLI_UNROLL_MATH_VEC_OP_SCALAR_VEC(|, a, b);
  }

  BLI_INT_OP(T) VecBase &operator|=(T b) &
  {
    BLI_UNROLL_MATH_VEC_OP_ASSIGN_SCALAR(|=, b);
  }

  BLI_INT_OP(T) VecBase &operator|=(const VecBase &b) &
  {
    BLI_UNROLL_MATH_VEC_OP_ASSIGN_VEC(|=, b);
  }

  BLI_INT_OP(T) friend VecBase operator^(const VecBase &a, const VecBase &b)
  {
    BLI_UNROLL_MATH_VEC_OP_VEC_VEC(^, a, b);
  }

  BLI_INT_OP(T) friend VecBase operator^(const VecBase &a, T b)
  {
    BLI_UNROLL_MATH_VEC_OP_VEC_SCALAR(^, a, b);
  }

  BLI_INT_OP(T) friend VecBase operator^(T a, const VecBase &b)
  {
    BLI_UNROLL_MATH_VEC_OP_SCALAR_VEC(^, a, b);
  }

  BLI_INT_OP(T) VecBase &operator^=(T b) &
  {
    BLI_UNROLL_MATH_VEC_OP_ASSIGN_SCALAR(^=, b);
  }

  BLI_INT_OP(T) VecBase &operator^=(const VecBase &b) &
  {
    BLI_UNROLL_MATH_VEC_OP_ASSIGN_VEC(^=, b);
  }

  BLI_INT_OP(T) friend VecBase operator~(const VecBase &a)
  {
    BLI_UNROLL_MATH_VEC_OP_VEC(~, a);
  }

  /** Bit-shift operators. */

  BLI_INT_OP(T) friend VecBase operator<<(const VecBase &a, const VecBase &b)
  {
    BLI_UNROLL_MATH_VEC_OP_VEC_VEC(<<, a, b);
  }

  BLI_INT_OP(T) friend VecBase operator<<(const VecBase &a, T b)
  {
    BLI_UNROLL_MATH_VEC_OP_VEC_SCALAR(<<, a, b);
  }

  BLI_INT_OP(T) VecBase &operator<<=(T b) &
  {
    BLI_UNROLL_MATH_VEC_OP_ASSIGN_SCALAR(<<=, b);
  }

  BLI_INT_OP(T) VecBase &operator<<=(const VecBase &b) &
  {
    BLI_UNROLL_MATH_VEC_OP_ASSIGN_VEC(<<=, b);
  }

  BLI_INT_OP(T) friend VecBase operator>>(const VecBase &a, const VecBase &b)
  {
    BLI_UNROLL_MATH_VEC_OP_VEC_VEC(>>, a, b);
  }

  BLI_INT_OP(T) friend VecBase operator>>(const VecBase &a, T b)
  {
    BLI_UNROLL_MATH_VEC_OP_VEC_SCALAR(>>, a, b);
  }

  BLI_INT_OP(T) VecBase &operator>>=(T b) &
  {
    BLI_UNROLL_MATH_VEC_OP_ASSIGN_SCALAR(>>=, b);
  }

  BLI_INT_OP(T) VecBase &operator>>=(const VecBase &b) &
  {
    BLI_UNROLL_MATH_VEC_OP_ASSIGN_VEC(>>=, b);
  }

  /** Modulo operators. */

  BLI_INT_OP(T) friend VecBase operator%(const VecBase &a, const VecBase &b)
  {
    for (int i = 0; i < Size; i++) {
      BLI_assert(b[i] != T(0));
    }
    BLI_UNROLL_MATH_VEC_OP_VEC_VEC(%, a, b);
  }

  BLI_INT_OP(T) friend VecBase operator%(const VecBase &a, T b)
  {
    BLI_assert(b != 0);
    BLI_UNROLL_MATH_VEC_OP_VEC_SCALAR(%, a, b);
  }

  BLI_INT_OP(T) friend VecBase operator%(T a, const VecBase &b)
  {
    for (int i = 0; i < Size; i++) {
      BLI_assert(b[i] != T(0));
    }
    BLI_UNROLL_MATH_VEC_OP_SCALAR_VEC(%, a, b);
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

using char2 = blender::VecBase<int8_t, 2>;
using char3 = blender::VecBase<int8_t, 3>;
using char4 = blender::VecBase<int8_t, 4>;

using uchar2 = blender::VecBase<uint8_t, 2>;
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
using short4 = blender::VecBase<int16_t, 4>;

using ushort2 = VecBase<uint16_t, 2>;
using ushort3 = blender::VecBase<uint16_t, 3>;
using ushort4 = blender::VecBase<uint16_t, 4>;

using float1 = VecBase<float, 1>;
using float2 = VecBase<float, 2>;
using float3 = VecBase<float, 3>;
using float4 = VecBase<float, 4>;

using double2 = VecBase<double, 2>;
using double3 = VecBase<double, 3>;
using double4 = VecBase<double, 4>;

}  // namespace blender
