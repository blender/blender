/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"
#include "BLI_task.hh"

#include "GEO_curve_constraints.hh"

#include "BKE_bvhutils.h"

/**
 * The code below uses a prefix naming convention to indicate the coordinate space:
 * `cu`: Local space of the curves object that is being edited.
 * `su`: Local space of the surface object.
 * `wo`: World space.
 */

namespace blender::geometry::curve_constraints {

void compute_segment_lengths(const OffsetIndices<int> points_by_curve,
                             const Span<float3> positions,
                             const IndexMask &curve_selection,
                             MutableSpan<float> r_segment_lengths)
{
  BLI_assert(r_segment_lengths.size() == points_by_curve.total_size());

  curve_selection.foreach_segment(GrainSize(256), [&](const IndexMaskSegment segment) {
    for (const int curve_i : segment) {
      const IndexRange points = points_by_curve[curve_i].drop_back(1);
      for (const int point_i : points) {
        const float3 &p1 = positions[point_i];
        const float3 &p2 = positions[point_i + 1];
        const float length = math::distance(p1, p2);
        r_segment_lengths[point_i] = length;
      }
    }
  });
}

void solve_length_constraints(const OffsetIndices<int> points_by_curve,
                              const IndexMask &curve_selection,
                              const Span<float> segment_lenghts,
                              MutableSpan<float3> positions)
{
  BLI_assert(segment_lenghts.size() == points_by_curve.total_size());

  curve_selection.foreach_segment(GrainSize(256), [&](const IndexMaskSegment segment) {
    for (const int curve_i : segment) {
      const IndexRange points = points_by_curve[curve_i].drop_back(1);
      for (const int point_i : points) {
        const float3 &p1 = positions[point_i];
        float3 &p2 = positions[point_i + 1];
        const float3 direction = math::normalize(p2 - p1);
        const float goal_length = segment_lenghts[point_i];
        p2 = p1 + direction * goal_length;
      }
    }
  });
}

void solve_length_and_collision_constraints(const OffsetIndices<int> points_by_curve,
                                            const IndexMask &curve_selection,
                                            const Span<float> segment_lengths_cu,
                                            const Span<float3> start_positions_cu,
                                            const Mesh &surface,
                                            const bke::CurvesSurfaceTransforms &transforms,
                                            MutableSpan<float3> positions_cu)
{
  solve_length_constraints(points_by_curve, curve_selection, segment_lengths_cu, positions_cu);

  BVHTreeFromMesh surface_bvh;
  BKE_bvhtree_from_mesh_get(&surface_bvh, &surface, BVHTREE_FROM_LOOPTRI, 2);
  BLI_SCOPED_DEFER([&]() { free_bvhtree_from_mesh(&surface_bvh); });

  const float radius = 0.005f;
  const int max_collisions = 5;

  curve_selection.foreach_segment(GrainSize(64), [&](const IndexMaskSegment segment) {
    for (const int curve_i : segment) {
      const IndexRange points = points_by_curve[curve_i];

      /* Sometimes not all collisions can be handled. This happens relatively rarely, but if it
       * happens it's better to just not to move the curve instead of going into the surface. */
      bool revert_curve = false;
      for (const int point_i : points.drop_front(1)) {
        const float goal_segment_length_cu = segment_lengths_cu[point_i - 1];
        const float3 &prev_pos_cu = positions_cu[point_i - 1];
        const float3 &start_pos_cu = start_positions_cu[point_i];

        int used_iterations = 0;
        for ([[maybe_unused]] const int iteration : IndexRange(max_collisions)) {
          used_iterations++;
          const float3 &old_pos_cu = positions_cu[point_i];
          if (start_pos_cu == old_pos_cu) {
            /* The point did not move, done. */
            break;
          }

          /* Check if the point moved through a surface. */
          const float3 start_pos_su = math::transform_point(transforms.curves_to_surface,
                                                            start_pos_cu);
          const float3 old_pos_su = math::transform_point(transforms.curves_to_surface,
                                                          old_pos_cu);
          const float3 pos_diff_su = old_pos_su - start_pos_su;
          float max_ray_length_su;
          const float3 ray_direction_su = math::normalize_and_get_length(pos_diff_su,
                                                                         max_ray_length_su);
          BVHTreeRayHit hit;
          hit.index = -1;
          hit.dist = max_ray_length_su + radius;
          BLI_bvhtree_ray_cast(surface_bvh.tree,
                               start_pos_su,
                               ray_direction_su,
                               radius,
                               &hit,
                               surface_bvh.raycast_callback,
                               &surface_bvh);
          if (hit.index == -1) {
            break;
          }
          const float3 hit_pos_su = hit.co;
          const float3 hit_normal_su = hit.no;
          if (math::dot(hit_normal_su, ray_direction_su) > 0.0f) {
            /* Moving from the inside to the outside is ok. */
            break;
          }

          /* The point was moved through a surface. Now put it back on the correct side of the
           * surface and slide it on the surface to keep the length the same. */

          const float3 hit_pos_cu = math::transform_point(transforms.surface_to_curves,
                                                          hit_pos_su);
          const float3 hit_normal_cu = math::normalize(
              math::transform_direction(transforms.surface_to_curves_normal, hit_normal_su));

          /* Slide on a plane that is slightly above the surface. */
          const float3 plane_pos_cu = hit_pos_cu + hit_normal_cu * radius;
          const float3 plane_normal_cu = hit_normal_cu;

          /* Decompose the current segment into the part normal and tangent to the collision
           * surface. */
          const float3 collided_segment_cu = plane_pos_cu - prev_pos_cu;
          const float3 slide_normal_cu = plane_normal_cu *
                                         math::dot(collided_segment_cu, plane_normal_cu);
          const float3 slide_direction_cu = collided_segment_cu - slide_normal_cu;

          float slide_direction_length_cu;
          const float3 normalized_slide_direction_cu = math::normalize_and_get_length(
              slide_direction_cu, slide_direction_length_cu);
          const float slide_normal_length_sq_cu = math::length_squared(slide_normal_cu);

          if (pow2f(goal_segment_length_cu) > slide_normal_length_sq_cu) {
            /* Use pythagorian theorem to determine how far to slide. */
            const float slide_distance_cu = std::sqrt(pow2f(goal_segment_length_cu) -
                                                      slide_normal_length_sq_cu) -
                                            slide_direction_length_cu;
            positions_cu[point_i] = plane_pos_cu +
                                    normalized_slide_direction_cu * slide_distance_cu;
          }
          else {
            /* Minimum distance is larger than allowed segment length.
             * The unilateral collision constraint is satisfied by just clamping segment length. */
            positions_cu[point_i] = prev_pos_cu + math::normalize(old_pos_su - prev_pos_cu) *
                                                      goal_segment_length_cu;
          }
        }
        if (used_iterations == max_collisions) {
          revert_curve = true;
          break;
        }
      }
      if (revert_curve) {
        positions_cu.slice(points).copy_from(start_positions_cu.slice(points));
      }
    }
  });
}

}  // namespace blender::geometry::curve_constraints
