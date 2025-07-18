/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_color.hh"

#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_paint.hh"

#include "grease_pencil_intern.hh"

namespace blender::ed::sculpt_paint::greasepencil {

class VertexPaintOperation : public GreasePencilStrokeOperationCommon {
  using GreasePencilStrokeOperationCommon::GreasePencilStrokeOperationCommon;

 public:
  void on_stroke_begin(const bContext &C, const InputSample &start_sample) override;
  void on_stroke_extended(const bContext &C, const InputSample &extension_sample) override;
  void on_stroke_done(const bContext & /*C*/) override {}
};

void VertexPaintOperation::on_stroke_begin(const bContext &C, const InputSample &start_sample)
{
  this->init_stroke(C, start_sample);
  this->on_stroke_extended(C, start_sample);
}

void VertexPaintOperation::on_stroke_extended(const bContext &C,
                                              const InputSample &extension_sample)
{
  const Scene &scene = *CTX_data_scene(&C);
  Paint &paint = *BKE_paint_get_active_from_context(&C);
  const Brush &brush = *BKE_paint_brush(&paint);
  const bool invert = this->is_inverted(brush);

  const bool use_selection_masking = ED_grease_pencil_any_vertex_mask_selection(
      scene.toolsettings);

  const bool do_points = do_vertex_color_points(brush);
  const bool do_fill = do_vertex_color_fill(brush);

  float color_linear[3];
  srgb_to_linearrgb_v3_v3(color_linear, BKE_brush_color_get(&paint, &brush));
  const ColorGeometry4f mix_color(color_linear[0], color_linear[1], color_linear[2], 1.0f);

  this->foreach_editable_drawing(C, GrainSize(1), [&](const GreasePencilStrokeParams &params) {
    IndexMaskMemory memory;
    const IndexMask point_selection = point_mask_for_stroke_operation(
        params, use_selection_masking, memory);
    if (!point_selection.is_empty() && do_points) {
      Array<float2> view_positions = calculate_view_positions(params, point_selection);
      MutableSpan<ColorGeometry4f> vertex_colors = params.drawing.vertex_colors_for_write();

      if (invert) {
        /* Erase vertex colors. */
        point_selection.foreach_index(GrainSize(4096), [&](const int64_t point_i) {
          const float influence = brush_point_influence(
              paint, brush, view_positions[point_i], extension_sample, params.multi_frame_falloff);

          ColorGeometry4f &color = vertex_colors[point_i];
          color.a -= influence;
          color.a = math::max(color.a, 0.0f);
        });
      }
      else {
        /* Mix brush color into vertex colors by influence using alpha over. */
        point_selection.foreach_index(GrainSize(4096), [&](const int64_t point_i) {
          const float influence = brush_point_influence(
              paint, brush, view_positions[point_i], extension_sample, params.multi_frame_falloff);

          ColorGeometry4f &color = vertex_colors[point_i];
          color = math::interpolate(color, mix_color, influence);
        });
      }
    }

    const IndexMask fill_selection = fill_mask_for_stroke_operation(
        params, use_selection_masking, memory);
    if (!fill_selection.is_empty() && do_fill) {
      const OffsetIndices<int> points_by_curve = params.drawing.strokes().points_by_curve();
      Array<float2> view_positions = calculate_view_positions(params, point_selection);
      MutableSpan<ColorGeometry4f> fill_colors = params.drawing.fill_colors_for_write();

      if (invert) {
        fill_selection.foreach_index(GrainSize(1024), [&](const int64_t curve_i) {
          const IndexRange points = points_by_curve[curve_i];
          const Span<float2> curve_view_positions = view_positions.as_span().slice(points);
          const float influence = brush_fill_influence(
              paint, brush, curve_view_positions, extension_sample, params.multi_frame_falloff);

          ColorGeometry4f &color = fill_colors[curve_i];
          color.a -= influence;
          color.a = math::max(color.a, 0.0f);
        });
      }
      else {
        fill_selection.foreach_index(GrainSize(1024), [&](const int64_t curve_i) {
          const IndexRange points = points_by_curve[curve_i];
          const Span<float2> curve_view_positions = view_positions.as_span().slice(points);
          const float influence = brush_fill_influence(
              paint, brush, curve_view_positions, extension_sample, params.multi_frame_falloff);

          ColorGeometry4f &color = fill_colors[curve_i];
          color = math::interpolate(color, mix_color, influence);
        });
      }
    }

    return true;
  });
}

std::unique_ptr<GreasePencilStrokeOperation> new_vertex_paint_operation(
    const BrushStrokeMode stroke_mode)
{
  return std::make_unique<VertexPaintOperation>(stroke_mode);
}

}  // namespace blender::ed::sculpt_paint::greasepencil
