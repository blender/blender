/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BLI_math_color.h"

#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_paint.hh"

#include "DEG_depsgraph.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"

namespace blender::ed::greasepencil {

enum class VertexColorMode : int8_t {
  Stroke = 0,
  Fill = 1,
  Both = 2,
};

static const EnumPropertyItem prop_grease_pencil_vertex_mode[] = {
    {int(VertexColorMode::Stroke), "STROKE", 0, "Stroke", ""},
    {int(VertexColorMode::Fill), "FILL", 0, "Fill", ""},
    {int(VertexColorMode::Both), "BOTH", 0, "Stroke & Fill", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

template<typename Fn>
static bool apply_color_operation_for_mode(const VertexColorMode mode,
                                           Object &object,
                                           MutableDrawingInfo &info,
                                           const bool use_selection_mask,
                                           Fn &&fn)
{
  bool changed = false;
  IndexMaskMemory memory;
  using namespace ed::greasepencil;
  if (ELEM(mode, VertexColorMode::Stroke, VertexColorMode::Both)) {
    if (info.drawing.strokes().attributes().contains("vertex_color")) {
      const IndexMask points = use_selection_mask ?
                                   retrieve_editable_and_selected_points(
                                       object, info.drawing, info.layer_index, memory) :
                                   retrieve_editable_points(
                                       object, info.drawing, info.layer_index, memory);
      if (!points.is_empty()) {
        MutableSpan<ColorGeometry4f> vertex_colors = info.drawing.vertex_colors_for_write();
        points.foreach_index(GrainSize(4096), [&](const int64_t point_i) {
          ColorGeometry4f &color = vertex_colors[point_i];
          if (color.a > 0.0f) {
            color = fn(color);
          }
        });
        changed = true;
      }
    }
  }
  if (ELEM(mode, VertexColorMode::Fill, VertexColorMode::Both)) {
    if (info.drawing.strokes().attributes().contains("fill_color")) {
      const IndexMask strokes = use_selection_mask ?
                                    ed::greasepencil::retrieve_editable_and_selected_strokes(
                                        object, info.drawing, info.layer_index, memory) :
                                    ed::greasepencil::retrieve_editable_strokes(
                                        object, info.drawing, info.layer_index, memory);
      if (!strokes.is_empty()) {
        MutableSpan<ColorGeometry4f> fill_colors = info.drawing.fill_colors_for_write();
        strokes.foreach_index(GrainSize(1024), [&](const int64_t curve_i) {
          ColorGeometry4f &color = fill_colors[curve_i];
          if (color.a > 0.0f) {
            color = fn(color);
          }
        });
        changed = true;
      }
    }
  }
  return changed;
}

static wmOperatorStatus grease_pencil_vertex_paint_brightness_contrast_exec(bContext *C,
                                                                            wmOperator *op)
{
  const Scene &scene = *CTX_data_scene(C);
  Object &object = *CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);
  const VertexColorMode mode = VertexColorMode(RNA_enum_get(op->ptr, "mode"));
  const float brightness = RNA_float_get(op->ptr, "brightness");
  const float contrast = RNA_float_get(op->ptr, "contrast");
  float delta = contrast / 2.0f;
  const bool use_selection_mask = ED_grease_pencil_any_vertex_mask_selection(scene.toolsettings);

  /*
   * The algorithm is by Werner D. Streidt
   * (http://visca.com/ffactory/archives/5-99/msg00021.html)
   * Extracted of OpenCV `demhist.c`.
   */
  float gain, offset;
  if (contrast > 0.0f) {
    gain = 1.0f - delta * 2.0f;
    gain = 1.0f / math::max(gain, FLT_EPSILON);
    offset = gain * (brightness - delta);
  }
  else {
    delta *= -1.0f;
    gain = math::max(1.0f - delta * 2.0f, 0.0f);
    offset = gain * brightness + delta;
  }

  std::atomic<bool> any_changed;
  Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](MutableDrawingInfo info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    if (curves.is_empty()) {
      return;
    }
    const bool changed = apply_color_operation_for_mode(
        mode,
        object,
        info,
        use_selection_mask,
        [&](const ColorGeometry4f &color) -> ColorGeometry4f {
          const float3 result = float3(color) * gain + offset;
          return ColorGeometry4f(result[0], result[1], result[2], color.a);
        });
    any_changed.store(any_changed | changed, std::memory_order_relaxed);
  });

