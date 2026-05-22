/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GEO_xpbd_constraint_align_rotations.hh"
#include "GEO_xpbd_constraint_coloring_utils.hh"
#include "GEO_xpbd_constraint_set_templated.hh"

namespace blender::xpbd {

/** Aligns rotations of two consecutive rods based on a rest rotation. */
class RodBendAndTwistConstraintSet : public TemplatedConstraintSet<RodBendAndTwistConstraintSet> {
 private:
  /** Curves that are effected by this constraint set. Each curve is seen as one constraint. */
  IndexRange curves_range_;
  OffsetIndices<int> points_by_curve_;

  /** Indexed by point index. */
  Span<math::Quaternion> rest_rotations_;

  /** Indexed by `point_i - first_point_i_in_constraint_set`. */
  Span<float> compliances_;

  /* Scale factor for residual error. */
  float error_scale_;

  MutableSpan<float4> lambdas_;

 public:
  static constexpr StringRefNull debug_name = "Rod Bend and Twist";

  RodBendAndTwistConstraintSet(const int geo_i,
                               const IndexRange curves_range,
                               const OffsetIndices<int> points_by_curve,
                               const Span<math::Quaternion> rest_rotations,
                               const Span<float> compliances,
                               const float error_scale,
                               MutableSpan<float4> lambdas)
      : TemplatedConstraintSet<RodBendAndTwistConstraintSet>(curves_range.size(), {geo_i}),
        curves_range_(curves_range),
        points_by_curve_(points_by_curve),
        rest_rotations_(rest_rotations),
        compliances_(compliances),
        error_scale_(error_scale),
        lambdas_(lambdas)
  {
  }

  void reset_force(const int constraint_i) const
  {
    const int curve_i = curves_range_[constraint_i];
    const IndexRange points = points_by_curve_[curve_i];
    lambdas_.slice(points).fill(float4(0.0f));
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

    /* Could implement bilateral interleaving ordering for better stability. */
    /* Note that the last segment does not have this constraint, because the rotation of the last
     * point in the rod is meaningless.*/
    for (const int point_i0 : points.drop_back(2)) {
      const int point_i1 = point_i0 + 1;
      const float compliance = compliances_[point_i0 - first_point_i_in_constraint_set];
      const AlignRotationsConstraintResult result = evaluate_align_rotations_constraint(
          params.rotation(geo_i, point_i0),
          params.rotation(geo_i, point_i1),
          params.moment_of_inertia(geo_i, point_i0),
          params.moment_of_inertia(geo_i, point_i1),
          rest_rotations_[point_i0],
          compliance * params.compliance_term_factor,
          lambdas_[point_i0]);
      lambdas_[point_i0] += result.delta_lambda;
      updater.update_rotation(geo_i, point_i0, result.offset0);
      updater.update_rotation(geo_i, point_i1, result.offset1);
      updater.add_residual_error(geo_i, result.residual_error_squared * error_scale_);
    }
  }

  ConstraintColoring color_constraints(IndexMaskMemory & /*memory*/) const override
  {
    return color_constraints__all_independent(constraints_num_);
  }
};

}  // namespace blender::xpbd
