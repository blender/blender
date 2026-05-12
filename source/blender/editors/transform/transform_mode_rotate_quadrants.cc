/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 *
 * Used to apply 90 degree rotations directly, without imprecision from radian angles,
 * see #150900.
 */

#include <optional>

#include "BLI_math_base.hh"
#include "BLI_math_rotation.h"

#include "ED_numinput.hh"

#include "transform.hh"
#include "transform_snap.hh"

#include "transform_mode_rotate_quadrants.hh"

namespace blender::ed::transform {

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

/**
 * Return the quadrant index (0..3) if a degree value is an integer multiple of 90,
 * or #std::nullopt otherwise.
 */
static std::optional<int> angle_to_quadrant_or_null_from_degrees(const double degrees)
{
  if (!ED_numinput_double_is_int(degrees)) {
    return std::nullopt;
  }
  const int64_t deg = int64_t(degrees);
  if ((deg % 90) != 0) {
    return std::nullopt;
  }
  return int(math::mod_periodic(deg / 90, int64_t(4)));
}

/**
 * Return the quadrant index (0..3) if the numeric input is a multiple of 90 degrees,
 * or #std::nullopt otherwise.
 * Uses the pre-unit-scale value stored in #NumInput.val_no_units,
 * and #NUM_INT_INPUT_VALUE to confirm it was an integer.
 */
static std::optional<int> angle_to_quadrant_or_null_from_numinput(const NumInput *n)
{
  if (!(n->val_flag[0] & NUM_INT_INPUT_VALUE)) {
    return std::nullopt;
  }
  return angle_to_quadrant_or_null_from_degrees(n->val_no_units[0]);
}

/**
 * Return the quadrant index (0..3) if a snapped angle (radians) corresponds
 * to a multiple of 90 degrees, or #std::nullopt otherwise.
 *
 * \param snap_increment: The snap increment in radians (e.g. DEG2RAD(5)).
 * Uses the increment to recover the exact integer degree value via step count.
 */
static std::optional<int> angle_to_quadrant_or_null_from_radians(const float angle,
                                                                 const float snap_increment)
{
  if (angle == 0.0f) {
    return angle_to_quadrant_or_null_from_degrees(0.0);
  }
  /* Snap increment is zero when snapping is not active, avoid division by zero below. */
  if (snap_increment == 0.0f) {
    return std::nullopt;
  }
  /* Recover the integer step count and degree increment. */
  const double increment_deg = round(RAD2DEG(double(snap_increment)));
  if (!ED_numinput_double_is_int(increment_deg) || increment_deg == 0.0) {
    return std::nullopt;
  }
  const int64_t steps = int64_t(roundf(angle / snap_increment));
  return angle_to_quadrant_or_null_from_degrees(double(steps * int64_t(increment_deg)));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Functions
 * \{ */

std::optional<int> transform_angle_to_quadrant_or_null(const TransInfo *t,
                                                       const bool is_large_rotation_limited)
{
  /* `large_rotation_limit` uses float `fmodf` which loses precision for very large angles,
   * the resulting radian value no longer corresponds to the original degree input. */
  if (is_large_rotation_limited) {
    return std::nullopt;
  }
  if (hasNumInput(&t->num)) {
    return angle_to_quadrant_or_null_from_numinput(&t->num);
  }
  /* Only use snap quadrant when increment snap was used (not geometry snap). */
  if (transform_snap_is_active(t) && !validSnap(t)) {
    return angle_to_quadrant_or_null_from_radians(t->values_final[0],
                                                  transform_snap_increment_get(t));
  }
  return std::nullopt;
}

void axis_angle_normalized_to_mat3_with_quadrant(float mat[3][3],
                                                 const float axis[3],
                                                 const float angle,
                                                 const std::optional<int> quadrant)
{
  if (quadrant.has_value()) {
    BLI_assert(*quadrant >= 0 && *quadrant < 4);
    const float sin_lut[4] = {0.0f, 1.0f, 0.0f, -1.0f};
    const float cos_lut[4] = {1.0f, 0.0f, -1.0f, 0.0f};
    axis_angle_normalized_to_mat3_ex(mat, axis, sin_lut[*quadrant], cos_lut[*quadrant]);
  }
  else {
    axis_angle_normalized_to_mat3(mat, axis, angle);
  }
}

/** \} */

}  // namespace blender::ed::transform