  if (any_changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }
  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_vertex_color_brightness_contrast(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Vertex Paint Brightness/Contrast";
  ot->idname = "GREASE_PENCIL_OT_vertex_color_brightness_contrast";
  ot->description = "Adjust vertex color brightness/contrast";

  /* API callbacks. */
  ot->exec = grease_pencil_vertex_paint_brightness_contrast_exec;
  ot->poll = grease_pencil_vertex_painting_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* params */
  ot->prop = RNA_def_enum(
      ot->srna, "mode", prop_grease_pencil_vertex_mode, int(VertexColorMode::Both), "Mode", "");

  RNA_def_float(ot->srna, "brightness", 0.0f, -1.0f, 1.0f, "Brightness", "", -1.0f, 1.0f);
  RNA_def_float(ot->srna, "contrast", 0.0f, -1.0f, 1.0f, "Contrast", "", -1.0f, 1.0f);
}

static wmOperatorStatus grease_pencil_vertex_paint_hsv_exec(bContext *C, wmOperator *op)
{
  const Scene &scene = *CTX_data_scene(C);
  Object &object = *CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);
  const VertexColorMode mode = VertexColorMode(RNA_enum_get(op->ptr, "mode"));
  const float hue = RNA_float_get(op->ptr, "h");
  const float sat = RNA_float_get(op->ptr, "s");
  const float val = RNA_float_get(op->ptr, "v");
  const bool use_selection_mask = ED_grease_pencil_any_vertex_mask_selection(scene.toolsettings);

  std::atomic<bool> any_changed;
  Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](MutableDrawingInfo info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    if (curves.is_empty()) {
      return;
    }
    const bool changed = apply_color_operation_for_mode(
        mode,
        object,
        info,
        use_selection_mask,
        [&](const ColorGeometry4f &color) -> ColorGeometry4f {
          float3 hsv;
          rgb_to_hsv_v(float3(color), hsv);

          hsv[0] += (hue - 0.5f);
          if (hsv[0] > 1.0f) {
            hsv[0] -= 1.0f;
          }
          else if (hsv[0] < 0.0f) {
            hsv[0] += 1.0f;
          }
          hsv[1] *= sat;
          hsv[2] *= val;

          float3 rgb_result;
          hsv_to_rgb_v(hsv, rgb_result);
          return ColorGeometry4f(rgb_result[0], rgb_result[1], rgb_result[2], color.a);
        });
    any_changed.store(any_changed | changed, std::memory_order_relaxed);
  });

  if (any_changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }
  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_vertex_color_hsv(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Vertex Paint Hue/Saturation/Value";
  ot->idname = "GREASE_PENCIL_OT_vertex_color_hsv";
  ot->description = "Adjust vertex color HSV values";

  /* API callbacks. */
  ot->exec = grease_pencil_vertex_paint_hsv_exec;
  ot->poll = grease_pencil_vertex_painting_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* params */
  ot->prop = RNA_def_enum(
      ot->srna, "mode", prop_grease_pencil_vertex_mode, int(VertexColorMode::Both), "Mode", "");
  RNA_def_float(ot->srna, "h", 0.5f, 0.0f, 1.0f, "Hue", "", 0.0f, 1.0f);
  RNA_def_float(ot->srna, "s", 1.0f, 0.0f, 2.0f, "Saturation", "", 0.0f, 2.0f);
  RNA_def_float(ot->srna, "v", 1.0f, 0.0f, 2.0f, "Value", "", 0.0f, 2.0f);
}

