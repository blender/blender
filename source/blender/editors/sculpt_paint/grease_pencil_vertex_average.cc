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

class VertexAverageOperation : public GreasePencilStrokeOperationCommon {
  using GreasePencilStrokeOperationCommon::GreasePencilStrokeOperationCommon;

 public:
  void on_stroke_begin(const bContext &C, const InputSample &start_sample) override;
  void on_stroke_extended(const bContext &C, const InputSample &extension_sample) override;
  void on_stroke_done(const bContext &C) override;
};

void VertexAverageOperation::on_stroke_begin(const bContext &C, const InputSample &start_sample)
{
  this->init_stroke(C, start_sample);
  this->on_stroke_extended(C, start_sample);
}

void VertexAverageOperation::on_stroke_extended(const bContext &C,
                                                const InputSample &extension_sample)
{
  const Scene &scene = *CTX_data_scene(&C);
  Paint &paint = *BKE_paint_get_active_from_context(&C);
  const Brush &brush = *BKE_paint_brush(&paint);
  const float radius = brush_radius(paint, brush, extension_sample.pressure);
  const float radius_squared = radius * radius;

  const bool use_selection_masking = ED_grease_pencil_any_vertex_mask_selection(
      scene.toolsettings);

  const bool do_points = do_vertex_color_points(brush);
  const bool do_fill = do_vertex_color_fill(brush);

  /* Compute the average color under the brush. */
  float3 average_color(0.0f);
  int color_count = 0;
  this->foreach_editable_drawing(C, [&](const GreasePencilStrokeParams &params) {
    IndexMaskMemory memory;
    const IndexMask point_selection = point_mask_for_stroke_operation(
        params, use_selection_masking, memory);
    if (!point_selection.is_empty() && do_points) {
      const Array<float2> view_positions = calculate_view_positions(params, point_selection);
      const VArray<ColorGeometry4f> vertex_colors = params.drawing.vertex_colors();

      point_selection.foreach_index([&](const int64_t point_i) {
        const ColorGeometry4f color = vertex_colors[point_i];
        if (color.a > 0.0f && math::distance_squared(extension_sample.mouse_position,
                                                     view_positions[point_i]) < radius_squared)
        {
          average_color += float3(color.r, color.g, color.b);
          color_count++;
        }
      });
    }
    const IndexMask fill_selection = fill_mask_for_stroke_operation(
        params, use_selection_masking, memory);
    if (!fill_selection.is_empty() && do_fill) {
      const OffsetIndices<int> points_by_curve = params.drawing.strokes().points_by_curve();
      const Array<float2> view_positions = calculate_view_positions(params, point_selection);
      const VArray<ColorGeometry4f> fill_colors = params.drawing.fill_colors();

      fill_selection.foreach_index([&](const int64_t curve_i) {
        const IndexRange points = points_by_curve[curve_i];
        const Span<float2> curve_view_positions = view_positions.as_span().slice(points);
        const ColorGeometry4f color = fill_colors[curve_i];
        if (color.a > 0.0f && closest_distance_to_surface_2d(extension_sample.mouse_position,
                                                             curve_view_positions) < radius)
        {
          average_color += float3(color.r, color.g, color.b);
          color_count++;
        }
      });
    }
    return true;
  });

  if (color_count <= 0) {
    return;
  }
  average_color = average_color / color_count;
  /* The average color is the color that will be mixed in. */
  const ColorGeometry4f mix_color(average_color.x, average_color.y, average_color.z, 1.0f);

  this->foreach_editable_drawing(C, GrainSize(1), [&](const GreasePencilStrokeParams &params) {
    IndexMaskMemory memory;
    const IndexMask point_selection = point_mask_for_stroke_operation(
        params, use_selection_masking, memory);
    if (!point_selection.is_empty() && do_points) {
      const Array<float2> view_positions = calculate_view_positions(params, point_selection);
      MutableSpan<ColorGeometry4f> vertex_colors = params.drawing.vertex_colors_for_write();

      point_selection.foreach_index(GrainSize(4096), [&](const int64_t point_i) {
        const float influence = brush_point_influence(
            paint, brush, view_positions[point_i], extension_sample, params.multi_frame_falloff);

        ColorGeometry4f &color = vertex_colors[point_i];
        color = math::interpolate(color, mix_color, influence);
      });
    }

    const IndexMask fill_selection = fill_mask_for_stroke_operation(
        params, use_selection_masking, memory);
    if (!fill_selection.is_empty() && do_fill) {
      const OffsetIndices<int> points_by_curve = params.drawing.strokes().points_by_curve();
      const Array<float2> view_positions = calculate_view_positions(params, point_selection);
      MutableSpan<ColorGeometry4f> fill_colors = params.drawing.fill_colors_for_write();

      fill_selection.foreach_index(GrainSize(1024), [&](const int64_t curve_i) {
        const IndexRange points = points_by_curve[curve_i];
        const Span<float2> curve_view_positions = view_positions.as_span().slice(points);
        const float influence = brush_fill_influence(
            paint, brush, curve_view_positions, extension_sample, params.multi_frame_falloff);

        ColorGeometry4f &color = fill_colors[curve_i];
        color = math::interpolate(color, mix_color, influence);
      });
    }
    return true;
  });
}

void VertexAverageOperation::on_stroke_done(const bContext & /*C*/) {}

std::unique_ptr<GreasePencilStrokeOperation> new_vertex_average_operation()
{
  return std::make_unique<VertexAverageOperation>();
}

}  // namespace blender::ed::sculpt_paint::greasepencil
