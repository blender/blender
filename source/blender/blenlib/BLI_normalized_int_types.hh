/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_math_vector.hh"

namespace blender {

namespace detail {
template<typename T, int CompLen, int... CompBitCounts> struct NormalizedIntVec;

template<typename T, int SizeX, int SizeY> struct NormalizedIntVec<T, 2, SizeX, SizeY> {
  using VecT = VecBase<float, 2>;
  using IntVecT = VecBase<T, 2>;
  constexpr static bool is_signed = std::is_signed<T>();
  constexpr static T x_max = (1 << (SizeX - int(is_signed))) - 1;
  constexpr static T y_max = (1 << (SizeY - int(is_signed))) - 1;
#if defined(_MSC_VER) && !defined(__clang__) && _MSC_VER >= 1944 && _MSC_VER < 1950
  // Workaround for MSVC 17.14 ICE: use constexpr to indirectly reference size arguments
  // Does not make a whole lot of sense, but it sidesteps the ICE.
  constexpr static T SizeX_workaround = SizeX;
  constexpr static T SizeY_workaround = SizeY;
  T x : SizeX_workaround;
  T y : SizeY_workaround;
#else
  T x : SizeX;
  T y : SizeY;
#endif
  NormalizedIntVec() = default;
  constexpr NormalizedIntVec(IntVecT value) : x(value.x), y(value.y) {}

  operator IntVecT() const
  {
    return IntVecT(x, y);
  }

  static constexpr VecT max()
  {
    return VecT(x_max, y_max);
  }

  static constexpr VecT min()
  {
    if (is_signed) {
      return VecT(-x_max, -y_max);
    }
    return VecT(0.0f, 0.0f);
  }
};

template<typename T, int SizeX, int SizeY, int SizeZ, int SizeW>
struct NormalizedIntVec<T, 4, SizeX, SizeY, SizeZ, SizeW> {
  using VecT = VecBase<float, 4>;
  using IntVecT = VecBase<T, 4>;
  constexpr static bool is_signed = std::is_signed<T>();
  constexpr static T x_max = (1 << (SizeX - int(is_signed))) - 1;
  constexpr static T y_max = (1 << (SizeY - int(is_signed))) - 1;
  constexpr static T z_max = (1 << (SizeZ - int(is_signed))) - 1;
  constexpr static T w_max = (1 << (SizeW - int(is_signed))) - 1;

#if defined(_MSC_VER) && !defined(__clang__) && _MSC_VER >= 1944 && _MSC_VER < 1950
  // Workaround for MSVC 17.14 ICE: use constexpr to indirectly reference size arguments
  // Does not make a whole lot of sense, but it sidesteps the ICE.
  constexpr static T SizeX_workaround = SizeX;
  constexpr static T SizeY_workaround = SizeY;
  constexpr static T SizeZ_workaround = SizeZ;
  constexpr static T SizeW_workaround = SizeW;
  T x : SizeX_workaround;
  T y : SizeY_workaround;
  T z : SizeZ_workaround;
  T w : SizeW_workaround;
#else
  T x : SizeX;
  T y : SizeY;
  T z : SizeZ;
  T w : SizeW;
#endif
  NormalizedIntVec() = default;
  constexpr NormalizedIntVec(IntVecT value) : x(value.x), y(value.y), z(value.z), w(value.w) {}

  operator IntVecT() const
  {
    return IntVecT(x, y, z, w);
  }

  static constexpr VecT max()
  {
    return VecT(x_max, y_max, z_max, w_max);
  }

  static constexpr VecT min()
  {
    if constexpr (is_signed) {
      return VecT(-x_max, -y_max, -z_max, -w_max);
    }
    return VecT(0.0f, 0.0f, 0.0f, 0.0f);
  }
};

}  // namespace detail

template<typename T, int CompLen, int... CompBitCounts>
  requires(std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t>)
struct NormalizedIntVecBase : detail::NormalizedIntVec<T, CompLen, CompBitCounts...> {
  using IntPacked = detail::NormalizedIntVec<T, CompLen, CompBitCounts...>;
  using typename IntPacked::IntVecT;
  using typename IntPacked::VecT;

  NormalizedIntVecBase() = default;

  NormalizedIntVecBase(IntVecT value) : IntPacked(value) {}

  /* Adding rounding would be the standard compliant conversion.
   * But this would introduce perf regression. */
  NormalizedIntVecBase(VecT val)
      : IntPacked(IntVecT(math::clamp(val * IntPacked::max(), IntPacked::min(), IntPacked::max())))
  {
  }

  operator VecT() const
  {
    return VecT(IntVecT(*this)) / IntPacked::max();
  }
};

using char4_norm = NormalizedIntVecBase<int32_t, 4, 8, 8, 8, 8>;
using uchar4_norm = NormalizedIntVecBase<uint32_t, 4, 8, 8, 8, 8>;
using short2_norm = NormalizedIntVecBase<int32_t, 2, 16, 16>;
using ushort2_norm = NormalizedIntVecBase<uint32_t, 2, 16, 16>;
using short4_norm = NormalizedIntVecBase<int32_t, 4, 16, 16, 16, 16>;
using ushort4_norm = NormalizedIntVecBase<uint32_t, 4, 16, 16, 16, 16>;
using int1010102_norm = NormalizedIntVecBase<int32_t, 4, 10, 10, 10, 2>;
using uint1010102_norm = NormalizedIntVecBase<uint32_t, 4, 10, 10, 10, 2>;

}  // namespace blender