static wmOperatorStatus grease_pencil_vertex_paint_invert_exec(bContext *C, wmOperator *op)
{
  const Scene &scene = *CTX_data_scene(C);
  Object &object = *CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);
  const VertexColorMode mode = VertexColorMode(RNA_enum_get(op->ptr, "mode"));
  const bool use_selection_mask = ED_grease_pencil_any_vertex_mask_selection(scene.toolsettings);

  std::atomic<bool> any_changed;
  Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](MutableDrawingInfo info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    if (curves.is_empty()) {
      return;
    }
    const bool changed = apply_color_operation_for_mode(
        mode,
        object,
        info,
        use_selection_mask,
        [&](const ColorGeometry4f &color) -> ColorGeometry4f {
          /* Invert the color. */
          return ColorGeometry4f(1.0f - color.r, 1.0f - color.g, 1.0f - color.b, color.a);
        });
    any_changed.store(any_changed | changed, std::memory_order_relaxed);
  });

  if (any_changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }
  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_vertex_color_invert(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Vertex Paint Invert";
  ot->idname = "GREASE_PENCIL_OT_vertex_color_invert";
  ot->description = "Invert RGB values";

  /* API callbacks. */
  ot->exec = grease_pencil_vertex_paint_invert_exec;
  ot->poll = grease_pencil_vertex_painting_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* params */
  ot->prop = RNA_def_enum(
      ot->srna, "mode", prop_grease_pencil_vertex_mode, int(VertexColorMode::Both), "Mode", "");
}

static wmOperatorStatus grease_pencil_vertex_paint_levels_exec(bContext *C, wmOperator *op)
{
  const Scene &scene = *CTX_data_scene(C);
  Object &object = *CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);
  const VertexColorMode mode = VertexColorMode(RNA_enum_get(op->ptr, "mode"));
  const float gain = RNA_float_get(op->ptr, "gain");
  const float offset = RNA_float_get(op->ptr, "offset");
  const bool use_selection_mask = ED_grease_pencil_any_vertex_mask_selection(scene.toolsettings);

  std::atomic<bool> any_changed;
  Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](MutableDrawingInfo info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    if (curves.is_empty()) {
      return;
    }
    const bool changed = apply_color_operation_for_mode(
        mode,
        object,
        info,
        use_selection_mask,
        [&](const ColorGeometry4f &color) -> ColorGeometry4f {
          const float3 result = float3(color) * gain + offset;
          return ColorGeometry4f(result[0], result[1], result[2], color.a);
        });
    any_changed.store(any_changed | changed, std::memory_order_relaxed);
  });

  if (any_changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }
  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_vertex_color_levels(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Vertex Paint Levels";
  ot->idname = "GREASE_PENCIL_OT_vertex_color_levels";
  ot->description = "Adjust levels of vertex colors";

  /* API callbacks. */
  ot->exec = grease_pencil_vertex_paint_levels_exec;
  ot->poll = grease_pencil_vertex_painting_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* params */
  ot->prop = RNA_def_enum(
      ot->srna, "mode", prop_grease_pencil_vertex_mode, int(VertexColorMode::Both), "Mode", "");

  RNA_def_float(
      ot->srna, "offset", 0.0f, -1.0f, 1.0f, "Offset", "Value to add to colors", -1.0f, 1.0f);
  RNA_def_float(
      ot->srna, "gain", 1.0f, 0.0f, FLT_MAX, "Gain", "Value to multiply colors by", 0.0f, 10.0f);
}

