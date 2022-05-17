/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>

#include "curves_sculpt_intern.hh"

#include "BKE_bvhutils.h"
#include "BKE_context.h"
#include "BKE_curves.hh"

#include "ED_view3d.h"

#include "UI_interface.h"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_task.hh"

/**
 * The code below uses a prefix naming convention to indicate the coordinate space:
 * cu: Local space of the curves object that is being edited.
 * su: Local space of the surface object.
 * wo: World space.
 * re: 2D coordinates within the region.
 */

namespace blender::ed::sculpt_paint {

struct BrushPositionCandidate {
  /** 3D position of the brush. */
  float3 position_cu;
  /** Squared distance from the mouse position in screen space. */
  float distance_sq_re = FLT_MAX;
  /** Measure for how far away the candidate is from the camera. */
  float depth_sq_cu = FLT_MAX;
};

/**
 * Determine the 3D position of a brush based on curve segments under a screen position.
 */
static std::optional<float3> find_curves_brush_position(const CurvesGeometry &curves,
                                                        const float3 &ray_start_cu,
                                                        const float3 &ray_end_cu,
                                                        const float brush_radius_re,
                                                        const ARegion &region,
                                                        const RegionView3D &rv3d,
                                                        const Object &object)
{
  /* This value might have to be adjusted based on user feedback. */
  const float brush_inner_radius_re = std::min<float>(brush_radius_re, (float)UI_UNIT_X / 3.0f);
  const float brush_inner_radius_sq_re = pow2f(brush_inner_radius_re);

  float4x4 projection;
  ED_view3d_ob_project_mat_get(&rv3d, &object, projection.values);

  float2 brush_pos_re;
  ED_view3d_project_float_v2_m4(&region, ray_start_cu, brush_pos_re, projection.values);

  const float max_depth_sq_cu = math::distance_squared(ray_start_cu, ray_end_cu);

  /* Contains the logic that checks if `b` is a better candidate than `a`. */
  auto is_better_candidate = [&](const BrushPositionCandidate &a,
                                 const BrushPositionCandidate &b) {
    if (b.distance_sq_re <= brush_inner_radius_sq_re) {
      if (a.distance_sq_re > brush_inner_radius_sq_re) {
        /* New candidate is in inner radius while old one is not. */
        return true;
      }
      if (b.depth_sq_cu < a.depth_sq_cu) {
        /* Both candidates are in inner radius, but new one is closer to the camera. */
        return true;
      }
    }
    else if (b.distance_sq_re < a.distance_sq_re) {
      /* Both candidates are outside of inner radius, but new on is closer to the brush center. */
      return true;
    }
    return false;
  };

  auto update_if_better = [&](BrushPositionCandidate &a, const BrushPositionCandidate &b) {
    if (is_better_candidate(a, b)) {
      a = b;
    }
  };

  const Span<float3> positions = curves.positions();

  BrushPositionCandidate best_candidate = threading::parallel_reduce(
      curves.curves_range(),
      128,
      BrushPositionCandidate(),
      [&](IndexRange curves_range, const BrushPositionCandidate &init) {
        BrushPositionCandidate best_candidate = init;

        for (const int curve_i : curves_range) {
          const IndexRange points = curves.points_for_curve(curve_i);
          const int tot_segments = points.size() - 1;

          for (const int segment_i : IndexRange(tot_segments)) {
            const float3 &p1_cu = positions[points[segment_i]];
            const float3 &p2_cu = positions[points[segment_i] + 1];

            float2 p1_re, p2_re;
            ED_view3d_project_float_v2_m4(&region, p1_cu, p1_re, projection.values);
            ED_view3d_project_float_v2_m4(&region, p2_cu, p2_re, projection.values);

            float2 closest_re;
            const float lambda = closest_to_line_segment_v2(
                closest_re, brush_pos_re, p1_re, p2_re);

            const float3 closest_cu = math::interpolate(p1_cu, p2_cu, lambda);
            const float depth_sq_cu = math::distance_squared(ray_start_cu, closest_cu);
            if (depth_sq_cu > max_depth_sq_cu) {
              continue;
            }

            const float distance_sq_re = math::distance_squared(brush_pos_re, closest_re);

            float3 brush_position_cu;
            closest_to_line_segment_v3(brush_position_cu, closest_cu, ray_start_cu, ray_end_cu);

            BrushPositionCandidate candidate;
            candidate.position_cu = brush_position_cu;
            candidate.depth_sq_cu = depth_sq_cu;
            candidate.distance_sq_re = distance_sq_re;

            update_if_better(best_candidate, candidate);
          }
        }
        return best_candidate;
      },
      [&](const BrushPositionCandidate &a, const BrushPositionCandidate &b) {
        return is_better_candidate(a, b) ? b : a;
      });

  if (best_candidate.distance_sq_re == FLT_MAX) {
    /* Nothing found. */
    return std::nullopt;
  }

  return best_candidate.position_cu;
}

std::optional<CurvesBrush3D> sample_curves_3d_brush(const Depsgraph &depsgraph,
                                                    const ARegion &region,
                                                    const View3D &v3d,
                                                    const RegionView3D &rv3d,
                                                    const Object &curves_object,
                                                    const float2 &brush_pos_re,
                                                    const float brush_radius_re)
{
  const Curves &curves_id = *static_cast<Curves *>(curves_object.data);
  const CurvesGeometry &curves = CurvesGeometry::wrap(curves_id.geometry);
  const Object *surface_object = curves_id.surface;

  float3 center_ray_start_wo, center_ray_end_wo;
  ED_view3d_win_to_segment_clipped(
      &depsgraph, &region, &v3d, brush_pos_re, center_ray_start_wo, center_ray_end_wo, true);

  /* Shorten ray when the surface object is hit. */
  if (surface_object != nullptr) {
    const float4x4 surface_to_world_mat = surface_object->obmat;
    const float4x4 world_to_surface_mat = surface_to_world_mat.inverted();

    Mesh &surface = *static_cast<Mesh *>(surface_object->data);
    BVHTreeFromMesh surface_bvh;
    BKE_bvhtree_from_mesh_get(&surface_bvh, &surface, BVHTREE_FROM_LOOPTRI, 2);
    BLI_SCOPED_DEFER([&]() { free_bvhtree_from_mesh(&surface_bvh); });

    const float3 center_ray_start_su = world_to_surface_mat * center_ray_start_wo;
    float3 center_ray_end_su = world_to_surface_mat * center_ray_end_wo;
    const float3 center_ray_direction_su = math::normalize(center_ray_end_su -
                                                           center_ray_start_su);

    BVHTreeRayHit center_ray_hit;
    center_ray_hit.dist = FLT_MAX;
    center_ray_hit.index = -1;
    BLI_bvhtree_ray_cast(surface_bvh.tree,
                         center_ray_start_su,
                         center_ray_direction_su,
                         0.0f,
                         &center_ray_hit,
                         surface_bvh.raycast_callback,
                         &surface_bvh);
    if (center_ray_hit.index >= 0) {
      const float3 hit_position_su = center_ray_hit.co;
      if (math::distance(center_ray_start_su, center_ray_end_su) >
          math::distance(center_ray_start_su, hit_position_su)) {
        center_ray_end_su = hit_position_su;
        center_ray_end_wo = surface_to_world_mat * center_ray_end_su;
      }
    }
  }

  const float4x4 curves_to_world_mat = curves_object.obmat;
  const float4x4 world_to_curves_mat = curves_to_world_mat.inverted();

  const float3 center_ray_start_cu = world_to_curves_mat * center_ray_start_wo;
  const float3 center_ray_end_cu = world_to_curves_mat * center_ray_end_wo;

  const std::optional<float3> brush_position_optional_cu = find_curves_brush_position(
      curves,
      center_ray_start_cu,
      center_ray_end_cu,
      brush_radius_re,
      region,
      rv3d,
      curves_object);
  if (!brush_position_optional_cu.has_value()) {
    /* Nothing found. */
    return std::nullopt;
  }
  const float3 brush_position_cu = *brush_position_optional_cu;

  /* Determine the 3D brush radius. */
  float3 radius_ray_start_wo, radius_ray_end_wo;
  ED_view3d_win_to_segment_clipped(&depsgraph,
                                   &region,
                                   &v3d,
                                   brush_pos_re + float2(brush_radius_re, 0.0f),
                                   radius_ray_start_wo,
                                   radius_ray_end_wo,
                                   true);
  const float3 radius_ray_start_cu = world_to_curves_mat * radius_ray_start_wo;
  const float3 radius_ray_end_cu = world_to_curves_mat * radius_ray_end_wo;

  CurvesBrush3D brush_3d;
  brush_3d.position_cu = brush_position_cu;
  brush_3d.radius_cu = dist_to_line_v3(brush_position_cu, radius_ray_start_cu, radius_ray_end_cu);
  return brush_3d;
}

Vector<float4x4> get_symmetry_brush_transforms(const eCurvesSymmetryType symmetry)
{
  Vector<float4x4> matrices;

  auto symmetry_to_factors = [&](const eCurvesSymmetryType type) -> Span<float> {
    if (symmetry & type) {
      static std::array<float, 2> values = {1.0f, -1.0f};
      return values;
    }
    static std::array<float, 1> values = {1.0f};
    return values;
  };

  for (const float x : symmetry_to_factors(CURVES_SYMMETRY_X)) {
    for (const float y : symmetry_to_factors(CURVES_SYMMETRY_Y)) {
      for (const float z : symmetry_to_factors(CURVES_SYMMETRY_Z)) {
        float4x4 matrix = float4x4::identity();
        matrix.values[0][0] = x;
        matrix.values[1][1] = y;
        matrix.values[2][2] = z;
        matrices.append(matrix);
      }
    }
  }

  return matrices;
}

}  // namespace blender::ed::sculpt_paint
