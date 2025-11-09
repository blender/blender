/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>

#include "BLI_math_vector.hh"

#include "BLI_length_parameterize.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "DEG_depsgraph.hh"

#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_paint.hh"

#include "DNA_brush_enums.h"
#include "DNA_brush_types.h"
#include "DNA_curves_types.h"
#include "DNA_object_types.h"

#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "WM_api.hh"

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
                       const IndexMask &curve_mask,
                       Span<float> move_distances_cu,
                       MutableSpan<float3> positions_cu) = 0;
};

/**
 * Make curves smaller by trimming the end off.
 */
class ShrinkCurvesEffect : public CurvesEffect {
 private:
  const Brush &brush_;

  /** Storage of per-curve parameterization data to avoid reallocation. */
  struct ParameterizationBuffers {
    Vector<float3> old_positions;
    Vector<float> old_lengths;
    Vector<float> sample_lengths;
    Vector<int> indices;
    Vector<float> factors;

    void resize(const int points_num)
    {
      this->old_positions.resize(points_num);
      this->old_lengths.resize(length_parameterize::segments_num(points_num, false));
      this->sample_lengths.resize(points_num);
      this->indices.resize(points_num);
      this->factors.resize(points_num);
    }
  };

 public:
  ShrinkCurvesEffect(const Brush &brush) : brush_(brush) {}

  void execute(CurvesGeometry &curves,
               const IndexMask &curve_mask,
               const Span<float> move_distances_cu,
               MutableSpan<float3> positions_cu) override
  {
    const OffsetIndices points_by_curve = curves.points_by_curve();
    curve_mask.foreach_segment(GrainSize(256), [&](IndexMaskSegment segment) {
      ParameterizationBuffers data;
      for (const int curve_i : segment) {
        const float move_distance_cu = move_distances_cu[curve_i];
        const IndexRange points = points_by_curve[curve_i];
        this->shrink_curve(positions_cu.slice(points), move_distance_cu, data);
      }
    });
  }

 private:
  void shrink_curve(MutableSpan<float3> positions,
                    const float shrink_length,
                    ParameterizationBuffers &data) const
  {
    namespace lp = length_parameterize;
    data.resize(positions.size());

    /* Copy the old positions to facilitate mixing from neighbors for the resulting curve. */
    data.old_positions.as_mutable_span().copy_from(positions);

    lp::accumulate_lengths<float3>(data.old_positions, false, data.old_lengths);

    const float min_length = brush_.curves_sculpt_settings->minimum_length;
    const float old_length = data.old_lengths.last();
    const float new_length = std::max(min_length, old_length - shrink_length);
    const float length_factor = std::clamp(new_length / old_length, 0.0f, 1.0f);

    data.sample_lengths.first() = 0.0f;
    for (const int i : data.old_lengths.index_range()) {
      data.sample_lengths[i + 1] = data.old_lengths[i] * length_factor;
    }

    lp::sample_at_lengths(data.old_lengths, data.sample_lengths, data.indices, data.factors);

    lp::interpolate<float3>(data.old_positions, data.indices, data.factors, positions);
  }
};

/**
 * Make the curves longer by extrapolating them linearly.
 */
