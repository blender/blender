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

  const bool use_selection_masking = ED_grease_pencil_any_vertex_mask_selection(
      scene.toolsettings);

  const bool do_points = do_vertex_color_points(brush);
  const bool do_fill = do_vertex_color_fill(brush);

  float3 color_linear;
  copy_v3_v3(color_linear, BKE_brush_color_get(&paint, &brush));
  const ColorGeometry4f replace_color(color_linear.x, color_linear.y, color_linear.z, 1.0f);

  this->foreach_editable_drawing(
      C,
      [&](const GreasePencilStrokeParams &params) {
        IndexMaskMemory memory;
        const IndexMask point_selection = point_mask_for_stroke_operation(
            params, use_selection_masking, memory);
        if (!point_selection.is_empty() && do_points) {
          const Array<float2> view_positions = view_positions_from_point_mask(params,
                                                                              point_selection);
          MutableSpan<ColorGeometry4f> vertex_colors = params.drawing.vertex_colors_for_write();
          point_selection.foreach_index(
              [&](const int64_t point_i) {
                const float influence = brush_point_influence(paint,
                                                              brush,
                                                              view_positions[point_i],
                                                              extension_sample,
                                                              params.multi_frame_falloff);
                if (influence > 0.0f && vertex_colors[point_i].a > 0.0f) {
                  vertex_colors[point_i] = replace_color;
                }
              },
              exec_mode::grain_size(4096));
        }

        const IndexMask fill_selection = fill_mask_for_stroke_operation(
            params, use_selection_masking, memory);
        if (!fill_selection.is_empty() && do_fill) {
          BLI_assert(params.drawing.fills().has_value());
          const GroupedSpan<int> fills = *params.drawing.fills();
          const bke::CurvesGeometry &curves = params.drawing.strokes();
          const OffsetIndices<int> points_by_curve = curves.points_by_curve();
          MutableSpan<ColorGeometry4f> fill_colors = params.drawing.fill_colors_for_write();
          /* TODO. Only calculate needed positions. */
          const Array<float2> view_positions = view_positions_from_curve_mask(
              params, curves.curves_range());

          fill_selection.foreach_index(
              [&](const int64_t fill_i) {
                const Span<int> fill_curves = fills[fill_i];
                const IndexMask fill_curve_mask = IndexMask::from_indices(fill_curves, memory);

                float influence = 0.0f;
                fill_curve_mask.foreach_index([&](const int64_t curve) {
                  const IndexRange points = points_by_curve[curve];
                  const Span<float2> curve_view_positions = view_positions.as_span().slice(points);
                  influence = math::max(influence,
                                        brush_fill_influence(paint,
                                                             brush,
                                                             curve_view_positions,
                                                             extension_sample,
                                                             params.multi_frame_falloff));
                });

                ColorGeometry4f color = fill_colors[fill_curves.first()];
                color.a -= influence;
                color.a = math::max(color.a, 0.0f);

                if (influence > 0.0f && color.a > 0.0f) {
                  color = replace_color;
                }

                index_mask::masked_fill(fill_colors, color, fill_curve_mask);
              },

              exec_mode::grain_size(1024));
        }
        return true;
      },
      exec_mode::grain_size(1));
}

void VertexReplaceOperation::on_stroke_done(const bContext & /*C*/) {}

std::unique_ptr<GreasePencilStrokeOperation> new_vertex_replace_operation()
{
  return std::make_unique<VertexReplaceOperation>();
}

}  // namespace blender::ed::sculpt_paint::greasepencil
