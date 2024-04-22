/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_context.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph_query.hh"

#include "DNA_brush_types.h"
#include "DNA_grease_pencil_types.h"

#include "DNA_scene_types.h"
#include "ED_grease_pencil.hh"
#include "ED_image.hh"
#include "ED_object.hh"
#include "ED_screen.hh"

#include "ANIM_keyframing.hh"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_toolsystem.hh"

#include "grease_pencil_intern.hh"
#include "paint_intern.hh"

namespace blender::ed::sculpt_paint {

/* -------------------------------------------------------------------- */
/** \name Common Paint Operator Functions
 * \{ */

static bool stroke_get_location(bContext * /*C*/,
                                float out[3],
                                const float mouse[2],
                                bool /*force_original*/)
{
  out[0] = mouse[0];
  out[1] = mouse[1];
  out[2] = 0;
  return true;
}

static void stroke_start(bContext &C,
                         wmOperator &op,
                         const float2 &mouse,
                         GreasePencilStrokeOperation &operation)
{
  PaintStroke *paint_stroke = static_cast<PaintStroke *>(op.customdata);

  InputSample start_sample;
  start_sample.mouse_position = float2(mouse);
  start_sample.pressure = 0.0f;

  paint_stroke_set_mode_data(paint_stroke, &operation);
  operation.on_stroke_begin(C, start_sample);
}

static void stroke_update_step(bContext *C,
                               wmOperator * /*op*/,
                               PaintStroke *stroke,
                               PointerRNA *stroke_element)
{
  GreasePencilStrokeOperation *operation = static_cast<GreasePencilStrokeOperation *>(
      paint_stroke_mode_data(stroke));

  InputSample extension_sample;
  RNA_float_get_array(stroke_element, "mouse", extension_sample.mouse_position);
  extension_sample.pressure = RNA_float_get(stroke_element, "pressure");

  if (operation) {
    operation->on_stroke_extended(*C, extension_sample);
  }
}

static void stroke_redraw(const bContext *C, PaintStroke * /*stroke*/, bool /*final*/)
{
  ED_region_tag_redraw(CTX_wm_region(C));
}

static void stroke_done(const bContext *C, PaintStroke *stroke)
{
  GreasePencilStrokeOperation *operation = static_cast<GreasePencilStrokeOperation *>(
      paint_stroke_mode_data(stroke));
  operation->on_stroke_done(*C);
  operation->~GreasePencilStrokeOperation();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Brush Stroke Operator
 * \{ */

static bool grease_pencil_brush_stroke_poll(bContext *C)
{
  if (!ed::greasepencil::grease_pencil_painting_poll(C)) {
    return false;
  }
  if (!WM_toolsystem_active_tool_is_brush(C)) {
    return false;
  }
  return true;
}

static GreasePencilStrokeOperation *grease_pencil_brush_stroke_operation(bContext &C)
{
  const Scene &scene = *CTX_data_scene(&C);
  const GpPaint &gp_paint = *scene.toolsettings->gp_paint;
  const Brush &brush = *BKE_paint_brush_for_read(&gp_paint.paint);
  switch (eBrushGPaintTool(brush.gpencil_tool)) {
    case GPAINT_TOOL_DRAW:
      /* FIXME: Somehow store the unique_ptr in the PaintStroke. */
      return greasepencil::new_paint_operation().release();
    case GPAINT_TOOL_ERASE:
      return greasepencil::new_erase_operation().release();
    case GPAINT_TOOL_FILL:
      return nullptr;
    case GPAINT_TOOL_TINT:
      return greasepencil::new_tint_operation().release();
  }
  return nullptr;
}

static bool grease_pencil_brush_stroke_test_start(bContext *C,
                                                  wmOperator *op,
                                                  const float mouse[2])
{
  GreasePencilStrokeOperation *operation = grease_pencil_brush_stroke_operation(*C);
  if (operation) {
    stroke_start(*C, *op, float2(mouse), *operation);
    return true;
  }
  return false;
}

static int grease_pencil_brush_stroke_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  int return_value = ed::greasepencil::grease_pencil_draw_operator_invoke(C, op);
  if (return_value != OPERATOR_RUNNING_MODAL) {
    return return_value;
  }

  op->customdata = paint_stroke_new(C,
                                    op,
                                    stroke_get_location,
                                    grease_pencil_brush_stroke_test_start,
                                    stroke_update_step,
                                    stroke_redraw,
                                    stroke_done,
                                    event->type);

  return_value = op->type->modal(C, op, event);
  if (return_value == OPERATOR_FINISHED) {
    return OPERATOR_FINISHED;
  }

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static int grease_pencil_brush_stroke_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  return paint_stroke_modal(C, op, event, reinterpret_cast<PaintStroke **>(&op->customdata));
}

static void grease_pencil_brush_stroke_cancel(bContext *C, wmOperator *op)
{
  paint_stroke_cancel(C, op, static_cast<PaintStroke *>(op->customdata));
}

static void GREASE_PENCIL_OT_brush_stroke(wmOperatorType *ot)
{
  ot->name = "Grease Pencil Draw";
  ot->idname = "GREASE_PENCIL_OT_brush_stroke";
  ot->description = "Draw a new stroke in the active Grease Pencil object";

  ot->poll = grease_pencil_brush_stroke_poll;
  ot->invoke = grease_pencil_brush_stroke_invoke;
  ot->modal = grease_pencil_brush_stroke_modal;
  ot->cancel = grease_pencil_brush_stroke_cancel;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  paint_stroke_operator_properties(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Operator
 * \{ */

static bool grease_pencil_sculpt_paint_poll(bContext *C)
{
  if (!ed::greasepencil::grease_pencil_sculpting_poll(C)) {
    return false;
  }
  if (!WM_toolsystem_active_tool_is_brush(C)) {
    return false;
  }
  return true;
}

static GreasePencilStrokeOperation *grease_pencil_sculpt_paint_operation(bContext &C)
{
  const Scene &scene = *CTX_data_scene(&C);
  const GpSculptPaint &gp_sculptpaint = *scene.toolsettings->gp_sculptpaint;
  const Brush &brush = *BKE_paint_brush_for_read(&gp_sculptpaint.paint);
  switch (eBrushGPSculptTool(brush.gpencil_sculpt_tool)) {
    case GPSCULPT_TOOL_SMOOTH:
      return nullptr;
    case GPSCULPT_TOOL_THICKNESS:
      return nullptr;
    case GPSCULPT_TOOL_STRENGTH:
      return nullptr;
    case GPSCULPT_TOOL_GRAB:
      return nullptr;
    case GPSCULPT_TOOL_PUSH:
      return nullptr;
    case GPSCULPT_TOOL_TWIST:
      return nullptr;
    case GPSCULPT_TOOL_PINCH:
      return nullptr;
    case GPSCULPT_TOOL_RANDOMIZE:
      return nullptr;
    case GPSCULPT_TOOL_CLONE:
      return nullptr;
  }
  return nullptr;
}

static bool grease_pencil_sculpt_paint_test_start(bContext *C,
                                                  wmOperator *op,
                                                  const float mouse[2])
{
  GreasePencilStrokeOperation *operation = grease_pencil_sculpt_paint_operation(*C);
  if (operation) {
    stroke_start(*C, *op, float2(mouse), *operation);
    return true;
  }
  return false;
}

static int grease_pencil_sculpt_paint_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const Scene *scene = CTX_data_scene(C);
  const Object *object = CTX_data_active_object(C);
  if (!object || object->type != OB_GREASE_PENCIL) {
    return OPERATOR_CANCELLED;
  }

  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  if (!grease_pencil.has_active_layer()) {
    BKE_report(op->reports, RPT_ERROR, "No active Grease Pencil layer");
    return OPERATOR_CANCELLED;
  }

  const Paint *paint = BKE_paint_get_active_from_context(C);
  const Brush *brush = BKE_paint_brush_for_read(paint);
  if (brush == nullptr) {
    return OPERATOR_CANCELLED;
  }

  bke::greasepencil::Layer &active_layer = *grease_pencil.get_active_layer();

  if (!active_layer.is_editable()) {
    BKE_report(op->reports, RPT_ERROR, "Active layer is locked or hidden");
    return OPERATOR_CANCELLED;
  }

  /* Ensure a drawing at the current keyframe. */
  if (!ed::greasepencil::ensure_active_keyframe(*scene, grease_pencil)) {
    BKE_report(op->reports, RPT_ERROR, "No Grease Pencil frame to draw on");
    return OPERATOR_CANCELLED;
  }

  op->customdata = paint_stroke_new(C,
                                    op,
                                    stroke_get_location,
                                    grease_pencil_sculpt_paint_test_start,
                                    stroke_update_step,
                                    stroke_redraw,
                                    stroke_done,
                                    event->type);

  const int return_value = op->type->modal(C, op, event);
  if (return_value == OPERATOR_FINISHED) {
    return OPERATOR_FINISHED;
  }

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static int grease_pencil_sculpt_paint_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  return paint_stroke_modal(C, op, event, reinterpret_cast<PaintStroke **>(&op->customdata));
}

static void grease_pencil_sculpt_paint_cancel(bContext *C, wmOperator *op)
{
  paint_stroke_cancel(C, op, static_cast<PaintStroke *>(op->customdata));
}

static void GREASE_PENCIL_OT_sculpt_paint(wmOperatorType *ot)
{
  ot->name = "Grease Pencil Draw";
  ot->idname = "GREASE_PENCIL_OT_sculpt_paint";
  ot->description = "Draw a new stroke in the active Grease Pencil object";

  ot->poll = grease_pencil_sculpt_paint_poll;
  ot->invoke = grease_pencil_sculpt_paint_invoke;
  ot->modal = grease_pencil_sculpt_paint_modal;
  ot->cancel = grease_pencil_sculpt_paint_cancel;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  paint_stroke_operator_properties(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Draw Mode
 * \{ */

static bool grease_pencil_mode_poll_paint_cursor(bContext *C)
{
  if (!grease_pencil_brush_stroke_poll(C)) {
    return false;
  }
  if (CTX_wm_region_view3d(C) == nullptr) {
    return false;
  }
  return true;
}

static void grease_pencil_draw_mode_enter(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  wmMsgBus *mbus = CTX_wm_message_bus(C);

  Object *ob = CTX_data_active_object(C);
  GpPaint *grease_pencil_paint = scene->toolsettings->gp_paint;
  BKE_paint_ensure(scene->toolsettings, (Paint **)&grease_pencil_paint);

  ob->mode = OB_MODE_PAINT_GREASE_PENCIL;

  /* TODO: Setup cursor color. BKE_paint_init() could be used, but creates an additional brush. */
  ED_paint_cursor_start(&grease_pencil_paint->paint, grease_pencil_mode_poll_paint_cursor);
  paint_init_pivot(ob, scene);

  /* Necessary to change the object mode on the evaluated object. */
  DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
  WM_msg_publish_rna_prop(mbus, &ob->id, ob, Object, mode);
  WM_event_add_notifier(C, NC_SCENE | ND_MODE, nullptr);
}

static void grease_pencil_draw_mode_exit(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  ob->mode = OB_MODE_OBJECT;
}

static int grease_pencil_draw_mode_toggle_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  wmMsgBus *mbus = CTX_wm_message_bus(C);

  const bool is_mode_set = ob->mode == OB_MODE_PAINT_GREASE_PENCIL;

  if (is_mode_set) {
    if (!object::mode_compat_set(C, ob, OB_MODE_PAINT_GREASE_PENCIL, op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  if (is_mode_set) {
    grease_pencil_draw_mode_exit(C);
  }
  else {
    grease_pencil_draw_mode_enter(C);
  }

  WM_toolsystem_update_from_context_view3d(C);

  /* Necessary to change the object mode on the evaluated object. */
  DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
  WM_msg_publish_rna_prop(mbus, &ob->id, ob, Object, mode);
  WM_event_add_notifier(C, NC_SCENE | ND_MODE, nullptr);
  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_draw_mode_toggle(wmOperatorType *ot)
{
  ot->name = "Grease Pencil Draw Mode Toggle";
  ot->idname = "GREASE_PENCIL_OT_draw_mode_toggle";
  ot->description = "Enter/Exit draw mode for grease pencil";

  ot->exec = grease_pencil_draw_mode_toggle_exec;
  ot->poll = ed::greasepencil::active_grease_pencil_poll;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;
}

/** \} */

}  // namespace blender::ed::sculpt_paint

/* -------------------------------------------------------------------- */
/** \name Registration
 * \{ */

void ED_operatortypes_grease_pencil_draw()
{
  using namespace blender::ed::sculpt_paint;
  WM_operatortype_append(GREASE_PENCIL_OT_brush_stroke);
  WM_operatortype_append(GREASE_PENCIL_OT_sculpt_paint);
  WM_operatortype_append(GREASE_PENCIL_OT_draw_mode_toggle);
}

/** \} */
