/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GEO_xpbd_constraint_coloring_utils.hh"
#include "GEO_xpbd_constraint_math.hh"
#include "GEO_xpbd_constraint_set_templated.hh"

namespace blender::xpbd {

/* Constraint implementation for static and dynamic friction is based on
 * "Detailed Rigid Body Simulation with Extended Position Based Dynamics",
 * Mueller, Macklin, et al., 2020 */
class CollisionEdgeConstraintSet : public TemplatedConstraintSet<CollisionEdgeConstraintSet> {
 private:
  int geo_i_;
  Span<int2> point_pairs_;
  Span<float2> point_radii_;
  Span<float3> contact_points_on_edge_;
  Span<float3> contact_points_motion_;
  /* Direction of the collider edges. */
  Span<float3> edge_directions_;
  /* Normal vectors in the plane of the adjacent face.
   * The edge normal is cross(edge_direction, face_normal). */
  Span<float3> edge_normals_;
  /* Margin of the collider surface. */
  Span<float> edge_margins_;
  Span<float> compliance_terms_;
  Span<float> static_frictions_;
  Span<float> dynamic_frictions_;
  /* Scale factor for residual error. */
  Span<float> error_scales_;
  MutableSpan<bool> active_states_;
  MutableSpan<float> point_mix_factors_;
  MutableSpan<float> lambdas_normal_;

 public:
  static constexpr StringRefNull debug_name = "Collision Plane";

  CollisionEdgeConstraintSet(const int geo_i,
                             const Span<int2> point_pairs,
                             const Span<float2> point_radii,
                             const Span<float3> contact_points_on_edge,
                             const Span<float3> contact_points_motion,
                             const Span<float3> edge_directions,
                             const Span<float3> edge_normals,
                             const Span<float> edge_margins,
                             const Span<float> compliance_terms,
                             const Span<float> static_frictions,
                             const Span<float> dynamic_frictions,
                             const Span<float> error_scales,
                             MutableSpan<bool> active_states,
                             MutableSpan<float> point_mix_factors,
                             MutableSpan<float> lambdas_normal)
      : TemplatedConstraintSet<CollisionEdgeConstraintSet>(point_pairs.size(), {geo_i}),
        geo_i_(geo_i),
        point_pairs_(point_pairs),
        point_radii_(point_radii),
        contact_points_on_edge_(contact_points_on_edge),
        contact_points_motion_(contact_points_motion),
        edge_directions_(edge_directions),
        edge_normals_(edge_normals),
        edge_margins_(edge_margins),
        compliance_terms_(compliance_terms),
        static_frictions_(static_frictions),
        dynamic_frictions_(dynamic_frictions),
        error_scales_(error_scales),
        active_states_(active_states),
        point_mix_factors_(point_mix_factors),
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
    const int2 &point_pair = point_pairs_[constraint_i];
    const float3 &pos0 = params.position(geo_i_, point_pair[0]);
    const float3 &pos1 = params.position(geo_i_, point_pair[1]);
    const float3 &edge_pos = contact_points_on_edge_[constraint_i];
    const float3 &edge_dir = edge_directions_[constraint_i];
    const float3 &edge_nor = edge_normals_[constraint_i];
    const float margin = edge_margins_[constraint_i];
    const float compliance_term = compliance_terms_[constraint_i];
    const float inv_m0 = params.inv_mass(geo_i_, point_pair[0]);
    const float inv_m1 = params.inv_mass(geo_i_, point_pair[1]);
    const float radius0 = point_radii_[constraint_i][0];
    const float radius1 = point_radii_[constraint_i][1];
    BLI_assert(math::is_unit(edge_dir));
    BLI_assert(math::is_unit(edge_nor));
    bool &is_active = active_states_[constraint_i];
    float &point_mix_factor = point_mix_factors_[constraint_i];

    if (inv_m0 <= 0.0f && inv_m1 <= 0.0f) {
      /* Points with infinite mass are pinned and don't collide dynamically. */
      is_active = false;
      return;
    }

