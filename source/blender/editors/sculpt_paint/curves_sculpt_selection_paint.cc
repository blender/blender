/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>
#include <numeric>

#include "BLI_memory_utils.hh"
#include "BLI_task.hh"

#include "DNA_brush_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_curves.hh"

#include "DEG_depsgraph.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "WM_api.h"

#include "curves_sculpt_intern.hh"

/**
 * The code below uses a suffix naming convention to indicate the coordinate space:
 * cu: Local space of the curves object that is being edited.
 * wo: World space.
 * re: 2D coordinates within the region.
 */

namespace blender::ed::sculpt_paint {

using bke::CurvesGeometry;

class SelectionPaintOperation : public CurvesSculptStrokeOperation {
 private:
  bool use_select_;
  bool clear_selection_;

  CurvesBrush3D brush_3d_;

  friend struct SelectionPaintOperationExecutor;

 public:
  SelectionPaintOperation(const bool use_select, const bool clear_selection)
      : use_select_(use_select), clear_selection_(clear_selection)
  {
  }
  void on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension) override;
};

struct SelectionPaintOperationExecutor {
  SelectionPaintOperation *self_ = nullptr;
  CurvesSculptCommonContext ctx_;

  Object *object_ = nullptr;
  Curves *curves_id_ = nullptr;
  CurvesGeometry *curves_ = nullptr;

  const Brush *brush_ = nullptr;
  float brush_radius_base_re_;
  float brush_radius_factor_;
  float brush_strength_;

  float selection_goal_;

  float2 brush_pos_re_;

  CurvesSculptTransforms transforms_;

  SelectionPaintOperationExecutor(const bContext &C) : ctx_(C)
  {
  }

  void execute(SelectionPaintOperation &self,
               const bContext &C,
               const StrokeExtension &stroke_extension)
  {
    self_ = &self;
    object_ = CTX_data_active_object(&C);

    curves_id_ = static_cast<Curves *>(object_->data);
    curves_ = &CurvesGeometry::wrap(curves_id_->geometry);
    curves_id_->flag |= CV_SCULPT_SELECTION_ENABLED;
    if (curves_->curves_num() == 0) {
      return;
    }

    brush_ = BKE_paint_brush_for_read(&ctx_.scene->toolsettings->curves_sculpt->paint);
    brush_radius_base_re_ = BKE_brush_size_get(ctx_.scene, brush_);
    brush_radius_factor_ = brush_radius_factor(*brush_, stroke_extension);
    brush_strength_ = BKE_brush_alpha_get(ctx_.scene, brush_);

    brush_pos_re_ = stroke_extension.mouse_position;

    if (self.clear_selection_) {
      if (stroke_extension.is_first) {
        if (curves_id_->selection_domain == ATTR_DOMAIN_POINT) {
          curves_->selection_point_float_for_write().fill(0.0f);
        }
        else if (curves_id_->selection_domain == ATTR_DOMAIN_CURVE) {
          curves_->selection_curve_float_for_write().fill(0.0f);
        }
      }
    }

    transforms_ = CurvesSculptTransforms(*object_, curves_id_->surface);

    const eBrushFalloffShape falloff_shape = static_cast<eBrushFalloffShape>(
        brush_->falloff_shape);

    selection_goal_ = self_->use_select_ ? 1.0f : 0.0f;

    if (stroke_extension.is_first) {
      if (falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
        this->initialize_spherical_brush_reference_point();
      }
    }

    if (curves_id_->selection_domain == ATTR_DOMAIN_POINT) {
      MutableSpan<float> selection = curves_->selection_point_float_for_write();
      if (falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
        this->paint_point_selection_projected_with_symmetry(selection);
      }
      else if (falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
        this->paint_point_selection_spherical_with_symmetry(selection);
      }
    }
    else {
      MutableSpan<float> selection = curves_->selection_curve_float_for_write();
      if (falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
        this->paint_curve_selection_projected_with_symmetry(selection);
      }
      else if (falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
        this->paint_curve_selection_spherical_with_symmetry(selection);
      }
    }

    /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because
     * selection is handled as a generic attribute for now. */
    DEG_id_tag_update(&curves_id_->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &curves_id_->id);
    ED_region_tag_redraw(ctx_.region);
  }

