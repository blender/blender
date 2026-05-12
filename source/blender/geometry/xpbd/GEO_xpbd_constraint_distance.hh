/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GEO_xpbd_constraint_coloring_utils.hh"
#include "GEO_xpbd_constraint_set_templated.hh"

namespace blender::xpbd {

struct DistanceConstraintResult {
  float delta_lambda = 0.0f;
  float3 offset0 = float3(0.0f);
  float3 offset1 = float3(0.0f);
};

inline DistanceConstraintResult evaluate_distance_constraint(const float3 &p0,
                                                             const float3 &p1,
                                                             const float inv_m0,
                                                             const float inv_m1,
                                                             const float rest_distance,
                                                             const float compliance_term,
                                                             const float lambda_prev)
{
  if (inv_m0 == 0.0f && inv_m1 == 0.0f) {
    return {};
  }

  const float3 p_diff = p1 - p0;
  float length;
  const float3 normalized_dir = math::normalize_and_get_length(p_diff, length);
  const float length_diff = length - rest_distance;
  const float delta_lambda = (-length_diff - compliance_term * lambda_prev) /
                             (inv_m0 + inv_m1 + compliance_term);

  const float3 offset0 = -delta_lambda * inv_m0 * normalized_dir;
  const float3 offset1 = delta_lambda * inv_m1 * normalized_dir;

  return {delta_lambda, offset0, offset1};
}

class DistanceConstraintSet : public TemplatedConstraintSet<DistanceConstraintSet> {
 private:
  /** Indexed by constraint index. */
  Span<int2> point_pairs_;
  Span<float> distances_;
  Span<float> compliances_;
  MutableSpan<float> lambdas_;

 public:
  static constexpr StringRefNull debug_name = "Distance Constraint";

  DistanceConstraintSet(const int geo_i,
                        const Span<int2> point_pairs,
                        const Span<float> distances,
                        const Span<float> compliances,
                        MutableSpan<float> lambdas)
      : TemplatedConstraintSet<DistanceConstraintSet>(point_pairs.size(), {geo_i}),
        point_pairs_(point_pairs),
        distances_(distances),
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
    const int2 &point_pair = point_pairs_[constraint_i];
    const int point_i0 = point_pair[0];
    const int point_i1 = point_pair[1];
    const DistanceConstraintResult result = evaluate_distance_constraint(
        params.position(geo_i, point_i0),
        params.position(geo_i, point_i1),
        params.inv_mass(geo_i, point_i0),
        params.inv_mass(geo_i, point_i1),
        distances_[constraint_i],
        compliances_[constraint_i] * params.compliance_term_factor,
        lambdas_[constraint_i]);
    lambdas_[constraint_i] += result.delta_lambda;
    updater.update_position(geo_i, point_i0, result.offset0);
    updater.update_position(geo_i, point_i1, result.offset1);
  }

  ConstraintColoring color_constraints(IndexMaskMemory &memory) const override
  {
    return color_constraints__binary(point_pairs_, memory);
  }
};

}  // namespace blender::xpbd
