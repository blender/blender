/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include <cmath>
#include <type_traits>

#include "BLI_color.hh"
#include "BLI_math_base.hh"

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

}  // namespace blender::math
