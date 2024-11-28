/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_paint.hh"

#include "DNA_brush_enums.h"
#include "DNA_brush_types.h"
#include "DNA_gpencil_legacy_types.h"

#include "GEO_smooth_curves.hh"

#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "grease_pencil_intern.hh"
#include "paint_intern.hh"

namespace blender::ed::sculpt_paint::greasepencil {

class SmoothOperation : public GreasePencilStrokeOperationCommon {
 private:
  bool temp_smooth_;

 public:
  using GreasePencilStrokeOperationCommon::GreasePencilStrokeOperationCommon;

  SmoothOperation(const BrushStrokeMode stroke_mode, const bool temp_smooth = false)
      : GreasePencilStrokeOperationCommon(stroke_mode), temp_smooth_(temp_smooth)
  {
  }

  void on_stroke_begin(const bContext &C, const InputSample &start_sample) override;
  void on_stroke_extended(const bContext &C, const InputSample &extension_sample) override;
  void on_stroke_done(const bContext & /*C*/) override {}
};

void SmoothOperation::on_stroke_begin(const bContext &C, const InputSample &start_sample)
{
  if (temp_smooth_) {
    Brush *brush = BKE_paint_brush_from_essentials(
        CTX_data_main(&C), OB_MODE_SCULPT_GREASE_PENCIL, "Smooth");
    BLI_assert(brush != nullptr);

    init_brush(*brush);

    this->start_mouse_position = start_sample.mouse_position;
    this->prev_mouse_position = start_sample.mouse_position;
  }
  else {
    this->init_stroke(C, start_sample);
  }
}

void SmoothOperation::on_stroke_extended(const bContext &C, const InputSample &extension_sample)
{
  const Scene &scene = *CTX_data_scene(&C);
  const Brush &brush = [&]() -> const Brush & {
    if (temp_smooth_) {
      const Brush *brush = BKE_paint_brush_from_essentials(
          CTX_data_main(&C), OB_MODE_SCULPT_GREASE_PENCIL, "Smooth");
      BLI_assert(brush != nullptr);
      return *brush;
    }
    Paint &paint = *BKE_paint_get_active_from_context(&C);
    return *BKE_paint_brush(&paint);
  }();
  const int sculpt_mode_flag = brush.gpencil_settings->sculpt_mode_flag;

  const bool is_masking = GPENCIL_ANY_SCULPT_MASK(
      eGP_Sculpt_SelectMaskFlag(scene.toolsettings->gpencil_selectmode_sculpt));

  this->foreach_editable_drawing(C, [&](const GreasePencilStrokeParams &params) {
    IndexMaskMemory selection_memory;
    const IndexMask selection = point_selection_mask(params, is_masking, selection_memory);
    if (selection.is_empty()) {
      return false;
    }

    Array<float2> view_positions = calculate_view_positions(params, selection);
    bke::CurvesGeometry &curves = params.drawing.strokes_for_write();
    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
    const OffsetIndices points_by_curve = curves.points_by_curve();
    const VArray<bool> cyclic = curves.cyclic();
    const int iterations = 2;

    const VArray<float> influences = VArray<float>::ForFunc(
        view_positions.size(), [&](const int64_t point_) {
          return brush_point_influence(
              scene, brush, view_positions[point_], extension_sample, params.multi_frame_falloff);
        });
    Array<bool> selection_array(curves.points_num());
    selection.to_bools(selection_array);
    const VArray<bool> selection_varray = VArray<bool>::ForSpan(selection_array);

    bool changed = false;
    if (sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_POSITION) {
      MutableSpan<float3> positions = curves.positions_for_write();
      geometry::smooth_curve_attribute(curves.curves_range(),
                                       points_by_curve,
                                       selection_varray,
                                       cyclic,
                                       iterations,
                                       influences,
                                       false,
                                       false,
                                       positions);
      params.drawing.tag_positions_changed();
      changed = true;
    }
    if (sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_STRENGTH) {
      MutableSpan<float> opacities = params.drawing.opacities_for_write();
      geometry::smooth_curve_attribute(curves.curves_range(),
                                       points_by_curve,
                                       selection_varray,
                                       cyclic,
                                       iterations,
                                       influences,
                                       true,
                                       false,
                                       opacities);
      changed = true;
    }
    if (sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_THICKNESS) {
      const MutableSpan<float> radii = params.drawing.radii_for_write();
      geometry::smooth_curve_attribute(curves.curves_range(),
                                       points_by_curve,
                                       selection_varray,
                                       cyclic,
                                       iterations,
                                       influences,
                                       true,
                                       false,
                                       radii);
      curves.tag_radii_changed();
      changed = true;
    }
    if (sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_UV) {
      bke::SpanAttributeWriter<float> rotations = attributes.lookup_or_add_for_write_span<float>(
          "rotation", bke::AttrDomain::Point);
      geometry::smooth_curve_attribute(curves.curves_range(),
                                       points_by_curve,
                                       selection_varray,
                                       cyclic,
                                       iterations,
                                       influences,
                                       true,
                                       false,
                                       rotations.span);
      rotations.finish();
      changed = true;
    }
    return changed;
  });
  this->stroke_extended(extension_sample);
}

std::unique_ptr<GreasePencilStrokeOperation> new_smooth_operation(
    const BrushStrokeMode stroke_mode, const bool temp_smooth)
{
  return std::make_unique<SmoothOperation>(stroke_mode, temp_smooth);
}

}  // namespace blender::ed::sculpt_paint::greasepencil
