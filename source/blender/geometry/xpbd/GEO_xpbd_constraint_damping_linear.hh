/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GEO_xpbd_constraint_set_templated.hh"

namespace blender::xpbd {

class LinearDampingConstraintSet
    : public TemplatedVelocityConstraintSet<LinearDampingConstraintSet> {
 private:
  int geo_i_;
  IndexRange points_;
  /** Indexed by constraint index. */
  Span<float> linear_dampings_;
  MutableSpan<float> lambdas_;

 public:
  static constexpr StringRefNull debug_name = "Linear Damping";

  LinearDampingConstraintSet(const int geo_i,
                             const IndexRange points,
                             const Span<float> linear_dampings,
                             MutableSpan<float> lambdas)
      : TemplatedVelocityConstraintSet<LinearDampingConstraintSet>(points.size(), {geo_i}),
        geo_i_(geo_i),
        points_(points),
        linear_dampings_(linear_dampings),
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
    const float3 &velocity = params.velocity(geo_i_, point_i);
    const float damping = linear_dampings_[constraint_i];
    const float damping_factor = std::clamp(params.delta_time * damping, 0.0f, 1.0f);
    float residual;
    const float3 gradient = math::normalize_and_get_length(velocity, residual);
    const float delta_lambda = -residual * damping_factor - lambdas_[constraint_i];
    const float3 offset = gradient * delta_lambda;
    lambdas_[constraint_i] += delta_lambda;
    updater.update_velocity(geo_i_, point_i, offset);
  }
};

}  // namespace blender::xpbd
