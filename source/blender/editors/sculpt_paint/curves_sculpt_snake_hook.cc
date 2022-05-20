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
#include "BKE_spline.hh"

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
  const Depsgraph *depsgraph_ = nullptr;
  const Scene *scene_ = nullptr;
  ARegion *region_ = nullptr;
  const View3D *v3d_ = nullptr;
  const RegionView3D *rv3d_ = nullptr;

  const CurvesSculpt *curves_sculpt_ = nullptr;
  const Brush *brush_ = nullptr;
  float brush_radius_re_;
  float brush_strength_;
  eBrushFalloffShape falloff_shape_;

  Object *object_ = nullptr;
  Curves *curves_id_ = nullptr;
  CurvesGeometry *curves_ = nullptr;

  float4x4 curves_to_world_mat_;
  float4x4 world_to_curves_mat_;

  float2 brush_pos_prev_re_;
  float2 brush_pos_re_;
  float2 brush_pos_diff_re_;

  void execute(SnakeHookOperation &self,
               const bContext &C,
               const StrokeExtension &stroke_extension)
  {
    BLI_SCOPED_DEFER([&]() { self.last_mouse_position_re_ = stroke_extension.mouse_position; });

    self_ = &self;
    depsgraph_ = CTX_data_depsgraph_pointer(&C);
    scene_ = CTX_data_scene(&C);
    scene_ = CTX_data_scene(&C);
    object_ = CTX_data_active_object(&C);
    region_ = CTX_wm_region(&C);
    v3d_ = CTX_wm_view3d(&C);
    rv3d_ = CTX_wm_region_view3d(&C);

    curves_sculpt_ = scene_->toolsettings->curves_sculpt;
    brush_ = BKE_paint_brush_for_read(&curves_sculpt_->paint);
    brush_radius_re_ = brush_radius_get(*scene_, *brush_, stroke_extension);
    brush_strength_ = brush_strength_get(*scene_, *brush_, stroke_extension);
    falloff_shape_ = static_cast<eBrushFalloffShape>(brush_->falloff_shape);

    curves_to_world_mat_ = object_->obmat;
    world_to_curves_mat_ = curves_to_world_mat_.inverted();

    curves_id_ = static_cast<Curves *>(object_->data);
    curves_ = &CurvesGeometry::wrap(curves_id_->geometry);
    if (curves_->curves_num() == 0) {
      return;
    }

    brush_pos_prev_re_ = self.last_mouse_position_re_;
    brush_pos_re_ = stroke_extension.mouse_position;
    brush_pos_diff_re_ = brush_pos_re_ - brush_pos_prev_re_;

    if (stroke_extension.is_first) {
      if (falloff_shape_ == PAINT_FALLOFF_SHAPE_SPHERE) {
        std::optional<CurvesBrush3D> brush_3d = sample_curves_3d_brush(
            *depsgraph_, *region_, *v3d_, *rv3d_, *object_, brush_pos_re_, brush_radius_re_);
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
    ED_region_tag_redraw(region_);
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

    MutableSpan<float3> positions_cu = curves_->positions_for_write();

    float4x4 projection;
    ED_view3d_ob_project_mat_get(rv3d_, object_, projection.values);

    threading::parallel_for(curves_->curves_range(), 256, [&](const IndexRange curves_range) {
      for (const int curve_i : curves_range) {
        const IndexRange points = curves_->points_for_curve(curve_i);
        const int last_point_i = points.last();
        const float3 old_pos_cu = brush_transform_inv * positions_cu[last_point_i];

        float2 old_pos_re;
        ED_view3d_project_float_v2_m4(region_, old_pos_cu, old_pos_re, projection.values);

        const float distance_to_brush_re = math::distance(old_pos_re, brush_pos_prev_re_);
        if (distance_to_brush_re > brush_radius_re_) {
          continue;
        }

        const float radius_falloff = BKE_brush_curve_strength(
            brush_, distance_to_brush_re, brush_radius_re_);
        const float weight = brush_strength_ * radius_falloff;

        const float2 new_position_re = old_pos_re + brush_pos_diff_re_ * weight;
        float3 new_position_wo;
        ED_view3d_win_to_3d(
            v3d_, region_, curves_to_world_mat_ * old_pos_cu, new_position_re, new_position_wo);
        const float3 new_position_cu = brush_transform * (world_to_curves_mat_ * new_position_wo);

        this->move_last_point_and_resample(positions_cu.slice(points), new_position_cu);
      }
    });
  }

  void spherical_snake_hook_with_symmetry()
  {
    float4x4 projection;
    ED_view3d_ob_project_mat_get(rv3d_, object_, projection.values);

    float3 brush_start_wo, brush_end_wo;
    ED_view3d_win_to_3d(v3d_,
                        region_,
                        curves_to_world_mat_ * self_->brush_3d_.position_cu,
                        brush_pos_prev_re_,
                        brush_start_wo);
    ED_view3d_win_to_3d(v3d_,
                        region_,
                        curves_to_world_mat_ * self_->brush_3d_.position_cu,
                        brush_pos_re_,
                        brush_end_wo);
    const float3 brush_start_cu = world_to_curves_mat_ * brush_start_wo;
    const float3 brush_end_cu = world_to_curves_mat_ * brush_end_wo;

    const float brush_radius_cu = self_->brush_3d_.radius_cu;

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
    MutableSpan<float3> positions_cu = curves_->positions_for_write();
    const float3 brush_diff_cu = brush_end_cu - brush_start_cu;
    const float brush_radius_sq_cu = pow2f(brush_radius_cu);

    threading::parallel_for(curves_->curves_range(), 256, [&](const IndexRange curves_range) {
      for (const int curve_i : curves_range) {
        const IndexRange points = curves_->points_for_curve(curve_i);
        const int last_point_i = points.last();
        const float3 old_pos_cu = positions_cu[last_point_i];

        const float distance_to_brush_sq_cu = dist_squared_to_line_segment_v3(
            old_pos_cu, brush_start_cu, brush_end_cu);
        if (distance_to_brush_sq_cu > brush_radius_sq_cu) {
          continue;
        }

        const float distance_to_brush_cu = std::sqrt(distance_to_brush_sq_cu);

        const float radius_falloff = BKE_brush_curve_strength(
            brush_, distance_to_brush_cu, brush_radius_cu);
        const float weight = brush_strength_ * radius_falloff;

        const float3 new_pos_cu = old_pos_cu + weight * brush_diff_cu;

        this->move_last_point_and_resample(positions_cu.slice(points), new_pos_cu);
      }
    });
  }

  void move_last_point_and_resample(MutableSpan<float3> positions,
                                    const float3 &new_last_position) const
  {
    /* Find the accumulated length of each point in the original curve,
     * treating it as a poly curve for performance reasons and simplicity. */
    Array<float> orig_lengths(length_parameterize::lengths_num(positions.size(), false));
    length_parameterize::accumulate_lengths<float3>(positions, false, orig_lengths);
    const float orig_total_length = orig_lengths.last();

    /* Find the factor by which the new curve is shorter or longer than the original. */
    const float new_last_segment_length = math::distance(positions.last(1), new_last_position);
    const float new_total_length = orig_lengths.last(1) + new_last_segment_length;
    const float length_factor = new_total_length / orig_total_length;

    /* Calculate the lengths to sample the original curve with by scaling the original lengths. */
    Array<float> new_lengths(positions.size() - 1);
    new_lengths.first() = 0.0f;
    for (const int i : new_lengths.index_range().drop_front(1)) {
      new_lengths[i] = orig_lengths[i - 1] * length_factor;
    }

    Array<int> indices(positions.size() - 1);
    Array<float> factors(positions.size() - 1);
    length_parameterize::create_samples_from_sorted_lengths(
        orig_lengths, new_lengths, false, indices, factors);

    Array<float3> new_positions(positions.size() - 1);
    length_parameterize::linear_interpolation<float3>(positions, indices, factors, new_positions);
    positions.drop_back(1).copy_from(new_positions);
    positions.last() = new_last_position;
  }
};

void SnakeHookOperation::on_stroke_extended(const bContext &C,
                                            const StrokeExtension &stroke_extension)
{
  SnakeHookOperatorExecutor executor;
  executor.execute(*this, C, stroke_extension);
}

std::unique_ptr<CurvesSculptStrokeOperation> new_snake_hook_operation()
{
  return std::make_unique<SnakeHookOperation>();
}

}  // namespace blender::ed::sculpt_paint
