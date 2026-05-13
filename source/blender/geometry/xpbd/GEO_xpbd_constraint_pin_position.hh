/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GEO_xpbd_constraint_coloring_utils.hh"
#include "GEO_xpbd_constraint_distance.hh"
#include "GEO_xpbd_constraint_set_templated.hh"

namespace blender::xpbd {

class PinPositionConstraintSet : public TemplatedConstraintSet<PinPositionConstraintSet> {
 private:
  /** Indexed by constraint index. */
  Span<int> point_indices_;
  Span<float3> pin_positions_;
  Span<float> compliances_;
  MutableSpan<float> lambdas_;

 public:
  static constexpr StringRefNull debug_name = "Pinned Position";

  PinPositionConstraintSet(const int geo_i,
                           const Span<int> point_indices,
                           const Span<float3> pin_positions,
                           const Span<float> compliances,
                           const MutableSpan<float> lambdas)
      : TemplatedConstraintSet<PinPositionConstraintSet>(point_indices.size(), {geo_i}),
        point_indices_(point_indices),
        pin_positions_(pin_positions),
        compliances_(compliances),
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
    const int geo_i = affected_geo_indices_[0];
    const int point_i = point_indices_[constraint_i];
    const DistanceConstraintResult result = evaluate_distance_constraint(
        params.position(geo_i, point_i),
        pin_positions_[constraint_i],
        params.inv_mass(geo_i, point_i),
        0.0f,
        0.0f,
        compliances_[constraint_i] * params.compliance_term_factor,
        lambdas_[constraint_i]);
    lambdas_[constraint_i] += result.delta_lambda;
    updater.update_position(geo_i, point_i, result.offset0);
  }

  ConstraintColoring color_constraints(IndexMaskMemory &memory) const override
  {
    return color_constraints__unary(point_indices_, memory);
  }
};

}  // namespace blender::xpbd