  void paint_point_selection_projected_with_symmetry(MutableSpan<float> selection)
  {
    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      this->paint_point_selection_projected(brush_transform, selection);
    }
  }

  void paint_point_selection_projected(const float4x4 &brush_transform,
                                       MutableSpan<float> selection)
  {
    const float4x4 brush_transform_inv = brush_transform.inverted();

    float4x4 projection;
    ED_view3d_ob_project_mat_get(ctx_.rv3d, object_, projection.values);

    Span<float3> positions_cu = curves_->positions();

    const float brush_radius_re = brush_radius_base_re_ * brush_radius_factor_;
    const float brush_radius_sq_re = pow2f(brush_radius_re);

    threading::parallel_for(curves_->points_range(), 1024, [&](const IndexRange point_range) {
      for (const int point_i : point_range) {
        const float3 pos_cu = brush_transform_inv * positions_cu[point_i];

        /* Find the position of the point in screen space. */
        float2 pos_re;
        ED_view3d_project_float_v2_m4(ctx_.region, pos_cu, pos_re, projection.values);

        const float distance_to_brush_sq_re = math::distance_squared(pos_re, brush_pos_re_);
        if (distance_to_brush_sq_re > brush_radius_sq_re) {
          /* Ignore the point because it's too far away. */
          continue;
        }

        const float distance_to_brush_re = std::sqrt(distance_to_brush_sq_re);
        /* A falloff that is based on how far away the point is from the stroke. */
        const float radius_falloff = BKE_brush_curve_strength(
            brush_, distance_to_brush_re, brush_radius_re);
        /* Combine the falloff and brush strength. */
        const float weight = brush_strength_ * radius_falloff;

        selection[point_i] = math::interpolate(selection[point_i], selection_goal_, weight);
      }
    });
  }

  void paint_point_selection_spherical_with_symmetry(MutableSpan<float> selection)
  {
    float4x4 projection;
    ED_view3d_ob_project_mat_get(ctx_.rv3d, object_, projection.values);

    float3 brush_wo;
    ED_view3d_win_to_3d(ctx_.v3d,
                        ctx_.region,
                        transforms_.curves_to_world * self_->brush_3d_.position_cu,
                        brush_pos_re_,
                        brush_wo);
    const float3 brush_cu = transforms_.world_to_curves * brush_wo;

    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));

    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      this->paint_point_selection_spherical(selection, brush_transform * brush_cu);
    }
  }

  void paint_point_selection_spherical(MutableSpan<float> selection, const float3 &brush_cu)
  {
    Span<float3> positions_cu = curves_->positions();

    const float brush_radius_cu = self_->brush_3d_.radius_cu;
    const float brush_radius_sq_cu = pow2f(brush_radius_cu);

    threading::parallel_for(curves_->points_range(), 1024, [&](const IndexRange point_range) {
      for (const int i : point_range) {
        const float3 pos_old_cu = positions_cu[i];

        /* Compute distance to the brush. */
        const float distance_to_brush_sq_cu = math::distance_squared(pos_old_cu, brush_cu);
        if (distance_to_brush_sq_cu > brush_radius_sq_cu) {
          /* Ignore the point because it's too far away. */
          continue;
        }

        const float distance_to_brush_cu = std::sqrt(distance_to_brush_sq_cu);

        /* A falloff that is based on how far away the point is from the stroke. */
        const float radius_falloff = BKE_brush_curve_strength(
            brush_, distance_to_brush_cu, brush_radius_cu);
        /* Combine the falloff and brush strength. */
        const float weight = brush_strength_ * radius_falloff;

        selection[i] = math::interpolate(selection[i], selection_goal_, weight);
      }
    });
  }

  void paint_curve_selection_projected_with_symmetry(MutableSpan<float> selection)
  {
    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      this->paint_curve_selection_projected(brush_transform, selection);
    }
  }

  void paint_curve_selection_projected(const float4x4 &brush_transform,
                                       MutableSpan<float> selection)
  {
    const Span<float3> positions_cu = curves_->positions();
    const float4x4 brush_transform_inv = brush_transform.inverted();

    float4x4 projection;
    ED_view3d_ob_project_mat_get(ctx_.rv3d, object_, projection.values);

    const float brush_radius_re = brush_radius_base_re_ * brush_radius_factor_;
    const float brush_radius_sq_re = pow2f(brush_radius_re);

    threading::parallel_for(curves_->curves_range(), 1024, [&](const IndexRange curves_range) {
      for (const int curve_i : curves_range) {
        const float max_weight = threading::parallel_reduce(
            curves_->points_for_curve(curve_i).drop_back(1),
            1024,
            0.0f,
            [&](const IndexRange segment_range, const float init) {
              float max_weight = init;
              for (const int segment_i : segment_range) {
                const float3 pos1_cu = brush_transform_inv * positions_cu[segment_i];
                const float3 pos2_cu = brush_transform_inv * positions_cu[segment_i + 1];

                float2 pos1_re;
                float2 pos2_re;
                ED_view3d_project_float_v2_m4(ctx_.region, pos1_cu, pos1_re, projection.values);
                ED_view3d_project_float_v2_m4(ctx_.region, pos2_cu, pos2_re, projection.values);

                const float distance_sq_re = dist_squared_to_line_segment_v2(
                    brush_pos_re_, pos1_re, pos2_re);
                if (distance_sq_re > brush_radius_sq_re) {
                  continue;
                }
                const float radius_falloff = BKE_brush_curve_strength(
                    brush_, std::sqrt(distance_sq_re), brush_radius_re);
                const float weight = brush_strength_ * radius_falloff;
                max_weight = std::max(max_weight, weight);
              }
              return max_weight;
            },
            [](float a, float b) { return std::max(a, b); });
        selection[curve_i] = math::interpolate(selection[curve_i], selection_goal_, max_weight);
      }
    });
  }

  void paint_curve_selection_spherical_with_symmetry(MutableSpan<float> selection)
  {
    float4x4 projection;
    ED_view3d_ob_project_mat_get(ctx_.rv3d, object_, projection.values);

    float3 brush_wo;
    ED_view3d_win_to_3d(ctx_.v3d,
                        ctx_.region,
                        transforms_.curves_to_world * self_->brush_3d_.position_cu,
                        brush_pos_re_,
                        brush_wo);
    const float3 brush_cu = transforms_.world_to_curves * brush_wo;

    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));

    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      this->paint_curve_selection_spherical(selection, brush_transform * brush_cu);
    }
  }

  void paint_curve_selection_spherical(MutableSpan<float> selection, const float3 &brush_cu)
  {
    const Span<float3> positions_cu = curves_->positions();

    const float brush_radius_cu = self_->brush_3d_.radius_cu;
    const float brush_radius_sq_cu = pow2f(brush_radius_cu);

    threading::parallel_for(curves_->curves_range(), 1024, [&](const IndexRange curves_range) {
      for (const int curve_i : curves_range) {
        const float max_weight = threading::parallel_reduce(
            curves_->points_for_curve(curve_i).drop_back(1),
            1024,
            0.0f,
            [&](const IndexRange segment_range, const float init) {
              float max_weight = init;
              for (const int segment_i : segment_range) {
                const float3 &pos1_cu = positions_cu[segment_i];
                const float3 &pos2_cu = positions_cu[segment_i + 1];

                const float distance_sq_cu = dist_squared_to_line_segment_v3(
                    brush_cu, pos1_cu, pos2_cu);
                if (distance_sq_cu > brush_radius_sq_cu) {
                  continue;
                }
                const float radius_falloff = BKE_brush_curve_strength(
                    brush_, std::sqrt(distance_sq_cu), brush_radius_cu);
                const float weight = brush_strength_ * radius_falloff;
                max_weight = std::max(max_weight, weight);
              }
              return max_weight;
            },
            [](float a, float b) { return std::max(a, b); });
        selection[curve_i] = math::interpolate(selection[curve_i], selection_goal_, max_weight);
      }
    });
  }

  void initialize_spherical_brush_reference_point()
  {
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
};

void SelectionPaintOperation::on_stroke_extended(const bContext &C,
                                                 const StrokeExtension &stroke_extension)
{
  SelectionPaintOperationExecutor executor{C};
  executor.execute(*this, C, stroke_extension);
}

std::unique_ptr<CurvesSculptStrokeOperation> new_selection_paint_operation(
    const BrushStrokeMode brush_mode, const bContext &C)
{
  Scene &scene = *CTX_data_scene(&C);
  Brush &brush = *BKE_paint_brush(&scene.toolsettings->curves_sculpt->paint);
  const bool use_select = ELEM(brush_mode, BRUSH_STROKE_INVERT) ==
                          ((brush.flag & BRUSH_DIR_IN) != 0);
  const bool clear_selection = use_select && brush_mode != BRUSH_STROKE_SMOOTH;

  return std::make_unique<SelectionPaintOperation>(use_select, clear_selection);
}

}  // namespace blender::ed::sculpt_paint
