/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GEO_xpbd_constraint_set_templated.hh"

namespace blender::xpbd {

class FrictionFaceConstraintSet
    : public TemplatedVelocityConstraintSet<FrictionFaceConstraintSet> {
 private:
  int geo_i_;
  /* Constraint index for each point. */
  Span<int> points_;
  Span<float3> separating_axes_;
  Span<float3> contact_velocities_;
  /* Constraint multiplier lambda for the normal displacement divided by time step. */
  Span<float> dynamic_friction_terms_;
  Span<float> lambdas_normal_;
  MutableSpan<float> lambdas_;

 public:
  static constexpr StringRefNull debug_name = "Friction";

  FrictionFaceConstraintSet(const int geo_i,
                            const Span<int> points,
                            const Span<float3> separating_axes,
                            const Span<float3> contact_velocities,
                            const Span<float> dynamic_friction_terms,
                            const Span<float> lambdas_normal,
                            MutableSpan<float> lambdas)
      : TemplatedVelocityConstraintSet<FrictionFaceConstraintSet>(points.size(), {geo_i}),
        geo_i_(geo_i),
        points_(points),
        separating_axes_(separating_axes),
        contact_velocities_(contact_velocities),
        dynamic_friction_terms_(dynamic_friction_terms),
        lambdas_normal_(lambdas_normal),
        lambdas_(lambdas)
  {
  }

  void reset_force(const int constraint_i) const
  {
    lambdas_[constraint_i] = 0.0f;
  }

  template<typename UpdaterT>
  void solve_single(const ConstraintSetParams &params,
                    UpdaterT &updater,
                    const int constraint_i) const
  {
    const int point_i = points_[constraint_i];
    const float inv_m = params.inv_mass(geo_i_, point_i);
    /* Should be inactive if weight is zero. */
    BLI_assert(inv_m > 0.0f);

    const float dynamic_friction = dynamic_friction_terms_[constraint_i] *
                                   params.dynamic_friction_factor;
    const float lambda_normal = lambdas_normal_[constraint_i];
    const float3 &axis = separating_axes_[constraint_i];
    const float3 &contact_velocity = contact_velocities_[constraint_i];
    const float3 &velocity = params.velocity(geo_i_, point_i) - contact_velocity;
    const float3 velocity_tangent = velocity - math::dot(velocity, axis) * axis;
    float residual;
    const float3 gradient = math::normalize_and_get_length(velocity_tangent, residual);
    /* Note: lambda_normal already includes the 1/inv_m weighting factor. */
    const float delta_lambda = std::min(dynamic_friction * lambda_normal, residual / inv_m);

    lambdas_[constraint_i] += delta_lambda;
    updater.update_velocity(geo_i_, point_i, -gradient * inv_m * delta_lambda);
  }
};

}  // namespace blender::xpbd
