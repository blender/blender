/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>

#include "curves_sculpt_intern.hh"

#include "BLI_math_matrix_types.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "DEG_depsgraph.h"

#include "BKE_attribute_math.hh"
#include "BKE_brush.hh"
#include "BKE_bvhutils.h"
#include "BKE_context.h"
#include "BKE_curves.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_sample.hh"
#include "BKE_object.h"
#include "BKE_paint.hh"
#include "BKE_report.h"

#include "DNA_brush_enums.h"
#include "DNA_curves_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "ED_screen.hh"
#include "ED_view3d.h"

#include "WM_api.hh"

#include "DEG_depsgraph_query.h"

#include "GEO_add_curves_on_mesh.hh"
#include "GEO_reverse_uv_sampler.hh"

#include "BLT_translation.h"

namespace blender::ed::sculpt_paint {

using geometry::ReverseUVSampler;

struct SlideCurveInfo {
  /** Index of the curve to slide. */
  int curve_i;
  /** A weight based on the initial distance to the brush. */
  float radius_falloff;
  /**
   * Normal of the surface where the curve was attached. This is used to rotate the curve if it is
   * moved to a place with a different normal.
   */
  float3 initial_normal_cu;
};

struct SlideInfo {
  /** The transform used for the curves below (e.g. for symmetry). */
  float4x4 brush_transform;
  Vector<SlideCurveInfo> curves_to_slide;
};

class SlideOperation : public CurvesSculptStrokeOperation {
 private:
  float2 initial_brush_pos_re_;
  /** Information about which curves to slide. This is initialized when the brush starts. */
  Vector<SlideInfo> slide_info_;
  /** Positions of all curve points at the start of sliding. */
  Array<float3> initial_positions_cu_;
  /** Deformed positions of all curve points at the start of sliding. */
  Array<float3> initial_deformed_positions_cu_;

  friend struct SlideOperationExecutor;

 public:
  void on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension) override;
};

/**
 * Utility class that actually executes the update when the stroke is updated. That's useful
 * because it avoids passing a very large number of parameters between functions.
 */
struct SlideOperationExecutor {
  SlideOperation *self_ = nullptr;
  CurvesSculptCommonContext ctx_;

  const CurvesSculpt *curves_sculpt_ = nullptr;
  const Brush *brush_ = nullptr;
  float brush_radius_base_re_;
  float brush_radius_factor_;
  float brush_strength_;

  Object *curves_ob_orig_ = nullptr;
  Curves *curves_id_orig_ = nullptr;
  CurvesGeometry *curves_orig_ = nullptr;

  Object *surface_ob_orig_ = nullptr;
  Mesh *surface_orig_ = nullptr;
  Span<MLoopTri> surface_looptris_orig_;
  VArraySpan<float2> surface_uv_map_orig_;
  Span<float3> corner_normals_orig_su_;

  Object *surface_ob_eval_ = nullptr;
  Mesh *surface_eval_ = nullptr;
  Span<float3> surface_positions_eval_;
  Span<int> surface_corner_verts_eval_;
  Span<MLoopTri> surface_looptris_eval_;
  VArraySpan<float2> surface_uv_map_eval_;
  BVHTreeFromMesh surface_bvh_eval_;

  VArray<float> curve_factors_;
  IndexMaskMemory selected_curve_memory_;
  IndexMask curve_selection_;

  float2 brush_pos_re_;

  CurvesSurfaceTransforms transforms_;

  std::atomic<bool> found_invalid_uv_mapping_{false};

  SlideOperationExecutor(const bContext &C) : ctx_(C) {}

