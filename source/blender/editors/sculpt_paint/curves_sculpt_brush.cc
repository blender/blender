/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>

#include "curves_sculpt_intern.hh"

#include "BKE_attribute_math.hh"
#include "BKE_bvhutils.h"
#include "BKE_context.h"
#include "BKE_curves.hh"

#include "ED_view3d.h"

#include "UI_interface.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_length_parameterize.hh"
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

          if (points.size() == 1) {
            const float3 &pos_cu = positions[points.first()];

            const float depth_sq_cu = math::distance_squared(ray_start_cu, pos_cu);
            if (depth_sq_cu > max_depth_sq_cu) {
              continue;
            }

            float2 pos_re;
            ED_view3d_project_float_v2_m4(&region, pos_cu, pos_re, projection.values);

            BrushPositionCandidate candidate;
            candidate.position_cu = pos_cu;
            candidate.depth_sq_cu = depth_sq_cu;
            candidate.distance_sq_re = math::distance_squared(brush_pos_re, pos_re);

            update_if_better(best_candidate, candidate);
            continue;
          }

          for (const int segment_i : points.drop_back(1)) {
            const float3 &p1_cu = positions[segment_i];
            const float3 &p2_cu = positions[segment_i + 1];

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

std::optional<CurvesBrush3D> sample_curves_surface_3d_brush(
    const Depsgraph &depsgraph,
    const ARegion &region,
    const View3D &v3d,
    const CurvesSurfaceTransforms &transforms,
    const BVHTreeFromMesh &surface_bvh,
    const float2 &brush_pos_re,
    const float brush_radius_re)
{
  float3 brush_ray_start_wo, brush_ray_end_wo;
  ED_view3d_win_to_segment_clipped(
      &depsgraph, &region, &v3d, brush_pos_re, brush_ray_start_wo, brush_ray_end_wo, true);
  const float3 brush_ray_start_su = transforms.world_to_surface * brush_ray_start_wo;
  const float3 brush_ray_end_su = transforms.world_to_surface * brush_ray_end_wo;

  const float3 brush_ray_direction_su = math::normalize(brush_ray_end_su - brush_ray_start_su);

  BVHTreeRayHit ray_hit;
  ray_hit.dist = FLT_MAX;
  ray_hit.index = -1;
  BLI_bvhtree_ray_cast(surface_bvh.tree,
                       brush_ray_start_su,
                       brush_ray_direction_su,
                       0.0f,
                       &ray_hit,
                       surface_bvh.raycast_callback,
                       const_cast<void *>(static_cast<const void *>(&surface_bvh)));
  if (ray_hit.index == -1) {
    return std::nullopt;
  }

  float3 brush_radius_ray_start_wo, brush_radius_ray_end_wo;
  ED_view3d_win_to_segment_clipped(&depsgraph,
                                   &region,
                                   &v3d,
                                   brush_pos_re + float2(brush_radius_re, 0),
                                   brush_radius_ray_start_wo,
                                   brush_radius_ray_end_wo,
                                   true);
  const float3 brush_radius_ray_start_cu = transforms.world_to_curves * brush_radius_ray_start_wo;
  const float3 brush_radius_ray_end_cu = transforms.world_to_curves * brush_radius_ray_end_wo;

  const float3 brush_pos_su = ray_hit.co;
  const float3 brush_pos_cu = transforms.surface_to_curves * brush_pos_su;
  const float brush_radius_cu = dist_to_line_v3(
      brush_pos_cu, brush_radius_ray_start_cu, brush_radius_ray_end_cu);
  return CurvesBrush3D{brush_pos_cu, brush_radius_cu};
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

float transform_brush_radius(const float4x4 &transform,
                             const float3 &brush_position,
                             const float old_radius)
{
  const float3 offset_position = brush_position + float3(old_radius, 0.0f, 0.0f);
  const float3 new_position = transform * brush_position;
  const float3 new_offset_position = transform * offset_position;
  return math::distance(new_position, new_offset_position);
}

void move_last_point_and_resample(MutableSpan<float3> positions, const float3 &new_last_position)
{
  /* Find the accumulated length of each point in the original curve,
   * treating it as a poly curve for performance reasons and simplicity. */
  Array<float> orig_lengths(length_parameterize::segments_num(positions.size(), false));
  length_parameterize::accumulate_lengths<float3>(positions, false, orig_lengths);
  const float orig_total_length = orig_lengths.last();

  /* Find the factor by which the new curve is shorter or longer than the original. */
  const float new_last_segment_length = math::distance(positions.last(1), new_last_position);
  const float new_total_length = orig_lengths.last(1) + new_last_segment_length;
  const float length_factor = safe_divide(new_total_length, orig_total_length);

  /* Calculate the lengths to sample the original curve with by scaling the original lengths. */
  Array<float> new_lengths(positions.size() - 1);
  new_lengths.first() = 0.0f;
  for (const int i : new_lengths.index_range().drop_front(1)) {
    new_lengths[i] = orig_lengths[i - 1] * length_factor;
  }

  Array<int> indices(positions.size() - 1);
  Array<float> factors(positions.size() - 1);
  length_parameterize::sample_at_lengths(orig_lengths, new_lengths, indices, factors);

  Array<float3> new_positions(positions.size() - 1);
  length_parameterize::linear_interpolation<float3>(positions, indices, factors, new_positions);
  positions.drop_back(1).copy_from(new_positions);
  positions.last() = new_last_position;
}

CurvesSculptCommonContext::CurvesSculptCommonContext(const bContext &C)
{
  this->depsgraph = CTX_data_depsgraph_pointer(&C);
  this->scene = CTX_data_scene(&C);
  this->region = CTX_wm_region(&C);
  this->v3d = CTX_wm_view3d(&C);
  this->rv3d = CTX_wm_region_view3d(&C);
}

}  // namespace blender::ed::sculpt_paint
