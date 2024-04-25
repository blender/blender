/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "BKE_context.hh"
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

class ThicknessOperation : public GreasePencilStrokeOperationCommon {
 public:
  using GreasePencilStrokeOperationCommon::GreasePencilStrokeOperationCommon;

  void on_stroke_begin(const bContext &C, const InputSample &start_sample) override;
  void on_stroke_extended(const bContext &C, const InputSample &extension_sample) override;
  void on_stroke_done(const bContext & /*C*/) override {}
};

void ThicknessOperation::on_stroke_begin(const bContext &C, const InputSample &start_sample)
{
  this->init_stroke(C, start_sample);
}

void ThicknessOperation::on_stroke_extended(const bContext &C, const InputSample &extension_sample)
{
  const Scene &scene = *CTX_data_scene(&C);
  Paint &paint = *BKE_paint_get_active_from_context(&C);
  const Brush &brush = *BKE_paint_brush(&paint);
  const bool invert = this->is_inverted(brush);

  this->foreach_editable_drawing(C, [&](const GreasePencilStrokeParams &params) {
    IndexMaskMemory selection_memory;
    const IndexMask selection = point_selection_mask(params, selection_memory);
    if (selection.is_empty()) {
      return false;
    }

    Array<float2> view_positions = calculate_view_positions(params, selection);
    bke::CurvesGeometry &curves = params.drawing.strokes_for_write();
    BLI_assert(view_positions.size() == curves.points_num());
    MutableSpan<float> radii = params.drawing.radii_for_write();

    selection.foreach_index(GrainSize(4096), [&](const int64_t point_i) {
      float &radius = radii[point_i];
      const float influence = brush_influence(
          scene, brush, view_positions[point_i], extension_sample, params.multi_frame_falloff);
      /* Factor 1/1000 is used to map arbitrary influence value to a sensible radius. */
      const float delta_radius = (invert ? -influence : influence) * 0.001f;
      radius = std::max(radius + delta_radius, 0.0f);
    });

    curves.tag_radii_changed();
    return true;
  });
  this->stroke_extended(extension_sample);
}

std::unique_ptr<GreasePencilStrokeOperation> new_thickness_operation(
    const BrushStrokeMode stroke_mode)
{
  return std::make_unique<ThicknessOperation>(stroke_mode);
}

}  // namespace blender::ed::sculpt_paint::greasepencil
