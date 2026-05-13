/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GEO_xpbd_constraint_math.hh"
#include "GEO_xpbd_constraint_set_templated.hh"

namespace blender::xpbd {

class FrictionEdgeConstraintSet
    : public TemplatedVelocityConstraintSet<FrictionEdgeConstraintSet> {
 private:
  int geo_i_;
  /* Constraint index for each point. */
  Span<int2> point_pairs_;
  Span<float3> separating_axes_;
  Span<float3> contact_velocities_;
  /* Constraint multiplier lambda for the normal displacement divided by time step. */
  Span<float> dynamic_friction_terms_;
  Span<float> lambdas_normal_;
  Span<float> point_mix_factors_;
  MutableSpan<float> lambdas_;

 public:
  static constexpr StringRefNull debug_name = "Friction";

  FrictionEdgeConstraintSet(const int geo_i,
                            const Span<int2> point_pairs,
                            const Span<float3> separating_axes,
                            const Span<float3> contact_velocities,
                            const Span<float> dynamic_friction_terms,
                            const Span<float> lambdas_normal,
                            const Span<float> point_mix_factors,
                            MutableSpan<float> lambdas)
      : TemplatedVelocityConstraintSet<FrictionEdgeConstraintSet>(point_pairs.size(), {geo_i}),
        geo_i_(geo_i),
        point_pairs_(point_pairs),
        separating_axes_(separating_axes),
        contact_velocities_(contact_velocities),
        dynamic_friction_terms_(dynamic_friction_terms),
        lambdas_normal_(lambdas_normal),
        point_mix_factors_(point_mix_factors),
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
    const int2 point_pair = point_pairs_[constraint_i];
    const float segment_factor = point_mix_factors_[constraint_i];

    /* Effective weight/mass */
    const float inv_m0 = params.inv_mass(geo_i_, point_pair[0]);
    const float inv_m1 = params.inv_mass(geo_i_, point_pair[1]);
    const float weight0 = (1.0f - segment_factor) * inv_m0;
    const float weight1 = segment_factor * inv_m1;
    /* Should be inactive if both are zero.  */
    BLI_assert(weight0 > 0.0f || weight1 > 0.0f);

    const float dynamic_friction = dynamic_friction_terms_[constraint_i] *
                                   params.dynamic_friction_factor;
    const float lambda_normal = lambdas_normal_[constraint_i];
    const float3 &axis = separating_axes_[constraint_i];
    const float3 &contact_velocity = contact_velocities_[constraint_i];
    const float3 &velocity = math::interpolate(params.velocity(geo_i_, point_pair[0]),
                                               params.velocity(geo_i_, point_pair[1]),
                                               segment_factor) -
                             contact_velocity;
    const float3 velocity_tangent = velocity - math::dot(velocity, axis) * axis;
    float residual;
    const float3 gradient = math::normalize_and_get_length(velocity_tangent, residual);
    const float delta_lambda = std::min(dynamic_friction * lambda_normal,
                                        residual / (weight0 + weight1));

    lambdas_[constraint_i] += delta_lambda;
    updater.update_velocity(geo_i_, point_pair[0], -weight0 * gradient * delta_lambda);
    updater.update_velocity(geo_i_, point_pair[1], -weight1 * gradient * delta_lambda);
  }
};

}  // namespace blender::xpbd
