/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_utildefines.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_curves.hh"
#include "BKE_paint.h"

#include "WM_api.h"
#include "WM_toolsystem.h"

#include "ED_curves_sculpt.h"
#include "ED_image.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "DEG_depsgraph.h"

#include "DNA_brush_types.h"
#include "DNA_curves_types.h"
#include "DNA_screen_types.h"

#include "RNA_access.h"

#include "curves_sculpt_intern.h"
#include "curves_sculpt_intern.hh"
#include "paint_intern.h"

/* -------------------------------------------------------------------- */
/** \name Poll Functions
 * \{ */

bool CURVES_SCULPT_mode_poll(bContext *C)
{
  const Object *ob = CTX_data_active_object(C);
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

/** \} */

namespace blender::ed::sculpt_paint {

using blender::bke::CurvesGeometry;

/* -------------------------------------------------------------------- */
/** \name * SCULPT_CURVES_OT_brush_stroke
 * \{ */

float brush_radius_factor(const Brush &brush, const StrokeExtension &stroke_extension)
{
  if (BKE_brush_use_size_pressure(&brush)) {
    return stroke_extension.pressure;
  }
  return 1.0f;
}

float brush_radius_get(const Scene &scene,
                       const Brush &brush,
                       const StrokeExtension &stroke_extension)
{
  return BKE_brush_size_get(&scene, &brush) * brush_radius_factor(brush, stroke_extension);
}

float brush_strength_factor(const Brush &brush, const StrokeExtension &stroke_extension)
{
  if (BKE_brush_use_alpha_pressure(&brush)) {
    return stroke_extension.pressure;
  }
  return 1.0f;
}

float brush_strength_get(const Scene &scene,
                         const Brush &brush,
                         const StrokeExtension &stroke_extension)
{
  return BKE_brush_alpha_get(&scene, &brush) * brush_strength_factor(brush, stroke_extension);
}

static std::unique_ptr<CurvesSculptStrokeOperation> start_brush_operation(bContext &C,
                                                                          wmOperator &op)
{
  const BrushStrokeMode mode = static_cast<BrushStrokeMode>(RNA_enum_get(op.ptr, "mode"));

  const Scene &scene = *CTX_data_scene(&C);
  const CurvesSculpt &curves_sculpt = *scene.toolsettings->curves_sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&curves_sculpt.paint);
  switch (brush.curves_sculpt_tool) {
    case CURVES_SCULPT_TOOL_COMB:
      return new_comb_operation();
    case CURVES_SCULPT_TOOL_DELETE:
      return new_delete_operation();
    case CURVES_SCULPT_TOOL_SNAKE_HOOK:
      return new_snake_hook_operation();
    case CURVES_SCULPT_TOOL_ADD:
      return new_add_operation(C, op.reports);
    case CURVES_SCULPT_TOOL_GROW_SHRINK:
      return new_grow_shrink_operation(mode, C);
    case CURVES_SCULPT_TOOL_SELECTION_PAINT:
      return new_selection_paint_operation(mode, C);
  }
  BLI_assert_unreachable();
  return {};
}

struct SculptCurvesBrushStrokeData {
  std::unique_ptr<CurvesSculptStrokeOperation> operation;
  PaintStroke *stroke;
};

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
                               PaintStroke *UNUSED(stroke),
                               PointerRNA *stroke_element)
{
  SculptCurvesBrushStrokeData *op_data = static_cast<SculptCurvesBrushStrokeData *>(
      op->customdata);

  StrokeExtension stroke_extension;
  RNA_float_get_array(stroke_element, "mouse", stroke_extension.mouse_position);
  stroke_extension.pressure = RNA_float_get(stroke_element, "pressure");

  if (!op_data->operation) {
    stroke_extension.is_first = true;
    op_data->operation = start_brush_operation(*C, *op);
  }
  else {
    stroke_extension.is_first = false;
  }

  if (op_data->operation) {
    op_data->operation->on_stroke_extended(*C, stroke_extension);
  }
}

static void stroke_done(const bContext *C, PaintStroke *stroke)
{
  UNUSED_VARS(C, stroke);
}

static int sculpt_curves_stroke_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const Paint *paint = BKE_paint_get_active_from_context(C);
  const Brush *brush = BKE_paint_brush_for_read(paint);
  if (brush == nullptr) {
    return OPERATOR_CANCELLED;
  }

  SculptCurvesBrushStrokeData *op_data = MEM_new<SculptCurvesBrushStrokeData>(__func__);
  op_data->stroke = paint_stroke_new(C,
                                     op,
                                     stroke_get_location,
                                     stroke_test_start,
                                     stroke_update_step,
                                     nullptr,
                                     stroke_done,
                                     event->type);
  op->customdata = op_data;

  int return_value = op->type->modal(C, op, event);
  if (return_value == OPERATOR_FINISHED) {
    paint_stroke_free(C, op, op_data->stroke);
    MEM_delete(op_data);
    return OPERATOR_FINISHED;
  }

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_curves_stroke_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  SculptCurvesBrushStrokeData *op_data = static_cast<SculptCurvesBrushStrokeData *>(
      op->customdata);
  int return_value = paint_stroke_modal(C, op, event, &op_data->stroke);
  if (ELEM(return_value, OPERATOR_FINISHED, OPERATOR_CANCELLED)) {
    MEM_delete(op_data);
  }
  return return_value;
}

static void sculpt_curves_stroke_cancel(bContext *C, wmOperator *op)
{
  SculptCurvesBrushStrokeData *op_data = static_cast<SculptCurvesBrushStrokeData *>(
      op->customdata);
  paint_stroke_cancel(C, op, op_data->stroke);
  MEM_delete(op_data);
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name * CURVES_OT_sculptmode_toggle
 * \{ */

static bool curves_sculptmode_toggle_poll(bContext *C)
{
  const Object *ob = CTX_data_active_object(C);
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

  ED_paint_cursor_start(&curves_sculpt->paint, CURVES_SCULPT_mode_poll_view3d);

  /* Update for mode change. */
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
  WM_event_add_notifier(C, NC_SCENE | ND_MODE, nullptr);
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
  return OPERATOR_FINISHED;
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

/** \} */

}  // namespace blender::ed::sculpt_paint

/* -------------------------------------------------------------------- */
/** \name * Registration
 * \{ */

void ED_operatortypes_sculpt_curves()
{
  using namespace blender::ed::sculpt_paint;
  WM_operatortype_append(SCULPT_CURVES_OT_brush_stroke);
  WM_operatortype_append(CURVES_OT_sculptmode_toggle);
}

/** \} */
