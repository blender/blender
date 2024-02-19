/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>

#include "curves_sculpt_intern.hh"

#include "BLI_math_geom.h"

#include "BKE_attribute_math.hh"
#include "BKE_bvhutils.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_object.hh"
#include "BKE_report.h"

#include "ED_view3d.hh"

#include "UI_interface.hh"

#include "BLI_length_parameterize.hh"
#include "BLI_task.hh"

#include "DEG_depsgraph_query.hh"

#include "BLT_translation.h"

#include "GEO_curve_constraints.hh"

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
                                                        const Object &object,
                                                        const Span<float3> positions)
{
  /* This value might have to be adjusted based on user feedback. */
  const float brush_inner_radius_re = std::min<float>(brush_radius_re, float(UI_UNIT_X) / 3.0f);
  const float brush_inner_radius_sq_re = pow2f(brush_inner_radius_re);

  const float4x4 projection = ED_view3d_ob_project_mat_get(&rv3d, &object);
  const float2 brush_pos_re = ED_view3d_project_float_v2_m4(&region, ray_start_cu, projection);

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

  const OffsetIndices points_by_curve = curves.points_by_curve();

  BrushPositionCandidate best_candidate = threading::parallel_reduce(
      curves.curves_range(),
      128,
      BrushPositionCandidate(),
      [&](IndexRange curves_range, const BrushPositionCandidate &init) {
        BrushPositionCandidate best_candidate = init;

        for (const int curve_i : curves_range) {
          const IndexRange points = points_by_curve[curve_i];

          if (points.size() == 1) {
            const float3 &pos_cu = positions[points.first()];

            const float depth_sq_cu = math::distance_squared(ray_start_cu, pos_cu);
            if (depth_sq_cu > max_depth_sq_cu) {
              continue;
            }

            const float2 pos_re = ED_view3d_project_float_v2_m4(&region, pos_cu, projection);

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

            const float2 p1_re = ED_view3d_project_float_v2_m4(&region, p1_cu, projection);
            const float2 p2_re = ED_view3d_project_float_v2_m4(&region, p2_cu, projection);

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
  const CurvesGeometry &curves = curves_id.geometry.wrap();
  Object *surface_object = curves_id.surface;
  Object *surface_object_eval = DEG_get_evaluated_object(&depsgraph, surface_object);

  float3 center_ray_start_wo, center_ray_end_wo;
  ED_view3d_win_to_segment_clipped(
      &depsgraph, &region, &v3d, brush_pos_re, center_ray_start_wo, center_ray_end_wo, true);

  /* Shorten ray when the surface object is hit. */
  if (surface_object_eval != nullptr) {
    const float4x4 surface_to_world_mat(surface_object->object_to_world);
    const float4x4 world_to_surface_mat = math::invert(surface_to_world_mat);

    Mesh *surface_eval = BKE_object_get_evaluated_mesh(surface_object_eval);
    BVHTreeFromMesh surface_bvh;
    BKE_bvhtree_from_mesh_get(&surface_bvh, surface_eval, BVHTREE_FROM_CORNER_TRIS, 2);
    BLI_SCOPED_DEFER([&]() { free_bvhtree_from_mesh(&surface_bvh); });

    const float3 center_ray_start_su = math::transform_point(world_to_surface_mat,
                                                             center_ray_start_wo);
    float3 center_ray_end_su = math::transform_point(world_to_surface_mat, center_ray_end_wo);
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
          math::distance(center_ray_start_su, hit_position_su))
      {
        center_ray_end_su = hit_position_su;
        center_ray_end_wo = math::transform_point(surface_to_world_mat, center_ray_end_su);
      }
    }
  }

  const float4x4 curves_to_world_mat(curves_object.object_to_world);
  const float4x4 world_to_curves_mat = math::invert(curves_to_world_mat);

  const float3 center_ray_start_cu = math::transform_point(world_to_curves_mat,
                                                           center_ray_start_wo);
  const float3 center_ray_end_cu = math::transform_point(world_to_curves_mat, center_ray_end_wo);

  const bke::crazyspace::GeometryDeformation deformation =
      bke::crazyspace::get_evaluated_curves_deformation(depsgraph, curves_object);

  const std::optional<float3> brush_position_optional_cu = find_curves_brush_position(
      curves,
      center_ray_start_cu,
      center_ray_end_cu,
      brush_radius_re,
      region,
      rv3d,
      curves_object,
      deformation.positions);
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
  const float3 radius_ray_start_cu = math::transform_point(world_to_curves_mat,
                                                           radius_ray_start_wo);
  const float3 radius_ray_end_cu = math::transform_point(world_to_curves_mat, radius_ray_end_wo);

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
  const float3 brush_ray_start_su = math::transform_point(transforms.world_to_surface,
                                                          brush_ray_start_wo);
  const float3 brush_ray_end_su = math::transform_point(transforms.world_to_surface,
                                                        brush_ray_end_wo);

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
  const float3 brush_radius_ray_start_cu = math::transform_point(transforms.world_to_curves,
                                                                 brush_radius_ray_start_wo);
  const float3 brush_radius_ray_end_cu = math::transform_point(transforms.world_to_curves,
                                                               brush_radius_ray_end_wo);

  const float3 brush_pos_su = ray_hit.co;
  const float3 brush_pos_cu = math::transform_point(transforms.surface_to_curves, brush_pos_su);
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
        matrix.ptr()[0][0] = x;
        matrix.ptr()[1][1] = y;
        matrix.ptr()[2][2] = z;
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
  const float3 new_position = math::transform_point(transform, brush_position);
  const float3 new_offset_position = math::transform_point(transform, offset_position);
  return math::distance(new_position, new_offset_position);
}

void move_last_point_and_resample(MoveAndResampleBuffers &buffer,
                                  MutableSpan<float3> positions,
                                  const float3 &new_last_position)
{
  /* Find the accumulated length of each point in the original curve,
   * treating it as a poly curve for performance reasons and simplicity. */
  buffer.orig_lengths.reinitialize(length_parameterize::segments_num(positions.size(), false));
  length_parameterize::accumulate_lengths<float3>(positions, false, buffer.orig_lengths);
  const float orig_total_length = buffer.orig_lengths.last();

  /* Find the factor by which the new curve is shorter or longer than the original. */
  const float new_last_segment_length = math::distance(positions.last(1), new_last_position);
  const float new_total_length = buffer.orig_lengths.last(1) + new_last_segment_length;
  const float length_factor = math::safe_divide(new_total_length, orig_total_length);

  /* Calculate the lengths to sample the original curve with by scaling the original lengths. */
  buffer.new_lengths.reinitialize(positions.size() - 1);
  buffer.new_lengths.first() = 0.0f;
  for (const int i : buffer.new_lengths.index_range().drop_front(1)) {
    buffer.new_lengths[i] = buffer.orig_lengths[i - 1] * length_factor;
  }

  buffer.sample_indices.reinitialize(positions.size() - 1);
  buffer.sample_factors.reinitialize(positions.size() - 1);
  length_parameterize::sample_at_lengths(
      buffer.orig_lengths, buffer.new_lengths, buffer.sample_indices, buffer.sample_factors);

  buffer.new_positions.reinitialize(positions.size() - 1);
  length_parameterize::interpolate<float3>(
      positions, buffer.sample_indices, buffer.sample_factors, buffer.new_positions);
  positions.drop_back(1).copy_from(buffer.new_positions);
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

void report_empty_original_surface(ReportList *reports)
{
  BKE_report(reports, RPT_WARNING, "Original surface mesh is empty");
}

void report_empty_evaluated_surface(ReportList *reports)
{
  BKE_report(reports, RPT_WARNING, "Evaluated surface mesh is empty");
}

void report_missing_surface(ReportList *reports)
{
  BKE_report(reports, RPT_WARNING, "Missing surface mesh");
}

void report_missing_uv_map_on_original_surface(ReportList *reports)
{
  BKE_report(reports, RPT_WARNING, "Missing UV map for attaching curves on original surface");
}

void report_missing_uv_map_on_evaluated_surface(ReportList *reports)
{
  BKE_report(reports, RPT_WARNING, "Missing UV map for attaching curves on evaluated surface");
}

void report_invalid_uv_map(ReportList *reports)
{
  BKE_report(reports, RPT_WARNING, "Invalid UV map: UV islands must not overlap");
}

void CurvesConstraintSolver::initialize(const bke::CurvesGeometry &curves,
                                        const IndexMask &curve_selection,
                                        const bool use_surface_collision)
{
  use_surface_collision_ = use_surface_collision;
  segment_lengths_.reinitialize(curves.points_num());
  geometry::curve_constraints::compute_segment_lengths(
      curves.points_by_curve(), curves.positions(), curve_selection, segment_lengths_);
  if (use_surface_collision_) {
    start_positions_ = curves.positions();
  }
}

void CurvesConstraintSolver::solve_step(bke::CurvesGeometry &curves,
                                        const IndexMask &curve_selection,
                                        const Mesh *surface,
                                        const CurvesSurfaceTransforms &transforms)
{
  if (use_surface_collision_ && surface != nullptr) {
    geometry::curve_constraints::solve_length_and_collision_constraints(
        curves.points_by_curve(),
        curve_selection,
        segment_lengths_,
        start_positions_,
        *surface,
        transforms,
        curves.positions_for_write());
    start_positions_ = curves.positions();
  }
  else {
    geometry::curve_constraints::solve_length_constraints(
        curves.points_by_curve(), curve_selection, segment_lengths_, curves.positions_for_write());
  }
  curves.tag_positions_changed();
}

}  // namespace blender::ed::sculpt_paint
