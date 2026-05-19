/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Defines an abstraction around various structs to modify their transform properties via a
 * unified API.
 */
#pragma once

#include "BLI_array.hh"
#include "BLI_span.hh"

#include "DNA_action_types.h"

#include "RNA_types.hh"

namespace blender {

struct bPoseChannel;
struct ID;

namespace ed {

/**
 * Used to limit the modification of properties to certain axes.
 */
enum AxisMutable : int8_t {
  AXIS_MUTABLE_X = 1 << 0,
  AXIS_MUTABLE_Y = 1 << 1,
  AXIS_MUTABLE_Z = 1 << 2,
  AXIS_MUTABLE_ALL = AXIS_MUTABLE_X | AXIS_MUTABLE_Y | AXIS_MUTABLE_Z,
  /* There is currently no support for a W axis.
   * This was already the case when porting this enum from the pose slide code. */
};
ENUM_OPERATORS(AxisMutable);

/**
 * Interpolate the values linearly based on `factor` and returns a new Array. Asserts that both
 * spans are the same length. With the factor at `0` the values will match `a`.
 */
Array<float> property_interpolated(Span<float> a, Span<float> b, float factor);

}  // namespace ed
}  // namespace blender