    const SegmentClosestToRay closest = closest_on_segment_to_ray(
        pos0, pos1, edge_pos, edge_dir, true);
    const float segment_factor = closest.segment_lambda;
    point_mix_factor = segment_factor;

    /* Effective weight/mass */
    const float weight0 = (1.0f - segment_factor) * inv_m0;
    const float weight1 = segment_factor * inv_m1;
    if (weight0 <= 0.0f && weight1 <= 0.0f) {
      /* Could happen if the segment_factor is exactly 0 or 1. */
      is_active = false;
      return;
    }

    const float3 closest_on_segment = math::interpolate(pos0, pos1, segment_factor);
    const float3 closest_on_ray = edge_pos + closest.ray_lambda * edge_dir;
    /* Curve surface is ambiguous: The closest point on the center line isn't necessarily the
     * contact point of the implicit curve surface as defined by the curve radius. A prospective
     * surface point is the intersection of connecting line between the closest points on the
     * segment and the edge.
     * If the curve is "inside" the collider (intersects the half-plane under the edge) then use
     * the curve surface point on the opposite side is used as the contact. */
    float3 seg_nor = closest_on_ray - closest_on_segment;
    const std::optional<PlaneIntersection> intersection = intersect_plane(
        pos0, pos1, edge_pos, edge_dir, edge_nor);
    if (intersection && math::dot(intersection->position - edge_pos, edge_nor) < 0.0f) {
      /* Reflect on segment direction. This produces the correct normal also in case the closest
       * segment point is clamped and the normal isn't perpendicular to the edge direction. */
      seg_nor = 2.0f * edge_dir * math::dot(edge_dir, seg_nor) - seg_nor;
    }
    float seg_dist;
    seg_nor = math::normalize_and_get_length(seg_nor, seg_dist);

    const float radius = math::interpolate(radius0, radius1, segment_factor);
    const float3 distance = (closest_on_segment + radius * seg_nor) -
                            (closest_on_ray + margin * edge_nor);
    const float residual = -math::dot(distance, seg_nor);
    if (residual >= 0.0f) {
      is_active = false;
      return;
    }
    const float3 gradient = -seg_nor;

    /* Positional correction for penetration. */
    float3 offset0 = float3(0.0f);
    float3 offset1 = float3(0.0f);
    float &lambda_normal = lambdas_normal_[constraint_i];
    const float error_squared = math::square(residual + compliance_term * lambda_normal);
    const float delta_lambda_normal = -residual / (weight0 + weight1 + compliance_term);
    offset0 += delta_lambda_normal * weight0 * gradient;
    offset1 += delta_lambda_normal * weight1 * gradient;
    lambda_normal += delta_lambda_normal;

    /* Apply static friction as a direct positional update. */
    const float3 &prev_pos0 = params.prev_position(geo_i_, point_pair[0]);
    const float3 &prev_pos1 = params.prev_position(geo_i_, point_pair[1]);
    const float3 &collider_velocity = contact_points_motion_[constraint_i];
    const float3 velocity = math::interpolate(pos0 - prev_pos0, pos1 - prev_pos1, segment_factor) -
                            collider_velocity;
    const float3 velocity_tangent = velocity - math::dot(velocity, gradient) * gradient;
    const float lambda_tangent_sq = math::length_squared(velocity_tangent /
                                                         (weight0 + weight1 + compliance_term));
    const bool is_static = lambda_tangent_sq <
                           math::square(static_frictions_[constraint_i] * lambda_normal);
    if (is_static) {
      offset0 -= velocity_tangent * weight0 / (weight0 + weight1 + compliance_term);
      offset1 -= velocity_tangent * weight1 / (weight0 + weight1 + compliance_term);
    }

    is_active = true;
    updater.update_position(geo_i_, point_pair[0], offset0);
    updater.update_position(geo_i_, point_pair[1], offset1);
    updater.add_residual_error(geo_i_, error_squared * error_scales_[constraint_i]);
  }

  ConstraintColoring color_constraints(LinearAllocator<> &memory) const override
  {
    return color_constraints__binary(point_pairs_, memory);
  }
};

}  // namespace blender::xpbd
