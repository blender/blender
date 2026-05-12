/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GEO_xpbd_constraint_align_rotations.hh"
#include "GEO_xpbd_constraint_coloring_utils.hh"
#include "GEO_xpbd_constraint_set_templated.hh"

namespace blender::xpbd {

class PinRotationConstraintSet : public TemplatedConstraintSet<PinRotationConstraintSet> {
 private:
  /** Indexed by constraint index. */
  Span<float> compliances_;
  Span<int> point_indices_;
  Span<math::Quaternion> pin_rotations_;
  MutableSpan<float4> lambdas_;

 public:
  static constexpr StringRefNull debug_name = "Pin Rotation";

  PinRotationConstraintSet(const int geo_i,
                           const Span<int> point_indices,
                           const Span<math::Quaternion> pin_rotations,
                           const Span<float> compliances,
                           MutableSpan<float4> lambdas)
      : TemplatedConstraintSet<PinRotationConstraintSet>(point_indices.size(), {geo_i}),
        compliances_(compliances),
        point_indices_(point_indices),
        pin_rotations_(pin_rotations),
        lambdas_(lambdas)
  {
  }

  void reset_force(const int constraint_i) const
  {
    lambdas_[constraint_i] = float4(0.0f);
  }

  template<typename UpdaterT>
  void solve_single(const ConstraintSetParams &params,
                    UpdaterT &updater,
                    const int constraint_i) const
  {
    const int geo_i = affected_geo_indices_[0];
    const int point_i = point_indices_[constraint_i];
    const AlignRotationsConstraintResult result = evaluate_align_rotations_constraint(
        params.rotation(geo_i, point_i),
        pin_rotations_[constraint_i],
        params.moment_of_inertia(geo_i, point_i),
        float3(std::numeric_limits<float>::infinity()),
        math::Quaternion::identity(),
        compliances_[constraint_i] * params.compliance_term_factor,
        lambdas_[constraint_i]);
    lambdas_[constraint_i] += result.delta_lambda;
    updater.update_rotation(geo_i, point_i, result.offset0);
  }

  ConstraintColoring color_constraints(IndexMaskMemory &memory) const override
  {
    return color_constraints__unary(point_indices_, memory);
  }
};

}  // namespace blender::xpbd
