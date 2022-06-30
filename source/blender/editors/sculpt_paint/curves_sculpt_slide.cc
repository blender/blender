/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>

#include "curves_sculpt_intern.hh"

#include "BLI_float4x4.hh"
#include "BLI_vector.hh"

#include "PIL_time.h"

#include "DEG_depsgraph.h"

#include "BKE_attribute_math.hh"
#include "BKE_brush.h"
#include "BKE_bvhutils.h"
#include "BKE_context.h"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_sample.hh"
#include "BKE_paint.h"

#include "DNA_brush_enums.h"
#include "DNA_brush_types.h"
#include "DNA_curves_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "UI_interface.h"

#include "WM_api.h"

namespace blender::ed::sculpt_paint {

struct SlideCurveInfo {
  /** Index of the curve to slide. */
  int curve_i;
  /** A weight based on the initial distance to the brush. */
  float radius_falloff;
};

struct SlideInfo {
  /** The transform used for the curves below (e.g. for symmetry). */
  float4x4 brush_transform;
  Vector<SlideCurveInfo> curves_to_slide;
};

class SlideOperation : public CurvesSculptStrokeOperation {
 private:
  /** Last mouse position. */
  float2 brush_pos_last_re_;
  /** Information about which curves to slide. This is initialized when the brush starts. */
  Vector<SlideInfo> slide_info_;

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

  Object *object_ = nullptr;
  Curves *curves_id_ = nullptr;
  CurvesGeometry *curves_ = nullptr;

  Object *surface_ob_ = nullptr;
  Mesh *surface_ = nullptr;
  Span<MLoopTri> surface_looptris_;
  VArray_Span<float2> surface_uv_map_;

  VArray<float> curve_factors_;
  VArray<float> point_factors_;
  Vector<int64_t> selected_curve_indices_;
  IndexMask curve_selection_;

  float2 brush_pos_prev_re_;
  float2 brush_pos_re_;
  float2 brush_pos_diff_re_;

  CurvesSculptTransforms transforms_;

  BVHTreeFromMesh surface_bvh_;

  SlideOperationExecutor(const bContext &C) : ctx_(C)
  {
  }

  void execute(SlideOperation &self, const bContext &C, const StrokeExtension &stroke_extension)
  {
    UNUSED_VARS(C, stroke_extension);
    self_ = &self;

    object_ = CTX_data_active_object(&C);
    curves_id_ = static_cast<Curves *>(object_->data);
    curves_ = &CurvesGeometry::wrap(curves_id_->geometry);
    if (curves_id_->surface == nullptr || curves_id_->surface->type != OB_MESH) {
      return;
    }
    if (curves_->curves_num() == 0) {
      return;
    }

    curves_sculpt_ = ctx_.scene->toolsettings->curves_sculpt;
    brush_ = BKE_paint_brush_for_read(&curves_sculpt_->paint);
    brush_radius_base_re_ = BKE_brush_size_get(ctx_.scene, brush_);
    brush_radius_factor_ = brush_radius_factor(*brush_, stroke_extension);
    brush_strength_ = brush_strength_get(*ctx_.scene, *brush_, stroke_extension);

    curve_factors_ = get_curves_selection(*curves_id_);
    point_factors_ = get_point_selection(*curves_id_);
    curve_selection_ = retrieve_selected_curves(*curves_id_, selected_curve_indices_);

    brush_pos_prev_re_ = self_->brush_pos_last_re_;
    brush_pos_re_ = stroke_extension.mouse_position;
    brush_pos_diff_re_ = brush_pos_re_ - brush_pos_prev_re_;
    BLI_SCOPED_DEFER([&]() { self_->brush_pos_last_re_ = brush_pos_re_; });

    transforms_ = CurvesSculptTransforms(*object_, curves_id_->surface);

    surface_ob_ = curves_id_->surface;
    surface_ = static_cast<Mesh *>(surface_ob_->data);

    BKE_bvhtree_from_mesh_get(&surface_bvh_, surface_, BVHTREE_FROM_LOOPTRI, 2);
    BLI_SCOPED_DEFER([&]() { free_bvhtree_from_mesh(&surface_bvh_); });

    surface_looptris_ = {BKE_mesh_runtime_looptri_ensure(surface_),
                         BKE_mesh_runtime_looptri_len(surface_)};

    if (curves_id_->surface_uv_map != nullptr) {
      MeshComponent surface_component;
      surface_component.replace(surface_, GeometryOwnershipType::ReadOnly);
      surface_uv_map_ = surface_component
                            .attribute_try_get_for_read(curves_id_->surface_uv_map,
                                                        ATTR_DOMAIN_CORNER)
                            .typed<float2>();
    }

    if (stroke_extension.is_first) {
      const Vector<float4x4> brush_transforms = get_symmetry_brush_transforms(
          eCurvesSymmetryType(curves_id_->symmetry));
      for (const float4x4 &brush_transform : brush_transforms) {
        this->detect_curves_to_slide(brush_transform);
      }
      return;
    }
    this->slide_projected();

    curves_->tag_positions_changed();
    DEG_id_tag_update(&curves_id_->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &curves_id_->id);
    ED_region_tag_redraw(ctx_.region);
  }

