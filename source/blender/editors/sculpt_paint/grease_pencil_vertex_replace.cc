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

class VertexReplaceOperation : public GreasePencilStrokeOperationCommon {
  using GreasePencilStrokeOperationCommon::GreasePencilStrokeOperationCommon;

 public:
  void on_stroke_begin(const bContext &C, const InputSample &start_sample) override;
  void on_stroke_extended(const bContext &C, const InputSample &extension_sample) override;
  void on_stroke_done(const bContext &C) override;
};

void VertexReplaceOperation::on_stroke_begin(const bContext &C, const InputSample &start_sample)
{
  this->init_stroke(C, start_sample);
  this->on_stroke_extended(C, start_sample);
}

void VertexReplaceOperation::on_stroke_extended(const bContext &C,
                                                const InputSample &extension_sample)
{
  const Scene &scene = *CTX_data_scene(&C);
  Paint &paint = *BKE_paint_get_active_from_context(&C);
  const Brush &brush = *BKE_paint_brush(&paint);

  const bool is_masking = GPENCIL_ANY_VERTEX_MASK(
      eGP_vertex_SelectMaskFlag(scene.toolsettings->gpencil_selectmode_vertex));

  const bool do_points = do_vertex_color_points(brush);
  const bool do_fill = do_vertex_color_fill(brush);

  float3 color_linear;
  srgb_to_linearrgb_v3_v3(color_linear, BKE_brush_color_get(&scene, &paint, &brush));
  const ColorGeometry4f replace_color(color_linear.x, color_linear.y, color_linear.z, 1.0f);

  this->foreach_editable_drawing(C, GrainSize(1), [&](const GreasePencilStrokeParams &params) {
    IndexMaskMemory memory;
    const IndexMask point_selection = point_selection_mask(params, is_masking, memory);
    if (!point_selection.is_empty() && do_points) {
      Array<float2> view_positions = calculate_view_positions(params, point_selection);
      MutableSpan<ColorGeometry4f> vertex_colors = params.drawing.vertex_colors_for_write();
      point_selection.foreach_index(GrainSize(4096), [&](const int64_t point_i) {
        const float influence = brush_point_influence(
            scene, brush, view_positions[point_i], extension_sample, params.multi_frame_falloff);
        if (influence > 0.0f && vertex_colors[point_i].a > 0.0f) {
          vertex_colors[point_i] = replace_color;
        }
      });
    }

    const IndexMask fill_selection = fill_selection_mask(params, is_masking, memory);
    if (!fill_selection.is_empty() && do_fill) {
      const OffsetIndices<int> points_by_curve = params.drawing.strokes().points_by_curve();
      Array<float2> view_positions = calculate_view_positions(params, point_selection);
      MutableSpan<ColorGeometry4f> fill_colors = params.drawing.fill_colors_for_write();

      fill_selection.foreach_index(GrainSize(1024), [&](const int64_t curve_i) {
        const IndexRange points = points_by_curve[curve_i];
        const Span<float2> curve_view_positions = view_positions.as_span().slice(points);
        const float influence = brush_fill_influence(
            scene, brush, curve_view_positions, extension_sample, params.multi_frame_falloff);
        if (influence > 0.0f && fill_colors[curve_i].a > 0.0f) {
          fill_colors[curve_i] = replace_color;
        }
      });
    }
    return true;
  });
}

void VertexReplaceOperation::on_stroke_done(const bContext & /*C*/) {}

std::unique_ptr<GreasePencilStrokeOperation> new_vertex_replace_operation()
{
  return std::make_unique<VertexReplaceOperation>();
}

}  // namespace blender::ed::sculpt_paint::greasepencil
