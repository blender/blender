/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_paint.h"

#include "WM_api.h"

#include "ED_curves_sculpt.h"

#include "curves_sculpt_intern.h"
#include "paint_intern.h"

bool CURVES_SCULPT_mode_poll(struct bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  return ob && ob->mode & OB_MODE_SCULPT_CURVES;
}

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

static void stroke_update_step(bContext *C, PaintStroke *stroke, PointerRNA *itemptr)
{
  UNUSED_VARS(C, stroke, itemptr);
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

static void sculpt_curves_stroke_cancel(bContext *C, wmOperator *op)
{
  paint_stroke_cancel(C, op);
}

static void SCULPT_CURVES_OT_brush_stroke(struct wmOperatorType *ot)
{
  ot->name = "Stroke Curves Sculpt";
  ot->idname = "SCULPT_CURVES_OT_brush_stroke";
  ot->description = "Sculpt curves using a brush";

  ot->invoke = sculpt_curves_stroke_invoke;
  ot->modal = paint_stroke_modal;
  ot->cancel = sculpt_curves_stroke_cancel;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  paint_stroke_operator_properties(ot);
}

void ED_operatortypes_sculpt_curves()
{
  WM_operatortype_append(SCULPT_CURVES_OT_brush_stroke);
}
