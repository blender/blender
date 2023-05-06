/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>

#include "curves_sculpt_intern.hh"

#include "BLI_index_mask_ops.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "PIL_time.h"

#include "DEG_depsgraph.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_curves.hh"
#include "BKE_paint.h"

#include "DNA_brush_enums.h"
#include "DNA_brush_types.h"
#include "DNA_curves_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "WM_api.h"

/**
 * The code below uses a prefix naming convention to indicate the coordinate space:
 * `cu`: Local space of the curves object that is being edited.
 * `su`: Local space of the surface object.
 * `wo`: World space.
 * `re`: 2D coordinates within the region.
 */

namespace blender::ed::sculpt_paint {

class PinchOperation : public CurvesSculptStrokeOperation {
 private:
  bool invert_pinch_;

  /** Solver for length and collision constraints. */
  CurvesConstraintSolver constraint_solver_;

  /** Only used when a 3D brush is used. */
  CurvesBrush3D brush_3d_;

  friend struct PinchOperationExecutor;

 public:
  PinchOperation(const bool invert_pinch) : invert_pinch_(invert_pinch) {}

  void on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension) override;
};

struct PinchOperationExecutor {
  PinchOperation *self_ = nullptr;
  CurvesSculptCommonContext ctx_;

  Object *object_ = nullptr;
  Curves *curves_id_ = nullptr;
  CurvesGeometry *curves_ = nullptr;

  VArray<float> point_factors_;
  Vector<int64_t> selected_curve_indices_;
  IndexMask curve_selection_;

  CurvesSurfaceTransforms transforms_;

  const CurvesSculpt *curves_sculpt_ = nullptr;
  const Brush *brush_ = nullptr;
  float brush_radius_base_re_;
  float brush_radius_factor_;
  float brush_strength_;

  float invert_factor_;

  float2 brush_pos_re_;

  PinchOperationExecutor(const bContext &C) : ctx_(C) {}

  void execute(PinchOperation &self, const bContext &C, const StrokeExtension &stroke_extension)
  {
    self_ = &self;

    object_ = CTX_data_active_object(&C);
    curves_id_ = static_cast<Curves *>(object_->data);
    curves_ = &curves_id_->geometry.wrap();
    if (curves_->curves_num() == 0) {
      return;
    }

    curves_sculpt_ = ctx_.scene->toolsettings->curves_sculpt;
    brush_ = BKE_paint_brush_for_read(&curves_sculpt_->paint);
    brush_radius_base_re_ = BKE_brush_size_get(ctx_.scene, brush_);
    brush_radius_factor_ = brush_radius_factor(*brush_, stroke_extension);
    brush_strength_ = BKE_brush_alpha_get(ctx_.scene, brush_);

    invert_factor_ = self_->invert_pinch_ ? -1.0f : 1.0f;

    transforms_ = CurvesSurfaceTransforms(*object_, curves_id_->surface);

    point_factors_ = *curves_->attributes().lookup_or_default<float>(
        ".selection", ATTR_DOMAIN_POINT, 1.0f);
    curve_selection_ = curves::retrieve_selected_curves(*curves_id_, selected_curve_indices_);

    brush_pos_re_ = stroke_extension.mouse_position;
    const eBrushFalloffShape falloff_shape = static_cast<eBrushFalloffShape>(
        brush_->falloff_shape);

    if (stroke_extension.is_first) {
      if (falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
        self_->brush_3d_ = *sample_curves_3d_brush(*ctx_.depsgraph,
                                                   *ctx_.region,
                                                   *ctx_.v3d,
                                                   *ctx_.rv3d,
                                                   *object_,
                                                   brush_pos_re_,
                                                   brush_radius_base_re_);
      }

      self_->constraint_solver_.initialize(
          *curves_, curve_selection_, curves_id_->flag & CV_SCULPT_COLLISION_ENABLED);
    }

    Array<bool> changed_curves(curves_->curves_num(), false);
    if (falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
      this->pinch_projected_with_symmetry(changed_curves);
    }
    else if (falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
      this->pinch_spherical_with_symmetry(changed_curves);
    }
    else {
      BLI_assert_unreachable();
    }

    Vector<int64_t> indices;
    const IndexMask changed_curves_mask = index_mask_ops::find_indices_from_array(changed_curves,
                                                                                  indices);
    const Mesh *surface = curves_id_->surface && curves_id_->surface->type == OB_MESH ?
                              static_cast<const Mesh *>(curves_id_->surface->data) :
                              nullptr;
    self_->constraint_solver_.solve_step(*curves_, changed_curves_mask, surface, transforms_);

    curves_->tag_positions_changed();
    DEG_id_tag_update(&curves_id_->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &curves_id_->id);
    ED_region_tag_redraw(ctx_.region);
  }

