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
#include "BLI_math_vec_types.hh"
#include "BLI_utildefines.h"

#ifdef WITH_GMP
#  include "BLI_math_mpq.hh"
#endif

namespace blender::math {

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

template<typename T> inline T clamp(const T &a, const T &min, const T &max)
{
  return std::clamp(a, min, max);
}

template<typename T, BLI_ENABLE_IF((is_math_float_type<T>))> inline T mod(const T &a, const T &b)
{
  return std::fmod(a, b);
}

template<typename T, BLI_ENABLE_IF((is_math_float_type<T>))>
inline T safe_mod(const T &a, const T &b)
{
  return (b != 0) ? std::fmod(a, b) : 0;
}

template<typename T> inline void min_max(const T &value, T &min, T &max)
{
  min = math::min(value, min);
  max = math::max(value, max);
}

template<typename T, BLI_ENABLE_IF((is_math_float_type<T>))>
inline T safe_divide(const T &a, const T &b)
{
  return (b != 0) ? a / b : T(0.0f);
}

template<typename T, BLI_ENABLE_IF((is_math_float_type<T>))> inline T floor(const T &a)
{
  return std::floor(a);
}

template<typename T, BLI_ENABLE_IF((is_math_float_type<T>))> inline T ceil(const T &a)
{
  return std::ceil(a);
}

template<typename T, BLI_ENABLE_IF((is_math_float_type<T>))> inline T fract(const T &a)
{
  return a - std::floor(a);
}

template<typename T, BLI_ENABLE_IF((is_math_float_type<T>))>
inline T interpolate(const T &a, const T &b, const T &t)
{
  return a * (1 - t) + b * t;
}

template<typename T, BLI_ENABLE_IF((is_math_float_type<T>))>
inline T midpoint(const T &a, const T &b)
{
  return (a + b) * T(0.5);
}

}  // namespace blender::math
