/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>

#include "curves_sculpt_intern.hh"

#include "BLI_float4x4.hh"
#include "BLI_index_mask_ops.hh"
#include "BLI_kdtree.h"
#include "BLI_length_parameterize.hh"
#include "BLI_rand.hh"
#include "BLI_vector.hh"

#include "PIL_time.h"

#include "DEG_depsgraph.h"

#include "BKE_attribute_math.hh"
#include "BKE_brush.h"
#include "BKE_bvhutils.h"
#include "BKE_context.h"
#include "BKE_curves.hh"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
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

#include "WM_api.h"

/**
 * The code below uses a prefix naming convention to indicate the coordinate space:
 * - `cu`: Local space of the curves object that is being edited.
 * - `su`: Local space of the surface object.
 * - `wo`: World space.
 * - `re`: 2D coordinates within the region.
 */

namespace blender::ed::sculpt_paint {

using blender::bke::CurvesGeometry;

/**
 * Drags the tip point of each curve and resamples the rest of the curve.
 */
class SnakeHookOperation : public CurvesSculptStrokeOperation {
 private:
  float2 last_mouse_position_re_;

  CurvesBrush3D brush_3d_;

  friend struct SnakeHookOperatorExecutor;

 public:
  void on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension) override;
};

/**
 * Utility class that actually executes the update when the stroke is updated. That's useful
 * because it avoids passing a very large number of parameters between functions.
 */
struct SnakeHookOperatorExecutor {
  SnakeHookOperation *self_ = nullptr;
  CurvesSculptCommonContext ctx_;

  const CurvesSculpt *curves_sculpt_ = nullptr;
  const Brush *brush_ = nullptr;
  float brush_radius_base_re_;
  float brush_radius_factor_;
  float brush_strength_;

  eBrushFalloffShape falloff_shape_;

  Object *object_ = nullptr;
  Curves *curves_id_ = nullptr;
  CurvesGeometry *curves_ = nullptr;

  VArray<float> curve_factors_;
  Vector<int64_t> selected_curve_indices_;
  IndexMask curve_selection_;

  CurvesSurfaceTransforms transforms_;

  float2 brush_pos_prev_re_;
  float2 brush_pos_re_;
  float2 brush_pos_diff_re_;

  SnakeHookOperatorExecutor(const bContext &C) : ctx_(C)
  {
  }

  void execute(SnakeHookOperation &self,
               const bContext &C,
               const StrokeExtension &stroke_extension)
  {
    BLI_SCOPED_DEFER([&]() { self.last_mouse_position_re_ = stroke_extension.mouse_position; });

    self_ = &self;
    object_ = CTX_data_active_object(&C);

    curves_sculpt_ = ctx_.scene->toolsettings->curves_sculpt;
    brush_ = BKE_paint_brush_for_read(&curves_sculpt_->paint);

    brush_radius_base_re_ = BKE_brush_size_get(ctx_.scene, brush_);
    brush_radius_factor_ = brush_radius_factor(*brush_, stroke_extension);
    brush_strength_ = brush_strength_get(*ctx_.scene, *brush_, stroke_extension);

    falloff_shape_ = static_cast<eBrushFalloffShape>(brush_->falloff_shape);

    curves_id_ = static_cast<Curves *>(object_->data);
    curves_ = &CurvesGeometry::wrap(curves_id_->geometry);
    if (curves_->curves_num() == 0) {
      return;
    }

    transforms_ = CurvesSurfaceTransforms(*object_, curves_id_->surface);

    curve_factors_ = curves_->attributes().lookup_or_default(
        ".selection", ATTR_DOMAIN_CURVE, 1.0f);
    curve_selection_ = curves::retrieve_selected_curves(*curves_id_, selected_curve_indices_);

    brush_pos_prev_re_ = self.last_mouse_position_re_;
    brush_pos_re_ = stroke_extension.mouse_position;
    brush_pos_diff_re_ = brush_pos_re_ - brush_pos_prev_re_;

    if (stroke_extension.is_first) {
      if (falloff_shape_ == PAINT_FALLOFF_SHAPE_SPHERE) {
        std::optional<CurvesBrush3D> brush_3d = sample_curves_3d_brush(*ctx_.depsgraph,
                                                                       *ctx_.region,
                                                                       *ctx_.v3d,
                                                                       *ctx_.rv3d,
                                                                       *object_,
                                                                       brush_pos_re_,
                                                                       brush_radius_base_re_);
        if (brush_3d.has_value()) {
          self_->brush_3d_ = *brush_3d;
        }
      }
      return;
    }

    if (falloff_shape_ == PAINT_FALLOFF_SHAPE_SPHERE) {
      this->spherical_snake_hook_with_symmetry();
    }
    else if (falloff_shape_ == PAINT_FALLOFF_SHAPE_TUBE) {
      this->projected_snake_hook_with_symmetry();
    }
    else {
      BLI_assert_unreachable();
    }

    curves_->tag_positions_changed();
    DEG_id_tag_update(&curves_id_->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &curves_id_->id);
    ED_region_tag_redraw(ctx_.region);
  }