  void detect_curves_to_slide(const float4x4 &brush_transform)
  {
    const float4x4 brush_transform_inv = brush_transform.inverted();

    const float brush_radius_re = brush_radius_base_re_ * brush_radius_factor_;
    const float brush_radius_sq_re = pow2f(brush_radius_re);

    const Span<float3> positions_cu = curves_->positions();

    float4x4 projection;
    ED_view3d_ob_project_mat_get(ctx_.rv3d, object_, projection.values);

    self_->slide_info_.append({brush_transform});
    Vector<SlideCurveInfo> &curves_to_slide = self_->slide_info_.last().curves_to_slide;

    /* Find curves in brush radius that should be moved. */
    for (const int curve_i : curve_selection_) {
      const int first_point_i = curves_->offsets()[curve_i];
      const float3 &first_pos_cu = brush_transform_inv * positions_cu[first_point_i];

      float2 first_pos_re;
      ED_view3d_project_float_v2_m4(ctx_.region, first_pos_cu, first_pos_re, projection.values);

      const float dist_to_brush_sq_re = math::distance_squared(first_pos_re, brush_pos_re_);
      if (dist_to_brush_sq_re > brush_radius_sq_re) {
        continue;
      }
      const float dist_to_brush_re = std::sqrt(dist_to_brush_sq_re);
      const float radius_falloff = BKE_brush_curve_strength(
          brush_, dist_to_brush_re, brush_radius_re);
      curves_to_slide.append({curve_i, radius_falloff});
    }
  }

  void slide_projected()
  {
    MutableSpan<float3> positions_cu = curves_->positions_for_write();

    MutableSpan<float2> surface_uv_coords;
    if (!surface_uv_map_.is_empty()) {
      surface_uv_coords = curves_->surface_uv_coords_for_write();
    }

    float4x4 projection;
    ED_view3d_ob_project_mat_get(ctx_.rv3d, object_, projection.values);

    for (const SlideInfo &slide_info : self_->slide_info_) {
      const float4x4 &brush_transform = slide_info.brush_transform;
      const float4x4 brush_transform_inv = brush_transform.inverted();
      const Span<SlideCurveInfo> curves_to_slide = slide_info.curves_to_slide;

      threading::parallel_for(curves_to_slide.index_range(), 256, [&](const IndexRange range) {
        for (const SlideCurveInfo &curve_slide_info : curves_to_slide.slice(range)) {
          const int curve_i = curve_slide_info.curve_i;
          const IndexRange points = curves_->points_for_curve(curve_i);
          const int first_point_i = points.first();
          const float3 old_first_pos_cu = brush_transform_inv * positions_cu[first_point_i];

          float2 old_first_pos_re;
          ED_view3d_project_float_v2_m4(
              ctx_.region, old_first_pos_cu, old_first_pos_re, projection.values);
          const float first_point_weight = brush_strength_ * curve_slide_info.radius_falloff;

          /* Slide root position in region space and then project it back onto the surface. */
          const float2 new_first_pos_re = old_first_pos_re +
                                          first_point_weight * brush_pos_diff_re_;

          float3 ray_start_wo, ray_end_wo;
          ED_view3d_win_to_segment_clipped(ctx_.depsgraph,
                                           ctx_.region,
                                           ctx_.v3d,
                                           new_first_pos_re,
                                           ray_start_wo,
                                           ray_end_wo,
                                           true);
          const float3 ray_start_su = transforms_.world_to_surface * ray_start_wo;
          const float3 ray_end_su = transforms_.world_to_surface * ray_end_wo;

          const float3 ray_direction_su = math::normalize(ray_end_su - ray_start_su);
          BVHTreeRayHit hit;
          hit.dist = FLT_MAX;
          hit.index = -1;
          BLI_bvhtree_ray_cast(surface_bvh_.tree,
                               ray_start_su,
                               ray_direction_su,
                               0.0f,
                               &hit,
                               surface_bvh_.raycast_callback,
                               &surface_bvh_);
          if (hit.index == -1) {
            continue;
          }

          const int looptri_index = hit.index;
          const float3 attached_pos_su = hit.co;

          const float3 attached_pos_cu = transforms_.surface_to_curves * attached_pos_su;
          const float3 pos_offset_cu = brush_transform * (attached_pos_cu - old_first_pos_cu);

          /* Update positions. The first point doesn't have an additional weight here, because then
           * it wouldn't be attached to the surface anymore. */
          positions_cu[first_point_i] += pos_offset_cu;
          for (const int point_i : points.drop_front(1)) {
            const float weight = point_factors_[point_i];
            positions_cu[point_i] += weight * pos_offset_cu;
          }

          /* Update surface attachment information if necessary. */
          if (!surface_uv_map_.is_empty()) {
            const MLoopTri &looptri = surface_looptris_[looptri_index];
            const float3 bary_coord = bke::mesh_surface_sample::compute_bary_coord_in_triangle(
                *surface_, looptri, attached_pos_su);
            const float2 &uv0 = surface_uv_map_[looptri.tri[0]];
            const float2 &uv1 = surface_uv_map_[looptri.tri[1]];
            const float2 &uv2 = surface_uv_map_[looptri.tri[2]];
            const float2 uv = attribute_math::mix3(bary_coord, uv0, uv1, uv2);
            surface_uv_coords[curve_i] = uv;
          }
        }
      });
    }
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
