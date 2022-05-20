/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>

#include "curves_sculpt_intern.hh"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_float4x4.hh"
#include "BLI_kdtree.h"
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

#include "curves_sculpt_intern.hh"

/**
 * The code below uses a suffix naming convention to indicate the coordinate space:
 * - `cu`: Local space of the curves object that is being edited.
 * - `su`: Local space of the surface object.
 * - `wo`: World space.
 * - `re`: 2D coordinates within the region.
 */

namespace blender::ed::sculpt_paint {

using bke::CurvesGeometry;

/**
 * Utility class to wrap different grow/shrink behaviors.
 * It might be useful to use this for other future brushes as well, but better see if this
 * abstraction holds up for a while before using it in more places.
 */
class CurvesEffect {
 public:
  virtual ~CurvesEffect() = default;
  virtual void execute(CurvesGeometry &curves,
                       Span<int> curve_indices,
                       Span<float> move_distances_cu) = 0;
};

/**
 * Make curves smaller by trimming the end off.
 */
class ShrinkCurvesEffect : public CurvesEffect {
 private:
  const Brush &brush_;

 public:
  ShrinkCurvesEffect(const Brush &brush) : brush_(brush)
  {
  }

  void execute(CurvesGeometry &curves,
               const Span<int> curve_indices,
               const Span<float> move_distances_cu) override
  {
    MutableSpan<float3> positions_cu = curves.positions_for_write();
    threading::parallel_for(curve_indices.index_range(), 256, [&](const IndexRange range) {
      for (const int influence_i : range) {
        const int curve_i = curve_indices[influence_i];
        const float move_distance_cu = move_distances_cu[influence_i];
        const IndexRange curve_points = curves.points_for_curve(curve_i);
        this->shrink_curve(positions_cu, curve_points, move_distance_cu);
      }
    });
  }

  void shrink_curve(MutableSpan<float3> positions,
                    const IndexRange curve_points,
                    const float shrink_length) const
  {
    PolySpline spline;
    spline.resize(curve_points.size());
    MutableSpan<float3> spline_positions = spline.positions();
    spline_positions.copy_from(positions.slice(curve_points));
    spline.mark_cache_invalid();
    const float min_length = brush_.curves_sculpt_settings->minimum_length;
    const float old_length = spline.length();
    const float new_length = std::max(min_length, old_length - shrink_length);
    const float length_factor = std::clamp(new_length / old_length, 0.0f, 1.0f);

    Vector<float> old_point_lengths;
    old_point_lengths.append(0.0f);
    for (const int i : spline_positions.index_range().drop_back(1)) {
      const float3 &p1 = spline_positions[i];
      const float3 &p2 = spline_positions[i + 1];
      const float length = math::distance(p1, p2);
      old_point_lengths.append(old_point_lengths.last() + length);
    }

    for (const int i : spline_positions.index_range()) {
      const float eval_length = old_point_lengths[i] * length_factor;
      const Spline::LookupResult lookup = spline.lookup_evaluated_length(eval_length);
      const float index_factor = lookup.evaluated_index + lookup.factor;
      float3 p;
      spline.sample_with_index_factors<float3>(spline_positions, {&index_factor, 1}, {&p, 1});
      positions[curve_points[i]] = p;
    }
  }
};

/**
 * Make the curves longer by extrapolating them linearly.
 */
class ExtrapolateCurvesEffect : public CurvesEffect {
  void execute(CurvesGeometry &curves,
               const Span<int> curve_indices,
               const Span<float> move_distances_cu) override
  {
    MutableSpan<float3> positions_cu = curves.positions_for_write();
    threading::parallel_for(curve_indices.index_range(), 256, [&](const IndexRange range) {
      for (const int influence_i : range) {
        const int curve_i = curve_indices[influence_i];
        const float move_distance_cu = move_distances_cu[influence_i];
        const IndexRange curve_points = curves.points_for_curve(curve_i);

        if (curve_points.size() <= 1) {
          continue;
        }

        const float3 old_last_pos_cu = positions_cu[curve_points.last()];
        /* Use some point within the curve rather than the end point to smooth out some random
         * variation. */
        const float3 direction_reference_point =
            positions_cu[curve_points[curve_points.size() / 2]];
        const float3 direction = math::normalize(old_last_pos_cu - direction_reference_point);

        const float3 new_last_pos_cu = old_last_pos_cu + direction * move_distance_cu;
        this->move_last_point_and_resample(positions_cu, curve_points, new_last_pos_cu);
      }
    });
  }

