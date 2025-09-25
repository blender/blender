/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include <algorithm>
#include <cmath>
#include <type_traits>

#include "BLI_math_numbers.hh"
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

template<typename T> inline T sign(const T &a)
{
  return (T(0) < a) - (a < T(0));
}

template<typename T> inline T min(const T &a, const T &b)
{
  static_assert(std::is_arithmetic_v<T>, "math::min on non-arithmetic type is likely unintended");
  return std::min(a, b);
}

template<typename T> inline T max(const T &a, const T &b)
{
  static_assert(std::is_arithmetic_v<T>, "math::max on non-arithmetic type is likely unintended");
  return std::max(a, b);
}

template<typename T> inline void max_inplace(T &a, const T &b)
{
  static_assert(std::is_arithmetic_v<T>,
                "math::max_inplace on non-arithmetic type is likely unintended");
  a = std::max(a, b);
}

template<typename T> inline void min_inplace(T &a, const T &b)
{
  static_assert(std::is_arithmetic_v<T>,
                "math::min_inplace on non-arithmetic type is likely unintended");
  a = std::min(a, b);
}

template<typename T> inline T clamp(const T &a, const T &min, const T &max)
{
  return std::clamp(a, min, max);
}

template<typename T> inline T step(const T &edge, const T &value)
{
  return value < edge ? 0 : 1;
}

template<typename T> inline T mod(const T &a, const T &b)
{
  return std::fmod(a, b);
}

template<typename T> inline T safe_mod(const T &a, const T &b)
{
  return (b != 0) ? std::fmod(a, b) : 0;
}

template<typename T> inline T floored_mod(const T &a, const T &b)
{
  return a - std::floor(a / b) * b;
}

template<typename T> inline T safe_floored_mod(const T &a, const T &b)
{
  return (b != 0) ? a - std::floor(a / b) * b : 0;
}

template<typename T> inline void min_max(const T &value, T &min, T &max)
{
  static_assert(std::is_arithmetic_v<T>,
                "math::min_max on non-arithmetic type is likely unintended");
  min = std::min(value, min);
  max = std::max(value, max);
}

template<typename T> inline T safe_divide(const T &a, const T &b)
{
  return (b != 0) ? a / b : T(0.0f);
}

template<typename T> inline T floor(const T &a)
{
  return std::floor(a);
}

template<typename T> inline T round(const T &a)
{
  return std::round(a);
}

/**
 * Repeats the saw-tooth pattern even on negative numbers.
 * ex: `mod_periodic(-3, 4) = 1`, `mod(-3, 4)= -3`. This will cause undefined behavior for negative
 * b.
 */
template<typename T> inline T mod_periodic(const T &a, const T &b)
{
  BLI_assert(b != 0);
  if constexpr (std::is_integral_v<T>) {
    BLI_assert(std::numeric_limits<T>::max() - math::abs(a) >= b);
    return ((a % b) + b) % b;
  }

  return a - (b * math::floor(a / b));
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

/* Inverse value.
 * If the input is zero the output is NaN. */
template<typename T> inline T rcp(const T &a)
{
  static_assert(!std::is_integral_v<T>, "T must not be an integral type.");
  return T(1) / a;
}

/* Inverse value.
 * If the input is zero the output is zero. */
template<typename T> inline T safe_rcp(const T &a)
{
  static_assert(!std::is_integral_v<T>, "T must be not be an integral type.");
  return a ? T(1) / a : T(0);
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

template<typename T> inline T pow(const T &x, const T &power)
{
  return std::pow(x, power);
}

template<typename T> inline T safe_pow(const T &x, const T &power)
{
  return (x < 0 || (x == 0 && power <= 0)) ? x : std::pow(x, power);
}

template<typename T> inline T fallback_pow(const T &x, const T &power, const T &fallback)
{
  return (x < 0 || (x == 0 && power <= 0)) ? fallback : std::pow(x, power);
}

template<typename T> inline T square(const T &a)
{
  return a * a;
}

template<typename T> inline T cube(const T &a)
{
  return a * a * a;
}

template<typename T> inline T exp(const T &x)
{
  return std::exp(x);
}

template<typename T> inline T safe_acos(const T &a)
{
  if (UNLIKELY(a <= T(-1))) {
    return T(numbers::pi);
  }
  if (UNLIKELY(a >= T(1))) {
    return T(0);
  }
  return math::acos((a));
}

/** Faster/approximate version of #safe_acos. Max error 4.51803e-5 (0.00258 degrees). */
inline float safe_acos_approx(float x)
{
  const float f = std::abs(x);
  /* Clamp and crush denormals. */
  const float m = (f < 1.0f) ? 1.0f - (1.0f - f) : 1.0f;
  /* Based on http://www.pouet.net/topic.php?which=9132&page=2
   * 85% accurate (ULP 0)
   * Examined 2130706434 values of `acos`:
   *   15.2000597 avg ULP diff, 4492 max ULP, 4.51803e-05 max error // without "denormal crush".
   * Examined 2130706434 values of `acos`:
   *   15.2007108 avg ULP diff, 4492 max ULP, 4.51803e-05 max error // with "denormal crush".
   */
  const float a = std::sqrt(1.0f - m) *
                  (1.5707963267f + m * (-0.213300989f + m * (0.077980478f + m * -0.02164095f)));
  return x < 0.0f ? float(numbers::pi) - a : a;
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
  if constexpr (std::is_integral_v<T>) {
    /** See std::midpoint from C++20. */
    using Unsigned = std::make_unsigned_t<T>;
    int sign = 1;
    Unsigned smaller = a;
    Unsigned larger = b;
    if (a > b) {
      sign = -1;
      smaller = b;
      larger = a;
    }
    return a + sign * T(Unsigned(larger - smaller) / 2);
  }
  else {
    return (a + b) * T(0.5);
  }
}

}  // namespace blender::math
