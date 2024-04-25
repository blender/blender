/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "BKE_context.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_paint.hh"

#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "grease_pencil_intern.hh"
#include "paint_intern.hh"

namespace blender::ed::sculpt_paint::greasepencil {

class StrengthOperation : public GreasePencilStrokeOperationCommon {
 public:
  using GreasePencilStrokeOperationCommon::GreasePencilStrokeOperationCommon;

  void on_stroke_begin(const bContext &C, const InputSample &start_sample) override;
  void on_stroke_extended(const bContext &C, const InputSample &extension_sample) override;
  void on_stroke_done(const bContext & /*C*/) override {}
};

void StrengthOperation::on_stroke_begin(const bContext &C, const InputSample &start_sample)
{
  this->init_stroke(C, start_sample);
}

void StrengthOperation::on_stroke_extended(const bContext &C, const InputSample &extension_sample)
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
    MutableSpan<float> opacities = params.drawing.opacities_for_write();

    selection.foreach_index(GrainSize(4096), [&](const int64_t point_i) {
      float &opacity = opacities[point_i];
      const float influence = brush_influence(
          scene, brush, view_positions[point_i], extension_sample, params.multi_frame_falloff);
      /* Brush influence mapped to opacity by a factor of 0.125. */
      const float delta_opacity = (invert ? -influence : influence) * 0.125f;
      opacity = std::clamp(opacity + delta_opacity, 0.0f, 1.0f);
    });

    return true;
  });
  this->stroke_extended(extension_sample);
}

std::unique_ptr<GreasePencilStrokeOperation> new_strength_operation(
    const BrushStrokeMode stroke_mode)
{
  return std::make_unique<StrengthOperation>(stroke_mode);
}

}  // namespace blender::ed::sculpt_paint::greasepencil