  void move_last_point_and_resample(MutableSpan<float3> positions,
                                    const IndexRange curve_points,
                                    const float3 &new_last_point_position) const
  {
    Vector<float> old_lengths;
    old_lengths.append(0.0f);
    /* Used to (1) normalize the segment sizes over time and (2) support making zero-length
     * segments */
    const float extra_length = 0.001f;
    for (const int segment_i : IndexRange(curve_points.size() - 1)) {
      const float3 &p1 = positions[curve_points[segment_i]];
      const float3 &p2 = positions[curve_points[segment_i] + 1];
      const float length = math::distance(p1, p2);
      old_lengths.append(old_lengths.last() + length + extra_length);
    }
    Vector<float> point_factors;
    for (float &old_length : old_lengths) {
      point_factors.append(old_length / old_lengths.last());
    }

    PolySpline new_spline;
    new_spline.resize(curve_points.size());
    MutableSpan<float3> new_spline_positions = new_spline.positions();
    for (const int i : IndexRange(curve_points.size() - 1)) {
      new_spline_positions[i] = positions[curve_points[i]];
    }
    new_spline_positions.last() = new_last_point_position;
    new_spline.mark_cache_invalid();

    for (const int i : IndexRange(curve_points.size())) {
      const float factor = point_factors[i];
      const Spline::LookupResult lookup = new_spline.lookup_evaluated_factor(factor);
      const float index_factor = lookup.evaluated_index + lookup.factor;
      float3 p;
      new_spline.sample_with_index_factors<float3>(
          new_spline_positions, {&index_factor, 1}, {&p, 1});
      positions[curve_points[i]] = p;
    }
  }
};

/**
 * Change the length of curves by scaling them uniformly.
 */
class ScaleCurvesEffect : public CurvesEffect {
 private:
  bool scale_up_;
  const Brush &brush_;

 public:
  ScaleCurvesEffect(bool scale_up, const Brush &brush) : scale_up_(scale_up), brush_(brush)
  {
  }

  void execute(CurvesGeometry &curves,
               const Span<int> curve_indices,
               const Span<float> move_distances_cu) override
  {
    MutableSpan<float3> positions_cu = curves.positions_for_write();
    threading::parallel_for(curve_indices.index_range(), 256, [&](const IndexRange range) {
      for (const int influence_i : range) {
        const int curve_i = curve_indices[influence_i];
        const float move_distance_cu = move_distances_cu[influence_i];
        const IndexRange points = curves.points_for_curve(curve_i);

        const float old_length = this->compute_poly_curve_length(positions_cu.slice(points));
        const float length_diff = scale_up_ ? move_distance_cu : -move_distance_cu;
        const float min_length = brush_.curves_sculpt_settings->minimum_length;
        const float new_length = std::max(min_length, old_length + length_diff);
        const float scale_factor = safe_divide(new_length, old_length);

        const float3 &root_pos_cu = positions_cu[points[0]];
        for (float3 &pos_cu : positions_cu.slice(points.drop_front(1))) {
          pos_cu = (pos_cu - root_pos_cu) * scale_factor + root_pos_cu;
        }
      }
    });
  }

  float compute_poly_curve_length(const Span<float3> positions)
  {
    float length = 0.0f;
    const int segments_num = positions.size() - 1;
    for (const int segment_i : IndexRange(segments_num)) {
      const float3 &p1 = positions[segment_i];
      const float3 &p2 = positions[segment_i + 1];
      length += math::distance(p1, p2);
    }
    return length;
  }
};

class CurvesEffectOperation : public CurvesSculptStrokeOperation {
 private:
  std::unique_ptr<CurvesEffect> effect_;
  float2 last_mouse_position_;
  CurvesBrush3D brush_3d_;

