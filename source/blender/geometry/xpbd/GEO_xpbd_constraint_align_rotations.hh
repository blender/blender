/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_quaternion.hh"
#include "BLI_math_vector.hh"

namespace blender::xpbd {

struct AlignRotationsConstraintResult {
  float4 delta_lambda;
  math::Quaternion offset0 = math::Quaternion(0.0f, 0.0f, 0.0f, 0.0f);
  math::Quaternion offset1 = math::Quaternion(0.0f, 0.0f, 0.0f, 0.0f);
  float residual_error_squared = 0.0f;
};

inline AlignRotationsConstraintResult evaluate_align_rotations_constraint(
    const math::Quaternion &r0,
    const math::Quaternion &r1,
    const float3 &inertia0,
    const float3 &inertia1,
    const math::Quaternion &rest_rotation,
    const float compliance_term,
    const float4 &lambda_prev)
{
  const float inv_lumped_inertia0 = math::safe_rcp(0.5f * (inertia0.x + inertia0.y + inertia0.z));
  const float inv_lumped_inertia1 = math::safe_rcp(0.5f * (inertia1.x + inertia1.y + inertia1.z));
  if (inv_lumped_inertia0 == 0.0f && inv_lumped_inertia1 == 0.0f) {
    /* Everything is pinned, so the constraint can't do anything. */
    return {};
  }

  const float4 rest_rot_f = float4(rest_rotation);

  /* Note In "Position and Orientation Based Cosserat Rods" (Kugelstadt, Schoemer) the W
   * component of the Darboux vector is ignored. In "Sag-Free Initialization for Strand-Based
   * Hybrid Hair Simulation" (Hsu et al.) it is included to improve stability in cases where the
   * hair is bent at nearly 180 degrees. */
  const math::Quaternion &rot_diff = math::invert_normalized(r0) * r1;
  const float4 rot_diff_f = float4(rot_diff);

  const float4 residual_neg = rot_diff_f - rest_rot_f;
  const float4 residual_pos = rot_diff_f + rest_rot_f;
  const float4 residual = math::length_squared(residual_neg) < math::length_squared(residual_pos) ?
                              residual_neg :
                              residual_pos;

  const float error_squared = math::length_squared(residual + compliance_term * lambda_prev);

  const float4 delta_lambda = (-residual - compliance_term * lambda_prev) /
                              (inv_lumped_inertia0 + inv_lumped_inertia1 + compliance_term);

  const math::Quaternion offset0 = r1 * math::conjugate(
                                            math::Quaternion(delta_lambda * inv_lumped_inertia0));
  const math::Quaternion offset1 = r0 * math::Quaternion(delta_lambda * inv_lumped_inertia1);

  return {delta_lambda, offset0, offset1, error_squared};
}

}  // namespace blender::xpbd