  void pinch_projected_with_symmetry(MutableSpan<bool> r_changed_curves)
  {
    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      this->pinch_projected(brush_transform, r_changed_curves);
    }
  }

  void pinch_projected(const float4x4 &brush_transform, MutableSpan<bool> r_changed_curves)
  {
    const float4x4 brush_transform_inv = math::invert(brush_transform);

    const bke::crazyspace::GeometryDeformation deformation =
        bke::crazyspace::get_evaluated_curves_deformation(*ctx_.depsgraph, *object_);
    const OffsetIndices points_by_curve = curves_->points_by_curve();

    float4x4 projection;
    ED_view3d_ob_project_mat_get(ctx_.rv3d, object_, projection.ptr());
    MutableSpan<float3> positions_cu = curves_->positions_for_write();
    const float brush_radius_re = brush_radius_base_re_ * brush_radius_factor_;
    const float brush_radius_sq_re = pow2f(brush_radius_re);

    threading::parallel_for(curve_selection_.index_range(), 256, [&](const IndexRange range) {
      for (const int curve_i : curve_selection_.slice(range)) {
        const IndexRange points = points_by_curve[curve_i];
        for (const int point_i : points.drop_front(1)) {
          const float3 old_pos_cu = deformation.positions[point_i];
          const float3 old_symm_pos_cu = math::transform_point(brush_transform_inv, old_pos_cu);
          float2 old_symm_pos_re;
          ED_view3d_project_float_v2_m4(
              ctx_.region, old_symm_pos_cu, old_symm_pos_re, projection.ptr());

          const float dist_to_brush_sq_re = math::distance_squared(old_symm_pos_re, brush_pos_re_);
          if (dist_to_brush_sq_re > brush_radius_sq_re) {
            continue;
          }

          const float dist_to_brush_re = std::sqrt(dist_to_brush_sq_re);
          const float t = safe_divide(dist_to_brush_re, brush_radius_base_re_);
          const float radius_falloff = t * BKE_brush_curve_strength(brush_, t, 1.0f);
          const float weight = invert_factor_ * 0.1f * brush_strength_ * radius_falloff *
                               point_factors_[point_i];

          const float2 new_symm_pos_re = math::interpolate(old_symm_pos_re, brush_pos_re_, weight);

          float3 new_symm_pos_wo;
          ED_view3d_win_to_3d(ctx_.v3d,
                              ctx_.region,
                              math::transform_point(transforms_.curves_to_world, old_symm_pos_cu),
                              new_symm_pos_re,
                              new_symm_pos_wo);

          float3 new_pos_cu = math::transform_point(transforms_.world_to_curves, new_symm_pos_wo);
          new_pos_cu = math::transform_point(brush_transform, new_pos_cu);
          const float3 translation_eval = new_pos_cu - old_pos_cu;
          const float3 translation_orig = deformation.translation_from_deformed_to_original(
              point_i, translation_eval);
          positions_cu[point_i] += translation_orig;
          r_changed_curves[curve_i] = true;
        }
      }
    });
  }

  void pinch_spherical_with_symmetry(MutableSpan<bool> r_changed_curves)
  {
    float3 brush_pos_wo;
    ED_view3d_win_to_3d(
        ctx_.v3d,
        ctx_.region,
        math::transform_point(transforms_.curves_to_world, self_->brush_3d_.position_cu),
        brush_pos_re_,
        brush_pos_wo);
    const float3 brush_pos_cu = math::transform_point(transforms_.world_to_curves, brush_pos_wo);
    const float brush_radius_cu = self_->brush_3d_.radius_cu * brush_radius_factor_;

    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      this->pinch_spherical(
          math::transform_point(brush_transform, brush_pos_cu), brush_radius_cu, r_changed_curves);
    }
  }

  void pinch_spherical(const float3 &brush_pos_cu,
                       const float brush_radius_cu,
                       MutableSpan<bool> r_changed_curves)
  {
    MutableSpan<float3> positions_cu = curves_->positions_for_write();
    const float brush_radius_sq_cu = pow2f(brush_radius_cu);

    const bke::crazyspace::GeometryDeformation deformation =
        bke::crazyspace::get_evaluated_curves_deformation(*ctx_.depsgraph, *object_);
    const OffsetIndices points_by_curve = curves_->points_by_curve();

    threading::parallel_for(curve_selection_.index_range(), 256, [&](const IndexRange range) {
      for (const int curve_i : curve_selection_.slice(range)) {
        const IndexRange points = points_by_curve[curve_i];
        for (const int point_i : points.drop_front(1)) {
          const float3 old_pos_cu = deformation.positions[point_i];

          const float dist_to_brush_sq_cu = math::distance_squared(old_pos_cu, brush_pos_cu);
          if (dist_to_brush_sq_cu > brush_radius_sq_cu) {
            continue;
          }

          const float dist_to_brush_cu = std::sqrt(dist_to_brush_sq_cu);
          const float t = safe_divide(dist_to_brush_cu, brush_radius_cu);
          const float radius_falloff = t * BKE_brush_curve_strength(brush_, t, 1.0f);
          const float weight = invert_factor_ * 0.1f * brush_strength_ * radius_falloff *
                               point_factors_[point_i];

          const float3 new_pos_cu = math::interpolate(old_pos_cu, brush_pos_cu, weight);
          const float3 translation_eval = new_pos_cu - old_pos_cu;
          const float3 translation_orig = deformation.translation_from_deformed_to_original(
              point_i, translation_eval);
          positions_cu[point_i] += translation_orig;

          r_changed_curves[curve_i] = true;
        }
      }
    });
  }
};

void PinchOperation::on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension)
{
  PinchOperationExecutor executor{C};
  executor.execute(*this, C, stroke_extension);
}

std::unique_ptr<CurvesSculptStrokeOperation> new_pinch_operation(const BrushStrokeMode brush_mode,
                                                                 const bContext &C)
{
  const Scene &scene = *CTX_data_scene(&C);
  const Brush &brush = *BKE_paint_brush_for_read(&scene.toolsettings->curves_sculpt->paint);

  const bool invert_pinch = (brush_mode == BRUSH_STROKE_INVERT) !=
                            ((brush.flag & BRUSH_DIR_IN) != 0);
  return std::make_unique<PinchOperation>(invert_pinch);
}

}  // namespace blender::ed::sculpt_paint