  friend struct CurvesEffectOperationExecutor;

 public:
  CurvesEffectOperation(std::unique_ptr<CurvesEffect> effect) : effect_(std::move(effect))
  {
  }

  void on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension) override;
};

/**
 * Utility class that actually executes the update when the stroke is updated. That's useful
 * because it avoids passing a very large number of parameters between functions.
 */
struct CurvesEffectOperationExecutor {
  CurvesEffectOperation *self_ = nullptr;
  const Depsgraph *depsgraph_ = nullptr;
  const Scene *scene_ = nullptr;
  ARegion *region_ = nullptr;
  const View3D *v3d_ = nullptr;
  const RegionView3D *rv3d_ = nullptr;

  Object *object_ = nullptr;
  Curves *curves_id_ = nullptr;
  CurvesGeometry *curves_ = nullptr;

  const Brush *brush_ = nullptr;
  float brush_radius_re_;
  float brush_radius_sq_re_;
  float brush_strength_;
  eBrushFalloffShape falloff_shape_;

  float4x4 curves_to_world_mat_;
  float4x4 world_to_curves_mat_;

  float2 brush_pos_start_re_;
  float2 brush_pos_end_re_;

  struct Influences {
    Vector<int> curve_indices;
    Vector<float> move_distances_cu;
  };

  void execute(CurvesEffectOperation &self,
               const bContext &C,
               const StrokeExtension &stroke_extension)
  {
    BLI_SCOPED_DEFER([&]() { self.last_mouse_position_ = stroke_extension.mouse_position; });

    self_ = &self;
    depsgraph_ = CTX_data_depsgraph_pointer(&C);
    scene_ = CTX_data_scene(&C);
    object_ = CTX_data_active_object(&C);
    region_ = CTX_wm_region(&C);
    v3d_ = CTX_wm_view3d(&C);
    rv3d_ = CTX_wm_region_view3d(&C);

    curves_id_ = static_cast<Curves *>(object_->data);
    curves_ = &CurvesGeometry::wrap(curves_id_->geometry);
    if (curves_->curves_num() == 0) {
      return;
    }

    const CurvesSculpt &curves_sculpt = *scene_->toolsettings->curves_sculpt;
    brush_ = BKE_paint_brush_for_read(&curves_sculpt.paint);
    brush_radius_re_ = brush_radius_get(*scene_, *brush_, stroke_extension);
    brush_strength_ = brush_strength_get(*scene_, *brush_, stroke_extension);
    brush_radius_sq_re_ = pow2f(brush_radius_re_);
    falloff_shape_ = eBrushFalloffShape(brush_->falloff_shape);

    curves_to_world_mat_ = object_->obmat;
    world_to_curves_mat_ = curves_to_world_mat_.inverted();

    brush_pos_start_re_ = self.last_mouse_position_;
    brush_pos_end_re_ = stroke_extension.mouse_position;

    if (stroke_extension.is_first) {
      if (falloff_shape_ == PAINT_FALLOFF_SHAPE_SPHERE) {
        if (std::optional<CurvesBrush3D> brush_3d = sample_curves_3d_brush(
                *depsgraph_,
                *region_,
                *v3d_,
                *rv3d_,
                *object_,
                stroke_extension.mouse_position,
                brush_radius_re_)) {
          self.brush_3d_ = *brush_3d;
        }
      }

      return;
    }

    /* Compute influences. */
    threading::EnumerableThreadSpecific<Influences> influences_for_thread;
    if (falloff_shape_ == PAINT_FALLOFF_SHAPE_TUBE) {
      this->gather_influences_projected(influences_for_thread);
    }
    else if (falloff_shape_ == PAINT_FALLOFF_SHAPE_SPHERE) {
      this->gather_influences_spherical(influences_for_thread);
    }

    /* Execute effect. */
    threading::parallel_for_each(influences_for_thread, [&](const Influences &influences) {
      BLI_assert(influences.curve_indices.size() == influences.move_distances_cu.size());
      self_->effect_->execute(*curves_, influences.curve_indices, influences.move_distances_cu);
    });

    curves_->tag_positions_changed();
    DEG_id_tag_update(&curves_id_->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &curves_id_->id);
    ED_region_tag_redraw(region_);
  }

