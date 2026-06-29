/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_base_c.hh"

#include "GEO_xpbd_constraint_coloring_utils.hh"
#include "GEO_xpbd_constraint_set_templated.hh"

namespace blender::xpbd {

struct RodStretchAndShearConstraintResult {
  float3 delta_lambda_pos;
  float3 delta_lambda_rot;
  float3 offset0 = float3(0.0f);
  float3 offset1 = float3(0.0f);
  math::Quaternion offset_rot = math::Quaternion(0.0f, 0.0f, 0.0f, 0.0f);
  float residual_error_squared = 0.0f;
};

inline RodStretchAndShearConstraintResult evaluate_rod_stretch_and_shear_constraint(
    const float3 &p0,
    const float3 &p1,
    const math::Quaternion &rot,
    const float inv_m0,
    const float inv_m1,
    const float3 &inertia,
    const float rest_length,
    const float compliance_term,
    const float3 &lambda_pos_prev,
    const float3 &lambda_rot_prev)
{
  /* Lumped weight for the rotation influence. The higher the inertia, the lower the change of
   * the rotation should be compared to the change in point positions. */
  const float inv_lumped_inertia = math::safe_rcp(0.5f * (inertia.x + inertia.y + inertia.z));

  if (inv_m0 == 0.0f && inv_m1 == 0.0f && inv_lumped_inertia == 0.0f) {
    /* Everything is pinned, so the constraint can't do anything. */
    return {};
  }

  /* TODO The positional and rotational parts use different residuals to avoid errors when the
   * current segment length deviates too much from the rest length. The rotational offset uses
   * the residual as the angle of rotation which becomes larger with stretching. To avoid
   * instabilities the rotation residual is computed relative to the current length.
   * This should be cleaned up and optimized if possible. */

  /* Current non-normalized tangent of the rod. */
  const float3 p_diff = p1 - p0;
  const float p_len = math::length(p_diff);
  /* Expected non-normalized tangent of the rod based on the rotation. */
  const float3 forward_rest = math::transform_point(rot, float3(0.0f, 0.0f, rest_length));
  const float3 forward = math::transform_point(rot, float3(0.0f, 0.0f, p_len));
  /* How much the rod is stretched and sheared. */
  const float3 residual_pos = p_diff - forward_rest;
  const float3 residual_rot = p_diff - forward;

  const float error_squared = math::length_squared(residual_pos +
                                                   compliance_term * lambda_pos_prev);

  /* Based on "Position and Orientation Based Cosserat Rods" (Kugelstadt, Schömer, 2016). */
  const float weight_sum = inv_m0 + inv_m1 + 4.0f * inv_lumped_inertia * pow2f(rest_length);
  const float weight_sum_rot = inv_m0 + inv_m1 + 4.0f * inv_lumped_inertia * pow2f(p_len);
  const float3 delta_lambda_pos = (-residual_pos - compliance_term * lambda_pos_prev) /
                                  (weight_sum + compliance_term);
  const float3 delta_lambda_rot = (-residual_rot - compliance_term * lambda_rot_prev) /
                                  (weight_sum_rot + compliance_term);

  RodStretchAndShearConstraintResult result;
  result.delta_lambda_pos = delta_lambda_pos;
  result.delta_lambda_rot = delta_lambda_rot;
  result.offset0 = -delta_lambda_pos * inv_m0;
  result.offset1 = delta_lambda_pos * inv_m1;
  result.offset_rot = math::Quaternion(0.0f, -delta_lambda_rot * inv_lumped_inertia * p_len) *
                      rot * math::Quaternion(0, 0, 0, -1);
  result.residual_error_squared = error_squared;
  return result;
}

/**
 * Considers a single rod at a time. Tries to enforce that the rotation of the rod is aligned with
 * the actual tangent of the rod. If it is misaligned, it moves the start position, end position
 * and rotation of the frame. At the same time, it enforces a certain length.
 */
class RodStretchAndShearConstraintSet
    : public TemplatedConstraintSet<RodStretchAndShearConstraintSet> {
 private:
  /** Curves that are effected by this constraint set. Each curve is seen as one constraint. */
  IndexRange curves_range_;
  OffsetIndices<int> points_by_curve_;

  /** Indexed by segment-end point index. */
  Span<float> rest_lengths_;
  MutableSpan<float3> lambdas_pos_;
  MutableSpan<float3> lambdas_rot_;

  /** Indexed by `point_i - first_point_i_in_constraint_set`. */
  Span<float> compliances_;

  /* Scale factor for residual error. */
  float error_scale_;

 public:
  static constexpr StringRefNull debug_name = "Rod Stretch and Shear";

  RodStretchAndShearConstraintSet(const int geo_i,
                                  const IndexRange curves_range,
                                  const OffsetIndices<int> points_by_curve,
                                  const Span<float> rest_lengths,
                                  const Span<float> compliances,
                                  const float error_scale,
                                  MutableSpan<float3> lambdas_pos,
                                  MutableSpan<float3> lambdas_rot)
      : TemplatedConstraintSet<RodStretchAndShearConstraintSet>(curves_range.size(), {geo_i}),
        curves_range_(curves_range),
        points_by_curve_(points_by_curve),
        rest_lengths_(rest_lengths),
        lambdas_pos_(lambdas_pos),
        lambdas_rot_(lambdas_rot),
        compliances_(compliances),
        error_scale_(error_scale)
  {
  }

  void reset_force(const int constraint_i) const
  {
    const int curve_i = curves_range_[constraint_i];
    const IndexRange points = points_by_curve_[curve_i];
    lambdas_pos_.slice(points).fill(float3(0.0f));
    lambdas_rot_.slice(points).fill(float3(0.0f));
  }

  template<typename UpdaterT>
  void solve_single(const ConstraintSetParams &params,
                    UpdaterT &updater,
                    const int constraint_i) const
  {
    const int curve_i = curves_range_[constraint_i];
    const IndexRange points = points_by_curve_[curve_i];
    const int geo_i = affected_geo_indices_[0];
    const int first_point_i_in_constraint_set = points_by_curve_[curves_range_.first()].first();

    /* Could try implementing bilateral interleaving ordering for better stability. */
    for (const int point_i0 : points.drop_back(1)) {
      const int point_i1 = point_i0 + 1;
      const float compliance = compliances_[point_i0 - first_point_i_in_constraint_set];
      const RodStretchAndShearConstraintResult result = evaluate_rod_stretch_and_shear_constraint(
          params.position(geo_i, point_i0),
          params.position(geo_i, point_i1),
          params.rotation(geo_i, point_i0),
          params.inv_mass(geo_i, point_i0),
          params.inv_mass(geo_i, point_i1),
          params.moment_of_inertia(geo_i, point_i0),
          rest_lengths_[point_i1],
          compliance * params.compliance_term_factor,
          lambdas_pos_[point_i1],
          lambdas_rot_[point_i1]);
      lambdas_pos_[point_i1] += result.delta_lambda_pos;
      lambdas_rot_[point_i1] += result.delta_lambda_rot;
      updater.update_position(geo_i, point_i0, result.offset0);
      updater.update_position(geo_i, point_i1, result.offset1);
      updater.update_rotation(geo_i, point_i0, result.offset_rot);
      updater.add_residual_error(geo_i, result.residual_error_squared * error_scale_);
    }
  }

  ConstraintColoring color_constraints(LinearAllocator<> & /*memory*/) const override
  {
    return color_constraints__all_independent(constraints_num_);
  }
};

}  // namespace blender::xpbd
