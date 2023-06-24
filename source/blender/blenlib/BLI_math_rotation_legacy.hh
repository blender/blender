/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_math_vector_types.hh"

namespace blender::math {

/**
 * Rotate the unit-length \a direction around the unit-length \a axis by the \a angle.
 */
float3 rotate_direction_around_axis(const float3 &direction, const float3 &axis, float angle);

/**
 * Rotate any arbitrary \a vector around the \a center position, with a unit-length \a axis
 * and the specified \a angle.
 */
float3 rotate_around_axis(const float3 &vector,
                          const float3 &center,
                          const float3 &axis,
                          float angle);

}  // namespace blender::math