  void gather_influences_projected(
      threading::EnumerableThreadSpecific<Influences> &influences_for_thread)
  {
    const Span<float3> positions_cu = curves_->positions();

    float4x4 projection;
    ED_view3d_ob_project_mat_get(rv3d_, object_, projection.values);

    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    Vector<float4x4> symmetry_brush_transforms_inv;
    for (const float4x4 brush_transform : symmetry_brush_transforms) {
      symmetry_brush_transforms_inv.append(brush_transform.inverted());
    }

    threading::parallel_for(curves_->curves_range(), 256, [&](const IndexRange curves_range) {
      Influences &local_influences = influences_for_thread.local();

      for (const int curve_i : curves_range) {
        const IndexRange points = curves_->points_for_curve(curve_i);

        float max_move_distance_cu = 0.0f;
        for (const float4x4 &brush_transform_inv : symmetry_brush_transforms_inv) {
          for (const int segment_i : points.drop_back(1)) {
            const float3 p1_cu = brush_transform_inv * positions_cu[segment_i];
            const float3 p2_cu = brush_transform_inv * positions_cu[segment_i + 1];

            float2 p1_re, p2_re;
            ED_view3d_project_float_v2_m4(region_, p1_cu, p1_re, projection.values);
            ED_view3d_project_float_v2_m4(region_, p2_cu, p2_re, projection.values);

            float2 closest_on_brush_re;
            float2 closest_on_segment_re;
            float lambda_on_brush;
            float lambda_on_segment;
            const float dist_to_brush_sq_re = closest_seg_seg_v2(closest_on_brush_re,
                                                                 closest_on_segment_re,
                                                                 &lambda_on_brush,
                                                                 &lambda_on_segment,
                                                                 brush_pos_start_re_,
                                                                 brush_pos_end_re_,
                                                                 p1_re,
                                                                 p2_re);

            if (dist_to_brush_sq_re > brush_radius_sq_re_) {
              continue;
            }

            const float dist_to_brush_re = std::sqrt(dist_to_brush_sq_re);
            const float radius_falloff = BKE_brush_curve_strength(
                brush_, dist_to_brush_re, brush_radius_re_);
            const float weight = brush_strength_ * radius_falloff;

            const float3 closest_on_segment_cu = math::interpolate(
                p1_cu, p2_cu, lambda_on_segment);

            float3 brush_start_pos_wo, brush_end_pos_wo;
            ED_view3d_win_to_3d(v3d_,
                                region_,
                                curves_to_world_mat_ * closest_on_segment_cu,
                                brush_pos_start_re_,
                                brush_start_pos_wo);
            ED_view3d_win_to_3d(v3d_,
                                region_,
                                curves_to_world_mat_ * closest_on_segment_cu,
                                brush_pos_end_re_,
                                brush_end_pos_wo);
            const float3 brush_start_pos_cu = world_to_curves_mat_ * brush_start_pos_wo;
            const float3 brush_end_pos_cu = world_to_curves_mat_ * brush_end_pos_wo;

            const float move_distance_cu = weight *
                                           math::distance(brush_start_pos_cu, brush_end_pos_cu);
            max_move_distance_cu = std::max(max_move_distance_cu, move_distance_cu);
          }
        }
        if (max_move_distance_cu > 0.0f) {
          local_influences.curve_indices.append(curve_i);
          local_influences.move_distances_cu.append(max_move_distance_cu);
        }
      }
    });
  }

