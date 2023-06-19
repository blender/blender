/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_context.h"
#include "BKE_grease_pencil.hh"

#include "DEG_depsgraph_query.h"

#include "DNA_brush_types.h"
#include "DNA_grease_pencil_types.h"

#include "ED_grease_pencil.h"
#include "ED_object.h"
#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"

#include "grease_pencil_intern.hh"
#include "paint_intern.hh"

namespace blender::ed::sculpt_paint {

/* -------------------------------------------------------------------- */
/** \name Brush Stroke Operator
 * \{ */

static bool start_brush_operation(bContext &C,
                                  wmOperator & /*op*/,
                                  PaintStroke *paint_stroke,
                                  const StrokeExtension & /*stroke_start*/)
{
  // const BrushStrokeMode mode = static_cast<BrushStrokeMode>(RNA_enum_get(op.ptr, "mode"));

  const Scene &scene = *CTX_data_scene(&C);
  const GpPaint &gp_paint = *scene.toolsettings->gp_paint;
  const Brush &brush = *BKE_paint_brush_for_read(&gp_paint.paint);
  GreasePencilStrokeOperation *operation = nullptr;
  switch (brush.gpencil_tool) {
    case GPAINT_TOOL_DRAW:
      /* FIXME: Somehow store the unique_ptr in the PaintStroke. */
      operation = greasepencil::new_paint_operation().release();
      break;
  }

  if (operation) {
    paint_stroke_set_mode_data(paint_stroke, operation);
    return true;
  }
  return false;
}

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

static bool stroke_test_start(bContext *C, wmOperator *op, const float mouse[2])
{
  PaintStroke *paint_stroke = static_cast<PaintStroke *>(op->customdata);

  StrokeExtension stroke_extension;
  stroke_extension.mouse_position = float2(mouse);
  stroke_extension.pressure = 0.0f;
  stroke_extension.is_first = true;

  if (!start_brush_operation(*C, *op, paint_stroke, stroke_extension)) {
    return false;
  }

  return true;
}

static void stroke_update_step(bContext *C,
                               wmOperator * /*op*/,
                               PaintStroke *stroke,
                               PointerRNA *stroke_element)
{
  GreasePencilStrokeOperation *operation = static_cast<GreasePencilStrokeOperation *>(
      paint_stroke_mode_data(stroke));

  StrokeExtension stroke_extension;
  RNA_float_get_array(stroke_element, "mouse", stroke_extension.mouse_position);
  stroke_extension.pressure = RNA_float_get(stroke_element, "pressure");
  stroke_extension.is_first = false;

  if (operation) {
    operation->on_stroke_extended(*C, stroke_extension);
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
}

static int grease_pencil_stroke_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const Paint *paint = BKE_paint_get_active_from_context(C);
  const Brush *brush = BKE_paint_brush_for_read(paint);
  if (brush == nullptr) {
    return OPERATOR_CANCELLED;
  }

  op->customdata = paint_stroke_new(C,
                                    op,
                                    stroke_get_location,
                                    stroke_test_start,
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

static int grease_pencil_stroke_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  return paint_stroke_modal(C, op, event, reinterpret_cast<PaintStroke **>(&op->customdata));
}

static void grease_pencil_stroke_cancel(bContext *C, wmOperator *op)
{
  paint_stroke_cancel(C, op, static_cast<PaintStroke *>(op->customdata));
}

static void GREASE_PENCIL_OT_brush_stroke(wmOperatorType *ot)
{
  ot->name = "Grease Pencil Draw";
  ot->idname = "GREASE_PENCIL_OT_brush_stroke";
  ot->description = "Draw a new stroke in the active Grease Pencil object";

  ot->invoke = grease_pencil_stroke_invoke;
  ot->modal = grease_pencil_stroke_modal;
  ot->cancel = grease_pencil_stroke_cancel;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  paint_stroke_operator_properties(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Draw Mode
 * \{ */

static bool grease_pencil_poll(bContext *C)
{
  Object *object = CTX_data_active_object(C);
  if (object == nullptr || object->type != OB_GREASE_PENCIL) {
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

  ob->mode = OB_MODE_PAINT_GPENCIL;

  /* TODO: Setup cursor color. BKE_paint_init() could be used, but creates an additional brush. */
  /* TODO: Call ED_paint_cursor_start(...) */

  paint_init_pivot(ob, scene);

  /* Necessary to change the object mode on the evaluated object. */
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
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

  const bool is_mode_set = ob->mode == OB_MODE_PAINT_GPENCIL;

  if (is_mode_set) {
    if (!ED_object_mode_compat_set(C, ob, OB_MODE_PAINT_GPENCIL, op->reports)) {
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
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
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
  ot->poll = grease_pencil_poll;

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
  WM_operatortype_append(GREASE_PENCIL_OT_draw_mode_toggle);
}

/** \} */