  void execute(SlideOperation &self, const bContext &C, const StrokeExtension &stroke_extension)
  {
    UNUSED_VARS(C, stroke_extension);
    self_ = &self;

    curves_ob_orig_ = CTX_data_active_object(&C);
    curves_id_orig_ = static_cast<Curves *>(curves_ob_orig_->data);
    curves_orig_ = &curves_id_orig_->geometry.wrap();
    if (curves_id_orig_->surface == nullptr || curves_id_orig_->surface->type != OB_MESH) {
      report_missing_surface(stroke_extension.reports);
      return;
    }
    if (curves_orig_->curves_num() == 0) {
      return;
    }
    if (curves_id_orig_->surface_uv_map == nullptr) {
      report_missing_uv_map_on_original_surface(stroke_extension.reports);
      return;
    }
    if (curves_orig_->surface_uv_coords().is_empty()) {
      BKE_report(stroke_extension.reports,
                 RPT_WARNING,
                 TIP_("Curves do not have surface attachment information"));
      return;
    }
    const StringRefNull uv_map_name = curves_id_orig_->surface_uv_map;

    curves_sculpt_ = ctx_.scene->toolsettings->curves_sculpt;
    brush_ = BKE_paint_brush_for_read(&curves_sculpt_->paint);
    brush_radius_base_re_ = BKE_brush_size_get(ctx_.scene, brush_);
    brush_radius_factor_ = brush_radius_factor(*brush_, stroke_extension);
    brush_strength_ = brush_strength_get(*ctx_.scene, *brush_, stroke_extension);

    curve_factors_ = *curves_orig_->attributes().lookup_or_default(
        ".selection", ATTR_DOMAIN_CURVE, 1.0f);
    curve_selection_ = curves::retrieve_selected_curves(*curves_id_orig_, selected_curve_memory_);

    brush_pos_re_ = stroke_extension.mouse_position;

    transforms_ = CurvesSurfaceTransforms(*curves_ob_orig_, curves_id_orig_->surface);

    surface_ob_orig_ = curves_id_orig_->surface;
    surface_orig_ = static_cast<Mesh *>(surface_ob_orig_->data);
    if (surface_orig_->faces_num == 0) {
      report_empty_original_surface(stroke_extension.reports);
      return;
    }
    surface_looptris_orig_ = surface_orig_->looptris();
    surface_uv_map_orig_ = *surface_orig_->attributes().lookup<float2>(uv_map_name,
                                                                       ATTR_DOMAIN_CORNER);
    if (surface_uv_map_orig_.is_empty()) {
      report_missing_uv_map_on_original_surface(stroke_extension.reports);
      return;
    }
    if (!CustomData_has_layer(&surface_orig_->loop_data, CD_NORMAL)) {
      BKE_mesh_calc_normals_split(surface_orig_);
    }
    corner_normals_orig_su_ = {reinterpret_cast<const float3 *>(
                                   CustomData_get_layer(&surface_orig_->loop_data, CD_NORMAL)),
                               surface_orig_->totloop};

    surface_ob_eval_ = DEG_get_evaluated_object(ctx_.depsgraph, surface_ob_orig_);
    if (surface_ob_eval_ == nullptr) {
      return;
    }
    surface_eval_ = BKE_object_get_evaluated_mesh(surface_ob_eval_);
    if (surface_eval_ == nullptr) {
      return;
    }
    if (surface_eval_->faces_num == 0) {
      report_empty_evaluated_surface(stroke_extension.reports);
      return;
    }
    surface_looptris_eval_ = surface_eval_->looptris();
    surface_positions_eval_ = surface_eval_->vert_positions();
    surface_corner_verts_eval_ = surface_eval_->corner_verts();
    surface_uv_map_eval_ = *surface_eval_->attributes().lookup<float2>(uv_map_name,
                                                                       ATTR_DOMAIN_CORNER);
    if (surface_uv_map_eval_.is_empty()) {
      report_missing_uv_map_on_evaluated_surface(stroke_extension.reports);
      return;
    }
    BKE_bvhtree_from_mesh_get(&surface_bvh_eval_, surface_eval_, BVHTREE_FROM_LOOPTRI, 2);
    BLI_SCOPED_DEFER([&]() { free_bvhtree_from_mesh(&surface_bvh_eval_); });

    if (stroke_extension.is_first) {
      self_->initial_brush_pos_re_ = brush_pos_re_;
      /* Remember original and deformed positions of all points. Otherwise this information is lost
       * when sliding starts, but it's still used. */
      const bke::crazyspace::GeometryDeformation deformation =
          bke::crazyspace::get_evaluated_curves_deformation(*ctx_.depsgraph, *curves_ob_orig_);
      self_->initial_positions_cu_ = curves_orig_->positions();
      self_->initial_deformed_positions_cu_ = deformation.positions;

      /* First find all curves to slide. When the mouse moves, only those curves  will be moved. */
      this->find_curves_to_slide_with_symmetry();
      return;
    }
    this->slide_with_symmetry();

    if (found_invalid_uv_mapping_) {
      BKE_report(
          stroke_extension.reports, RPT_WARNING, TIP_("UV map or surface attachment is invalid"));
    }

    curves_orig_->tag_positions_changed();
    DEG_id_tag_update(&curves_id_orig_->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &curves_id_orig_->id);
    ED_region_tag_redraw(ctx_.region);
  }

