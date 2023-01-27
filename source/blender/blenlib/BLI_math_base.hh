/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

#pragma once

/** \file
 * \ingroup bli
 */

#include <algorithm>
#include <cmath>
#include <type_traits>

#include "BLI_math_base_safe.h"
#include "BLI_utildefines.h"

namespace blender::math {

template<typename T> inline constexpr bool is_math_float_type = std::is_floating_point_v<T>;
template<typename T> inline constexpr bool is_math_integral_type = std::is_integral_v<T>;

template<typename T> inline bool is_zero(const T &a)
{
  return a == T(0);
}

template<typename T> inline bool is_any_zero(const T &a)
{
  return is_zero(a);
}

template<typename T> inline T abs(const T &a)
{
  return std::abs(a);
}

template<typename T> inline T min(const T &a, const T &b)
{
  return std::min(a, b);
}

template<typename T> inline T max(const T &a, const T &b)
{
  return std::max(a, b);
}

template<typename T> inline void max_inplace(T &a, const T &b)
{
  a = math::max(a, b);
}

template<typename T> inline void min_inplace(T &a, const T &b)
{
  a = math::min(a, b);
}

template<typename T> inline T clamp(const T &a, const T &min, const T &max)
{
  return std::clamp(a, min, max);
}

template<typename T> inline T mod(const T &a, const T &b)
{
  return std::fmod(a, b);
}

template<typename T> inline T safe_mod(const T &a, const T &b)
{
  return (b != 0) ? std::fmod(a, b) : 0;
}

template<typename T> inline void min_max(const T &value, T &min, T &max)
{
  min = math::min(value, min);
  max = math::max(value, max);
}

template<typename T> inline T safe_divide(const T &a, const T &b)
{
  return (b != 0) ? a / b : T(0.0f);
}

template<typename T> inline T floor(const T &a)
{
  return std::floor(a);
}

template<typename T> inline T ceil(const T &a)
{
  return std::ceil(a);
}

template<typename T> inline T distance(const T &a, const T &b)
{
  return std::abs(a - b);
}

template<typename T> inline T fract(const T &a)
{
  return a - std::floor(a);
}

template<typename T> inline T sqrt(const T &a)
{
  return std::sqrt(a);
}

template<typename T> inline T cos(const T &a)
{
  return std::cos(a);
}

template<typename T> inline T sin(const T &a)
{
  return std::sin(a);
}

template<typename T> inline T tan(const T &a)
{
  return std::tan(a);
}

template<typename T> inline T acos(const T &a)
{
  return std::acos(a);
}

template<typename T> inline T asin(const T &a)
{
  return std::asin(a);
}

template<typename T> inline T atan(const T &a)
{
  return std::atan(a);
}

template<typename T> inline T atan2(const T &y, const T &x)
{
  return std::atan2(y, x);
}

template<typename T> inline T hypot(const T &y, const T &x)
{
  return std::hypot(y, x);
}

template<typename T, typename FactorT>
inline T interpolate(const T &a, const T &b, const FactorT &t)
{
  auto result = a * (1 - t) + b * t;
  if constexpr (std::is_integral_v<T> && std::is_floating_point_v<FactorT>) {
    result = std::round(result);
  }
  return result;
}

template<typename T> inline T midpoint(const T &a, const T &b)
{
  auto result = (a + b) * T(0.5);
  if constexpr (std::is_integral_v<T>) {
    result = std::round(result);
  }
  return result;
}

}  // namespace blender::math
