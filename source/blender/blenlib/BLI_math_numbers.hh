/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * Contains the same definitions as the C++20 <numbers> header and will be replaced when we switch.
 */

#include <type_traits>

#include "BLI_utildefines.h"

namespace blender::math::numbers {

template<typename T, BLI_ENABLE_IF((std::is_floating_point_v<T>))>
inline constexpr T e_v = 2.718281828459045235360287471352662498L;

/* log_2 e */
template<typename T, BLI_ENABLE_IF((std::is_floating_point_v<T>))>
inline constexpr T log2e_v = 1.442695040888963407359924681001892137L;

/* log_10 e */
template<typename T, BLI_ENABLE_IF((std::is_floating_point_v<T>))>
inline constexpr T log10e_v = 0.434294481903251827651128918916605082L;

/* pi */
template<typename T, BLI_ENABLE_IF((std::is_floating_point_v<T>))>
inline constexpr T pi_v = 3.141592653589793238462643383279502884L;

/* 1/pi */
template<typename T, BLI_ENABLE_IF((std::is_floating_point_v<T>))>
inline constexpr T inv_pi_v = 0.318309886183790671537767526745028724L;

/* 1/sqrt(pi) */
template<typename T, BLI_ENABLE_IF((std::is_floating_point_v<T>))>
inline constexpr T inv_sqrtpi_v = 0.564189583547756286948079451560772586L;

/* log_e 2 */
template<typename T, BLI_ENABLE_IF((std::is_floating_point_v<T>))>
inline constexpr T ln2_v = 0.693147180559945309417232121458176568L;

/* log_e 10 */
template<typename T, BLI_ENABLE_IF((std::is_floating_point_v<T>))>
inline constexpr T ln10_v = 2.302585092994045684017991454684364208L;

/* sqrt(2) */
template<typename T, BLI_ENABLE_IF((std::is_floating_point_v<T>))>
inline constexpr T sqrt2_v = 1.414213562373095048801688724209698079L;

/* sqrt(3) */
template<typename T, BLI_ENABLE_IF((std::is_floating_point_v<T>))>
inline constexpr T sqrt3_v = 1.732050807568877293527446341505872367L;

/* 1/sqrt(3) */
template<typename T, BLI_ENABLE_IF((std::is_floating_point_v<T>))>
inline constexpr T inv_sqrt3_v = 0.577350269189625764509148780501957456L;

/* The Euler-Mascheroni constant */
template<typename T, BLI_ENABLE_IF((std::is_floating_point_v<T>))>
inline constexpr T egamma_v = 0.577215664901532860606512090082402431L;

/* The golden ratio, (1+sqrt(5))/2 */
template<typename T, BLI_ENABLE_IF((std::is_floating_point_v<T>))>
inline constexpr T phi_v = 1.618033988749894848204586834365638118L;

inline constexpr double e = e_v<double>;
inline constexpr double log2e = log2e_v<double>;
inline constexpr double log10e = log10e_v<double>;
inline constexpr double pi = pi_v<double>;
inline constexpr double inv_pi = inv_pi_v<double>;
inline constexpr double inv_sqrtpi = inv_sqrtpi_v<double>;
inline constexpr double ln2 = ln2_v<double>;
inline constexpr double ln10 = ln10_v<double>;
inline constexpr double sqrt2 = sqrt2_v<double>;
inline constexpr double sqrt3 = sqrt3_v<double>;
inline constexpr double inv_sqrt3 = inv_sqrt3_v<double>;
inline constexpr double egamma = egamma_v<double>;
inline constexpr double phi = phi_v<double>;

}  // namespace blender::math::numbers