  void projected_snake_hook_with_symmetry()
  {
    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      this->projected_snake_hook(brush_transform);
    }
  }

  void projected_snake_hook(const float4x4 &brush_transform)
  {
    const float4x4 brush_transform_inv = brush_transform.inverted();
    const bke::crazyspace::GeometryDeformation deformation =
        bke::crazyspace::get_evaluated_curves_deformation(*ctx_.depsgraph, *object_);

    MutableSpan<float3> positions_cu = curves_->positions_for_write();

    float4x4 projection;
    ED_view3d_ob_project_mat_get(ctx_.rv3d, object_, projection.values);

    const float brush_radius_re = brush_radius_base_re_ * brush_radius_factor_;
    const float brush_radius_sq_re = pow2f(brush_radius_re);

    threading::parallel_for(curves_->curves_range(), 256, [&](const IndexRange curves_range) {
      MoveAndResampleBuffers resample_buffer;
      for (const int curve_i : curves_range) {
        const IndexRange points = curves_->points_for_curve(curve_i);
        const int last_point_i = points.last();
        const float3 old_pos_cu = deformation.positions[last_point_i];
        const float3 old_symm_pos_cu = brush_transform_inv * old_pos_cu;

        float2 old_symm_pos_re;
        ED_view3d_project_float_v2_m4(
            ctx_.region, old_symm_pos_cu, old_symm_pos_re, projection.values);

        const float distance_to_brush_sq_re = math::distance_squared(old_symm_pos_re,
                                                                     brush_pos_prev_re_);
        if (distance_to_brush_sq_re > brush_radius_sq_re) {
          continue;
        }

        const float radius_falloff = BKE_brush_curve_strength(
            brush_, std::sqrt(distance_to_brush_sq_re), brush_radius_re);
        const float weight = brush_strength_ * radius_falloff * curve_factors_[curve_i];

        const float2 new_symm_pos_re = old_symm_pos_re + brush_pos_diff_re_ * weight;
        float3 new_symm_pos_wo;
        ED_view3d_win_to_3d(ctx_.v3d,
                            ctx_.region,
                            transforms_.curves_to_world * old_symm_pos_cu,
                            new_symm_pos_re,
                            new_symm_pos_wo);
        const float3 new_pos_cu = brush_transform *
                                  (transforms_.world_to_curves * new_symm_pos_wo);
        const float3 translation_eval = new_pos_cu - old_pos_cu;
        const float3 translation_orig = deformation.translation_from_deformed_to_original(
            last_point_i, translation_eval);

        const float3 last_point_cu = positions_cu[last_point_i] + translation_orig;
        move_last_point_and_resample(resample_buffer, positions_cu.slice(points), last_point_cu);
      }
    });
  }

  void spherical_snake_hook_with_symmetry()
  {
    float4x4 projection;
    ED_view3d_ob_project_mat_get(ctx_.rv3d, object_, projection.values);

    float3 brush_start_wo, brush_end_wo;
    ED_view3d_win_to_3d(ctx_.v3d,
                        ctx_.region,
                        transforms_.curves_to_world * self_->brush_3d_.position_cu,
                        brush_pos_prev_re_,
                        brush_start_wo);
    ED_view3d_win_to_3d(ctx_.v3d,
                        ctx_.region,
                        transforms_.curves_to_world * self_->brush_3d_.position_cu,
                        brush_pos_re_,
                        brush_end_wo);
    const float3 brush_start_cu = transforms_.world_to_curves * brush_start_wo;
    const float3 brush_end_cu = transforms_.world_to_curves * brush_end_wo;

    const float brush_radius_cu = self_->brush_3d_.radius_cu * brush_radius_factor_;

    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      this->spherical_snake_hook(
          brush_transform * brush_start_cu, brush_transform * brush_end_cu, brush_radius_cu);
    }
  }

  void spherical_snake_hook(const float3 &brush_start_cu,
                            const float3 &brush_end_cu,
                            const float brush_radius_cu)
  {
    const bke::crazyspace::GeometryDeformation deformation =
        bke::crazyspace::get_evaluated_curves_deformation(*ctx_.depsgraph, *object_);

    MutableSpan<float3> positions_cu = curves_->positions_for_write();
    const float3 brush_diff_cu = brush_end_cu - brush_start_cu;
    const float brush_radius_sq_cu = pow2f(brush_radius_cu);

    threading::parallel_for(curves_->curves_range(), 256, [&](const IndexRange curves_range) {
      MoveAndResampleBuffers resample_buffer;
      for (const int curve_i : curves_range) {
        const IndexRange points = curves_->points_for_curve(curve_i);
        const int last_point_i = points.last();
        const float3 old_pos_cu = deformation.positions[last_point_i];

        const float distance_to_brush_sq_cu = dist_squared_to_line_segment_v3(
            old_pos_cu, brush_start_cu, brush_end_cu);
        if (distance_to_brush_sq_cu > brush_radius_sq_cu) {
          continue;
        }

        const float distance_to_brush_cu = std::sqrt(distance_to_brush_sq_cu);

        const float radius_falloff = BKE_brush_curve_strength(
            brush_, distance_to_brush_cu, brush_radius_cu);
        const float weight = brush_strength_ * radius_falloff * curve_factors_[curve_i];

        const float3 translation_eval = weight * brush_diff_cu;
        const float3 translation_orig = deformation.translation_from_deformed_to_original(
            last_point_i, translation_eval);

        const float3 last_point_cu = positions_cu[last_point_i] + translation_orig;
        move_last_point_and_resample(resample_buffer, positions_cu.slice(points), last_point_cu);
      }
    });
  }
};

void SnakeHookOperation::on_stroke_extended(const bContext &C,
                                            const StrokeExtension &stroke_extension)
{
  SnakeHookOperatorExecutor executor{C};
  executor.execute(*this, C, stroke_extension);
}

std::unique_ptr<CurvesSculptStrokeOperation> new_snake_hook_operation()
{
  return std::make_unique<SnakeHookOperation>();
}

}  // namespace blender::ed::sculpt_paint
