/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_context.hh"
#include "BKE_crazyspace.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_paint.hh"

#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "grease_pencil_intern.hh"
#include "paint_intern.hh"

namespace blender::ed::sculpt_paint::greasepencil {

class PinchOperation : public GreasePencilStrokeOperationCommon {
 public:
  using GreasePencilStrokeOperationCommon::GreasePencilStrokeOperationCommon;

  void on_stroke_begin(const bContext &C, const InputSample &start_sample) override;
  void on_stroke_extended(const bContext &C, const InputSample &extension_sample) override;
  void on_stroke_done(const bContext & /*C*/) override {}
};

void PinchOperation::on_stroke_begin(const bContext &C, const InputSample &start_sample)
{
  this->init_stroke(C, start_sample);
  this->init_auto_masking(C, start_sample);
}

void PinchOperation::on_stroke_extended(const bContext &C, const InputSample &extension_sample)
{
  Paint &paint = *BKE_paint_get_active_from_context(&C);
  const Brush &brush = *BKE_paint_brush(&paint);
  const bool invert = this->is_inverted(brush);

  this->foreach_editable_drawing_with_automask(
      C,
      [&](const GreasePencilStrokeParams &params,
          const IndexMask &point_mask,
          const DeltaProjectionFunc &projection_fn) {
        bke::crazyspace::GeometryDeformation deformation = get_drawing_deformation(params);
        const Array<float2> view_positions = view_positions_from_point_mask(params, point_mask);
        bke::CurvesGeometry &curves = params.drawing.strokes_for_write();
        MutableSpan<float3> positions = curves.positions_for_write();

        const float2 target = extension_sample.mouse_position;

        point_mask.foreach_index(GrainSize(4096), [&](const int64_t point_i) {
          const float2 &co = view_positions[point_i];
          const float influence = brush_point_influence(
              paint, brush, co, extension_sample, params.multi_frame_falloff);
          if (influence <= 0.0f) {
            return;
          }

          const float influence_squared = influence * influence / 25.0f;
          const float influence_final = invert ? 1.0 + influence_squared :
                                                 1.0f - influence_squared;
          positions[point_i] += compute_orig_delta(
              projection_fn, deformation, point_i, (target - co) * (1.0f - influence_final));
        });

        MutableSpan<float3> handle_positions_left = curves.handle_positions_left_for_write();
        MutableSpan<float3> handle_positions_right = curves.handle_positions_right_for_write();

        if (!handle_positions_left.is_empty()) {
          const Array<float2> view_positions_left = view_positions_left_from_point_mask(
              params, point_mask);
          const Array<float2> view_positions_right = view_positions_right_from_point_mask(
              params, point_mask);

          point_mask.foreach_index(GrainSize(4096), [&](const int64_t point_i) {
            const float2 co_left = view_positions_left[point_i];
            const float2 co_right = view_positions_right[point_i];
            const float influence_left = brush_point_influence(
                paint, brush, co_left, extension_sample, params.multi_frame_falloff);
            const float influence_right = brush_point_influence(
                paint, brush, co_right, extension_sample, params.multi_frame_falloff);

            const float influence_left_squared = influence_left * influence_left / 25.0f;
            const float influence_left_final = invert ? 1.0f + influence_left_squared :
                                                        1.0f - influence_left_squared;
            const float influence_right_squared = influence_right * influence_right / 25.0f;
            const float influence_right_final = invert ? 1.0f + influence_right_squared :
                                                         1.0f - influence_right_squared;
            handle_positions_left[point_i] += compute_orig_delta(
                projection_fn,
                deformation,
                point_i,
                (target - co_left) * (1.0f - influence_left_final));
            handle_positions_right[point_i] += compute_orig_delta(
                projection_fn,
                deformation,
                point_i,
                (target - co_right) * (1.0f - influence_right_final));
          });

          curves.calculate_bezier_auto_handles();
          curves.calculate_bezier_aligned_handles();
        }

        params.drawing.tag_positions_changed();
        return true;
      });
  this->stroke_extended(extension_sample);
}

std::unique_ptr<GreasePencilStrokeOperation> new_pinch_operation(const BrushStrokeMode stroke_mode)
{
  return std::make_unique<PinchOperation>(stroke_mode);
}

}  // namespace blender::ed::sculpt_paint::greasepencil