class ExtrapolateCurvesEffect : public CurvesEffect {
  void execute(CurvesGeometry &curves,
               const IndexMask &curve_mask,
               const Span<float> move_distances_cu,
               MutableSpan<float3> positions_cu) override
  {
    const OffsetIndices points_by_curve = curves.points_by_curve();
    curve_mask.foreach_segment(GrainSize(256), [&](IndexMaskSegment segment) {
      MoveAndResampleBuffers resample_buffer;
      for (const int curve_i : segment) {
        const float move_distance_cu = move_distances_cu[curve_i];
        const IndexRange points = points_by_curve[curve_i];
        if (points.size() <= 1) {
          continue;
        }

        const float3 old_last_pos_cu = positions_cu[points.last()];
        /* Use some point within the curve rather than the end point to smooth out some random
         * variation. */
        const float3 direction_reference_point =
            positions_cu[points.size() > 2 ? points[points.size() / 2] : points.first()];
        const float3 direction = math::normalize(old_last_pos_cu - direction_reference_point);

        const float3 new_last_pos_cu = old_last_pos_cu + direction * move_distance_cu;
        move_last_point_and_resample(resample_buffer, positions_cu.slice(points), new_last_pos_cu);
      }
    });
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
  ScaleCurvesEffect(bool scale_up, const Brush &brush) : scale_up_(scale_up), brush_(brush) {}

  void execute(CurvesGeometry &curves,
               const IndexMask &curve_mask,
               const Span<float> move_distances_cu,
               MutableSpan<float3> positions_cu) override
  {
    const OffsetIndices points_by_curve = curves.points_by_curve();
    curve_mask.foreach_index(GrainSize(256), [&](const int64_t curve_i) {
      const float move_distance_cu = move_distances_cu[curve_i];
      const IndexRange points = points_by_curve[curve_i];

      const float old_length = this->compute_poly_curve_length(positions_cu.slice(points));
      const float length_diff = scale_up_ ? move_distance_cu : -move_distance_cu;
      const float min_length = brush_.curves_sculpt_settings->minimum_length;
      const float new_length = std::max(min_length, old_length + length_diff);
      const float scale_factor = math::safe_divide(new_length, old_length);

      const float3 &root_pos_cu = positions_cu[points[0]];
      for (float3 &pos_cu : positions_cu.slice(points.drop_front(1))) {
        pos_cu = (pos_cu - root_pos_cu) * scale_factor + root_pos_cu;
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
  CurvesEffectOperation(std::unique_ptr<CurvesEffect> effect) : effect_(std::move(effect)) {}

  void on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension) override;
};

/**
 * Utility class that actually executes the update when the stroke is updated. That's useful
 * because it avoids passing a very large number of parameters between functions.
 */
struct CurvesEffectOperationExecutor {
  CurvesEffectOperation *self_ = nullptr;
  CurvesSculptCommonContext ctx_;

  Object *object_ = nullptr;
  Curves *curves_id_ = nullptr;
  CurvesGeometry *curves_ = nullptr;

  VArray<float> curve_selection_factors_;
  IndexMaskMemory selected_curve_memory_;
  IndexMask curve_selection_;

  CurvesSculpt *curves_sculpt_ = nullptr;
  const Brush *brush_ = nullptr;
  float brush_radius_base_re_;
  float brush_radius_factor_;
  float brush_strength_;

  eBrushFalloffShape falloff_shape_;

  CurvesSurfaceTransforms transforms_;

  float2 brush_pos_start_re_;
  float2 brush_pos_end_re_;

  CurvesEffectOperationExecutor(const bContext &C) : ctx_(C) {}

  void execute(CurvesEffectOperation &self,
               const bContext &C,
               const StrokeExtension &stroke_extension)
  {
    BLI_SCOPED_DEFER([&]() { self.last_mouse_position_ = stroke_extension.mouse_position; });

    self_ = &self;
    object_ = CTX_data_active_object(&C);

    curves_id_ = static_cast<Curves *>(object_->data);
    curves_ = &curves_id_->geometry.wrap();
    if (curves_->is_empty()) {
      return;
    }

    curve_selection_factors_ = *curves_->attributes().lookup_or_default(
        ".selection", bke::AttrDomain::Curve, 1.0f);
    curve_selection_ = curves::retrieve_selected_curves(*curves_id_, selected_curve_memory_);

    curves_sculpt_ = ctx_.scene->toolsettings->curves_sculpt;
    brush_ = BKE_paint_brush_for_read(&curves_sculpt_->paint);
    brush_radius_base_re_ = BKE_brush_radius_get(&curves_sculpt_->paint, brush_);
    brush_radius_factor_ = brush_radius_factor(*brush_, stroke_extension);
    brush_strength_ = brush_strength_get(curves_sculpt_->paint, *brush_, stroke_extension);

    falloff_shape_ = eBrushFalloffShape(brush_->falloff_shape);

    transforms_ = CurvesSurfaceTransforms(*object_, curves_id_->surface);

    brush_pos_start_re_ = self.last_mouse_position_;
    brush_pos_end_re_ = stroke_extension.mouse_position;

    if (stroke_extension.is_first) {
      if (falloff_shape_ == PAINT_FALLOFF_SHAPE_SPHERE || (U.flag & USER_ORBIT_SELECTION)) {
        if (std::optional<CurvesBrush3D> brush_3d = sample_curves_3d_brush(
                *ctx_.depsgraph,
                *ctx_.region,
                *ctx_.v3d,
                *ctx_.rv3d,
                *object_,
                stroke_extension.mouse_position,
                brush_radius_base_re_))
        {
          self.brush_3d_ = *brush_3d;
          remember_stroke_position(
              *curves_sculpt_,
              math::transform_point(transforms_.curves_to_world, self_->brush_3d_.position_cu));
        }
      }

      return;
    }

    Array<float> move_distances_cu(curves_->curves_num());

    /* Compute influences. */
    if (falloff_shape_ == PAINT_FALLOFF_SHAPE_TUBE) {
      this->gather_influences_projected(move_distances_cu);
    }
    else if (falloff_shape_ == PAINT_FALLOFF_SHAPE_SPHERE) {
      this->gather_influences_spherical(move_distances_cu);
    }

    IndexMaskMemory memory;
    const IndexMask curves_mask = IndexMask::from_predicate(
        curve_selection_, GrainSize(4096), memory, [&](const int64_t curve_i) {
          return move_distances_cu[curve_i] > 0.0f;
        });

    /* Execute effect. */
    MutableSpan<float3> positions_cu = curves_->positions_for_write();
    self_->effect_->execute(*curves_, curves_mask, move_distances_cu, positions_cu);

    curves_->tag_positions_changed();
    DEG_id_tag_update(&curves_id_->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &curves_id_->id);
    ED_region_tag_redraw(ctx_.region);
  }

  void gather_influences_projected(MutableSpan<float> move_distances_cu)
  {
    const bke::crazyspace::GeometryDeformation deformation =
        bke::crazyspace::get_evaluated_curves_deformation(*ctx_.depsgraph, *object_);
    const OffsetIndices points_by_curve = curves_->points_by_curve();

    const float4x4 projection = ED_view3d_ob_project_mat_get(ctx_.rv3d, object_);

    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    Vector<float4x4> symmetry_brush_transforms_inv;
    for (const float4x4 &brush_transform : symmetry_brush_transforms) {
      /* Use explicit template call as MSVC 2019 has issues deducing the right template. */
      symmetry_brush_transforms_inv.append(math::invert<float, 4>(brush_transform));
    }

    const float brush_radius_re = brush_radius_base_re_ * brush_radius_factor_;
    const float brush_radius_sq_re = pow2f(brush_radius_re);

    curve_selection_.foreach_index(GrainSize(256), [&](int64_t curve_i) {
      const IndexRange points = points_by_curve[curve_i];

      const float curve_selection_factor = curve_selection_factors_[curve_i];

      float max_move_distance_cu = 0.0f;
      for (const float4x4 &brush_transform_inv : symmetry_brush_transforms_inv) {
        for (const int segment_i : points.drop_back(1)) {
          const float3 p1_cu = math::transform_point(brush_transform_inv,
                                                     deformation.positions[segment_i]);
          const float3 p2_cu = math::transform_point(brush_transform_inv,
                                                     deformation.positions[segment_i + 1]);

          const float2 p1_re = ED_view3d_project_float_v2_m4(ctx_.region, p1_cu, projection);
          const float2 p2_re = ED_view3d_project_float_v2_m4(ctx_.region, p2_cu, projection);

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

          if (dist_to_brush_sq_re > brush_radius_sq_re) {
            continue;
          }

          const float dist_to_brush_re = std::sqrt(dist_to_brush_sq_re);
          const float radius_falloff = BKE_brush_curve_strength(
              brush_, dist_to_brush_re, brush_radius_re);
          const float weight = brush_strength_ * radius_falloff * curve_selection_factor;

          const float3 closest_on_segment_cu = math::interpolate(p1_cu, p2_cu, lambda_on_segment);

          float3 brush_start_pos_wo, brush_end_pos_wo;
          ED_view3d_win_to_3d(
              ctx_.v3d,
              ctx_.region,
              math::transform_point(transforms_.curves_to_world, closest_on_segment_cu),
              brush_pos_start_re_,
              brush_start_pos_wo);
          ED_view3d_win_to_3d(
              ctx_.v3d,
              ctx_.region,
              math::transform_point(transforms_.curves_to_world, closest_on_segment_cu),
              brush_pos_end_re_,
              brush_end_pos_wo);
          const float3 brush_start_pos_cu = math::transform_point(transforms_.world_to_curves,
                                                                  brush_start_pos_wo);
          const float3 brush_end_pos_cu = math::transform_point(transforms_.world_to_curves,
                                                                brush_end_pos_wo);

          const float move_distance_cu = weight *
                                         math::distance(brush_start_pos_cu, brush_end_pos_cu);
          max_move_distance_cu = std::max(max_move_distance_cu, move_distance_cu);
        }
      }
      move_distances_cu[curve_i] = max_move_distance_cu;
    });
  }

  void gather_influences_spherical(MutableSpan<float> move_distances_cu)
  {
    const bke::crazyspace::GeometryDeformation deformation =
        bke::crazyspace::get_evaluated_curves_deformation(*ctx_.depsgraph, *object_);

    float3 brush_pos_wo = math::transform_point(transforms_.curves_to_world,
                                                self_->brush_3d_.position_cu);

    float3 brush_pos_start_wo, brush_pos_end_wo;
    ED_view3d_win_to_3d(
        ctx_.v3d, ctx_.region, brush_pos_wo, brush_pos_start_re_, brush_pos_start_wo);
    ED_view3d_win_to_3d(ctx_.v3d, ctx_.region, brush_pos_wo, brush_pos_end_re_, brush_pos_end_wo);
    const float3 brush_pos_start_cu = math::transform_point(transforms_.world_to_curves,
                                                            brush_pos_start_wo);
    const float3 brush_pos_end_cu = math::transform_point(transforms_.world_to_curves,
                                                          brush_pos_end_wo);
    const float3 brush_pos_diff_cu = brush_pos_end_cu - brush_pos_start_cu;
    const float brush_pos_diff_length_cu = math::length(brush_pos_diff_cu);
    const float brush_radius_cu = self_->brush_3d_.radius_cu * brush_radius_factor_;
    const float brush_radius_sq_cu = pow2f(brush_radius_cu);

    const Vector<float4x4> symmetry_brush_transforms = get_symmetry_brush_transforms(
        eCurvesSymmetryType(curves_id_->symmetry));
    const OffsetIndices points_by_curve = curves_->points_by_curve();

    curve_selection_.foreach_index(GrainSize(256), [&](int64_t curve_i) {
      const IndexRange points = points_by_curve[curve_i];

      const float curve_selection_factor = curve_selection_factors_[curve_i];

      float max_move_distance_cu = 0.0f;
      for (const float4x4 &brush_transform : symmetry_brush_transforms) {
        const float3 brush_pos_start_transformed_cu = math::transform_point(brush_transform,
                                                                            brush_pos_start_cu);
        const float3 brush_pos_end_transformed_cu = math::transform_point(brush_transform,
                                                                          brush_pos_end_cu);

        for (const int segment_i : points.drop_back(1)) {
          const float3 &p1_cu = deformation.positions[segment_i];
          const float3 &p2_cu = deformation.positions[segment_i + 1];

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
          const float weight = brush_strength_ * radius_falloff * curve_selection_factor;

          const float move_distance_cu = weight * brush_pos_diff_length_cu;
          max_move_distance_cu = std::max(max_move_distance_cu, move_distance_cu);
        }
      }
      move_distances_cu[curve_i] = max_move_distance_cu;
    });
  }
};

void CurvesEffectOperation::on_stroke_extended(const bContext &C,
                                               const StrokeExtension &stroke_extension)
{
  CurvesEffectOperationExecutor executor{C};
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
