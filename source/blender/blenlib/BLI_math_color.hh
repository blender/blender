/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_color_types.hh"
#include "BLI_math_base.hh"
#include "BLI_math_matrix_types.hh"

namespace blender::math {

template<eAlpha Alpha>
inline ColorSceneLinear4f<Alpha> interpolate(const ColorSceneLinear4f<Alpha> &a,
                                             const ColorSceneLinear4f<Alpha> &b,
                                             const float t)
{
  return {math::interpolate(a.r, b.r, t),
          math::interpolate(a.g, b.g, t),
          math::interpolate(a.b, b.b, t),
          math::interpolate(a.a, b.a, t)};
}

template<eAlpha Alpha>
inline ColorSceneLinearByteEncoded4b<Alpha> interpolate(
    const ColorSceneLinearByteEncoded4b<Alpha> &a,
    const ColorSceneLinearByteEncoded4b<Alpha> &b,
    const float t)
{
  return {math::interpolate(a.r, b.r, t),
          math::interpolate(a.g, b.g, t),
          math::interpolate(a.b, b.b, t),
          math::interpolate(a.a, b.a, t)};
}

float3 whitepoint_from_temp_tint(float temperature, float tint);

bool whitepoint_to_temp_tint(const float3 &white, float &temperature, float &tint);

/* Computes a matrix to perform chromatic adaption from a source white point (given in the form of
 * temperature and tint) to a target white point (given as its XYZ values).
 * The resulting matrix operates on XYZ values, the caller is responsible for RGB conversion. */
float3x3 chromatic_adaption_matrix(const float3 &from_XYZ, const float3 &to_XYZ);

}  // namespace blender::math