  void gather_influences_spherical(
      threading::EnumerableThreadSpecific<Influences> &influences_for_thread)
  {
    const Span<float3> positions_cu = curves_->positions();

    float3 brush_pos_start_wo, brush_pos_end_wo;
    ED_view3d_win_to_3d(v3d_,
                        region_,
                        curves_to_world_mat_ * self_->brush_3d_.position_cu,
                        brush_pos_start_re_,
                        brush_pos_start_wo);
    ED_view3d_win_to_3d(v3d_,
                        region_,
                        curves_to_world_mat_ * self_->brush_3d_.position_cu,
                        brush_pos_end_re_,
                        brush_pos_end_wo);
    const float3 brush_pos_start_cu = world_to_curves_mat_ * brush_pos_start_wo;
    const float3 brush_pos_end_cu = world_to_curves_mat_ * brush_pos_end_wo;
    const float3 brush_pos_diff_cu = brush_pos_end_cu - brush_pos_start_cu;
    const float brush_pos_diff_length_cu = math::length(brush_pos_diff_cu);
    const float brush_radius_cu = self_->brush_3d_.radius_cu;
    const float brush_radius_sq_cu = pow2f(brush_radius_cu);

    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));

    threading::parallel_for(curves_->curves_range(), 256, [&](const IndexRange curves_range) {
      Influences &local_influences = influences_for_thread.local();

      for (const int curve_i : curves_range) {
        const IndexRange points = curves_->points_for_curve(curve_i);

        float max_move_distance_cu = 0.0f;
        for (const float4x4 &brush_transform : symmetry_brush_transforms) {
          const float3 brush_pos_start_transformed_cu = brush_transform * brush_pos_start_cu;
          const float3 brush_pos_end_transformed_cu = brush_transform * brush_pos_end_cu;

          for (const int segment_i : points.drop_back(1)) {
            const float3 &p1_cu = positions_cu[segment_i];
            const float3 &p2_cu = positions_cu[segment_i + 1];

            float3 closest_on_segment_cu;
            float3 closest_on_brush_cu;
            isect_seg_seg_v3(p1_cu,
                             p2_cu,
                             brush_pos_start_transformed_cu,
                             brush_pos_end_transformed_cu,
                             closest_on_segment_cu,
                             closest_on_brush_cu);

            const float dist_to_brush_sq_cu = math::distance_squared(closest_on_segment_cu,
                                                                     closest_on_brush_cu);
            if (dist_to_brush_sq_cu > brush_radius_sq_cu) {
              continue;
            }

            const float dist_to_brush_cu = std::sqrt(dist_to_brush_sq_cu);
            const float radius_falloff = BKE_brush_curve_strength(
                brush_, dist_to_brush_cu, brush_radius_cu);
            const float weight = brush_strength_ * radius_falloff;

            const float move_distance_cu = weight * brush_pos_diff_length_cu;
            max_move_distance_cu = std::max(max_move_distance_cu, move_distance_cu);
          }
        }
        if (max_move_distance_cu > 0.0f) {
          local_influences.curve_indices.append(curve_i);
          local_influences.move_distances_cu.append(max_move_distance_cu);
        }
      }
    });
  }
};

void CurvesEffectOperation::on_stroke_extended(const bContext &C,
                                               const StrokeExtension &stroke_extension)
{
  CurvesEffectOperationExecutor executor;
  executor.execute(*this, C, stroke_extension);
}

std::unique_ptr<CurvesSculptStrokeOperation> new_grow_shrink_operation(
    const BrushStrokeMode brush_mode, const bContext &C)
{
  const Scene &scene = *CTX_data_scene(&C);
  const Brush &brush = *BKE_paint_brush_for_read(&scene.toolsettings->curves_sculpt->paint);
  const bool use_scale_uniform = brush.curves_sculpt_settings->flag &
                                 BRUSH_CURVES_SCULPT_FLAG_SCALE_UNIFORM;
  const bool use_grow = (brush_mode == BRUSH_STROKE_INVERT) == ((brush.flag & BRUSH_DIR_IN) != 0);

  if (use_grow) {
    if (use_scale_uniform) {
      return std::make_unique<CurvesEffectOperation>(
          std::make_unique<ScaleCurvesEffect>(true, brush));
    }
    return std::make_unique<CurvesEffectOperation>(std::make_unique<ExtrapolateCurvesEffect>());
  }
  if (use_scale_uniform) {
    return std::make_unique<CurvesEffectOperation>(
        std::make_unique<ScaleCurvesEffect>(false, brush));
  }
  return std::make_unique<CurvesEffectOperation>(std::make_unique<ShrinkCurvesEffect>(brush));
}

}  // namespace blender::ed::sculpt_paint
