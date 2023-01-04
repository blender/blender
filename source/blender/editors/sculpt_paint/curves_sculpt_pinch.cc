/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>

#include "curves_sculpt_intern.hh"

#include "BLI_float4x4.hh"
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
  Array<float> segment_lengths_cu_;

  /** Only used when a 3D brush is used. */
  CurvesBrush3D brush_3d_;

  friend struct PinchOperationExecutor;

 public:
  PinchOperation(const bool invert_pinch) : invert_pinch_(invert_pinch)
  {
  }

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

  PinchOperationExecutor(const bContext &C) : ctx_(C)
  {
  }

  void execute(PinchOperation &self, const bContext &C, const StrokeExtension &stroke_extension)
  {
    self_ = &self;

    object_ = CTX_data_active_object(&C);
    curves_id_ = static_cast<Curves *>(object_->data);
    curves_ = &CurvesGeometry::wrap(curves_id_->geometry);
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

    point_factors_ = curves_->attributes().lookup_or_default<float>(
        ".selection", ATTR_DOMAIN_POINT, 1.0f);
    curve_selection_ = curves::retrieve_selected_curves(*curves_id_, selected_curve_indices_);

    brush_pos_re_ = stroke_extension.mouse_position;
    const eBrushFalloffShape falloff_shape = static_cast<eBrushFalloffShape>(
        brush_->falloff_shape);

    if (stroke_extension.is_first) {
      this->initialize_segment_lengths();

      if (falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
        self_->brush_3d_ = *sample_curves_3d_brush(*ctx_.depsgraph,
                                                   *ctx_.region,
                                                   *ctx_.v3d,
                                                   *ctx_.rv3d,
                                                   *object_,
                                                   brush_pos_re_,
                                                   brush_radius_base_re_);
      }
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

    this->restore_segment_lengths(changed_curves);
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
    const float4x4 brush_transform_inv = brush_transform.inverted();

    const bke::crazyspace::GeometryDeformation deformation =
        bke::crazyspace::get_evaluated_curves_deformation(*ctx_.depsgraph, *object_);

    float4x4 projection;
    ED_view3d_ob_project_mat_get(ctx_.rv3d, object_, projection.values);
    MutableSpan<float3> positions_cu = curves_->positions_for_write();
    const float brush_radius_re = brush_radius_base_re_ * brush_radius_factor_;
    const float brush_radius_sq_re = pow2f(brush_radius_re);

    threading::parallel_for(curve_selection_.index_range(), 256, [&](const IndexRange range) {
      for (const int curve_i : curve_selection_.slice(range)) {
        const IndexRange points = curves_->points_for_curve(curve_i);
        for (const int point_i : points.drop_front(1)) {
          const float3 old_pos_cu = deformation.positions[point_i];
          const float3 old_symm_pos_cu = brush_transform_inv * old_pos_cu;
          float2 old_symm_pos_re;
          ED_view3d_project_float_v2_m4(
              ctx_.region, old_symm_pos_cu, old_symm_pos_re, projection.values);

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
                              transforms_.curves_to_world * old_symm_pos_cu,
                              new_symm_pos_re,
                              new_symm_pos_wo);

          const float3 new_pos_cu = brush_transform * transforms_.world_to_curves *
                                    new_symm_pos_wo;
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
    ED_view3d_win_to_3d(ctx_.v3d,
                        ctx_.region,
                        transforms_.curves_to_world * self_->brush_3d_.position_cu,
                        brush_pos_re_,
                        brush_pos_wo);
    const float3 brush_pos_cu = transforms_.world_to_curves * brush_pos_wo;
    const float brush_radius_cu = self_->brush_3d_.radius_cu * brush_radius_factor_;

    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      this->pinch_spherical(brush_transform * brush_pos_cu, brush_radius_cu, r_changed_curves);
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

    threading::parallel_for(curve_selection_.index_range(), 256, [&](const IndexRange range) {
      for (const int curve_i : curve_selection_.slice(range)) {
        const IndexRange points = curves_->points_for_curve(curve_i);
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

  void initialize_segment_lengths()
  {
    const Span<float3> positions_cu = curves_->positions();
    self_->segment_lengths_cu_.reinitialize(curves_->points_num());
    threading::parallel_for(curve_selection_.index_range(), 256, [&](const IndexRange range) {
      for (const int curve_i : curve_selection_.slice(range)) {
        const IndexRange points = curves_->points_for_curve(curve_i);
        for (const int point_i : points.drop_back(1)) {
          const float3 &p1_cu = positions_cu[point_i];
          const float3 &p2_cu = positions_cu[point_i + 1];
          const float length_cu = math::distance(p1_cu, p2_cu);
          self_->segment_lengths_cu_[point_i] = length_cu;
        }
      }
    });
  }

  void restore_segment_lengths(const Span<bool> changed_curves)
  {
    const Span<float> expected_lengths_cu = self_->segment_lengths_cu_;
    MutableSpan<float3> positions_cu = curves_->positions_for_write();

    threading::parallel_for(changed_curves.index_range(), 256, [&](const IndexRange range) {
      for (const int curve_i : range) {
        if (!changed_curves[curve_i]) {
          continue;
        }
        const IndexRange points = curves_->points_for_curve(curve_i);
        for (const int segment_i : IndexRange(points.size() - 1)) {
          const float3 &p1_cu = positions_cu[points[segment_i]];
          float3 &p2_cu = positions_cu[points[segment_i] + 1];
          const float3 direction = math::normalize(p2_cu - p1_cu);
          const float expected_length_cu = expected_lengths_cu[points[segment_i]];
          p2_cu = p1_cu + direction * expected_length_cu;
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
