/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_paint.h"

#include "WM_api.h"
#include "WM_toolsystem.h"

#include "ED_curves_sculpt.h"
#include "ED_object.h"

#include "curves_sculpt_intern.h"
#include "paint_intern.h"

bool CURVES_SCULPT_mode_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  return ob && ob->mode & OB_MODE_SCULPT_CURVES;
}

bool CURVES_SCULPT_mode_poll_view3d(bContext *C)
{
  if (!CURVES_SCULPT_mode_poll(C)) {
    return false;
  }
  if (CTX_wm_region_view3d(C) == nullptr) {
    return false;
  }
  return true;
}

namespace blender::ed::sculpt_paint {

/* --------------------------------------------------------------------
 * SCULPT_CURVES_OT_brush_stroke.
 */

static bool stroke_get_location(bContext *C, float out[3], const float mouse[2])
{
  out[0] = mouse[0];
  out[1] = mouse[1];
  out[2] = 0;
  UNUSED_VARS(C);
  return true;
}

static bool stroke_test_start(bContext *C, struct wmOperator *op, const float mouse[2])
{
  UNUSED_VARS(C, op, mouse);
  return true;
}

static void stroke_update_step(bContext *C,
                               wmOperator *op,
                               PaintStroke *stroke,
                               PointerRNA *itemptr)
{
  UNUSED_VARS(C, op, stroke, itemptr);
}

static void stroke_done(const bContext *C, PaintStroke *stroke)
{
  UNUSED_VARS(C, stroke);
}

static int sculpt_curves_stroke_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  PaintStroke *stroke = paint_stroke_new(C,
                                         op,
                                         stroke_get_location,
                                         stroke_test_start,
                                         stroke_update_step,
                                         nullptr,
                                         stroke_done,
                                         event->type);
  op->customdata = stroke;

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_curves_stroke_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  return paint_stroke_modal(C, op, event, static_cast<PaintStroke *>(op->customdata));
}

static void sculpt_curves_stroke_cancel(bContext *C, wmOperator *op)
{
  paint_stroke_cancel(C, op, static_cast<PaintStroke *>(op->customdata));
}

static void SCULPT_CURVES_OT_brush_stroke(struct wmOperatorType *ot)
{
  ot->name = "Stroke Curves Sculpt";
  ot->idname = "SCULPT_CURVES_OT_brush_stroke";
  ot->description = "Sculpt curves using a brush";

  ot->invoke = sculpt_curves_stroke_invoke;
  ot->modal = sculpt_curves_stroke_modal;
  ot->cancel = sculpt_curves_stroke_cancel;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  paint_stroke_operator_properties(ot);
}

/* --------------------------------------------------------------------
 * CURVES_OT_sculptmode_toggle.
 */

static bool curves_sculptmode_toggle_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return false;
  }
  if (ob->type != OB_CURVES) {
    return false;
  }
  return true;
}

static void curves_sculptmode_enter(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  BKE_paint_ensure(scene->toolsettings, (Paint **)&scene->toolsettings->curves_sculpt);
  CurvesSculpt *curves_sculpt = scene->toolsettings->curves_sculpt;

  ob->mode = OB_MODE_SCULPT_CURVES;

  paint_cursor_start(&curves_sculpt->paint, CURVES_SCULPT_mode_poll_view3d);
}

static void curves_sculptmode_exit(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  ob->mode = OB_MODE_OBJECT;
}

static int curves_sculptmode_toggle_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  const bool is_mode_set = ob->mode == OB_MODE_SCULPT_CURVES;

  if (is_mode_set) {
    if (!ED_object_mode_compat_set(C, ob, OB_MODE_SCULPT_CURVES, op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  if (is_mode_set) {
    curves_sculptmode_exit(C);
  }
  else {
    curves_sculptmode_enter(C);
  }

  WM_toolsystem_update_from_context_view3d(C);
  WM_event_add_notifier(C, NC_SCENE | ND_MODE, nullptr);
  return OPERATOR_CANCELLED;
}

static void CURVES_OT_sculptmode_toggle(wmOperatorType *ot)
{
  ot->name = "Curve Sculpt Mode Toggle";
  ot->idname = "CURVES_OT_sculptmode_toggle";
  ot->description = "Enter/Exit sculpt mode for curves";

  ot->exec = curves_sculptmode_toggle_exec;
  ot->poll = curves_sculptmode_toggle_poll;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;
}

}  // namespace blender::ed::sculpt_paint

/* --------------------------------------------------------------------
 * Registration.
 */

void ED_operatortypes_sculpt_curves()
{
  using namespace blender::ed::sculpt_paint;
  WM_operatortype_append(SCULPT_CURVES_OT_brush_stroke);
  WM_operatortype_append(CURVES_OT_sculptmode_toggle);
}
