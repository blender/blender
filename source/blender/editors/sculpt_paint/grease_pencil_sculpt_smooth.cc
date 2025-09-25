/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute.hh"
#include "BKE_brush.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_paint.hh"
#include "BKE_paint_types.hh"

#include "DNA_brush_enums.h"
#include "DNA_brush_types.h"

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

  /* Used when temporarily switching to smooth brush, save the previous active brush. */
  Brush *saved_active_brush_;
  char saved_mask_brush_tool_;
  int saved_smooth_size_; /* Smooth tool copies the size of the current tool. */

  void toggle_smooth_brush_on(const bContext &C);
  void toggle_smooth_brush_off(const bContext &C);

 public:
  using GreasePencilStrokeOperationCommon::GreasePencilStrokeOperationCommon;

  SmoothOperation(const BrushStrokeMode stroke_mode, const bool temp_smooth = false)
      : GreasePencilStrokeOperationCommon(stroke_mode), temp_smooth_(temp_smooth)
  {
  }

  void on_stroke_begin(const bContext &C, const InputSample &start_sample) override;
  void on_stroke_extended(const bContext &C, const InputSample &extension_sample) override;
  void on_stroke_done(const bContext &C) override;
};

void SmoothOperation::toggle_smooth_brush_on(const bContext &C)
{
  Paint *paint = BKE_paint_get_active_from_context(&C);
  Main *bmain = CTX_data_main(&C);
  Brush *current_brush = BKE_paint_brush(paint);

  if (current_brush->sculpt_brush_type == SCULPT_BRUSH_TYPE_MASK) {
    saved_mask_brush_tool_ = current_brush->mask_tool;
    current_brush->mask_tool = BRUSH_MASK_SMOOTH;
    return;
  }

  /* Switch to the smooth brush if possible. */
  BKE_paint_brush_set_essentials(bmain, paint, "Smooth");
  Brush *smooth_brush = BKE_paint_brush(paint);
  BLI_assert(smooth_brush != nullptr);

  init_brush(*smooth_brush);

  saved_active_brush_ = current_brush;
  saved_smooth_size_ = BKE_brush_size_get(paint, smooth_brush);

  const int current_brush_size = BKE_brush_size_get(paint, current_brush);
  BKE_brush_size_set(paint, smooth_brush, current_brush_size);
  BKE_curvemapping_init(smooth_brush->curve_distance_falloff);
}

void SmoothOperation::toggle_smooth_brush_off(const bContext &C)
{
  Paint *paint = BKE_paint_get_active_from_context(&C);
  Brush &brush = *BKE_paint_brush(paint);

  if (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_MASK) {
    brush.mask_tool = saved_mask_brush_tool_;
    return;
  }

  /* If saved_active_brush is not set, brush was not switched/affected in
   * toggle_temp_on(). */
  if (saved_active_brush_) {
    BKE_brush_size_set(paint, &brush, saved_smooth_size_);
    BKE_paint_brush_set(paint, saved_active_brush_);
    saved_active_brush_ = nullptr;
  }
}

void SmoothOperation::on_stroke_begin(const bContext &C, const InputSample &start_sample)
{
  if (temp_smooth_) {
    toggle_smooth_brush_on(C);
    this->start_mouse_position = start_sample.mouse_position;
    this->prev_mouse_position = start_sample.mouse_position;
  }
  else {
    this->init_stroke(C, start_sample);
  }
  this->init_auto_masking(C, start_sample);
}

void SmoothOperation::on_stroke_extended(const bContext &C, const InputSample &extension_sample)
{
  Paint &paint = *BKE_paint_get_active_from_context(&C);
  const Brush &brush = [&]() -> const Brush & {
    if (temp_smooth_) {
      const Brush *brush = BKE_paint_brush_from_essentials(
          CTX_data_main(&C), PaintMode::SculptGPencil, "Smooth");
      BLI_assert(brush != nullptr);
      return *brush;
    }
    return *BKE_paint_brush(&paint);
  }();
  const int sculpt_mode_flag = brush.gpencil_settings->sculpt_mode_flag;

  this->foreach_editable_drawing_with_automask(
      C, [&](const GreasePencilStrokeParams &params, const IndexMask &point_mask) {
        Array<float2> view_positions = calculate_view_positions(params, point_mask);
        bke::CurvesGeometry &curves = params.drawing.strokes_for_write();
        bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
        const OffsetIndices points_by_curve = curves.points_by_curve();
        const VArray<bool> cyclic = curves.cyclic();
        const int iterations = 2;

        const VArray<float> influences = VArray<float>::from_func(
            view_positions.size(), [&](const int64_t point_) {
              return brush_point_influence(paint,
                                           brush,
                                           view_positions[point_],
                                           extension_sample,
                                           params.multi_frame_falloff);
            });
        Array<bool> selection_array(curves.points_num());
        point_mask.to_bools(selection_array);
        const VArray<bool> selection_varray = VArray<bool>::from_span(selection_array);

        bool changed = false;
        if (sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_POSITION) {
          geometry::smooth_curve_positions(curves,
                                           curves.curves_range(),
                                           selection_varray,
                                           iterations,
                                           influences,
                                           false,
                                           false);

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
          if (bke::SpanAttributeWriter<float> rotations =
                  attributes.lookup_or_add_for_write_span<float>("rotation",
                                                                 bke::AttrDomain::Point))
          {
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
        }
        return changed;
      });
  this->stroke_extended(extension_sample);
}

void SmoothOperation::on_stroke_done(const bContext &C)
{
  if (temp_smooth_) {
    toggle_smooth_brush_off(C);
  }
}

std::unique_ptr<GreasePencilStrokeOperation> new_smooth_operation(
    const BrushStrokeMode stroke_mode, const bool temp_smooth)
{
  return std::make_unique<SmoothOperation>(stroke_mode, temp_smooth);
}

}  // namespace blender::ed::sculpt_paint::greasepencil
