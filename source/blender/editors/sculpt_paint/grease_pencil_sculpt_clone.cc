/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_paint.hh"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "grease_pencil_intern.hh"
#include "paint_intern.hh"

namespace blender::ed::sculpt_paint::greasepencil {

class CloneOperation : public GreasePencilStrokeOperationCommon {
 public:
  using GreasePencilStrokeOperationCommon::GreasePencilStrokeOperationCommon;

  void on_stroke_begin(const bContext &C, const InputSample &start_sample) override;
  void on_stroke_extended(const bContext &C, const InputSample &extension_sample) override;
  void on_stroke_done(const bContext & /*C*/) override {}
};

static float2 arithmetic_mean(Span<float2> values)
{
  return std::accumulate(values.begin(), values.end(), float2(0)) / values.size();
}

void CloneOperation::on_stroke_begin(const bContext &C, const InputSample &start_sample)
{
  Main &bmain = *CTX_data_main(&C);
  Object &object = *CTX_data_active_object(&C);

  this->init_stroke(C, start_sample);

  /* NOTE: Only one copy is created at the beginning of each stroke.
   * GPv2 supposedly has 2 modes:
   * - Stamp: Clone on stroke start and then transform (the transform part doesn't work)
   * - Continuous: Create multiple copies during the stroke (disabled)
   *
   * Here we only have the GPv2 behavior that actually works for now. */
  this->foreach_editable_drawing(C, [&](const GreasePencilStrokeParams &params) {
    const IndexRange pasted_curves = ed::greasepencil::clipboard_paste_strokes(
        bmain, object, params.drawing, false);
    if (pasted_curves.is_empty()) {
      return false;
    }

    bke::CurvesGeometry &curves = params.drawing.strokes_for_write();
    const OffsetIndices<int> pasted_points_by_curve = curves.points_by_curve().slice(
        pasted_curves);
    const IndexRange pasted_points = IndexRange::from_begin_size(
        pasted_points_by_curve[0].start(),
        pasted_points_by_curve.total_size() - pasted_points_by_curve[0].start());

    Array<float2> view_positions = calculate_view_positions(params, pasted_points);
    const float2 center = arithmetic_mean(view_positions.as_mutable_span().slice(pasted_points));
    const float2 &mouse_delta = start_sample.mouse_position - center;

    MutableSpan<float3> positions = curves.positions_for_write();
    threading::parallel_for(pasted_points, 4096, [&](const IndexRange range) {
      for (const int point_i : range) {
        positions[point_i] = params.placement.project(view_positions[point_i] + mouse_delta);
      }
    });
    params.drawing.tag_positions_changed();

    return true;
  });
}

void CloneOperation::on_stroke_extended(const bContext & /*C*/,
                                        const InputSample &extension_sample)
{
  this->stroke_extended(extension_sample);
}

std::unique_ptr<GreasePencilStrokeOperation> new_clone_operation(const BrushStrokeMode stroke_mode)
{
  return std::make_unique<CloneOperation>(stroke_mode);
}

}  // namespace blender::ed::sculpt_paint::greasepencil
