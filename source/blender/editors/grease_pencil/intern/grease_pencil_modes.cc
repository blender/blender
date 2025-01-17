/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "DNA_brush_types.h"

#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_gpencil_legacy.h"
#include "BKE_paint.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "ED_grease_pencil.hh"
#include "ED_image.hh"
#include "ED_object.hh"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_toolsystem.hh"

namespace blender::ed::greasepencil {

/* -------------------------------------------------------------------- */
/** \name Toggle Stroke Paint Mode Operator
 * \{ */

static bool brush_cursor_poll(bContext *C)
{
  if (WM_toolsystem_active_tool_is_brush(C) && !WM_toolsystem_active_tool_has_custom_cursor(C)) {
    return true;
  }
  return false;
}

static bool paintmode_toggle_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if ((ob) && ob->type == OB_GREASE_PENCIL) {
    return ob->data != nullptr;
  }
  return false;
}

static int paintmode_toggle_exec(bContext *C, wmOperator *op)
{
  const bool back = RNA_boolean_get(op->ptr, "back");

  wmMsgBus *mbus = CTX_wm_message_bus(C);
  Main *bmain = CTX_data_main(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  short mode;
  Object *ob = CTX_data_active_object(C);
  BLI_assert(ob != nullptr);

  const bool is_mode_set = (ob->mode & OB_MODE_PAINT_GREASE_PENCIL) != 0;
  if (!is_mode_set) {
    Scene *scene = CTX_data_scene(C);
    BKE_paint_init(bmain, scene, PaintMode::GPencil, PAINT_CURSOR_PAINT_GREASE_PENCIL);
    Paint *paint = BKE_paint_get_active_from_paintmode(scene, PaintMode::GPencil);
    ED_paint_cursor_start(paint, brush_cursor_poll);
    mode = OB_MODE_PAINT_GREASE_PENCIL;
  }
  else {
    mode = OB_MODE_OBJECT;
  }

  if ((ob->restore_mode) && ((ob->mode & OB_MODE_PAINT_GREASE_PENCIL) == 0) && (back == 1)) {
    mode = ob->restore_mode;
  }
  ob->restore_mode = ob->mode;
  ob->mode = mode;

  if (mode == OB_MODE_PAINT_GREASE_PENCIL) {
    /* Be sure we have brushes and Paint settings.
     * Need Draw and Vertex (used for Tint). */
    BKE_paint_ensure(ts, (Paint **)&ts->gp_paint);
    BKE_paint_brushes_ensure(bmain, &ts->gp_paint->paint);
    BKE_paint_ensure(ts, (Paint **)&ts->gp_vertexpaint);
    BKE_paint_brushes_ensure(bmain, &ts->gp_vertexpaint->paint);

    /* Ensure Palette by default. */
    BKE_gpencil_palette_ensure(bmain, CTX_data_scene(C));

    Paint *paint = &ts->gp_paint->paint;
    Brush *brush = BKE_paint_brush(paint);
    if (brush && !brush->gpencil_settings) {
      BKE_brush_init_gpencil_settings(brush);
    }
    BKE_paint_brushes_validate(bmain, &ts->gp_paint->paint);
  }

  GreasePencil *grease_pencil = static_cast<GreasePencil *>(ob->data);
  DEG_id_tag_update(&grease_pencil->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | ND_GPENCIL_EDITMODE, nullptr);
  WM_event_add_notifier(C, NC_SCENE | ND_MODE, nullptr);

  WM_msg_publish_rna_prop(mbus, &ob->id, ob, Object, mode);

  if (G.background == false) {
    WM_toolsystem_update_from_context_view3d(C);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_paintmode_toggle(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Strokes Paint Mode Toggle";
  ot->idname = "GREASE_PENCIL_OT_paintmode_toggle";
  ot->description = "Enter/Exit paint mode for Grease Pencil strokes";

  ot->exec = paintmode_toggle_exec;
  ot->poll = paintmode_toggle_poll;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  prop = RNA_def_boolean(
      ot->srna, "back", false, "Return to Previous Mode", "Return to previous mode");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Stroke Sculpt Mode Operator
 * \{ */

static bool sculptmode_toggle_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return false;
  }
  if (ob->type == OB_GREASE_PENCIL) {
    return ob->data != nullptr;
  }
  return false;
}

static bool sculpt_poll_view3d(bContext *C)
{
  const Object *ob = CTX_data_active_object(C);
  if (ob == nullptr || (ob->mode & OB_MODE_SCULPT_GREASE_PENCIL) == 0) {
    return false;
  }
  if (CTX_wm_region_view3d(C) == nullptr) {
    return false;
  }
  return true;
}

static int sculptmode_toggle_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  const bool back = RNA_boolean_get(op->ptr, "back");

  wmMsgBus *mbus = CTX_wm_message_bus(C);
  short mode;
  Object *ob = CTX_data_active_object(C);
  BLI_assert(ob != nullptr);
  const bool is_mode_set = (ob->mode & OB_MODE_SCULPT_GREASE_PENCIL) != 0;
  if (is_mode_set) {
    mode = OB_MODE_OBJECT;
  }
  else {
    Scene *scene = CTX_data_scene(C);
    BKE_paint_init(bmain, scene, PaintMode::SculptGPencil, PAINT_CURSOR_SCULPT_GREASE_PENCIL);
    Paint *paint = BKE_paint_get_active_from_paintmode(scene, PaintMode::SculptGPencil);
    ED_paint_cursor_start(paint, sculpt_poll_view3d);
    mode = OB_MODE_SCULPT_GREASE_PENCIL;
  }

  if ((ob->restore_mode) && ((ob->mode & OB_MODE_SCULPT_GREASE_PENCIL) == 0) && (back == 1)) {
    mode = ob->restore_mode;
  }
  ob->restore_mode = ob->mode;
  ob->mode = mode;

  if (mode == OB_MODE_SCULPT_GREASE_PENCIL) {
    BKE_paint_ensure(ts, (Paint **)&ts->gp_sculptpaint);
    BKE_paint_brushes_ensure(bmain, &ts->gp_sculptpaint->paint);
    BKE_paint_brushes_validate(bmain, &ts->gp_sculptpaint->paint);
  }

  GreasePencil *grease_pencil = static_cast<GreasePencil *>(ob->data);
  DEG_id_tag_update(&grease_pencil->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | ND_GPENCIL_EDITMODE, nullptr);
  WM_event_add_notifier(C, NC_SCENE | ND_MODE, nullptr);

  WM_msg_publish_rna_prop(mbus, &ob->id, ob, Object, mode);

  if (G.background == false) {
    WM_toolsystem_update_from_context_view3d(C);
  }

  return OPERATOR_FINISHED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Weight Paint Mode Operator
 * \{ */

static void GREASE_PENCIL_OT_sculptmode_toggle(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Strokes Sculpt Mode Toggle";
  ot->idname = "GREASE_PENCIL_OT_sculptmode_toggle";
  ot->description = "Enter/Exit sculpt mode for Grease Pencil strokes";

  ot->exec = sculptmode_toggle_exec;
  ot->poll = sculptmode_toggle_poll;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  prop = RNA_def_boolean(
      ot->srna, "back", false, "Return to Previous Mode", "Return to previous mode");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

static bool grease_pencil_poll_weight_cursor(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  return ob && (ob->mode & OB_MODE_WEIGHT_GREASE_PENCIL) && (ob->type == OB_GREASE_PENCIL) &&
         CTX_wm_region_view3d(C) && WM_toolsystem_active_tool_is_brush(C);
}

static bool weightmode_toggle_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if ((ob) && ob->type == OB_GREASE_PENCIL) {
    return ob->data != nullptr;
  }
  return false;
}

static int weightmode_toggle_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  const bool back = RNA_boolean_get(op->ptr, "back");

  wmMsgBus *mbus = CTX_wm_message_bus(C);
  short mode;
  Object *ob = CTX_data_active_object(C);
  BLI_assert(ob != nullptr);
  const bool is_mode_set = (ob->mode & OB_MODE_WEIGHT_GREASE_PENCIL) != 0;
  if (!is_mode_set) {
    mode = OB_MODE_WEIGHT_GREASE_PENCIL;
  }
  else {
    mode = OB_MODE_OBJECT;
  }

  if ((ob->restore_mode) && ((ob->mode & OB_MODE_WEIGHT_GREASE_PENCIL) == 0) && (back == 1)) {
    mode = ob->restore_mode;
  }
  ob->restore_mode = ob->mode;
  ob->mode = mode;

  /* Prepare armature posemode. */
  blender::ed::object::posemode_set_for_weight_paint(C, bmain, ob, is_mode_set);

  if (mode == OB_MODE_WEIGHT_GREASE_PENCIL) {
    /* Be sure we have brushes. */
    BKE_paint_ensure(ts, (Paint **)&ts->gp_weightpaint);
    Paint *weight_paint = BKE_paint_get_active_from_paintmode(scene, PaintMode::WeightGPencil);

    ED_paint_cursor_start(weight_paint, grease_pencil_poll_weight_cursor);

    BKE_paint_init(bmain, scene, PaintMode::WeightGPencil, PAINT_CURSOR_PAINT_GREASE_PENCIL);
    BKE_paint_brushes_validate(bmain, weight_paint);
  }

  GreasePencil *grease_pencil = static_cast<GreasePencil *>(ob->data);
  DEG_id_tag_update(&grease_pencil->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | ND_GPENCIL_EDITMODE, nullptr);
  WM_event_add_notifier(C, NC_SCENE | ND_MODE, nullptr);

  WM_msg_publish_rna_prop(mbus, &ob->id, ob, Object, mode);

  if (G.background == false) {
    WM_toolsystem_update_from_context_view3d(C);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_weightmode_toggle(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Strokes Weight Mode Toggle";
  ot->idname = "GREASE_PENCIL_OT_weightmode_toggle";
  ot->description = "Enter/Exit weight paint mode for Grease Pencil strokes";

  ot->exec = weightmode_toggle_exec;
  ot->poll = weightmode_toggle_poll;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  prop = RNA_def_boolean(
      ot->srna, "back", false, "Return to Previous Mode", "Return to previous mode");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Vertex Paint Mode Operator
 * \{ */

static bool grease_pencil_poll_vertex_cursor(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  return ob && (ob->mode & OB_MODE_VERTEX_GREASE_PENCIL) && (ob->type == OB_GREASE_PENCIL) &&
         CTX_wm_region_view3d(C) && WM_toolsystem_active_tool_is_brush(C);
}

static bool vertexmode_toggle_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if ((ob) && ob->type == OB_GREASE_PENCIL) {
    return ob->data != nullptr;
  }
  return false;
}

static int vertexmode_toggle_exec(bContext *C, wmOperator *op)
{
  const bool back = RNA_boolean_get(op->ptr, "back");

  wmMsgBus *mbus = CTX_wm_message_bus(C);
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  short mode;
  Object *ob = CTX_data_active_object(C);
  BLI_assert(ob != nullptr);
  const bool is_mode_set = (ob->mode & OB_MODE_VERTEX_GREASE_PENCIL) != 0;
  if (!is_mode_set) {
    mode = OB_MODE_VERTEX_GREASE_PENCIL;
  }
  else {
    mode = OB_MODE_OBJECT;
  }

  if ((ob->restore_mode) && ((ob->mode & OB_MODE_VERTEX_GREASE_PENCIL) == 0) && (back == 1)) {
    mode = ob->restore_mode;
  }
  ob->restore_mode = ob->mode;
  ob->mode = mode;

  if (mode == OB_MODE_VERTEX_GREASE_PENCIL) {
    /* Be sure we have brushes.
     * Need Draw as well (used for Palettes). */
    BKE_paint_ensure(ts, (Paint **)&ts->gp_paint);
    BKE_paint_ensure(ts, (Paint **)&ts->gp_vertexpaint);
    Paint *gp_paint = BKE_paint_get_active_from_paintmode(scene, PaintMode::GPencil);
    Paint *vertex_paint = BKE_paint_get_active_from_paintmode(scene, PaintMode::VertexGPencil);

    BKE_paint_brushes_ensure(bmain, gp_paint);
    BKE_paint_brushes_ensure(bmain, vertex_paint);
    BKE_paint_brushes_validate(bmain, vertex_paint);

    ED_paint_cursor_start(vertex_paint, grease_pencil_poll_vertex_cursor);

    /* Ensure Palette by default. */
    BKE_gpencil_palette_ensure(bmain, scene);
  }

  GreasePencil *grease_pencil = static_cast<GreasePencil *>(ob->data);
  DEG_id_tag_update(&grease_pencil->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | ND_GPENCIL_EDITMODE, nullptr);
  WM_event_add_notifier(C, NC_SCENE | ND_MODE, nullptr);

  WM_msg_publish_rna_prop(mbus, &ob->id, ob, Object, mode);

  if (G.background == false) {
    WM_toolsystem_update_from_context_view3d(C);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_vertexmode_toggle(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Strokes Vertex Mode Toggle";
  ot->idname = "GREASE_PENCIL_OT_vertexmode_toggle";
  ot->description = "Enter/Exit vertex paint mode for Grease Pencil strokes";

  ot->exec = vertexmode_toggle_exec;
  ot->poll = vertexmode_toggle_poll;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  prop = RNA_def_boolean(
      ot->srna, "back", false, "Return to Previous Mode", "Return to previous mode");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

}  // namespace blender::ed::greasepencil

void ED_operatortypes_grease_pencil_modes()
{
  using namespace blender::ed::greasepencil;
  WM_operatortype_append(GREASE_PENCIL_OT_paintmode_toggle);
  WM_operatortype_append(GREASE_PENCIL_OT_sculptmode_toggle);
  WM_operatortype_append(GREASE_PENCIL_OT_weightmode_toggle);
  WM_operatortype_append(GREASE_PENCIL_OT_vertexmode_toggle);
}
