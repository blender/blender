/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

#pragma once

/** \file
 * \ingroup bli
 */

#include <cmath>
#include <type_traits>

#include "BLI_color.hh"
#include "BLI_math_base.hh"

namespace blender::math {

inline ColorGeometry4f interpolate(const ColorGeometry4f &a,
                                   const ColorGeometry4f &b,
                                   const float t)
{
  return {math::interpolate(a.r, b.r, t),
          math::interpolate(a.g, b.g, t),
          math::interpolate(a.b, b.b, t),
          math::interpolate(a.a, b.a, t)};
}

}  // namespace blender::math
