/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GEO_xpbd_constraint_coloring_utils.hh"
#include "GEO_xpbd_constraint_set_templated.hh"

namespace blender::xpbd {

/* Constraint implementation for static and dynamic friction is based on
 * "Detailed Rigid Body Simulation with Extended Position Based Dynamics",
 * Mueller, Macklin, et al., 2020 */
class CollisionFaceConstraintSet : public TemplatedConstraintSet<CollisionFaceConstraintSet> {
 private:
  int geo_i_;
  Span<int> points_;
  Span<float> point_radii_;
  Span<float3> contact_points_on_face_;
  Span<float3> contact_points_motion_;
  Span<float3> face_normals_;
  Span<float> face_margins_;
  Span<float> compliance_terms_;
  Span<float> static_frictions_;
  Span<float> dynamic_frictions_;
  /* Scale factor for residual error. */
  Span<float> error_scales_;
  MutableSpan<bool> active_states_;
  MutableSpan<float> lambdas_normal_;

 public:
  static constexpr StringRefNull debug_name = "Collision Plane";

  CollisionFaceConstraintSet(const int geo_i,
                             const Span<int> points,
                             const Span<float> point_radii,
                             const Span<float3> contact_points_on_face,
                             const Span<float3> contact_points_motion,
                             const Span<float3> face_normals,
                             const Span<float> face_margins,
                             const Span<float> compliance_terms,
                             const Span<float> static_frictions,
                             const Span<float> dynamic_frictions,
                             const Span<float> error_scales,
                             MutableSpan<bool> active_states,
                             MutableSpan<float> lambdas_normal)
      : TemplatedConstraintSet<CollisionFaceConstraintSet>(points.size(), {geo_i}),
        geo_i_(geo_i),
        points_(points),
        point_radii_(point_radii),
        contact_points_on_face_(contact_points_on_face),
        contact_points_motion_(contact_points_motion),
        face_normals_(face_normals),
        face_margins_(face_margins),
        compliance_terms_(compliance_terms),
        static_frictions_(static_frictions),
        dynamic_frictions_(dynamic_frictions),
        error_scales_(error_scales),
        active_states_(active_states),
        lambdas_normal_(lambdas_normal)
  {
  }

  void reset_force(const int constraint_i) const
  {
    active_states_[constraint_i] = false;
    lambdas_normal_[constraint_i] = 0.0f;
  }

  template<typename UpdaterT>
  void solve_single(const ConstraintSetParams &params,
                    UpdaterT &updater,
                    const int constraint_i) const
  {
    const int point_i = points_[constraint_i];
    const float radius = point_radii_[constraint_i];
    const float3 &pos = params.position(geo_i_, point_i);
    const float3 &face_pos = contact_points_on_face_[constraint_i];
    const float3 &face_nor = face_normals_[constraint_i];
    const float margin = face_margins_[constraint_i];
    const float compliance_term = compliance_terms_[constraint_i];
    const float inv_m = params.inv_mass(geo_i_, point_i);
    bool &is_active = active_states_[constraint_i];

    if (inv_m <= 0.0f) {
      /* Points with infinite mass are pinned and don't collide dynamically. */
      is_active = false;
      return;
    }

    const float3 diff = pos - face_pos;
    const float normal_distance = math::dot(diff, face_nor) - margin - radius;
    is_active = normal_distance < 0.0f;
    if (!is_active) {
      return;
    }

    /* Positional correction for penetration. */
    float3 offset = float3(0.0f);
    float &lambda_normal = lambdas_normal_[constraint_i];
    const float error_squared = math::square(normal_distance + compliance_term * lambda_normal);
    const float delta_lambda_normal = -normal_distance / (inv_m + compliance_term);
    offset += delta_lambda_normal * inv_m * face_nor;
    lambda_normal += delta_lambda_normal;

    /* Apply static friction as a direct positional update. */
    const float3 &prev_pos = params.prev_position(geo_i_, point_i);
    const float3 &collider_velocity = contact_points_motion_[constraint_i];
    const float3 velocity = (pos - prev_pos) - collider_velocity;
    const float3 velocity_tangent = velocity - math::dot(velocity, face_nor) * face_nor;
    const float lambda_tangent_sq = math::length_squared(velocity_tangent /
                                                         (inv_m + compliance_term));
    const bool is_static = lambda_tangent_sq <
                           math::square(static_frictions_[constraint_i] * lambda_normal);
    if (is_static) {
      offset -= velocity_tangent * inv_m / (inv_m + compliance_term);
    }

    updater.update_position(geo_i_, point_i, offset);
    updater.add_residual_error(geo_i_, error_squared * error_scales_[constraint_i]);
  }

  ConstraintColoring color_constraints(LinearAllocator<> &memory) const override
  {
    return color_constraints__unary(points_, memory);
  }
};

}  // namespace blender::xpbd