  void find_curves_to_slide_with_symmetry()
  {
    const Vector<float4x4> brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_orig_->symmetry));
    const float brush_radius_re = brush_radius_base_re_ * brush_radius_factor_;
    const std::optional<CurvesBrush3D> brush_3d = sample_curves_surface_3d_brush(*ctx_.depsgraph,
                                                                                 *ctx_.region,
                                                                                 *ctx_.v3d,
                                                                                 transforms_,
                                                                                 surface_bvh_eval_,
                                                                                 brush_pos_re_,
                                                                                 brush_radius_re);
    if (!brush_3d.has_value()) {
      return;
    }
    const ReverseUVSampler reverse_uv_sampler_orig{surface_uv_map_orig_, surface_looptris_orig_};
    for (const float4x4 &brush_transform : brush_transforms) {
      self_->slide_info_.append_as();
      SlideInfo &slide_info = self_->slide_info_.last();
      slide_info.brush_transform = brush_transform;
      this->find_curves_to_slide(math::transform_point(brush_transform, brush_3d->position_cu),
                                 brush_3d->radius_cu,
                                 reverse_uv_sampler_orig,
                                 slide_info.curves_to_slide);
    }
  }

  void find_curves_to_slide(const float3 &brush_pos_cu,
                            const float brush_radius_cu,
                            const ReverseUVSampler &reverse_uv_sampler_orig,
                            Vector<SlideCurveInfo> &r_curves_to_slide)
  {
    const Span<float2> surface_uv_coords = curves_orig_->surface_uv_coords();
    const float brush_radius_sq_cu = pow2f(brush_radius_cu);

    const Span<int> offsets = curves_orig_->offsets();
    curve_selection_.foreach_segment([&](const IndexMaskSegment segment) {
      for (const int curve_i : segment) {
        const int first_point_i = offsets[curve_i];
        const float3 old_pos_cu = self_->initial_deformed_positions_cu_[first_point_i];
        const float dist_to_brush_sq_cu = math::distance_squared(old_pos_cu, brush_pos_cu);
        if (dist_to_brush_sq_cu > brush_radius_sq_cu) {
          /* Root point is too far away from curve center. */
          continue;
        }
        const float dist_to_brush_cu = std::sqrt(dist_to_brush_sq_cu);
        const float radius_falloff = BKE_brush_curve_strength(
            brush_, dist_to_brush_cu, brush_radius_cu);

        const float2 uv = surface_uv_coords[curve_i];
        ReverseUVSampler::Result result = reverse_uv_sampler_orig.sample(uv);
        if (result.type != ReverseUVSampler::ResultType::Ok) {
          /* The curve does not have a valid surface attachment. */
          found_invalid_uv_mapping_.store(true);
          continue;
        }
        /* Compute the normal at the initial surface position. */
        const float3 point_no = geometry::compute_surface_point_normal(
            surface_looptris_orig_[result.looptri_index],
            result.bary_weights,
            corner_normals_orig_su_);
        const float3 normal_cu = math::normalize(
            math::transform_point(transforms_.surface_to_curves_normal, point_no));

        r_curves_to_slide.append({curve_i, radius_falloff, normal_cu});
      }
    });
  }

  void slide_with_symmetry()
  {
    const ReverseUVSampler reverse_uv_sampler_orig{surface_uv_map_orig_, surface_looptris_orig_};
    for (const SlideInfo &slide_info : self_->slide_info_) {
      this->slide(slide_info.curves_to_slide, reverse_uv_sampler_orig, slide_info.brush_transform);
    }
  }

  void slide(const Span<SlideCurveInfo> slide_curves,
             const ReverseUVSampler &reverse_uv_sampler_orig,
             const float4x4 &brush_transform)
  {
    const float4x4 brush_transform_inv = math::invert(brush_transform);

    const Span<float3> positions_orig_su = surface_orig_->vert_positions();
    const Span<int> corner_verts_orig = surface_orig_->corner_verts();
    const OffsetIndices points_by_curve = curves_orig_->points_by_curve();

    MutableSpan<float3> positions_orig_cu = curves_orig_->positions_for_write();
    MutableSpan<float2> surface_uv_coords = curves_orig_->surface_uv_coords_for_write();

    float4x4 projection;
    ED_view3d_ob_project_mat_get(ctx_.rv3d, curves_ob_orig_, projection.ptr());

    const float2 brush_pos_diff_re = brush_pos_re_ - self_->initial_brush_pos_re_;

    /* The brush transformation has to be applied in curves space. */
    const float4x4 world_to_surface_with_symmetry_mat = transforms_.curves_to_surface *
                                                        brush_transform *
                                                        transforms_.world_to_curves;

    threading::parallel_for(slide_curves.index_range(), 256, [&](const IndexRange range) {
      for (const SlideCurveInfo &slide_curve_info : slide_curves.slice(range)) {
        const int curve_i = slide_curve_info.curve_i;
        const IndexRange points = points_by_curve[curve_i];
        const int first_point_i = points[0];

        const float3 old_first_pos_eval_cu = self_->initial_deformed_positions_cu_[first_point_i];
        const float3 old_first_symm_pos_eval_cu = math::transform_point(brush_transform_inv,
                                                                        old_first_pos_eval_cu);
        const float3 old_first_pos_eval_su = math::transform_point(transforms_.curves_to_surface,
                                                                   old_first_pos_eval_cu);

        float2 old_first_symm_pos_eval_re;
        ED_view3d_project_float_v2_m4(
            ctx_.region, old_first_symm_pos_eval_cu, old_first_symm_pos_eval_re, projection.ptr());

        const float radius_falloff = slide_curve_info.radius_falloff;
        const float curve_weight = brush_strength_ * radius_falloff * curve_factors_[curve_i];
        const float2 new_first_symm_pos_eval_re = old_first_symm_pos_eval_re +
                                                  curve_weight * brush_pos_diff_re;

        /* Compute the ray that will be used to find the new position on the surface. */
        float3 ray_start_wo, ray_end_wo;
        ED_view3d_win_to_segment_clipped(ctx_.depsgraph,
                                         ctx_.region,
                                         ctx_.v3d,
                                         new_first_symm_pos_eval_re,
                                         ray_start_wo,
                                         ray_end_wo,
                                         true);
        const float3 ray_start_su = math::transform_point(world_to_surface_with_symmetry_mat,
                                                          ray_start_wo);
        const float3 ray_end_su = math::transform_point(world_to_surface_with_symmetry_mat,
                                                        ray_end_wo);
        const float3 ray_direction_su = math::normalize(ray_end_su - ray_start_su);

        /* Find the ray hit that is closest to the initial curve root position. */
        int looptri_index_eval;
        float3 hit_pos_eval_su;
        if (!this->find_closest_ray_hit(ray_start_su,
                                        ray_direction_su,
                                        old_first_pos_eval_su,
                                        looptri_index_eval,
                                        hit_pos_eval_su))
        {
          continue;
        }

        /* Compute the uv of the new surface position on the evaluated mesh. */
        const MLoopTri &looptri_eval = surface_looptris_eval_[looptri_index_eval];
        const float3 bary_weights_eval = bke::mesh_surface_sample::compute_bary_coord_in_triangle(
            surface_positions_eval_, surface_corner_verts_eval_, looptri_eval, hit_pos_eval_su);
        const float2 uv = bke::attribute_math::mix3(bary_weights_eval,
                                                    surface_uv_map_eval_[looptri_eval.tri[0]],
                                                    surface_uv_map_eval_[looptri_eval.tri[1]],
                                                    surface_uv_map_eval_[looptri_eval.tri[2]]);

        /* Try to find the same uv on the original surface. */
        const ReverseUVSampler::Result result = reverse_uv_sampler_orig.sample(uv);
        if (result.type != ReverseUVSampler::ResultType::Ok) {
          found_invalid_uv_mapping_.store(true);
          continue;
        }
        const MLoopTri &looptri_orig = surface_looptris_orig_[result.looptri_index];
        const float3 &bary_weights_orig = result.bary_weights;

        /* Gather old and new surface normal. */
        const float3 &initial_normal_cu = slide_curve_info.initial_normal_cu;
        const float3 new_normal_cu = math::normalize(math::transform_point(
            transforms_.surface_to_curves_normal,
            geometry::compute_surface_point_normal(
                looptri_orig, result.bary_weights, corner_normals_orig_su_)));

        /* Gather old and new surface position. */
        const float3 new_first_pos_orig_su = bke::attribute_math::mix3<float3>(
            bary_weights_orig,
            positions_orig_su[corner_verts_orig[looptri_orig.tri[0]]],
            positions_orig_su[corner_verts_orig[looptri_orig.tri[1]]],
            positions_orig_su[corner_verts_orig[looptri_orig.tri[2]]]);
        const float3 old_first_pos_orig_cu = self_->initial_positions_cu_[first_point_i];
        const float3 new_first_pos_orig_cu = math::transform_point(transforms_.surface_to_curves,
                                                                   new_first_pos_orig_su);

        /* Actually transform curve points. */
        const float4x4 slide_transform = this->get_slide_transform(
            old_first_pos_orig_cu, new_first_pos_orig_cu, initial_normal_cu, new_normal_cu);
        for (const int point_i : points) {
          positions_orig_cu[point_i] = math::transform_point(
              slide_transform, self_->initial_positions_cu_[point_i]);
        }
        surface_uv_coords[curve_i] = uv;
      }
    });
  }

  bool find_closest_ray_hit(const float3 &ray_start_su,
                            const float3 &ray_direction_su,
                            const float3 &point_su,
                            int &r_looptri_index,
                            float3 &r_hit_pos)
  {
    float best_dist_sq_su = FLT_MAX;
    int best_looptri_index_eval;
    float3 best_hit_pos_su;
    BLI_bvhtree_ray_cast_all_cpp(
        *surface_bvh_eval_.tree,
        ray_start_su,
        ray_direction_su,
        0.0f,
        FLT_MAX,
        [&](const int looptri_index, const BVHTreeRay &ray, BVHTreeRayHit &hit) {
          surface_bvh_eval_.raycast_callback(&surface_bvh_eval_, looptri_index, &ray, &hit);
          if (hit.index < 0) {
            return;
          }
          const float3 &hit_pos_su = hit.co;
          const float dist_sq_su = math::distance_squared(hit_pos_su, point_su);
          if (dist_sq_su < best_dist_sq_su) {
            best_dist_sq_su = dist_sq_su;
            best_hit_pos_su = hit_pos_su;
            best_looptri_index_eval = hit.index;
          }
        });

    if (best_dist_sq_su == FLT_MAX) {
      return false;
    }
    r_looptri_index = best_looptri_index_eval;
    r_hit_pos = best_hit_pos_su;
    return true;
  }

  float4x4 get_slide_transform(const float3 &old_root_pos,
                               const float3 &new_root_pos,
                               const float3 &old_normal,
                               const float3 &new_normal)
  {
    float3x3 rotation_3x3;
    rotation_between_vecs_to_mat3(rotation_3x3.ptr(), old_normal, new_normal);

    float4x4 transform = float4x4::identity();
    transform.location() -= old_root_pos;
    transform = float4x4(rotation_3x3) * transform;
    transform.location() += new_root_pos;
    return transform;
  }
};

void SlideOperation::on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension)
{
  SlideOperationExecutor executor{C};
  executor.execute(*this, C, stroke_extension);
}

std::unique_ptr<CurvesSculptStrokeOperation> new_slide_operation()
{
  return std::make_unique<SlideOperation>();
}

}  // namespace blender::ed::sculpt_paint
