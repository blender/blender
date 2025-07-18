/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_paint.hh"

#include "grease_pencil_intern.hh"

namespace blender::ed::sculpt_paint::greasepencil {

class VertexBlurOperation : public GreasePencilStrokeOperationCommon {
  using GreasePencilStrokeOperationCommon::GreasePencilStrokeOperationCommon;

 public:
  void on_stroke_begin(const bContext &C, const InputSample &start_sample) override;
  void on_stroke_extended(const bContext &C, const InputSample &extension_sample) override;
  void on_stroke_done(const bContext &C) override;
};

void VertexBlurOperation::on_stroke_begin(const bContext &C, const InputSample &start_sample)
{
  this->init_stroke(C, start_sample);
  this->on_stroke_extended(C, start_sample);
}

void VertexBlurOperation::on_stroke_extended(const bContext &C,
                                             const InputSample &extension_sample)
{
  const Scene &scene = *CTX_data_scene(&C);
  Paint &paint = *BKE_paint_get_active_from_context(&C);
  const Brush &brush = *BKE_paint_brush(&paint);
  const float radius = brush_radius(paint, brush, extension_sample.pressure);
  const float radius_squared = radius * radius;

  const bool use_selection_masking = ED_grease_pencil_any_vertex_mask_selection(
      scene.toolsettings);

  this->foreach_editable_drawing(C, GrainSize(1), [&](const GreasePencilStrokeParams &params) {
    IndexMaskMemory memory;
    const IndexMask stroke_selection = curve_mask_for_stroke_operation(
        params, use_selection_masking, memory);
    if (stroke_selection.is_empty()) {
      return false;
    }
    const IndexMask point_selection = point_mask_for_stroke_operation(
        params, use_selection_masking, memory);
    const Array<float2> view_positions = calculate_view_positions(params, point_selection);
    const OffsetIndices<int> points_by_curve = params.drawing.strokes().points_by_curve();
    MutableSpan<ColorGeometry4f> vertex_colors = params.drawing.vertex_colors_for_write();
    stroke_selection.foreach_index(GrainSize(1024), [&](const int64_t curve) {
      const IndexRange points = points_by_curve[curve];

      float3 average_color(0.0f);
      int color_count = 0;
      for (const int point : points) {
        const ColorGeometry4f color = vertex_colors[point];
        const float distance = math::distance_squared(extension_sample.mouse_position,
                                                      view_positions[point]);
        if (color.a > 0.0f && distance < radius_squared) {
          average_color += float3(color.r, color.g, color.b);
          color_count++;
        }
      }

      if (color_count == 0) {
        return;
      }
      average_color = average_color / color_count;
      const ColorGeometry4f mix_color(average_color.x, average_color.y, average_color.z, 1.0f);

      for (const int point : points) {
        const float influence = brush_point_influence(
            paint, brush, view_positions[point], extension_sample, params.multi_frame_falloff);
        ColorGeometry4f &color = vertex_colors[point];
        if (color.a > 0.0f && influence > 0.0f) {
          color = math::interpolate(color, mix_color, influence);
        }
      }
    });
    return true;
  });
}

void VertexBlurOperation::on_stroke_done(const bContext & /*C*/) {}

std::unique_ptr<GreasePencilStrokeOperation> new_vertex_blur_operation()
{
  return std::make_unique<VertexBlurOperation>();
}

}  // namespace blender::ed::sculpt_paint::greasepencil
