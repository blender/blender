/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GEO_xpbd_constraint_set_templated.hh"

namespace blender::xpbd {

class AngularDampingConstraintSet
    : public TemplatedVelocityConstraintSet<AngularDampingConstraintSet> {
 private:
  int geo_i_;
  IndexRange points_;
  /** Indexed by constraint index. */
  Span<float> angular_dampings_;
  MutableSpan<float> lambdas_;

 public:
  static constexpr StringRefNull debug_name = "Angular Damping";

  AngularDampingConstraintSet(const int geo_i,
                              const IndexRange points,
                              const Span<float> angular_dampings,
                              MutableSpan<float> lambdas)
      : TemplatedVelocityConstraintSet<AngularDampingConstraintSet>(points.size(), {geo_i}),
        geo_i_(geo_i),
        points_(points),
        angular_dampings_(angular_dampings),
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
    const float3 &angular_velocity = params.angular_velocity(geo_i_, point_i);
    const float damping = angular_dampings_[constraint_i];
    const float damping_factor = damping * params.delta_time;
    float residual;
    const float3 gradient = math::normalize_and_get_length(angular_velocity, residual);
    const float delta_lambda = -residual * damping_factor - lambdas_[constraint_i];
    const float3 offset = gradient * delta_lambda;
    lambdas_[constraint_i] += delta_lambda;
    updater.update_angular_velocity(geo_i_, point_i, offset);
  }
};

}  // namespace blender::xpbd