static wmOperatorStatus grease_pencil_vertex_paint_set_exec(bContext *C, wmOperator *op)
{
  const Scene &scene = *CTX_data_scene(C);
  Object &object = *CTX_data_active_object(C);
  Paint &paint = *BKE_paint_get_active_from_context(C);
  const Brush &brush = *BKE_paint_brush(&paint);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);
  const VertexColorMode mode = VertexColorMode(RNA_enum_get(op->ptr, "mode"));
  const float factor = RNA_float_get(op->ptr, "factor");
  const bool use_selection_mask = ED_grease_pencil_any_vertex_mask_selection(scene.toolsettings);

  float3 color_linear = BKE_brush_color_get(&paint, &brush);
  const ColorGeometry4f target_color(color_linear[0], color_linear[1], color_linear[2], 1.0f);

  std::atomic<bool> any_changed;
  Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](MutableDrawingInfo info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    if (curves.is_empty()) {
      return;
    }
    /* Create the color attributes if they don't exist. */
    if (ELEM(mode, VertexColorMode::Stroke, VertexColorMode::Both)) {
      curves.attributes_for_write().add<ColorGeometry4f>(
          "vertex_color", bke::AttrDomain::Point, bke::AttributeInitDefaultValue());
    }
    if (ELEM(mode, VertexColorMode::Fill, VertexColorMode::Both)) {
      curves.attributes_for_write().add<ColorGeometry4f>(
          "fill_color", bke::AttrDomain::Curve, bke::AttributeInitDefaultValue());
    }
    const bool changed = apply_color_operation_for_mode(
        mode,
        object,
        info,
        use_selection_mask,
        [&](const ColorGeometry4f &color) -> ColorGeometry4f {
          /* Mix in the target color based on the factor. */
          return math::interpolate(color, target_color, factor);
        });
    any_changed.store(any_changed | changed, std::memory_order_relaxed);
  });

  if (any_changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }
  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_vertex_color_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Vertex Paint Set Color";
  ot->idname = "GREASE_PENCIL_OT_vertex_color_set";
  ot->description = "Set active color to all selected vertex";

  /* API callbacks. */
  ot->exec = grease_pencil_vertex_paint_set_exec;
  ot->poll = grease_pencil_vertex_painting_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* params */
  ot->prop = RNA_def_enum(
      ot->srna, "mode", prop_grease_pencil_vertex_mode, int(VertexColorMode::Both), "Mode", "");
  RNA_def_float(ot->srna, "factor", 1.0f, 0.0f, 1.0f, "Factor", "Mix Factor", 0.0f, 1.0f);
}

static wmOperatorStatus grease_pencil_vertex_paint_reset_exec(bContext *C, wmOperator *op)
{
  const Scene &scene = *CTX_data_scene(C);
  Object &object = *CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);
  const VertexColorMode mode = VertexColorMode(RNA_enum_get(op->ptr, "mode"));
  const bool use_selection_mask = ED_grease_pencil_any_vertex_mask_selection(scene.toolsettings);

  std::atomic<bool> any_changed;
  Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](MutableDrawingInfo info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    if (curves.is_empty()) {
      return;
    }

    bool changed = false;
    if (use_selection_mask) {
      changed |= apply_color_operation_for_mode(
          mode,
          object,
          info,
          use_selection_mask,
          [&](const ColorGeometry4f & /*color*/) -> ColorGeometry4f {
            return ColorGeometry4f(0.0f, 0.0f, 0.0f, 0.0f);
          });
      any_changed.store(any_changed | changed, std::memory_order_relaxed);
      return;
    }

    /* Remove the color attributes. */
    if (ELEM(mode, VertexColorMode::Stroke, VertexColorMode::Both)) {
      changed |= curves.attributes_for_write().remove("vertex_color");
    }
    if (ELEM(mode, VertexColorMode::Fill, VertexColorMode::Both)) {
      changed |= curves.attributes_for_write().remove("fill_color");
    }
    any_changed.store(any_changed | changed, std::memory_order_relaxed);
  });

  if (any_changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }
  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_stroke_reset_vertex_color(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reset Vertex Color";
  ot->idname = "GREASE_PENCIL_OT_stroke_reset_vertex_color";
  ot->description = "Reset vertex color for all or selected strokes";

  /* callbacks */
  ot->exec = grease_pencil_vertex_paint_reset_exec;
  ot->poll = grease_pencil_vertex_painting_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(
      ot->srna, "mode", prop_grease_pencil_vertex_mode, int(VertexColorMode::Both), "Mode", "");
}

}  // namespace blender::ed::greasepencil

void ED_operatortypes_grease_pencil_vertex_paint()
{
  using namespace blender::ed::greasepencil;
  WM_operatortype_append(GREASE_PENCIL_OT_vertex_color_brightness_contrast);
  WM_operatortype_append(GREASE_PENCIL_OT_vertex_color_hsv);
  WM_operatortype_append(GREASE_PENCIL_OT_vertex_color_invert);
  WM_operatortype_append(GREASE_PENCIL_OT_vertex_color_levels);
  WM_operatortype_append(GREASE_PENCIL_OT_vertex_color_set);
  WM_operatortype_append(GREASE_PENCIL_OT_stroke_reset_vertex_color);
}
