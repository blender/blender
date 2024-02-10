/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgpencil
 * Operators for editing Grease Pencil strokes.
 */

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_lasso_2d.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_deform.hh"
#include "BKE_global.hh"
#include "BKE_gpencil_curve_legacy.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_material.h"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_workspace.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "UI_view2d.hh"

#include "ED_armature.hh"
#include "ED_gpencil_legacy.hh"
#include "ED_object.hh"
#include "ED_outliner.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"
#include "ED_space_api.hh"
#include "ED_transform_snap_object_context.hh"
#include "ED_view3d.hh"

#include "ANIM_keyframing.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "gpencil_intern.h"

/* -------------------------------------------------------------------- */
/** \name Stroke Edit Mode Management
 * \{ */

/* poll callback for all stroke editing operators */
static bool gpencil_stroke_edit_poll(bContext *C)
{
  /* edit only supported with grease pencil objects */
  Object *ob = CTX_data_active_object(C);
  if ((ob == nullptr) || (ob->type != OB_GPENCIL_LEGACY)) {
    return false;
  }

  /* NOTE: this is a bit slower, but is the most accurate... */
  return CTX_DATA_COUNT(C, editable_gpencil_strokes) != 0;
}

/* poll callback to verify edit mode in 3D view only */
static bool gpencil_strokes_edit3d_poll(bContext *C)
{
  /* edit only supported with grease pencil objects */
  Object *ob = CTX_data_active_object(C);
  if ((ob == nullptr) || (ob->type != OB_GPENCIL_LEGACY)) {
    return false;
  }

  /* 2 Requirements:
   * - 1) Editable GP data
   * - 2) 3D View only
   */
  return (gpencil_stroke_edit_poll(C) && ED_operator_view3d_active(C));
}

static bool gpencil_editmode_toggle_poll(bContext *C)
{
  /* edit only supported with grease pencil objects */
  Object *ob = CTX_data_active_object(C);
  if ((ob == nullptr) || (ob->type != OB_GPENCIL_LEGACY)) {
    return false;
  }

  /* if using gpencil object, use this gpd */
  if (ob->type == OB_GPENCIL_LEGACY) {
    return ob->data != nullptr;
  }

  return ED_gpencil_data_get_active(C) != nullptr;
}

static bool gpencil_stroke_not_in_curve_edit_mode(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if ((ob == nullptr) || (ob->type != OB_GPENCIL_LEGACY)) {
    return false;
  }
  bGPdata *gpd = (bGPdata *)ob->data;
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);

  return (gpl != nullptr && !GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Edit Mode Operator
 * \{ */

static int gpencil_editmode_toggle_exec(bContext *C, wmOperator *op)
{
  const int back = RNA_boolean_get(op->ptr, "back");

  wmMsgBus *mbus = CTX_wm_message_bus(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bool is_object = false;
  short mode;
  /* if using a gpencil object, use this datablock */
  Object *ob = CTX_data_active_object(C);
  if ((ob) && (ob->type == OB_GPENCIL_LEGACY)) {
    gpd = static_cast<bGPdata *>(ob->data);
    is_object = true;
  }

  if (gpd == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "No active GP data");
    return OPERATOR_CANCELLED;
  }

  /* Just toggle editmode flag... */
  gpd->flag ^= GP_DATA_STROKE_EDITMODE;
  /* recalculate parent matrix */
  if (gpd->flag & GP_DATA_STROKE_EDITMODE) {
    Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    ED_gpencil_reset_layers_parent(depsgraph, ob, gpd);
  }
  /* set mode */
  if (gpd->flag & GP_DATA_STROKE_EDITMODE) {
    mode = OB_MODE_EDIT_GPENCIL_LEGACY;
  }
  else {
    mode = OB_MODE_OBJECT;
  }

  if (is_object) {
    /* try to back previous mode */
    if ((ob->restore_mode) && ((gpd->flag & GP_DATA_STROKE_EDITMODE) == 0) && (back == 1)) {
      mode = ob->restore_mode;
    }
    ob->restore_mode = ob->mode;
    ob->mode = mode;
  }

  /* Recalculate edit-curves for strokes where the geometry/vertex colors have changed. */
  if (GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd)) {
    GP_EDITABLE_CURVES_BEGIN(gps_iter, C, gpl, gps, gpc)
    {
      if (gpc->flag & GP_CURVE_NEEDS_STROKE_UPDATE) {
        BKE_gpencil_stroke_editcurve_update(gpd, gpl, gps);
        /* Update the selection from the stroke to the curve. */
        BKE_gpencil_editcurve_stroke_sync_selection(gpd, gps, gps->editcurve);

        gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
        BKE_gpencil_stroke_geometry_update(gpd, gps);
      }
    }
    GP_EDITABLE_CURVES_END(gps_iter);
  }

  /* setup other modes */
  ED_gpencil_setup_modes(C, gpd, mode);
  /* set cache as dirty */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA, nullptr);
  WM_event_add_notifier(C, NC_GPENCIL | ND_GPENCIL_EDITMODE, nullptr);
  WM_event_add_notifier(C, NC_SCENE | ND_MODE, nullptr);

  if (is_object) {
    WM_msg_publish_rna_prop(mbus, &ob->id, ob, Object, mode);
  }
  if (G.background == false) {
    WM_toolsystem_update_from_context_view3d(C);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_editmode_toggle(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Strokes Edit Mode Toggle";
  ot->idname = "GPENCIL_OT_editmode_toggle";
  ot->description = "Enter/Exit edit mode for Grease Pencil strokes";

  /* callbacks */
  ot->exec = gpencil_editmode_toggle_exec;
  ot->poll = gpencil_editmode_toggle_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  /* properties */
  prop = RNA_def_boolean(
      ot->srna, "back", false, "Return to Previous Mode", "Return to previous mode");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Select Mode Operator
 * \{ */

/* set select mode */
static bool gpencil_selectmode_toggle_poll(bContext *C)
{
  /* edit only supported with grease pencil objects */
  Object *ob = CTX_data_active_object(C);
  if ((ob == nullptr) || (ob->type != OB_GPENCIL_LEGACY) ||
      (ob->mode != OB_MODE_EDIT_GPENCIL_LEGACY))
  {
    return false;
  }

  return ED_operator_view3d_active(C);
}

static int gpencil_selectmode_toggle_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  Object *ob = CTX_data_active_object(C);
  const int mode = RNA_int_get(op->ptr, "mode");
  bool changed = false;

  if (ts->gpencil_selectmode_edit == mode) {
    return OPERATOR_FINISHED;
  }

  /* Just set mode */
  ts->gpencil_selectmode_edit = mode;

  /* If the mode is Stroke, extend selection. */
  if ((ob) && (ts->gpencil_selectmode_edit == GP_SELECTMODE_STROKE)) {
    bGPdata *gpd = (bGPdata *)ob->data;
    /* Extend selection to all points in all selected strokes. */
    CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
      if ((gps->flag & GP_STROKE_SELECT) && (gps->totpoints > 1)) {
        changed = true;
        bGPDspoint *pt;
        for (int i = 0; i < gps->totpoints; i++) {
          pt = &gps->points[i];
          pt->flag |= GP_SPOINT_SELECT;
        }
      }
    }
    CTX_DATA_END;
    if (changed) {
      DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    }
  }

  WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, nullptr);
  WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, nullptr);
  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_selectmode_toggle(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Select Mode Toggle";
  ot->idname = "GPENCIL_OT_selectmode_toggle";
  ot->description = "Set selection mode for Grease Pencil strokes";

  /* callbacks */
  ot->exec = gpencil_selectmode_toggle_exec;
  ot->poll = gpencil_selectmode_toggle_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  /* properties */
  prop = RNA_def_int(ot->srna, "mode", 0, 0, 2, "Select Mode", "Select mode", 0, 2);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Stroke Paint Mode Operator
 * \{ */

static bool gpencil_paintmode_toggle_poll(bContext *C)
{
  /* if using gpencil object, use this gpd */
  Object *ob = CTX_data_active_object(C);
  if ((ob) && (ob->type == OB_GPENCIL_LEGACY)) {
    return ob->data != nullptr;
  }
  return ED_gpencil_data_get_active(C) != nullptr;
}

static int gpencil_paintmode_toggle_exec(bContext *C, wmOperator *op)
{
  const bool back = RNA_boolean_get(op->ptr, "back");

  wmMsgBus *mbus = CTX_wm_message_bus(C);
  Main *bmain = CTX_data_main(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  bool is_object = false;
  short mode;
  /* if using a gpencil object, use this datablock */
  Object *ob = CTX_data_active_object(C);
  if ((ob) && (ob->type == OB_GPENCIL_LEGACY)) {
    gpd = static_cast<bGPdata *>(ob->data);
    is_object = true;
  }

  if (gpd == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* Just toggle paintmode flag... */
  gpd->flag ^= GP_DATA_STROKE_PAINTMODE;
  /* set mode */
  if (gpd->flag & GP_DATA_STROKE_PAINTMODE) {
    mode = OB_MODE_PAINT_GPENCIL_LEGACY;
  }
  else {
    mode = OB_MODE_OBJECT;
  }

  if (is_object) {
    /* try to back previous mode */
    if ((ob->restore_mode) && ((gpd->flag & GP_DATA_STROKE_PAINTMODE) == 0) && (back == 1)) {
      mode = ob->restore_mode;
    }
    ob->restore_mode = ob->mode;
    ob->mode = mode;
  }

  if (mode == OB_MODE_PAINT_GPENCIL_LEGACY) {
    /* Be sure we have brushes and Paint settings.
     * Need Draw and Vertex (used for Tint). */
    BKE_paint_ensure(ts, (Paint **)&ts->gp_paint);
    BKE_paint_ensure(ts, (Paint **)&ts->gp_vertexpaint);

    BKE_brush_gpencil_paint_presets(bmain, ts, false);

    /* Ensure Palette by default. */
    BKE_gpencil_palette_ensure(bmain, CTX_data_scene(C));

    Paint *paint = &ts->gp_paint->paint;
    /* if not exist, create a new one */
    if ((paint->brush == nullptr) || (paint->brush->gpencil_settings == nullptr)) {
      BKE_brush_gpencil_paint_presets(bmain, ts, true);
    }
    BKE_paint_toolslots_brush_validate(bmain, &ts->gp_paint->paint);
  }

  /* setup other modes */
  ED_gpencil_setup_modes(C, gpd, mode);
  /* set cache as dirty */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | ND_GPENCIL_EDITMODE, nullptr);
  WM_event_add_notifier(C, NC_SCENE | ND_MODE, nullptr);

  if (is_object) {
    WM_msg_publish_rna_prop(mbus, &ob->id, ob, Object, mode);
  }
  if (G.background == false) {
    WM_toolsystem_update_from_context_view3d(C);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_paintmode_toggle(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Strokes Paint Mode Toggle";
  ot->idname = "GPENCIL_OT_paintmode_toggle";
  ot->description = "Enter/Exit paint mode for Grease Pencil strokes";

  /* callbacks */
  ot->exec = gpencil_paintmode_toggle_exec;
  ot->poll = gpencil_paintmode_toggle_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  /* properties */
  prop = RNA_def_boolean(
      ot->srna, "back", false, "Return to Previous Mode", "Return to previous mode");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Stroke Sculpt Mode Operator
 * \{ */

static bool gpencil_sculptmode_toggle_poll(bContext *C)
{
  /* if using gpencil object, use this gpd */
  Object *ob = CTX_data_active_object(C);
  if ((ob) && (ob->type == OB_GPENCIL_LEGACY)) {
    return ob->data != nullptr;
  }
  return ED_gpencil_data_get_active(C) != nullptr;
}

static int gpencil_sculptmode_toggle_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  const bool back = RNA_boolean_get(op->ptr, "back");

  wmMsgBus *mbus = CTX_wm_message_bus(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bool is_object = false;
  short mode;
  /* if using a gpencil object, use this datablock */
  Object *ob = CTX_data_active_object(C);
  if ((ob) && (ob->type == OB_GPENCIL_LEGACY)) {
    gpd = static_cast<bGPdata *>(ob->data);
    is_object = true;
  }

  if (gpd == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* Just toggle sculptmode flag... */
  gpd->flag ^= GP_DATA_STROKE_SCULPTMODE;
  /* set mode */
  if (gpd->flag & GP_DATA_STROKE_SCULPTMODE) {
    mode = OB_MODE_SCULPT_GPENCIL_LEGACY;
  }
  else {
    mode = OB_MODE_OBJECT;
  }

  if (is_object) {
    /* try to back previous mode */
    if ((ob->restore_mode) && ((gpd->flag & GP_DATA_STROKE_SCULPTMODE) == 0) && (back == 1)) {
      mode = ob->restore_mode;
    }
    ob->restore_mode = ob->mode;
    ob->mode = mode;
  }

  if (mode == OB_MODE_SCULPT_GPENCIL_LEGACY) {
    /* Be sure we have brushes. */
    BKE_paint_ensure(ts, (Paint **)&ts->gp_sculptpaint);

    const bool reset_mode = (ts->gp_sculptpaint->paint.brush == nullptr);
    BKE_brush_gpencil_sculpt_presets(bmain, ts, reset_mode);

    BKE_paint_toolslots_brush_validate(bmain, &ts->gp_sculptpaint->paint);
  }

  /* setup other modes */
  ED_gpencil_setup_modes(C, gpd, mode);
  /* set cache as dirty */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | ND_GPENCIL_EDITMODE, nullptr);
  WM_event_add_notifier(C, NC_SCENE | ND_MODE, nullptr);

  if (is_object) {
    WM_msg_publish_rna_prop(mbus, &ob->id, ob, Object, mode);
  }
  if (G.background == false) {
    WM_toolsystem_update_from_context_view3d(C);
  }

  return OPERATOR_FINISHED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Weight Paint Mode Operator
 * \{ */

void GPENCIL_OT_sculptmode_toggle(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Strokes Sculpt Mode Toggle";
  ot->idname = "GPENCIL_OT_sculptmode_toggle";
  ot->description = "Enter/Exit sculpt mode for Grease Pencil strokes";

  /* callbacks */
  ot->exec = gpencil_sculptmode_toggle_exec;
  ot->poll = gpencil_sculptmode_toggle_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  /* properties */
  prop = RNA_def_boolean(
      ot->srna, "back", false, "Return to Previous Mode", "Return to previous mode");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/* Stroke Weight Paint Mode Management */

static bool gpencil_weightmode_toggle_poll(bContext *C)
{
  /* if using gpencil object, use this gpd */
  Object *ob = CTX_data_active_object(C);
  if ((ob) && (ob->type == OB_GPENCIL_LEGACY)) {
    return ob->data != nullptr;
  }
  return ED_gpencil_data_get_active(C) != nullptr;
}

static int gpencil_weightmode_toggle_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  const bool back = RNA_boolean_get(op->ptr, "back");

  wmMsgBus *mbus = CTX_wm_message_bus(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bool is_object = false;
  short mode;
  /* if using a gpencil object, use this datablock */
  Object *ob = CTX_data_active_object(C);
  if ((ob) && (ob->type == OB_GPENCIL_LEGACY)) {
    gpd = static_cast<bGPdata *>(ob->data);
    is_object = true;
  }
  const int mode_flag = OB_MODE_WEIGHT_GPENCIL_LEGACY;
  const bool is_mode_set = (ob->mode & mode_flag) != 0;

  if (gpd == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* Just toggle weightmode flag... */
  gpd->flag ^= GP_DATA_STROKE_WEIGHTMODE;
  /* set mode */
  if (gpd->flag & GP_DATA_STROKE_WEIGHTMODE) {
    mode = OB_MODE_WEIGHT_GPENCIL_LEGACY;
  }
  else {
    mode = OB_MODE_OBJECT;
  }

  if (is_object) {
    /* try to back previous mode */
    if ((ob->restore_mode) && ((gpd->flag & GP_DATA_STROKE_WEIGHTMODE) == 0) && (back == 1)) {
      mode = ob->restore_mode;
    }
    ob->restore_mode = ob->mode;
    ob->mode = mode;

    /* Prepare armature posemode. */
    ED_object_posemode_set_for_weight_paint(C, bmain, ob, is_mode_set);
  }

  if (mode == OB_MODE_WEIGHT_GPENCIL_LEGACY) {
    /* Be sure we have brushes. */
    BKE_paint_ensure(ts, (Paint **)&ts->gp_weightpaint);

    const bool reset_mode = (ts->gp_weightpaint->paint.brush == nullptr);
    BKE_brush_gpencil_weight_presets(bmain, ts, reset_mode);

    BKE_paint_toolslots_brush_validate(bmain, &ts->gp_weightpaint->paint);
  }

  /* setup other modes */
  ED_gpencil_setup_modes(C, gpd, mode);
  /* set cache as dirty */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | ND_GPENCIL_EDITMODE, nullptr);
  WM_event_add_notifier(C, NC_SCENE | ND_MODE, nullptr);

  if (is_object) {
    WM_msg_publish_rna_prop(mbus, &ob->id, ob, Object, mode);
  }
  if (G.background == false) {
    WM_toolsystem_update_from_context_view3d(C);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_weightmode_toggle(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Strokes Weight Mode Toggle";
  ot->idname = "GPENCIL_OT_weightmode_toggle";
  ot->description = "Enter/Exit weight paint mode for Grease Pencil strokes";

  /* callbacks */
  ot->exec = gpencil_weightmode_toggle_exec;
  ot->poll = gpencil_weightmode_toggle_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  /* properties */
  prop = RNA_def_boolean(
      ot->srna, "back", false, "Return to Previous Mode", "Return to previous mode");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Vertex Paint Mode Operator
 * \{ */

static bool gpencil_vertexmode_toggle_poll(bContext *C)
{
  /* if using gpencil object, use this gpd */
  Object *ob = CTX_data_active_object(C);
  if ((ob) && (ob->type == OB_GPENCIL_LEGACY)) {
    return ob->data != nullptr;
  }
  return ED_gpencil_data_get_active(C) != nullptr;
}
static int gpencil_vertexmode_toggle_exec(bContext *C, wmOperator *op)
{
  const bool back = RNA_boolean_get(op->ptr, "back");

  wmMsgBus *mbus = CTX_wm_message_bus(C);
  Main *bmain = CTX_data_main(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  bool is_object = false;
  short mode;
  /* if using a gpencil object, use this datablock */
  Object *ob = CTX_data_active_object(C);
  if ((ob) && (ob->type == OB_GPENCIL_LEGACY)) {
    gpd = static_cast<bGPdata *>(ob->data);
    is_object = true;
  }

  if (gpd == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* Just toggle paintmode flag... */
  gpd->flag ^= GP_DATA_STROKE_VERTEXMODE;
  /* set mode */
  if (gpd->flag & GP_DATA_STROKE_VERTEXMODE) {
    mode = OB_MODE_VERTEX_GPENCIL_LEGACY;
  }
  else {
    mode = OB_MODE_OBJECT;
  }

  if (is_object) {
    /* try to back previous mode */
    if ((ob->restore_mode) && ((gpd->flag & GP_DATA_STROKE_VERTEXMODE) == 0) && (back == 1)) {
      mode = ob->restore_mode;
    }
    ob->restore_mode = ob->mode;
    ob->mode = mode;
  }

  if (mode == OB_MODE_VERTEX_GPENCIL_LEGACY) {
    /* Be sure we have brushes.
     * Need Draw as well (used for Palettes). */
    BKE_paint_ensure(ts, (Paint **)&ts->gp_paint);
    BKE_paint_ensure(ts, (Paint **)&ts->gp_vertexpaint);

    const bool reset_mode = (ts->gp_vertexpaint->paint.brush == nullptr);
    BKE_brush_gpencil_vertex_presets(bmain, ts, reset_mode);

    BKE_paint_toolslots_brush_validate(bmain, &ts->gp_vertexpaint->paint);

    /* Ensure Palette by default. */
    BKE_gpencil_palette_ensure(bmain, CTX_data_scene(C));
  }

  /* setup other modes */
  ED_gpencil_setup_modes(C, gpd, mode);
  /* set cache as dirty */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | ND_GPENCIL_EDITMODE, nullptr);
  WM_event_add_notifier(C, NC_SCENE | ND_MODE, nullptr);

  if (is_object) {
    WM_msg_publish_rna_prop(mbus, &ob->id, ob, Object, mode);
  }
  if (G.background == false) {
    WM_toolsystem_update_from_context_view3d(C);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_vertexmode_toggle(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Strokes Vertex Mode Toggle";
  ot->idname = "GPENCIL_OT_vertexmode_toggle";
  ot->description = "Enter/Exit vertex paint mode for Grease Pencil strokes";

  /* callbacks */
  ot->exec = gpencil_vertexmode_toggle_exec;
  ot->poll = gpencil_vertexmode_toggle_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  /* properties */
  prop = RNA_def_boolean(
      ot->srna, "back", false, "Return to Previous Mode", "Return to previous mode");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Hide Selection Toggle Operator
 * \{ */

static int gpencil_hideselect_toggle_exec(bContext *C, wmOperator * /*op*/)
{
  View3D *v3d = CTX_wm_view3d(C);
  if (v3d == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* Just toggle alpha... */
  if (v3d->vertex_opacity > 0.0f) {
    v3d->vertex_opacity = 0.0f;
  }
  else {
    v3d->vertex_opacity = 1.0f;
  }

  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA, nullptr);
  WM_event_add_notifier(C, NC_GPENCIL | ND_GPENCIL_EDITMODE, nullptr);
  WM_event_add_notifier(C, NC_SCENE | ND_MODE, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_selection_opacity_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Hide Selected";
  ot->idname = "GPENCIL_OT_selection_opacity_toggle";
  ot->description = "Hide/Unhide selected points for Grease Pencil strokes setting alpha factor";

  /* callbacks */
  ot->exec = gpencil_hideselect_toggle_exec;
  ot->poll = gpencil_stroke_edit_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplicate Selected Strokes Operator
 * \{ */

/* Make copies of selected point segments in a selected stroke */
static void gpencil_duplicate_points(bGPdata *gpd,
                                     const bGPDstroke *gps,
                                     ListBase *new_strokes,
                                     const char *layername)
{
  bGPDspoint *pt;
  int i;

  int start_idx = -1;

  /* Step through the original stroke's points:
   * - We accumulate selected points (from start_idx to current index)
   *   and then convert that to a new stroke
   */
  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
    /* searching for start, are waiting for end? */
    if (start_idx == -1) {
      /* is this the first selected point for a new island? */
      if (pt->flag & GP_SPOINT_SELECT) {
        start_idx = i;
      }
    }
    if ((start_idx != -1) || (start_idx == gps->totpoints - 1)) {
      size_t len = 0;

      /* is this the end of current island yet?
       * 1) Point i-1 was the last one that was selected
       * 2) Point i is the last in the array
       */
      if ((pt->flag & GP_SPOINT_SELECT) == 0) {
        len = i - start_idx;
      }
      else if (i == gps->totpoints - 1) {
        len = i - start_idx + 1;
      }

      /* make copies of the relevant data */
      if (len) {
        bGPDstroke *gpsd;

        /* make a stupid copy first of the entire stroke (to get the flags too) */
        gpsd = BKE_gpencil_stroke_duplicate((bGPDstroke *)gps, false, true);

        /* saves original layer name */
        STRNCPY(gpsd->runtime.tmp_layerinfo, layername);

        /* now, make a new points array, and copy of the relevant parts */
        gpsd->points = static_cast<bGPDspoint *>(
            MEM_mallocN(sizeof(bGPDspoint) * len, "gps stroke points copy"));
        blender::dna::shallow_copy_array(gpsd->points, gps->points + start_idx, len);
        gpsd->totpoints = len;

        if (gps->dvert != nullptr) {
          gpsd->dvert = static_cast<MDeformVert *>(
              MEM_mallocN(sizeof(MDeformVert) * len, "gps stroke weights copy"));
          memcpy(gpsd->dvert, gps->dvert + start_idx, sizeof(MDeformVert) * len);

          /* Copy weights */
          int e = start_idx;
          for (int j = 0; j < gpsd->totpoints; j++) {
            MDeformVert *dvert_dst = &gps->dvert[e];
            MDeformVert *dvert_src = &gps->dvert[j];
            dvert_dst->dw = static_cast<MDeformWeight *>(MEM_dupallocN(dvert_src->dw));
            e++;
          }
        }

        BKE_gpencil_stroke_geometry_update(gpd, gpsd);

        /* add to temp buffer */
        gpsd->next = gpsd->prev = nullptr;

        BLI_addtail(new_strokes, gpsd);

        /* cleanup + reset for next */
        start_idx = -1;
      }
    }
  }
}

static int gpencil_duplicate_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));

  if (gpd == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
    return OPERATOR_CANCELLED;
  }

  bool changed = false;
  if (is_curve_edit) {
    BKE_report(op->reports, RPT_ERROR, "Not implemented!");
  }
  else {
    /* for each visible (and editable) layer's selected strokes,
     * copy the strokes into a temporary buffer, then append
     * once all done
     */
    CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
      ListBase new_strokes = {nullptr, nullptr};
      bGPDframe *gpf = gpl->actframe;

      if (gpf == nullptr) {
        continue;
      }

      /* make copies of selected strokes, and deselect these once we're done */
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        /* skip strokes that are invalid for current view */
        if (ED_gpencil_stroke_can_use(C, gps) == false) {
          continue;
        }

        if (gps->flag & GP_STROKE_SELECT) {
          if (gps->totpoints == 1) {
            /* Special Case: If there's just a single point in this stroke... */
            bGPDstroke *gpsd;

            /* make direct copies of the stroke and its points */
            gpsd = BKE_gpencil_stroke_duplicate(gps, true, true);

            STRNCPY(gpsd->runtime.tmp_layerinfo, gpl->info);

            /* Initialize triangle information. */
            BKE_gpencil_stroke_geometry_update(gpd, gpsd);

            /* add to temp buffer */
            gpsd->next = gpsd->prev = nullptr;
            BLI_addtail(&new_strokes, gpsd);
          }
          else {
            /* delegate to a helper, as there's too much to fit in here (for copying subsets)... */
            gpencil_duplicate_points(gpd, gps, &new_strokes, gpl->info);
          }

          /* deselect original stroke, or else the originals get moved too
           * (when using the copy + move macro)
           */
          bGPDspoint *pt;
          int i;
          for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
            pt->flag &= ~GP_SPOINT_SELECT;
          }
          gps->flag &= ~GP_STROKE_SELECT;
          BKE_gpencil_stroke_select_index_reset(gps);

          changed = true;
        }
      }

      /* add all new strokes in temp buffer to the frame (preventing double-copies) */
      BLI_movelisttolist(&gpf->strokes, &new_strokes);
      BLI_assert(new_strokes.first == nullptr);
    }
    CTX_DATA_END;
  }

  if (changed) {
    /* updates */
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

void GPENCIL_OT_duplicate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Duplicate Strokes";
  ot->idname = "GPENCIL_OT_duplicate";
  ot->description = "Duplicate the selected Grease Pencil strokes";

  /* callbacks */
  ot->exec = gpencil_duplicate_exec;
  ot->poll = gpencil_stroke_edit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extrude Selected Strokes Operator
 * \{ */

/* helper to copy a point to temp area */
static void gpencil_copy_move_point(bGPDstroke *gps,
                                    bGPDspoint *temp_points,
                                    MDeformVert *temp_dverts,
                                    int from_idx,
                                    int to_idx,
                                    const bool copy)
{
  bGPDspoint *pt = &temp_points[from_idx];
  bGPDspoint *pt_final = &gps->points[to_idx];

  copy_v3_v3(&pt_final->x, &pt->x);
  pt_final->pressure = pt->pressure;
  pt_final->strength = pt->strength;
  pt_final->time = pt->time;
  pt_final->flag = pt->flag;
  pt_final->uv_fac = pt->uv_fac;
  pt_final->uv_rot = pt->uv_rot;
  copy_v4_v4(pt_final->vert_color, pt->vert_color);

  if (gps->dvert != nullptr) {
    MDeformVert *dvert = &temp_dverts[from_idx];
    MDeformVert *dvert_final = &gps->dvert[to_idx];

    dvert_final->totweight = dvert->totweight;
    /* if copy, duplicate memory, otherwise move only the pointer */
    if (copy) {
      dvert_final->dw = static_cast<MDeformWeight *>(MEM_dupallocN(dvert->dw));
    }
    else {
      dvert_final->dw = dvert->dw;
    }
  }
}

static void gpencil_add_move_points(bGPdata *gpd, bGPDframe *gpf, bGPDstroke *gps)
{
  bGPDspoint *temp_points = nullptr;
  MDeformVert *temp_dverts = nullptr;
  bGPDspoint *pt = nullptr;
  const bGPDspoint *pt_start = &gps->points[0];
  const bGPDspoint *pt_last = &gps->points[gps->totpoints - 1];
  const bool do_first = (pt_start->flag & GP_SPOINT_SELECT);
  const bool do_last = ((pt_last->flag & GP_SPOINT_SELECT) && (pt_start != pt_last));
  const bool do_stroke = (do_first || do_last);

  /* review points in the middle of stroke to create new strokes */
  for (int i = 0; i < gps->totpoints; i++) {
    /* skip first and last point */
    if (ELEM(i, 0, gps->totpoints - 1)) {
      continue;
    }

    pt = &gps->points[i];
    if (pt->flag == GP_SPOINT_SELECT) {
      /* duplicate original stroke data */
      bGPDstroke *gps_new = BKE_gpencil_stroke_duplicate(gps, false, true);
      gps_new->prev = gps_new->next = nullptr;

      /* add new points array */
      gps_new->totpoints = 1;
      gps_new->points = static_cast<bGPDspoint *>(MEM_callocN(sizeof(bGPDspoint), __func__));
      gps_new->dvert = nullptr;

      if (gps->dvert != nullptr) {
        gps_new->dvert = static_cast<MDeformVert *>(MEM_callocN(sizeof(MDeformVert), __func__));
      }

      BLI_insertlinkafter(&gpf->strokes, gps, gps_new);

      /* copy selected point data to new stroke */
      gpencil_copy_move_point(gps_new, gps->points, gps->dvert, i, 0, true);

      /* Calc geometry data. */
      BKE_gpencil_stroke_geometry_update(gpd, gps);
      BKE_gpencil_stroke_geometry_update(gpd, gps_new);

      /* Deselect original point. */
      pt->flag &= ~GP_SPOINT_SELECT;
    }
  }

  /* review first and last point to reuse same stroke */
  int i2 = 0;
  int totnewpoints, oldtotpoints;
  /* if first or last, reuse stroke and resize */
  if ((do_first) || (do_last)) {
    totnewpoints = gps->totpoints;
    if (do_first) {
      totnewpoints++;
    }
    if (do_last) {
      totnewpoints++;
    }

    /* duplicate points in a temp area */
    temp_points = static_cast<bGPDspoint *>(MEM_dupallocN(gps->points));
    oldtotpoints = gps->totpoints;
    if (gps->dvert != nullptr) {
      temp_dverts = static_cast<MDeformVert *>(MEM_dupallocN(gps->dvert));
    }

    /* if first point, need move all one position */
    if (do_first) {
      i2 = 1;
    }

    /* resize the points arrays */
    gps->totpoints = totnewpoints;
    gps->points = static_cast<bGPDspoint *>(
        MEM_recallocN(gps->points, sizeof(*gps->points) * gps->totpoints));
    if (gps->dvert != nullptr) {
      gps->dvert = static_cast<MDeformVert *>(
          MEM_recallocN(gps->dvert, sizeof(*gps->dvert) * gps->totpoints));
    }

    /* move points to new position */
    for (int i = 0; i < oldtotpoints; i++) {
      gpencil_copy_move_point(gps, temp_points, temp_dverts, i, i2, false);
      i2++;
    }

    /* If first point, add new point at the beginning. */
    if (do_first) {
      gpencil_copy_move_point(gps, temp_points, temp_dverts, 0, 0, true);
      /* deselect old */
      pt = &gps->points[1];
      pt->flag &= ~GP_SPOINT_SELECT;
      /* select new */
      pt = &gps->points[0];
      pt->flag |= GP_SPOINT_SELECT;
    }

    /* if last point, add new point at the end */
    if (do_last) {
      gpencil_copy_move_point(
          gps, temp_points, temp_dverts, oldtotpoints - 1, gps->totpoints - 1, true);

      /* deselect old */
      pt = &gps->points[gps->totpoints - 2];
      pt->flag &= ~GP_SPOINT_SELECT;
      /* select new */
      pt = &gps->points[gps->totpoints - 1];
      pt->flag |= GP_SPOINT_SELECT;
    }

    /* Flip stroke if it was only one point to consider extrude point as last point. */
    if (gps->totpoints == 2) {
      BKE_gpencil_stroke_flip(gps);
    }

    /* Calc geometry data. */
    BKE_gpencil_stroke_geometry_update(gpd, gps);

    MEM_SAFE_FREE(temp_points);
    MEM_SAFE_FREE(temp_dverts);
  }

  /* if the stroke is not reused, deselect */
  if (!do_stroke) {
    gps->flag &= ~GP_STROKE_SELECT;
    BKE_gpencil_stroke_select_index_reset(gps);
  }
}

static void gpencil_curve_extrude_points(bGPdata *gpd,
                                         bGPDframe *gpf,
                                         bGPDstroke *gps,
                                         bGPDcurve *gpc)
{
  const int old_num_points = gpc->tot_curve_points;
  const bool first_select = gpc->curve_points[0].flag & GP_CURVE_POINT_SELECT;
  bool last_select = gpc->curve_points[old_num_points - 1].flag & GP_CURVE_POINT_SELECT;

  /* iterate over middle points */
  for (int i = 1; i < gpc->tot_curve_points - 1; i++) {
    bGPDcurve_point *gpc_pt = &gpc->curve_points[i];

    /* Create new stroke if selected point */
    if (gpc_pt->flag & GP_CURVE_POINT_SELECT) {
      bGPDstroke *gps_new = BKE_gpencil_stroke_duplicate(gps, false, false);
      gps_new->points = nullptr;
      gps_new->flag &= ~GP_STROKE_CYCLIC;
      gps_new->prev = gps_new->next = nullptr;

      gps_new->editcurve = BKE_gpencil_stroke_editcurve_new(2);
      bGPDcurve *new_gpc = gps_new->editcurve;
      for (int j = 0; j < new_gpc->tot_curve_points; j++) {
        bGPDcurve_point *gpc_pt_new = &new_gpc->curve_points[j];
        memcpy(gpc_pt_new, gpc_pt, sizeof(bGPDcurve_point));
        gpc_pt_new->flag &= ~GP_CURVE_POINT_SELECT;
        BEZT_DESEL_ALL(&gpc_pt_new->bezt);
      }

      /* select last point */
      bGPDcurve_point *gpc_pt_last = &new_gpc->curve_points[1];
      gpc_pt_last->flag |= GP_CURVE_POINT_SELECT;
      BEZT_SEL_IDX(&gpc_pt_last->bezt, 1);
      gps_new->editcurve->flag |= GP_CURVE_SELECT;

      BLI_insertlinkafter(&gpf->strokes, gps, gps_new);

      gps_new->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
      BKE_gpencil_stroke_geometry_update(gpd, gps_new);

      gpc_pt->flag &= ~GP_CURVE_POINT_SELECT;
      BEZT_DESEL_ALL(&gpc_pt->bezt);
    }
  }

  /* Edge-case for single curve point. */
  if (gpc->tot_curve_points == 1) {
    last_select = false;
  }

  if (first_select || last_select) {
    int new_num_points = old_num_points;

    if (first_select) {
      new_num_points++;
    }
    if (last_select) {
      new_num_points++;
    }

    /* Grow the array */
    gpc->tot_curve_points = new_num_points;
    gpc->curve_points = static_cast<bGPDcurve_point *>(
        MEM_recallocN(gpc->curve_points, sizeof(bGPDcurve_point) * new_num_points));

    if (first_select) {
      /* shift points by one */
      memmove(
          &gpc->curve_points[1], &gpc->curve_points[0], sizeof(bGPDcurve_point) * old_num_points);

      bGPDcurve_point *old_first = &gpc->curve_points[1];

      old_first->flag &= ~GP_CURVE_POINT_SELECT;
      BEZT_DESEL_ALL(&old_first->bezt);
    }

    if (last_select) {
      bGPDcurve_point *old_last = &gpc->curve_points[gpc->tot_curve_points - 2];
      bGPDcurve_point *new_last = &gpc->curve_points[gpc->tot_curve_points - 1];
      memcpy(new_last, old_last, sizeof(bGPDcurve_point));

      old_last->flag &= ~GP_CURVE_POINT_SELECT;
      BEZT_DESEL_ALL(&old_last->bezt);
    }

    gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
    BKE_gpencil_stroke_geometry_update(gpd, gps);
  }
}

static int gpencil_extrude_exec(bContext *C, wmOperator *op)
{
  Object *obact = CTX_data_active_object(C);
  bGPdata *gpd = (bGPdata *)obact->data;
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));

  if (gpd == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
    return OPERATOR_CANCELLED;
  }

  bool changed = false;
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == nullptr) {
          continue;
        }

        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }

          if (is_curve_edit) {
            if (gps->editcurve == nullptr) {
              continue;
            }
            bGPDcurve *gpc = gps->editcurve;
            if (gpc->flag & GP_CURVE_SELECT) {
              gpencil_curve_extrude_points(gpd, gpf, gps, gpc);
            }
          }
          else {
            if (gps->flag & GP_STROKE_SELECT) {
              gpencil_add_move_points(gpd, gpf, gps);
            }
          }

          changed = true;
        }
        /* If not multi-edit, exit loop. */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }
  CTX_DATA_END;

  if (changed) {
    /* updates */
    DEG_id_tag_update(&gpd->id,
                      ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);
    DEG_id_tag_update(&obact->id, ID_RECALC_COPY_ON_WRITE);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_extrude(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Extrude Stroke Points";
  ot->idname = "GPENCIL_OT_extrude";
  ot->description = "Extrude the selected Grease Pencil points";

  /* callbacks */
  ot->exec = gpencil_extrude_exec;
  ot->poll = gpencil_stroke_edit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy/Paste Strokes Utilities
 *
 * Grease Pencil stroke data copy/paste buffer:
 * - The copy operation collects all segments of selected strokes,
 *   dumping "ready to be copied" copies of the strokes into the buffer.
 * - The paste operation makes a copy of those elements, and adds them
 *   to the active layer. This effectively flattens down the strokes
 *   from several different layers into a single layer.
 * \{ */

ListBase gpencil_strokes_copypastebuf = {nullptr, nullptr};

/* Hash for hanging on to all the colors used by strokes in the buffer
 *
 * This is needed to prevent dangling and unsafe pointers when pasting across data-blocks,
 * or after a color used by a stroke in the buffer gets deleted (via user action or undo).
 */
static GHash *gpencil_strokes_copypastebuf_colors = nullptr;

static GHash *gpencil_strokes_copypastebuf_colors_material_to_name_create(Main *bmain)
{
  GHash *ma_to_name = BLI_ghash_ptr_new(__func__);

  for (Material *ma = static_cast<Material *>(bmain->materials.first); ma != nullptr;
       ma = static_cast<Material *>(ma->id.next))
  {
    char *name = BKE_id_to_unique_string_key(&ma->id);
    BLI_ghash_insert(ma_to_name, ma, name);
  }

  return ma_to_name;
}

static void gpencil_strokes_copypastebuf_colors_material_to_name_free(GHash *ma_to_name)
{
  BLI_ghash_free(ma_to_name, nullptr, MEM_freeN);
}

static GHash *gpencil_strokes_copypastebuf_colors_name_to_material_create(Main *bmain)
{
  GHash *name_to_ma = BLI_ghash_str_new(__func__);

  for (Material *ma = static_cast<Material *>(bmain->materials.first); ma != nullptr;
       ma = static_cast<Material *>(ma->id.next))
  {
    char *name = BKE_id_to_unique_string_key(&ma->id);
    BLI_ghash_insert(name_to_ma, name, ma);
  }

  return name_to_ma;
}

static void gpencil_strokes_copypastebuf_colors_name_to_material_free(GHash *name_to_ma)
{
  BLI_ghash_free(name_to_ma, MEM_freeN, nullptr);
}

void ED_gpencil_strokes_copybuf_free()
{
  bGPDstroke *gps, *gpsn;

  /* Free the colors buffer.
   * NOTE: This is done before the strokes so that the pointers are still safe. */
  if (gpencil_strokes_copypastebuf_colors) {
    BLI_ghash_free(gpencil_strokes_copypastebuf_colors, nullptr, MEM_freeN);
    gpencil_strokes_copypastebuf_colors = nullptr;
  }

  /* Free the stroke buffer */
  for (gps = static_cast<bGPDstroke *>(gpencil_strokes_copypastebuf.first); gps; gps = gpsn) {
    gpsn = gps->next;

    if (gps->points) {
      MEM_freeN(gps->points);
    }
    if (gps->dvert) {
      BKE_gpencil_free_stroke_weights(gps);
      MEM_freeN(gps->dvert);
    }

    MEM_SAFE_FREE(gps->triangles);

    BLI_freelinkN(&gpencil_strokes_copypastebuf, gps);
  }

  gpencil_strokes_copypastebuf.first = gpencil_strokes_copypastebuf.last = nullptr;
}

GHash *gpencil_copybuf_validate_colormap(bContext *C)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  GHash *new_colors = BLI_ghash_int_new("GPencil Paste Dst Colors");
  GHashIterator gh_iter;

  /* For each color, check if exist and add if not */
  GHash *name_to_ma = gpencil_strokes_copypastebuf_colors_name_to_material_create(bmain);

  GHASH_ITER (gh_iter, gpencil_strokes_copypastebuf_colors) {
    int *key = static_cast<int *>(BLI_ghashIterator_getKey(&gh_iter));
    char *ma_name = static_cast<char *>(BLI_ghashIterator_getValue(&gh_iter));
    Material *ma = static_cast<Material *>(BLI_ghash_lookup(name_to_ma, ma_name));

    BKE_gpencil_object_material_ensure(bmain, ob, ma);

    /* Store this mapping (for use later when pasting) */
    if (!BLI_ghash_haskey(new_colors, POINTER_FROM_INT(*key))) {
      BLI_ghash_insert(new_colors, POINTER_FROM_INT(*key), ma);
    }
  }

  gpencil_strokes_copypastebuf_colors_name_to_material_free(name_to_ma);

  return new_colors;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy Selected Strokes Operator
 * \{ */

static int gpencil_strokes_copy_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));

  if (gpd == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
    return OPERATOR_CANCELLED;
  }

  /* clear the buffer first */
  ED_gpencil_strokes_copybuf_free();

  if (is_curve_edit) {
    BKE_report(op->reports, RPT_ERROR, "Not implemented!");
  }
  else {
    /* for each visible (and editable) layer's selected strokes,
     * copy the strokes into a temporary buffer, then append
     * once all done
     */
    CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
      bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                      gpl->actframe);

      for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
        if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
          if (gpf == nullptr) {
            continue;
          }

          /* make copies of selected strokes, and deselect these once we're done */
          LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
            /* skip strokes that are invalid for current view */
            if (ED_gpencil_stroke_can_use(C, gps) == false) {
              continue;
            }

            if (gps->flag & GP_STROKE_SELECT) {
              if (gps->totpoints == 1) {
                /* Special Case: If there's just a single point in this stroke... */
                bGPDstroke *gpsd;

                /* make direct copies of the stroke and its points */
                gpsd = BKE_gpencil_stroke_duplicate(gps, false, true);

                /* saves original layer name */
                STRNCPY(gpsd->runtime.tmp_layerinfo, gpl->info);
                gpsd->points = static_cast<bGPDspoint *>(MEM_dupallocN(gps->points));
                if (gps->dvert != nullptr) {
                  gpsd->dvert = static_cast<MDeformVert *>(MEM_dupallocN(gps->dvert));
                  BKE_gpencil_stroke_weights_duplicate(gps, gpsd);
                }

                /* Calc geometry data. */
                BKE_gpencil_stroke_geometry_update(gpd, gpsd);

                /* add to temp buffer */
                gpsd->next = gpsd->prev = nullptr;
                BLI_addtail(&gpencil_strokes_copypastebuf, gpsd);
              }
              else {
                /* delegate to a helper, as there's too much to fit in here (for copying
                 * subsets)... */
                gpencil_duplicate_points(gpd, gps, &gpencil_strokes_copypastebuf, gpl->info);
              }
            }
          }
        }
      }
    }
    CTX_DATA_END;
  }

  /* Build up hash of material colors used in these strokes */
  if (gpencil_strokes_copypastebuf.first) {
    gpencil_strokes_copypastebuf_colors = BLI_ghash_int_new("GPencil CopyBuf Colors");
    GHash *ma_to_name = gpencil_strokes_copypastebuf_colors_material_to_name_create(bmain);
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpencil_strokes_copypastebuf) {
      if (ED_gpencil_stroke_can_use(C, gps)) {
        Material *ma = BKE_object_material_get(ob, gps->mat_nr + 1);
        /* Avoid default material. */
        if (ma == nullptr) {
          continue;
        }

        char **ma_name_val;
        if (!BLI_ghash_ensure_p(
                gpencil_strokes_copypastebuf_colors, &gps->mat_nr, (void ***)&ma_name_val))
        {
          char *ma_name = static_cast<char *>(BLI_ghash_lookup(ma_to_name, ma));
          *ma_name_val = static_cast<char *>(MEM_dupallocN(ma_name));
        }
      }
    }
    gpencil_strokes_copypastebuf_colors_material_to_name_free(ma_to_name);
  }

  /* updates (to ensure operator buttons are refreshed, when used via hotkeys) */
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA, nullptr); /* XXX? */

  /* done */
  return OPERATOR_FINISHED;
}

void GPENCIL_OT_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Strokes";
  ot->idname = "GPENCIL_OT_copy";
  ot->description = "Copy selected Grease Pencil points and strokes";

  /* callbacks */
  ot->exec = gpencil_strokes_copy_exec;
  ot->poll = gpencil_stroke_edit_poll;

  /* flags */
  // ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paste Selected Strokes Operator
 * \{ */

static bool gpencil_strokes_paste_poll(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (!((area != nullptr) && (area->spacetype == SPACE_VIEW3D))) {
    return false;
  }
  /* 1) Must have GP datablock to paste to
   *    - We don't need to have an active layer though, as that can easily get added
   *    - If the active layer is locked, we can't paste there,
   *      but that should prompt a warning instead.
   * 2) Copy buffer must at least have something (though it may be the wrong sort...).
   */
  return (ED_gpencil_data_get_active(C) != nullptr) &&
         !BLI_listbase_is_empty(&gpencil_strokes_copypastebuf);
}

enum eGP_PasteMode {
  GP_COPY_BY_LAYER = -1,
  GP_COPY_TO_ACTIVE = 1,
};

static int gpencil_strokes_paste_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = (bGPdata *)ob->data;
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd); /* only use active for copy merge */
  Scene *scene = CTX_data_scene(C);

  bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                  gpl->actframe);

  eGP_PasteMode type = eGP_PasteMode(RNA_enum_get(op->ptr, "type"));
  const bool on_back = RNA_boolean_get(op->ptr, "paste_back");
  GHash *new_colors;

  /* Check for various error conditions. */

  if (BLI_listbase_is_empty(&gpencil_strokes_copypastebuf)) {
    BKE_report(op->reports,
               RPT_ERROR,
               "No strokes to paste, select and copy some points before trying again");
    return OPERATOR_CANCELLED;
  }

  if (gpl == nullptr) {
    /* no active layer - let's just create one */
    gpl = BKE_gpencil_layer_addnew(gpd, DATA_("GP_Layer"), true, false);
  }
  else if ((BKE_gpencil_layer_is_editable(gpl) == false) && (type == GP_COPY_TO_ACTIVE)) {
    BKE_report(
        op->reports, RPT_ERROR, "Cannot paste strokes when active layer is hidden or locked");
    return OPERATOR_CANCELLED;
  }
  else {
    /* Check that some of the strokes in the buffer can be used */
    bool ok = false;

    LISTBASE_FOREACH (bGPDstroke *, gps, &gpencil_strokes_copypastebuf) {
      if (ED_gpencil_stroke_can_use(C, gps)) {
        ok = true;
        break;
      }
    }

    if (ok == false) {
      return OPERATOR_CANCELLED;
    }
  }

  /* Deselect all strokes first */
  CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
    bGPDspoint *pt;
    int i;

    for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
      pt->flag &= ~GP_SPOINT_SELECT;
    }

    gps->flag &= ~GP_STROKE_SELECT;
    BKE_gpencil_stroke_select_index_reset(gps);
  }
  CTX_DATA_END;

  /* Ensure that all the necessary colors exist */
  new_colors = gpencil_copybuf_validate_colormap(C);

  if (is_curve_edit) {
    BKE_report(op->reports, RPT_ERROR, "Not implemented!");
  }
  else {
    /* Copy over the strokes from the buffer (and adjust the colors) */
    bGPDstroke *gps_init = static_cast<bGPDstroke *>(
        (!on_back) ? gpencil_strokes_copypastebuf.first : gpencil_strokes_copypastebuf.last);

    for (bGPDstroke *gps = gps_init; gps; gps = (!on_back) ? gps->next : gps->prev) {
      if (ED_gpencil_stroke_can_use(C, gps)) {
        /* Need to verify if layer exists */
        if (type != GP_COPY_TO_ACTIVE) {
          gpl = static_cast<bGPDlayer *>(
              BLI_findstring(&gpd->layers, gps->runtime.tmp_layerinfo, offsetof(bGPDlayer, info)));
          if (gpl == nullptr) {
            /* no layer - use active (only if layer deleted before paste) */
            gpl = BKE_gpencil_layer_active_get(gpd);
          }
        }

        /* Ensure we have a frame to draw into.
         * NOTE: Since this is an op which creates strokes,
         *       we reuse active frame or add a new frame if one
         *       doesn't exist already depending on REC button status.
         */

        /* Multi-frame paste. */
        if (is_multiedit) {
          for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
            /* Active frame is copied later, so don't need duplicate the stroke here. */
            if (gpl->actframe == gpf) {
              continue;
            }
            if (gpf->flag & GP_FRAME_SELECT) {
              if (gpf) {
                /* Create new stroke */
                bGPDstroke *new_stroke = BKE_gpencil_stroke_duplicate(gps, true, true);
                new_stroke->runtime.tmp_layerinfo[0] = '\0';
                new_stroke->next = new_stroke->prev = nullptr;

                /* Calc geometry data. */
                BKE_gpencil_stroke_geometry_update(gpd, new_stroke);

                if (on_back) {
                  BLI_addhead(&gpf->strokes, new_stroke);
                }
                else {
                  BLI_addtail(&gpf->strokes, new_stroke);
                }

                /* Remap material */
                Material *ma = static_cast<Material *>(
                    BLI_ghash_lookup(new_colors, POINTER_FROM_INT(new_stroke->mat_nr)));
                new_stroke->mat_nr = BKE_gpencil_object_material_index_get(ob, ma);
                CLAMP_MIN(new_stroke->mat_nr, 0);
              }
            }
          }
        }

        bGPDframe *gpf;
        if (blender::animrig::is_autokey_on(scene) || (gpl->actframe == nullptr)) {
          gpf = BKE_gpencil_layer_frame_get(gpl, scene->r.cfra, GP_GETFRAME_ADD_NEW);
        }
        else {
          gpf = BKE_gpencil_layer_frame_get(gpl, scene->r.cfra, GP_GETFRAME_USE_PREV);
        }
        if (gpf) {
          /* Create new stroke */
          bGPDstroke *new_stroke = BKE_gpencil_stroke_duplicate(gps, true, true);
          new_stroke->runtime.tmp_layerinfo[0] = '\0';
          new_stroke->next = new_stroke->prev = nullptr;

          /* Calc geometry data. */
          BKE_gpencil_stroke_geometry_update(gpd, new_stroke);

          if (on_back) {
            BLI_addhead(&gpf->strokes, new_stroke);
          }
          else {
            BLI_addtail(&gpf->strokes, new_stroke);
          }

          /* Remap material */
          Material *ma = static_cast<Material *>(
              BLI_ghash_lookup(new_colors, POINTER_FROM_INT(new_stroke->mat_nr)));
          new_stroke->mat_nr = BKE_gpencil_object_material_index_get(ob, ma);
          CLAMP_MIN(new_stroke->mat_nr, 0);
        }
      }
    }
  }

  /* free temp data */
  BLI_ghash_free(new_colors, nullptr, nullptr);

  /* updates */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_paste(wmOperatorType *ot)
{
  PropertyRNA *prop;

  static const EnumPropertyItem copy_type[] = {
      {GP_COPY_TO_ACTIVE, "ACTIVE", 0, "Paste to Active", ""},
      {GP_COPY_BY_LAYER, "LAYER", 0, "Paste by Layer", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Paste Strokes";
  ot->idname = "GPENCIL_OT_paste";
  ot->description = "Paste previously copied strokes to active layer or to original layer";

  /* callbacks */
  ot->exec = gpencil_strokes_paste_exec;
  ot->poll = gpencil_strokes_paste_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", copy_type, GP_COPY_TO_ACTIVE, "Type", "");

  prop = RNA_def_boolean(
      ot->srna, "paste_back", false, "Paste on Back", "Add pasted strokes behind all strokes");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Move To Layer Operator
 * \{ */

static int gpencil_move_to_layer_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = (bGPdata *)ob->data;
  bGPDlayer *target_layer = nullptr;
  ListBase strokes = {nullptr, nullptr};
  int layer_num = RNA_int_get(op->ptr, "layer");
  const bool use_autolock = bool(gpd->flag & GP_DATA_AUTOLOCK_LAYERS);
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));

  /* If autolock enabled, disabled now. */
  if (use_autolock) {
    gpd->flag &= ~GP_DATA_AUTOLOCK_LAYERS;
  }

  /* Try to get layer */
  if (layer_num > -1) {
    target_layer = static_cast<bGPDlayer *>(BLI_findlink(&gpd->layers, layer_num));
  }
  else {
    /* Create a new layer. */
    PropertyRNA *prop;
    char name[128];
    prop = RNA_struct_find_property(op->ptr, "new_layer_name");
    if (RNA_property_is_set(op->ptr, prop)) {
      RNA_property_string_get(op->ptr, prop, name);
    }
    else {
      STRNCPY(name, "GP_Layer");
    }
    target_layer = BKE_gpencil_layer_addnew(gpd, name, true, false);
  }

  if (target_layer == nullptr) {
    /* back autolock status */
    if (use_autolock) {
      gpd->flag |= GP_DATA_AUTOLOCK_LAYERS;
    }
    BKE_reportf(op->reports, RPT_ERROR, "There is no layer number %d", layer_num);
    return OPERATOR_CANCELLED;
  }

  /* Extract all strokes to move to this layer. */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl_src, editable_gpencil_layers) {
    /* Skip if this is the layer we're moving strokes to. */
    if (gpl_src == target_layer) {
      continue;
    }
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl_src->frames.first :
                                                                    gpl_src->actframe);
    for (bGPDframe *gpf_src = init_gpf; gpf_src; gpf_src = gpf_src->next) {
      if ((gpf_src == gpl_src->actframe) || ((gpf_src->flag & GP_FRAME_SELECT) && (is_multiedit)))
      {
        if (gpf_src == nullptr) {
          continue;
        }

        bGPDstroke *gpsn = nullptr;
        BLI_listbase_clear(&strokes);
        for (bGPDstroke *gps = static_cast<bGPDstroke *>(gpf_src->strokes.first); gps; gps = gpsn)
        {
          gpsn = gps->next;
          /* Skip strokes that are invalid for current view. */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }
          /* Check if the color is editable. */
          if (ED_gpencil_stroke_material_editable(ob, gpl_src, gps) == false) {
            continue;
          }

          if (gps->flag & GP_STROKE_SELECT) {
            BLI_remlink(&gpf_src->strokes, gps);
            BLI_addtail(&strokes, gps);
          }
        }
        /* Paste them all in one go. */
        if (strokes.first) {
          bGPDframe *gpf_dst = BKE_gpencil_layer_frame_get(
              target_layer, gpf_src->framenum, GP_GETFRAME_ADD_NEW);

          BLI_movelisttolist(&gpf_dst->strokes, &strokes);
          BLI_assert((strokes.first == strokes.last) && (strokes.first == nullptr));
        }
      }
      /* If not multi-edit, exit loop. */
      if (!is_multiedit) {
        break;
      }
    }
    /* If new layer and autolock, lock old layer. */
    if ((layer_num == -1) && (use_autolock)) {
      gpl_src->flag |= GP_LAYER_LOCKED;
    }
  }
  CTX_DATA_END;

  /* back autolock status */
  if (use_autolock) {
    gpd->flag |= GP_DATA_AUTOLOCK_LAYERS;
  }

  /* updates */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static int gpencil_move_to_layer_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  const int tmp = ED_gpencil_new_layer_dialog(C, op);
  if (tmp != 0) {
    return tmp;
  }
  return gpencil_move_to_layer_exec(C, op);
}

void GPENCIL_OT_move_to_layer(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Move Strokes to Layer";
  ot->idname = "GPENCIL_OT_move_to_layer";
  ot->description =
      "Move selected strokes to another layer"; /* XXX: allow moving individual points too? */

  /* callbacks */
  ot->exec = gpencil_move_to_layer_exec;
  ot->invoke = gpencil_move_to_layer_invoke;
  ot->poll = gpencil_stroke_edit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* GPencil layer to use. */
  prop = RNA_def_int(ot->srna, "layer", 0, -1, INT_MAX, "Grease Pencil Layer", "", -1, INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  prop = RNA_def_string(
      ot->srna, "new_layer_name", nullptr, MAX_NAME, "Name", "Name of the newly added layer");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  ot->prop = prop;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Blank Frame Operator
 * \{ */

static int gpencil_blank_frame_add_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  Scene *scene = CTX_data_scene(C);
  int cfra = scene->r.cfra;

  bGPDlayer *active_gpl = BKE_gpencil_layer_active_get(gpd);

  const bool all_layers = RNA_boolean_get(op->ptr, "all_layers");

  /* Initialize data-block and an active layer if nothing exists yet. */
  if (ELEM(nullptr, gpd, active_gpl)) {
    /* Let's just be lazy, and call the "Add New Layer" operator,
     * which sets everything up as required. */
    WM_operator_name_call(C, "GPENCIL_OT_layer_add", WM_OP_EXEC_DEFAULT, nullptr, nullptr);
  }

  /* Go through each layer, adding a frame after the active one
   * and/or shunting all the others out of the way
   */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    if ((all_layers == false) && (gpl != active_gpl)) {
      continue;
    }

    /* 1) Check for an existing frame on the current frame */
    bGPDframe *gpf = BKE_gpencil_layer_frame_find(gpl, cfra);
    if (gpf) {
      /* Shunt all frames after (and including) the existing one later by 1-frame */
      for (; gpf; gpf = gpf->next) {
        gpf->framenum += 1;
      }
    }

    /* 2) Now add a new frame, with nothing in it */
    gpl->actframe = BKE_gpencil_layer_frame_get(gpl, cfra, GP_GETFRAME_ADD_NEW);
  }
  CTX_DATA_END;

  /* notifiers */
  if (gpd != nullptr) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  }
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_blank_frame_add(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Insert Blank Frame";
  ot->idname = "GPENCIL_OT_blank_frame_add";
  ot->description =
      "Insert a blank frame on the current frame "
      "(all subsequently existing frames, if any, are shifted right by one frame)";

  /* callbacks */
  ot->exec = gpencil_blank_frame_add_exec;
  ot->poll = gpencil_add_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_boolean(ot->srna,
                         "all_layers",
                         false,
                         "All Layers",
                         "Create blank frame in all layers, not only active");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Active Frame Operator
 * \{ */

static bool gpencil_actframe_delete_poll(bContext *C)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);

  /* only if there's an active layer with an active frame */
  return (gpl && gpl->actframe);
}

static bool annotation_actframe_delete_poll(bContext *C)
{
  bGPdata *gpd = ED_annotation_data_get_active(C);
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);

  /* only if there's an active layer with an active frame */
  return (gpl && gpl->actframe);
}

/* delete active frame - wrapper around API calls */
static int gpencil_actframe_delete_exec(bContext *C, wmOperator *op)
{
  const bool is_annotation = STREQ(op->idname, "GPENCIL_OT_annotation_active_frame_delete");

  bGPdata *gpd = (!is_annotation) ? ED_gpencil_data_get_active(C) :
                                    ED_annotation_data_get_active(C);

  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);

  Scene *scene = CTX_data_scene(C);

  bGPDframe *gpf = BKE_gpencil_layer_frame_get(gpl, scene->r.cfra, GP_GETFRAME_USE_PREV);

  /* if there's no existing Grease-Pencil data there, add some */
  if (gpd == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "No grease pencil data");
    return OPERATOR_CANCELLED;
  }
  if (ELEM(nullptr, gpl, gpf)) {
    BKE_report(op->reports, RPT_ERROR, "No active frame to delete");
    return OPERATOR_CANCELLED;
  }

  /* delete it... */
  BKE_gpencil_layer_frame_delete(gpl, gpf);

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_active_frame_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Active Frame";
  ot->idname = "GPENCIL_OT_active_frame_delete";
  ot->description = "Delete the active frame for the active Grease Pencil Layer";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* callbacks */
  ot->exec = gpencil_actframe_delete_exec;
  ot->poll = gpencil_actframe_delete_poll;
}

void GPENCIL_OT_annotation_active_frame_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Active Frame";
  ot->idname = "GPENCIL_OT_annotation_active_frame_delete";
  ot->description = "Delete the active frame for the active Annotation Layer";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* callbacks */
  ot->exec = gpencil_actframe_delete_exec;
  ot->poll = annotation_actframe_delete_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete All Active Frames
 * \{ */

static bool gpencil_actframe_delete_all_poll(bContext *C)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  /* 1) There must be grease pencil data
   * 2) Hopefully some of the layers have stuff we can use
   */
  return (gpd && gpd->layers.first);
}

static int gpencil_actframe_delete_all_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  Scene *scene = CTX_data_scene(C);

  bool success = false;

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    /* try to get the "active" frame - but only if it actually occurs on this frame */
    bGPDframe *gpf = BKE_gpencil_layer_frame_get(gpl, scene->r.cfra, GP_GETFRAME_USE_PREV);

    if (gpf == nullptr) {
      continue;
    }

    /* delete it... */
    BKE_gpencil_layer_frame_delete(gpl, gpf);

    /* we successfully modified something */
    success = true;
  }
  CTX_DATA_END;

  /* updates */
  if (success) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
    return OPERATOR_FINISHED;
  }
  BKE_report(op->reports, RPT_ERROR, "No active frame(s) to delete");
  return OPERATOR_CANCELLED;
}

void GPENCIL_OT_active_frames_delete_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete All Active Frames";
  ot->idname = "GPENCIL_OT_active_frames_delete_all";
  ot->description = "Delete the active frame(s) of all editable Grease Pencil layers";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* callbacks */
  ot->exec = gpencil_actframe_delete_all_exec;
  ot->poll = gpencil_actframe_delete_all_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete/Dissolve Utilities
 * \{ */

enum eGP_DeleteMode {
  /* delete selected stroke points */
  GP_DELETEOP_POINTS = 0,
  /* delete selected strokes */
  GP_DELETEOP_STROKES = 1,
  /* delete active frame */
  GP_DELETEOP_FRAME = 2,
};

enum eGP_DissolveMode {
  /* dissolve all selected points */
  GP_DISSOLVE_POINTS = 0,
  /* dissolve between selected points */
  GP_DISSOLVE_BETWEEN = 1,
  /* dissolve unselected points */
  GP_DISSOLVE_UNSELECT = 2,
};

/* Delete selected strokes */
static int gpencil_delete_selected_strokes(bContext *C)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));

  bool changed = false;
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {

        if (gpf == nullptr) {
          continue;
        }

        /* simply delete strokes which are selected */
        LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {

          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }

          /* free stroke if selected */
          if (gps->flag & GP_STROKE_SELECT) {
            BLI_remlink(&gpf->strokes, gps);
            /* free stroke memory arrays, then stroke itself */
            BKE_gpencil_free_stroke(gps);

            changed = true;
          }
        }
      }
    }
  }
  CTX_DATA_END;

  if (changed) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

/* ----------------------------------- */

static bool gpencil_dissolve_selected_curve_points(bContext *C,
                                                   bGPdata *gpd,
                                                   eGP_DissolveMode mode)
{
  bool changed = false;
  GP_EDITABLE_CURVES_BEGIN(gps_iter, C, gpl, gps, gpc)
  {
    if (gpc->flag & GP_CURVE_SELECT) {
      int first = 0, last = 0;
      int num_points_remaining = gpc->tot_curve_points;

      switch (mode) {
        case GP_DISSOLVE_POINTS:
          for (int i = 0; i < gpc->tot_curve_points; i++) {
            bGPDcurve_point *cpt = &gpc->curve_points[i];
            if (cpt->flag & GP_CURVE_POINT_SELECT) {
              num_points_remaining--;
            }
          }
          break;
        case GP_DISSOLVE_BETWEEN:
          first = -1;
          for (int i = 0; i < gpc->tot_curve_points; i++) {
            bGPDcurve_point *cpt = &gpc->curve_points[i];
            if (cpt->flag & GP_CURVE_POINT_SELECT) {
              if (first < 0) {
                first = i;
              }
              last = i;
            }
          }

          for (int i = first + 1; i < last; i++) {
            bGPDcurve_point *cpt = &gpc->curve_points[i];
            if ((cpt->flag & GP_CURVE_POINT_SELECT) == 0) {
              num_points_remaining--;
            }
          }
          break;
        case GP_DISSOLVE_UNSELECT:
          for (int i = 0; i < gpc->tot_curve_points; i++) {
            bGPDcurve_point *cpt = &gpc->curve_points[i];
            if ((cpt->flag & GP_CURVE_POINT_SELECT) == 0) {
              num_points_remaining--;
            }
          }
          break;
        default:
          return false;
          break;
      }

      if (num_points_remaining < 1) {
        /* Delete stroke */
        BLI_remlink(&gpf_->strokes, gps);
        BKE_gpencil_free_stroke(gps);
      }
      else {
        bGPDcurve_point *new_points = static_cast<bGPDcurve_point *>(
            MEM_callocN(sizeof(bGPDcurve_point) * num_points_remaining, __func__));

        int idx = 0;
        switch (mode) {
          case GP_DISSOLVE_POINTS:
            for (int i = 0; i < gpc->tot_curve_points; i++) {
              bGPDcurve_point *cpt = &gpc->curve_points[i];
              bGPDcurve_point *new_cpt = &new_points[idx];
              if ((cpt->flag & GP_CURVE_POINT_SELECT) == 0) {
                *new_cpt = *cpt;
                idx++;
              }
            }
            break;
          case GP_DISSOLVE_BETWEEN:
            for (int i = 0; i < first; i++) {
              bGPDcurve_point *cpt = &gpc->curve_points[i];
              bGPDcurve_point *new_cpt = &new_points[idx];

              *new_cpt = *cpt;
              idx++;
            }

            for (int i = first; i < last; i++) {
              bGPDcurve_point *cpt = &gpc->curve_points[i];
              bGPDcurve_point *new_cpt = &new_points[idx];
              if (cpt->flag & GP_CURVE_POINT_SELECT) {
                *new_cpt = *cpt;
                idx++;
              }
            }

            for (int i = last; i < gpc->tot_curve_points; i++) {
              bGPDcurve_point *cpt = &gpc->curve_points[i];
              bGPDcurve_point *new_cpt = &new_points[idx];

              *new_cpt = *cpt;
              idx++;
            }
            break;
          case GP_DISSOLVE_UNSELECT:
            for (int i = 0; i < gpc->tot_curve_points; i++) {
              bGPDcurve_point *cpt = &gpc->curve_points[i];
              bGPDcurve_point *new_cpt = &new_points[idx];
              if (cpt->flag & GP_CURVE_POINT_SELECT) {
                *new_cpt = *cpt;
                idx++;
              }
            }
            break;
          default:
            return false;
            break;
        }

        if (gpc->curve_points != nullptr) {
          MEM_freeN(gpc->curve_points);
        }

        gpc->curve_points = new_points;
        gpc->tot_curve_points = num_points_remaining;

        BKE_gpencil_editcurve_recalculate_handles(gps);
        gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
        BKE_gpencil_stroke_geometry_update(gpd, gps);
      }

      changed = true;
    }
  }
  GP_EDITABLE_CURVES_END(gps_iter);

  return changed;
}

static bool gpencil_dissolve_selected_stroke_points(bContext *C,
                                                    bGPdata *gpd,
                                                    eGP_DissolveMode mode)
{
  bool changed = false;
  int first = 0;
  int last = 0;

  GP_EDITABLE_STROKES_BEGIN (gpstroke_iter, C, gpl, gps) {
    /* the stroke must have at least one point selected for any operator */
    if (gps->flag & GP_STROKE_SELECT) {
      bGPDspoint *pt;
      MDeformVert *dvert = nullptr;
      int i;

      int tot = gps->totpoints; /* number of points in new buffer */

      /* first pass: count points to remove */
      switch (mode) {
        case GP_DISSOLVE_POINTS:
          /* Count how many points are selected (i.e. how many to remove) */
          for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
            if (pt->flag & GP_SPOINT_SELECT) {
              /* selected point - one of the points to remove */
              tot--;
            }
          }
          break;
        case GP_DISSOLVE_BETWEEN:
          /* need to find first and last point selected */
          first = -1;
          last = 0;
          for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
            if (pt->flag & GP_SPOINT_SELECT) {
              if (first < 0) {
                first = i;
              }
              last = i;
            }
          }
          /* count unselected points in the range */
          for (i = first, pt = gps->points + first; i < last; i++, pt++) {
            if ((pt->flag & GP_SPOINT_SELECT) == 0) {
              tot--;
            }
          }
          break;
        case GP_DISSOLVE_UNSELECT:
          /* count number of unselected points */
          for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
            if ((pt->flag & GP_SPOINT_SELECT) == 0) {
              tot--;
            }
          }
          break;
        default:
          return false;
          break;
      }

      /* if no points are left, we simply delete the entire stroke */
      if (tot <= 0) {
        /* remove the entire stroke */
        BLI_remlink(&gpf_->strokes, gps);
        BKE_gpencil_free_stroke(gps);
      }
      else {
        /* just copy all points to keep into a smaller buffer */
        bGPDspoint *new_points = static_cast<bGPDspoint *>(
            MEM_callocN(sizeof(bGPDspoint) * tot, "new gp stroke points copy"));
        bGPDspoint *npt = new_points;

        MDeformVert *new_dvert = nullptr;
        MDeformVert *ndvert = nullptr;

        if (gps->dvert != nullptr) {
          new_dvert = static_cast<MDeformVert *>(
              MEM_callocN(sizeof(MDeformVert) * tot, "new gp stroke weights copy"));
          ndvert = new_dvert;
        }

        switch (mode) {
          case GP_DISSOLVE_POINTS:
            (gps->dvert != nullptr) ? dvert = gps->dvert : nullptr;
            for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
              if ((pt->flag & GP_SPOINT_SELECT) == 0) {
                *npt = blender::dna::shallow_copy(*pt);
                npt++;

                if (gps->dvert != nullptr) {
                  *ndvert = *dvert;
                  ndvert->dw = static_cast<MDeformWeight *>(MEM_dupallocN(dvert->dw));
                  ndvert++;
                }
              }
              if (gps->dvert != nullptr) {
                dvert++;
              }
            }
            break;
          case GP_DISSOLVE_BETWEEN:
            /* copy first segment */
            (gps->dvert != nullptr) ? dvert = gps->dvert : nullptr;
            for (i = 0, pt = gps->points; i < first; i++, pt++) {
              *npt = blender::dna::shallow_copy(*pt);
              npt++;

              if (gps->dvert != nullptr) {
                *ndvert = *dvert;
                ndvert->dw = static_cast<MDeformWeight *>(MEM_dupallocN(dvert->dw));
                ndvert++;
                dvert++;
              }
            }
            /* copy segment (selected points) */
            (gps->dvert != nullptr) ? dvert = gps->dvert + first : nullptr;
            for (i = first, pt = gps->points + first; i < last; i++, pt++) {
              if (pt->flag & GP_SPOINT_SELECT) {
                *npt = blender::dna::shallow_copy(*pt);
                npt++;

                if (gps->dvert != nullptr) {
                  *ndvert = *dvert;
                  ndvert->dw = static_cast<MDeformWeight *>(MEM_dupallocN(dvert->dw));
                  ndvert++;
                }
              }
              if (gps->dvert != nullptr) {
                dvert++;
              }
            }
            /* copy last segment */
            (gps->dvert != nullptr) ? dvert = gps->dvert + last : nullptr;
            for (i = last, pt = gps->points + last; i < gps->totpoints; i++, pt++) {
              *npt = blender::dna::shallow_copy(*pt);
              npt++;

              if (gps->dvert != nullptr) {
                *ndvert = *dvert;
                ndvert->dw = static_cast<MDeformWeight *>(MEM_dupallocN(dvert->dw));
                ndvert++;
                dvert++;
              }
            }

            break;
          case GP_DISSOLVE_UNSELECT:
            /* copy any selected point */
            (gps->dvert != nullptr) ? dvert = gps->dvert : nullptr;
            for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
              if (pt->flag & GP_SPOINT_SELECT) {
                *npt = blender::dna::shallow_copy(*pt);
                npt++;

                if (gps->dvert != nullptr) {
                  *ndvert = *dvert;
                  ndvert->dw = static_cast<MDeformWeight *>(MEM_dupallocN(dvert->dw));
                  ndvert++;
                }
              }
              if (gps->dvert != nullptr) {
                dvert++;
              }
            }
            break;
        }

        /* free the old buffer */
        if (gps->points) {
          MEM_freeN(gps->points);
        }
        if (gps->dvert) {
          BKE_gpencil_free_stroke_weights(gps);
          MEM_freeN(gps->dvert);
        }

        /* save the new buffer */
        gps->points = new_points;
        gps->dvert = new_dvert;
        gps->totpoints = tot;

        /* Calc geometry data. */
        BKE_gpencil_stroke_geometry_update(gpd, gps);

        /* deselect the stroke, since none of its selected points will still be selected */
        gps->flag &= ~GP_STROKE_SELECT;
        BKE_gpencil_stroke_select_index_reset(gps);
        for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
          pt->flag &= ~GP_SPOINT_SELECT;
        }
      }

      changed = true;
    }
  }
  GP_EDITABLE_STROKES_END(gpstroke_iter);

  return changed;
}

/* Delete selected points but keep the stroke */
static int gpencil_dissolve_selected_points(bContext *C, eGP_DissolveMode mode)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = (bGPdata *)ob->data;
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));
  bool changed = false;

  if (is_curve_edit) {
    changed = gpencil_dissolve_selected_curve_points(C, gpd, mode);
  }
  else {
    changed = gpencil_dissolve_selected_stroke_points(C, gpd, mode);
  }

  if (changed) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

/* ----------------------------------- */

/* Split selected strokes into segments, splitting on selected points */
static int gpencil_delete_selected_points(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  bool changed = false;

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {

        if (gpf == nullptr) {
          continue;
        }

        /* simply delete strokes which are selected */
        LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {

          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }
          /* check if the color is editable */
          if (ED_gpencil_stroke_material_editable(ob, gpl, gps) == false) {
            continue;
          }

          if (gps->flag & GP_STROKE_SELECT) {
            /* deselect old stroke, since it will be used as template for the new strokes */
            gps->flag &= ~GP_STROKE_SELECT;
            BKE_gpencil_stroke_select_index_reset(gps);

            if (is_curve_edit) {
              bGPDcurve *gpc = gps->editcurve;
              BKE_gpencil_curve_delete_tagged_points(
                  gpd, gpf, gps, gps->next, gpc, GP_CURVE_POINT_SELECT);
            }
            else {
              /* delete unwanted points by splitting stroke into several smaller ones */
              BKE_gpencil_stroke_delete_tagged_points(
                  gpd, gpf, gps, gps->next, GP_SPOINT_SELECT, false, false, 0);
            }

            changed = true;
          }
        }
      }
    }
  }
  CTX_DATA_END;

  if (changed) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

int gpencil_delete_selected_point_wrap(bContext *C)
{
  return gpencil_delete_selected_points(C);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Operator
 * \{ */

static int gpencil_delete_exec(bContext *C, wmOperator *op)
{
  eGP_DeleteMode mode = eGP_DeleteMode(RNA_enum_get(op->ptr, "type"));
  int result = OPERATOR_CANCELLED;

  switch (mode) {
    case GP_DELETEOP_STROKES: /* selected strokes */
      result = gpencil_delete_selected_strokes(C);
      break;

    case GP_DELETEOP_POINTS: /* selected points (breaks the stroke into segments) */
      result = gpencil_delete_selected_points(C);
      break;

    case GP_DELETEOP_FRAME: /* active frame */
      result = gpencil_actframe_delete_exec(C, op);
      break;
  }

  return result;
}

void GPENCIL_OT_delete(wmOperatorType *ot)
{
  static const EnumPropertyItem prop_gpencil_delete_types[] = {
      {GP_DELETEOP_POINTS,
       "POINTS",
       0,
       "Points",
       "Delete selected points and split strokes into segments"},
      {GP_DELETEOP_STROKES, "STROKES", 0, "Strokes", "Delete selected strokes"},
      {GP_DELETEOP_FRAME, "FRAME", 0, "Frame", "Delete active frame"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Delete";
  ot->idname = "GPENCIL_OT_delete";
  ot->description = "Delete selected Grease Pencil strokes, vertices, or frames";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = gpencil_delete_exec;
  ot->poll = gpencil_stroke_edit_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  /* props */
  ot->prop = RNA_def_enum(ot->srna,
                          "type",
                          prop_gpencil_delete_types,
                          0,
                          "Type",
                          "Method used for deleting Grease Pencil data");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dissolve Operator
 * \{ */

static int gpencil_dissolve_exec(bContext *C, wmOperator *op)
{
  eGP_DissolveMode mode = eGP_DissolveMode(RNA_enum_get(op->ptr, "type"));

  return gpencil_dissolve_selected_points(C, mode);
}

void GPENCIL_OT_dissolve(wmOperatorType *ot)
{
  static EnumPropertyItem prop_gpencil_dissolve_types[] = {
      {GP_DISSOLVE_POINTS, "POINTS", 0, "Dissolve", "Dissolve selected points"},
      {GP_DISSOLVE_BETWEEN,
       "BETWEEN",
       0,
       "Dissolve Between",
       "Dissolve points between selected points"},
      {GP_DISSOLVE_UNSELECT, "UNSELECT", 0, "Dissolve Unselect", "Dissolve all unselected points"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Dissolve";
  ot->idname = "GPENCIL_OT_dissolve";
  ot->description = "Delete selected points without splitting strokes";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = gpencil_dissolve_exec;
  ot->poll = gpencil_stroke_edit_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  /* props */
  ot->prop = RNA_def_enum(ot->srna,
                          "type",
                          prop_gpencil_dissolve_types,
                          0,
                          "Type",
                          "Method used for dissolving stroke points");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snapping Selection to Grid Operator
 * \{ */

/* Poll callback for snap operators */
/* NOTE: For now, we only allow these in the 3D view, as other editors do not
 *       define a cursor or grid-step which can be used.
 */
static bool gpencil_snap_poll(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  Object *ob = CTX_data_active_object(C);

  return (ob != nullptr) && (ob->type == OB_GPENCIL_LEGACY) &&
         ((area != nullptr) && (area->spacetype == SPACE_VIEW3D));
}

static int gpencil_snap_to_grid_exec(bContext *C, wmOperator * /*op*/)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  ARegion *region = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *obact = CTX_data_active_object(C);
  const float gridf = ED_view3d_grid_view_scale(scene, v3d, region, nullptr);
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));

  bool changed = false;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* only editable and visible layers are considered */
    if (BKE_gpencil_layer_is_editable(gpl) && (gpl->actframe != nullptr)) {
      bGPDframe *gpf = gpl->actframe;
      float diff_mat[4][4];

      /* calculate difference matrix object */
      BKE_gpencil_layer_transform_matrix_get(depsgraph, obact, gpl, diff_mat);

      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        /* skip strokes that are invalid for current view */
        if (ED_gpencil_stroke_can_use(C, gps) == false) {
          continue;
        }
        /* check if the color is editable */
        if (ED_gpencil_stroke_material_editable(obact, gpl, gps) == false) {
          continue;
        }

        if (is_curve_edit) {
          if (gps->editcurve == nullptr) {
            continue;
          }
          float inv_diff_mat[4][4];
          invert_m4_m4_safe(inv_diff_mat, diff_mat);

          bGPDcurve *gpc = gps->editcurve;
          for (int i = 0; i < gpc->tot_curve_points; i++) {
            bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
            BezTriple *bezt = &gpc_pt->bezt;
            if (gpc_pt->flag & GP_CURVE_POINT_SELECT) {
              float tmp0[3], tmp1[3], tmp2[3], offset[3];
              mul_v3_m4v3(tmp0, diff_mat, bezt->vec[0]);
              mul_v3_m4v3(tmp1, diff_mat, bezt->vec[1]);
              mul_v3_m4v3(tmp2, diff_mat, bezt->vec[2]);

              /* calculate the offset vector */
              offset[0] = gridf * floorf(0.5f + tmp1[0] / gridf) - tmp1[0];
              offset[1] = gridf * floorf(0.5f + tmp1[1] / gridf) - tmp1[1];
              offset[2] = gridf * floorf(0.5f + tmp1[2] / gridf) - tmp1[2];

              /* shift bezTriple */
              add_v3_v3(bezt->vec[0], offset);
              add_v3_v3(bezt->vec[1], offset);
              add_v3_v3(bezt->vec[2], offset);

              mul_v3_m4v3(tmp0, inv_diff_mat, bezt->vec[0]);
              mul_v3_m4v3(tmp1, inv_diff_mat, bezt->vec[1]);
              mul_v3_m4v3(tmp2, inv_diff_mat, bezt->vec[2]);
              copy_v3_v3(bezt->vec[0], tmp0);
              copy_v3_v3(bezt->vec[1], tmp1);
              copy_v3_v3(bezt->vec[2], tmp2);

              changed = true;
            }
          }

          if (changed) {
            BKE_gpencil_editcurve_recalculate_handles(gps);
            gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
            BKE_gpencil_stroke_geometry_update(gpd, gps);
          }
        }
        else {
          /* TODO: if entire stroke is selected, offset entire stroke by same amount? */
          for (int i = 0; i < gps->totpoints; i++) {
            bGPDspoint *pt = &gps->points[i];
            /* only if point is selected */
            if (pt->flag & GP_SPOINT_SELECT) {
              /* apply parent transformations */
              float fpt[3];
              mul_v3_m4v3(fpt, diff_mat, &pt->x);

              fpt[0] = gridf * floorf(0.5f + fpt[0] / gridf);
              fpt[1] = gridf * floorf(0.5f + fpt[1] / gridf);
              fpt[2] = gridf * floorf(0.5f + fpt[2] / gridf);

              /* return data */
              copy_v3_v3(&pt->x, fpt);
              gpencil_world_to_object_space_point(depsgraph, obact, gpl, pt);

              changed = true;
            }
          }
        }
      }
    }
  }

  if (changed) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    DEG_id_tag_update(&obact->id, ID_RECALC_COPY_ON_WRITE);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_snap_to_grid(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Selection to Grid";
  ot->idname = "GPENCIL_OT_snap_to_grid";
  ot->description = "Snap selected points to the nearest grid points";

  /* callbacks */
  ot->exec = gpencil_snap_to_grid_exec;
  ot->poll = gpencil_snap_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snapping Selection to Cursor Operator
 * \{ */

static int gpencil_snap_to_cursor_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *obact = CTX_data_active_object(C);

  const bool use_offset = RNA_boolean_get(op->ptr, "use_offset");
  const float *cursor_global = scene->cursor.location;

  bool changed = false;
  if (is_curve_edit) {
    BKE_report(op->reports, RPT_ERROR, "Not implemented!");
  }
  else {
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
      /* only editable and visible layers are considered */
      if (BKE_gpencil_layer_is_editable(gpl) && (gpl->actframe != nullptr)) {
        bGPDframe *gpf = gpl->actframe;
        float diff_mat[4][4];

        /* calculate difference matrix */
        BKE_gpencil_layer_transform_matrix_get(depsgraph, obact, gpl, diff_mat);

        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          bGPDspoint *pt;
          int i;

          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }
          /* check if the color is editable */
          if (ED_gpencil_stroke_material_editable(obact, gpl, gps) == false) {
            continue;
          }
          /* only continue if this stroke is selected (editable doesn't guarantee this)... */
          if ((gps->flag & GP_STROKE_SELECT) == 0) {
            continue;
          }

          if (use_offset) {
            float offset[3];

            /* compute offset from first point of stroke to cursor */
            /* TODO: Allow using midpoint instead? */
            sub_v3_v3v3(offset, cursor_global, &gps->points->x);

            /* apply offset to all points in the stroke */
            for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
              add_v3_v3(&pt->x, offset);
            }

            changed = true;
          }
          else {
            /* affect each selected point */
            for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
              if (pt->flag & GP_SPOINT_SELECT) {
                copy_v3_v3(&pt->x, cursor_global);
                gpencil_world_to_object_space_point(depsgraph, obact, gpl, pt);

                changed = true;
              }
            }
          }
        }
      }
    }
  }

  if (changed) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    DEG_id_tag_update(&obact->id, ID_RECALC_COPY_ON_WRITE);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_snap_to_cursor(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Selection to Cursor";
  ot->idname = "GPENCIL_OT_snap_to_cursor";
  ot->description = "Snap selected points/strokes to the cursor";

  /* callbacks */
  ot->exec = gpencil_snap_to_cursor_exec;
  ot->poll = gpencil_snap_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  ot->prop = RNA_def_boolean(ot->srna,
                             "use_offset",
                             true,
                             "With Offset",
                             "Offset the entire stroke instead of selected points only");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snapping Cursor to Selection Operator
 * \{ */

static bool gpencil_stroke_points_centroid(Depsgraph *depsgraph,
                                           bContext *C,
                                           Object *obact,
                                           bGPdata *gpd,
                                           float r_centroid[3],
                                           float r_min[3],
                                           float r_max[3],
                                           size_t *count)
{
  bool changed = false;
  /* calculate midpoints from selected points */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* only editable and visible layers are considered */
    if (BKE_gpencil_layer_is_editable(gpl) && (gpl->actframe != nullptr)) {
      bGPDframe *gpf = gpl->actframe;
      float diff_mat[4][4];

      /* calculate difference matrix */
      BKE_gpencil_layer_transform_matrix_get(depsgraph, obact, gpl, diff_mat);

      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        bGPDspoint *pt;
        int i;

        /* skip strokes that are invalid for current view */
        if (ED_gpencil_stroke_can_use(C, gps) == false) {
          continue;
        }
        /* check if the color is editable */
        if (ED_gpencil_stroke_material_editable(obact, gpl, gps) == false) {
          continue;
        }
        /* only continue if this stroke is selected (editable doesn't guarantee this)... */
        if ((gps->flag & GP_STROKE_SELECT) == 0) {
          continue;
        }

        for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
          if (pt->flag & GP_SPOINT_SELECT) {
            /* apply parent transformations */
            float fpt[3];
            mul_v3_m4v3(fpt, diff_mat, &pt->x);

            add_v3_v3(r_centroid, fpt);
            minmax_v3v3_v3(r_min, r_max, fpt);

            (*count)++;
          }
        }

        changed = true;
      }
    }
  }

  return changed;
}

static int gpencil_snap_cursor_to_sel_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *obact = CTX_data_active_object(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));

  Scene *scene = CTX_data_scene(C);

  float *cursor = scene->cursor.location;
  float centroid[3] = {0.0f};
  float min[3], max[3];
  size_t count = 0;

  INIT_MINMAX(min, max);

  bool changed = false;
  if (is_curve_edit) {
    BKE_report(op->reports, RPT_ERROR, "Not implemented!");
  }
  else {
    changed = gpencil_stroke_points_centroid(depsgraph, C, obact, gpd, centroid, min, max, &count);
  }

  if (changed) {
    if (scene->toolsettings->transform_pivot_point == V3D_AROUND_CENTER_BOUNDS) {
      mid_v3_v3v3(cursor, min, max);
    }
    else { /* #V3D_AROUND_CENTER_MEDIAN. */
      zero_v3(cursor);
      if (count) {
        mul_v3_fl(centroid, 1.0f / float(count));
        copy_v3_v3(cursor, centroid);
      }
    }

    DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_snap_cursor_to_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Cursor to Selected Points";
  ot->idname = "GPENCIL_OT_snap_cursor_to_selected";
  ot->description = "Snap cursor to center of selected points";

  /* callbacks */
  ot->exec = gpencil_snap_cursor_to_sel_exec;
  ot->poll = gpencil_snap_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Apply Layer Thickness Change to Strokes Operator
 * \{ */

static int gpencil_stroke_apply_thickness_exec(bContext *C, wmOperator * /*op*/)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);

  /* sanity checks */
  if (ELEM(nullptr, gpd, gpl, gpl->frames.first)) {
    return OPERATOR_CANCELLED;
  }

  /* loop all strokes */
  LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
      /* Apply thickness */
      if ((gps->thickness == 0) && (gpl->line_change == 0)) {
        gps->thickness = gpl->thickness;
      }
      else {
        gps->thickness = gps->thickness + gpl->line_change;
      }
    }
  }

  /* clear value */
  gpl->thickness = 0.0f;
  gpl->line_change = 0;

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_apply_thickness(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Apply Stroke Thickness";
  ot->idname = "GPENCIL_OT_stroke_apply_thickness";
  ot->description = "Apply the thickness change of the layer to its strokes";

  /* api callbacks */
  ot->exec = gpencil_stroke_apply_thickness_exec;
  ot->poll = gpencil_active_layer_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Toggle Cyclic Operator
 * \{ */

enum {
  GP_STROKE_CYCLIC_CLOSE = 1,
  GP_STROKE_CYCLIC_OPEN = 2,
  GP_STROKE_CYCLIC_TOGGLE = 3,
};

static int gpencil_stroke_cyclical_set_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  Object *ob = CTX_data_active_object(C);

  const int type = RNA_enum_get(op->ptr, "type");
  const bool geometry = RNA_boolean_get(op->ptr, "geometry");
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));

  /* sanity checks */
  if (ELEM(nullptr, gpd)) {
    return OPERATOR_CANCELLED;
  }

  bool changed = false;
  /* loop all selected strokes */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == nullptr) {
          continue;
        }

        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);
          /* skip strokes that are not selected or invalid for current view */
          if (((gps->flag & GP_STROKE_SELECT) == 0) || ED_gpencil_stroke_can_use(C, gps) == false)
          {
            continue;
          }
          /* skip hidden or locked colors */
          if (!gp_style || (gp_style->flag & GP_MATERIAL_HIDE) ||
              (gp_style->flag & GP_MATERIAL_LOCKED))
          {
            continue;
          }

          eGPDstroke_Flag before = eGPDstroke_Flag(gps->flag & GP_STROKE_CYCLIC);
          switch (type) {
            case GP_STROKE_CYCLIC_CLOSE:
              /* Close all (enable) */
              gps->flag |= GP_STROKE_CYCLIC;
              break;
            case GP_STROKE_CYCLIC_OPEN:
              /* Open all (disable) */
              gps->flag &= ~GP_STROKE_CYCLIC;
              break;
            case GP_STROKE_CYCLIC_TOGGLE:
              /* Just toggle flag... */
              gps->flag ^= GP_STROKE_CYCLIC;
              break;
            default:
              BLI_assert(0);
              break;
          }

          if (before != eGPDstroke_Flag(gps->flag & GP_STROKE_CYCLIC)) {
            /* Create new geometry. */
            if (is_curve_edit) {
              BKE_gpencil_editcurve_recalculate_handles(gps);
              gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
              BKE_gpencil_stroke_geometry_update(gpd, gps);
            }
            else if ((gps->flag & GP_STROKE_CYCLIC) && geometry) {
              BKE_gpencil_stroke_close(gps);
              BKE_gpencil_stroke_geometry_update(gpd, gps);
            }

            changed = true;
          }
        }

        /* If not multi-edit, exit loop. */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }
  CTX_DATA_END;

  if (changed) {
    /* notifiers */
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

static bool gpencil_cyclical_set_curve_edit_poll_property(const bContext *C,
                                                          wmOperator * /*op*/,
                                                          const PropertyRNA *prop)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  if (gpd != nullptr && GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd)) {
    const char *prop_id = RNA_property_identifier(prop);
    /* Only show type in curve edit mode */
    if (!STREQ(prop_id, "type")) {
      return false;
    }
  }

  return true;
}

void GPENCIL_OT_stroke_cyclical_set(wmOperatorType *ot)
{
  PropertyRNA *prop;

  static const EnumPropertyItem cyclic_type[] = {
      {GP_STROKE_CYCLIC_CLOSE, "CLOSE", 0, "Close All", ""},
      {GP_STROKE_CYCLIC_OPEN, "OPEN", 0, "Open All", ""},
      {GP_STROKE_CYCLIC_TOGGLE, "TOGGLE", 0, "Toggle", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Set Cyclical State";
  ot->idname = "GPENCIL_OT_stroke_cyclical_set";
  ot->description = "Close or open the selected stroke adding a segment from last to first point";

  /* api callbacks */
  ot->exec = gpencil_stroke_cyclical_set_exec;
  ot->poll = gpencil_active_layer_poll;
  ot->poll_property = gpencil_cyclical_set_curve_edit_poll_property;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", cyclic_type, GP_STROKE_CYCLIC_TOGGLE, "Type", "");
  prop = RNA_def_boolean(
      ot->srna, "geometry", false, "Create Geometry", "Create new geometry for closing stroke");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Toggle Flat Caps Operator
 * \{ */

enum {
  GP_STROKE_CAPS_TOGGLE_BOTH = 0,
  GP_STROKE_CAPS_TOGGLE_START = 1,
  GP_STROKE_CAPS_TOGGLE_END = 2,
  GP_STROKE_CAPS_TOGGLE_DEFAULT = 3,
};

static int gpencil_stroke_caps_set_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  Object *ob = CTX_data_active_object(C);
  const int type = RNA_enum_get(op->ptr, "type");
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));

  /* sanity checks */
  if (ELEM(nullptr, gpd)) {
    return OPERATOR_CANCELLED;
  }

  bool changed = false;
  /* loop all selected strokes */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == nullptr) {
          continue;
        }

        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);

          /* skip strokes that are not selected or invalid for current view */
          if (((gps->flag & GP_STROKE_SELECT) == 0) ||
              (ED_gpencil_stroke_can_use(C, gps) == false))
          {
            continue;
          }
          /* skip hidden or locked colors */
          if (!gp_style || (gp_style->flag & GP_MATERIAL_HIDE) ||
              (gp_style->flag & GP_MATERIAL_LOCKED))
          {
            continue;
          }

          short prev_first = gps->caps[0];
          short prev_last = gps->caps[1];

          if (ELEM(type, GP_STROKE_CAPS_TOGGLE_BOTH, GP_STROKE_CAPS_TOGGLE_START)) {
            ++gps->caps[0];
            if (gps->caps[0] >= GP_STROKE_CAP_MAX) {
              gps->caps[0] = GP_STROKE_CAP_ROUND;
            }
          }
          if (ELEM(type, GP_STROKE_CAPS_TOGGLE_BOTH, GP_STROKE_CAPS_TOGGLE_END)) {
            ++gps->caps[1];
            if (gps->caps[1] >= GP_STROKE_CAP_MAX) {
              gps->caps[1] = GP_STROKE_CAP_ROUND;
            }
          }
          if (type == GP_STROKE_CAPS_TOGGLE_DEFAULT) {
            gps->caps[0] = GP_STROKE_CAP_ROUND;
            gps->caps[1] = GP_STROKE_CAP_ROUND;
          }

          if (prev_first != gps->caps[0] || prev_last != gps->caps[1]) {
            changed = true;
          }
        }
        /* If not multi-edit, exit loop. */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }
  CTX_DATA_END;

  if (changed) {
    /* notifiers */
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_caps_set(wmOperatorType *ot)
{
  static const EnumPropertyItem toggle_type[] = {
      {GP_STROKE_CAPS_TOGGLE_BOTH, "TOGGLE", 0, "Both", ""},
      {GP_STROKE_CAPS_TOGGLE_START, "START", 0, "Start", ""},
      {GP_STROKE_CAPS_TOGGLE_END, "END", 0, "End", ""},
      {GP_STROKE_CAPS_TOGGLE_DEFAULT, "DEFAULT", 0, "Default", "Set as default rounded"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Set Caps Mode";
  ot->idname = "GPENCIL_OT_stroke_caps_set";
  ot->description = "Change stroke caps mode (rounded or flat)";

  /* api callbacks */
  ot->exec = gpencil_stroke_caps_set_exec;
  ot->poll = gpencil_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", toggle_type, GP_STROKE_CAPS_TOGGLE_BOTH, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Join Operator
 * \{ */

struct tJoinStrokes {
  bGPDframe *gpf;
  bGPDstroke *gps;
  bool used;
};

static int gpencil_get_nearest_stroke_index(tJoinStrokes *strokes_list,
                                            const bGPDstroke *gps,
                                            const int totstrokes)
{
  int index = -1;
  float min_dist = FLT_MAX;
  float dist, start_a[3], end_a[3], start_b[3], end_b[3];

  bGPDspoint *pt = &gps->points[0];
  copy_v3_v3(start_a, &pt->x);

  pt = &gps->points[gps->totpoints - 1];
  copy_v3_v3(end_a, &pt->x);

  for (int i = 0; i < totstrokes; i++) {
    tJoinStrokes *elem = &strokes_list[i];
    if (elem->used) {
      continue;
    }
    pt = &elem->gps->points[0];
    copy_v3_v3(start_b, &pt->x);

    pt = &elem->gps->points[elem->gps->totpoints - 1];
    copy_v3_v3(end_b, &pt->x);

    dist = len_squared_v3v3(start_a, start_b);
    if (dist < min_dist) {
      min_dist = dist;
      index = i;
    }
    dist = len_squared_v3v3(start_a, end_b);
    if (dist < min_dist) {
      min_dist = dist;
      index = i;
    }
    dist = len_squared_v3v3(end_a, start_b);
    if (dist < min_dist) {
      min_dist = dist;
      index = i;
    }
    dist = len_squared_v3v3(end_a, end_b);
    if (dist < min_dist) {
      min_dist = dist;
      index = i;
    }
  }

  return index;
}

static int gpencil_stroke_join_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bGPDlayer *activegpl = BKE_gpencil_layer_active_get(gpd);
  Object *ob = CTX_data_active_object(C);
  /* Limit the number of strokes to join. It makes no sense to allow an very high number of
   * strokes for CPU time and because to have a stroke with thousands of points is unpractical,
   * so limit this number avoid to joining a full frame scene in one single stroke. */
  const int max_join_strokes = 128;

  const int type = RNA_enum_get(op->ptr, "type");
  const bool leave_gaps = RNA_boolean_get(op->ptr, "leave_gaps");

  /* sanity checks */
  if (ELEM(nullptr, gpd)) {
    return OPERATOR_CANCELLED;
  }

  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));
  if (is_curve_edit) {
    return OPERATOR_CANCELLED;
  }

  if (activegpl->flag & GP_LAYER_LOCKED) {
    return OPERATOR_CANCELLED;
  }

  BLI_assert(ELEM(type, GP_STROKE_JOIN, GP_STROKE_JOINCOPY));

  int tot_strokes = 0;
  /** Alloc memory. */
  tJoinStrokes *strokes_list = static_cast<tJoinStrokes *>(
      MEM_malloc_arrayN(max_join_strokes, sizeof(tJoinStrokes), __func__));
  tJoinStrokes *elem = nullptr;
  /* Read all selected strokes to create a list. */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *gpf = gpl->actframe;
    if (gpf == nullptr) {
      continue;
    }

    /* Add all stroke selected of the frame. */
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
      if (gps->flag & GP_STROKE_SELECT) {
        /* skip strokes that are invalid for current view */
        if (ED_gpencil_stroke_can_use(C, gps) == false) {
          continue;
        }
        /* check if the color is editable. */
        if (ED_gpencil_stroke_material_editable(ob, gpl, gps) == false) {
          continue;
        }
        elem = &strokes_list[tot_strokes];
        elem->gpf = gpf;
        elem->gps = gps;
        elem->used = false;

        tot_strokes++;
        /* Limit the number of strokes. */
        if (tot_strokes == max_join_strokes) {
          BKE_reportf(op->reports,
                      RPT_WARNING,
                      "Too many strokes selected, only joined first %d strokes",
                      max_join_strokes);
          break;
        }
      }
    }
  }
  CTX_DATA_END;

  /* Nothing to join. */
  if (tot_strokes < 2) {
    MEM_SAFE_FREE(strokes_list);
    return OPERATOR_CANCELLED;
  }

  /* Take first stroke. */
  elem = &strokes_list[0];
  elem->used = true;

  /* Create a new stroke. */
  bGPDstroke *gps_new = BKE_gpencil_stroke_duplicate(elem->gps, true, true);
  gps_new->flag &= ~GP_STROKE_CYCLIC;
  BLI_insertlinkbefore(&elem->gpf->strokes, elem->gps, gps_new);

  /* Join all strokes until the list is completed. */
  while (true) {
    int i = gpencil_get_nearest_stroke_index(strokes_list, gps_new, tot_strokes);
    if (i < 0) {
      break;
    }
    elem = &strokes_list[i];
    /* Join new_stroke and stroke B. */
    BKE_gpencil_stroke_join(gps_new, elem->gps, leave_gaps, true, false, true);
    elem->used = true;
  }

  /* Calc geometry data for new stroke. */
  BKE_gpencil_stroke_geometry_update(gpd, gps_new);

  /* If join only, delete old strokes. */
  if (type == GP_STROKE_JOIN) {
    for (int i = 0; i < tot_strokes; i++) {
      elem = &strokes_list[i];
      BLI_remlink(&elem->gpf->strokes, elem->gps);
      BKE_gpencil_free_stroke(elem->gps);
    }
  }

  /* Free memory. */
  MEM_SAFE_FREE(strokes_list);

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_join(wmOperatorType *ot)
{
  static const EnumPropertyItem join_type[] = {
      {GP_STROKE_JOIN, "JOIN", 0, "Join", ""},
      {GP_STROKE_JOINCOPY, "JOINCOPY", 0, "Join and Copy", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Join Strokes";
  ot->idname = "GPENCIL_OT_stroke_join";
  ot->description = "Join selected strokes (optionally as new stroke)";

  /* api callbacks */
  ot->exec = gpencil_stroke_join_exec;
  ot->poll = gpencil_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", join_type, GP_STROKE_JOIN, "Type", "");
  RNA_def_boolean(ot->srna,
                  "leave_gaps",
                  false,
                  "Leave Gaps",
                  "Leave gaps between joined strokes instead of linking them");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Flip Operator
 * \{ */

static int gpencil_stroke_flip_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  Object *ob = CTX_data_active_object(C);

  /* sanity checks */
  if (ELEM(nullptr, gpd)) {
    return OPERATOR_CANCELLED;
  }

  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));

  bool changed = false;
  /* Read all selected strokes. */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == nullptr) {
          continue;
        }
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          if (gps->flag & GP_STROKE_SELECT) {
            /* skip strokes that are invalid for current view */
            if (ED_gpencil_stroke_can_use(C, gps) == false) {
              continue;
            }
            /* check if the color is editable */
            if (ED_gpencil_stroke_material_editable(ob, gpl, gps) == false) {
              continue;
            }

            if (is_curve_edit) {
              BKE_report(op->reports, RPT_ERROR, "Not implemented!");
            }
            else {
              /* Flip stroke. */
              BKE_gpencil_stroke_flip(gps);
              changed = true;
            }
          }
        }
      }
      /* If not multi-edit, exit loop. */
      if (!is_multiedit) {
        break;
      }
    }
  }
  CTX_DATA_END;

  if (changed) {
    /* notifiers */
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_flip(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Flip Stroke";
  ot->idname = "GPENCIL_OT_stroke_flip";
  ot->description = "Change direction of the points of the selected strokes";

  /* api callbacks */
  ot->exec = gpencil_stroke_flip_exec;
  ot->poll = gpencil_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Start Set Operator
 * \{ */

static int gpencil_stroke_start_set_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);

  /* sanity checks */
  if (ELEM(nullptr, ob, gpd)) {
    return OPERATOR_CANCELLED;
  }

  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));
  if (is_curve_edit) {
    BKE_report(op->reports, RPT_ERROR, "Curve Edit mode not supported");
    return OPERATOR_CANCELLED;
  }

  bool changed = false;
  /* Read all selected strokes. */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == nullptr) {
          continue;
        }
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          if (gps->flag & GP_STROKE_SELECT) {
            /* skip strokes that are invalid for current view */
            if (ED_gpencil_stroke_can_use(C, gps) == false) {
              continue;
            }
            /* check if the color is editable */
            if (ED_gpencil_stroke_material_editable(ob, gpl, gps) == false) {
              continue;
            }

            /* Only cyclic strokes. */
            if ((gps->flag & GP_STROKE_CYCLIC) == 0) {
              continue;
            }

            /* Find first selected point and set start. */
            bGPDspoint *pt;
            for (int i = 0; i < gps->totpoints; i++) {
              pt = &gps->points[i];
              if (pt->flag & GP_SPOINT_SELECT) {
                if (i == gps->totpoints - 1) {
                  BKE_gpencil_stroke_flip(gps);
                }
                else {
                  BKE_gpencil_stroke_start_set(gps, i);
                }
                BKE_gpencil_stroke_geometry_update(gpd, gps);
                changed = true;
                break;
              }
            }
          }
        }
      }
      /* If not multi-edit, exit loop. */
      if (!is_multiedit) {
        break;
      }
    }
  }
  CTX_DATA_END;

  if (changed) {
    /* notifiers */
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_start_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Start Point";
  ot->idname = "GPENCIL_OT_stroke_start_set";
  ot->description = "Set start point for cyclic strokes";

  /* api callbacks */
  ot->exec = gpencil_stroke_start_set_exec;
  ot->poll = gpencil_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Re-project Operator
 * \{ */

static int gpencil_strokes_reproject_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  int oldframe = int(DEG_get_ctime(depsgraph));
  const eGP_ReprojectModes mode = eGP_ReprojectModes(RNA_enum_get(op->ptr, "type"));
  const bool keep_original = RNA_boolean_get(op->ptr, "keep_original");
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  const float offset = RNA_float_get(op->ptr, "offset");

  /* Init snap context for geometry projection. */
  SnapObjectContext *sctx = nullptr;
  sctx = ED_transform_snap_object_context_create(scene, 0);

  bool changed = false;
  /* Init space conversion stuff. */
  GP_SpaceConversion gsc = {nullptr};
  gpencil_point_conversion_init(C, &gsc);
  int cfra_prv = INT_MIN;

  /* Go through each editable + selected stroke, adjusting each of its points one by one... */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == nullptr) {
          continue;
        }
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }
          bool curve_select = false;
          if (is_curve_edit && gps->editcurve != nullptr) {
            curve_select = gps->editcurve->flag & GP_CURVE_SELECT;
          }

          if (gps->flag & GP_STROKE_SELECT || curve_select) {

            /* update frame to get the new location of objects */
            if ((mode == GP_REPROJECT_SURFACE) && (cfra_prv != gpf->framenum)) {
              cfra_prv = gpf->framenum;
              scene->r.cfra = gpf->framenum;
              BKE_scene_graph_update_for_newframe(depsgraph);
            }

            ED_gpencil_stroke_reproject(
                depsgraph, &gsc, sctx, gpl, gpf, gps, mode, keep_original, offset);

            if (is_curve_edit && gps->editcurve != nullptr) {
              BKE_gpencil_stroke_editcurve_update(gpd, gpl, gps);
              /* Update the selection from the stroke to the curve. */
              BKE_gpencil_editcurve_stroke_sync_selection(gpd, gps, gps->editcurve);

              gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
              BKE_gpencil_stroke_geometry_update(gpd, gps);
            }

            changed = true;
          }
        }
      }
      /* If not multi-edit, exit loop. */
      if (!is_multiedit) {
        break;
      }
    }
  }
  CTX_DATA_END;

  /* return frame state and DB to original state */
  scene->r.cfra = oldframe;
  BKE_scene_graph_update_for_newframe(depsgraph);

  if (sctx != nullptr) {
    ED_transform_snap_object_context_destroy(sctx);
  }

  if (changed) {
    /* update changed data */
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

static void gpencil_strokes_reproject_ui(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;
  uiLayout *row;

  const eGP_ReprojectModes type = eGP_ReprojectModes(RNA_enum_get(op->ptr, "type"));

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  row = uiLayoutRow(layout, true);
  uiItemR(row, op->ptr, "type", UI_ITEM_NONE, nullptr, ICON_NONE);

  if (type == GP_REPROJECT_SURFACE) {
    row = uiLayoutRow(layout, true);
    uiItemR(row, op->ptr, "offset", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  row = uiLayoutRow(layout, true);
  uiItemR(row, op->ptr, "keep_original", UI_ITEM_NONE, nullptr, ICON_NONE);
}

void GPENCIL_OT_reproject(wmOperatorType *ot)
{
  PropertyRNA *prop;
  static const EnumPropertyItem reproject_type[] = {
      {GP_REPROJECT_FRONT, "FRONT", 0, "Front", "Reproject the strokes using the X-Z plane"},
      {GP_REPROJECT_SIDE, "SIDE", 0, "Side", "Reproject the strokes using the Y-Z plane"},
      {GP_REPROJECT_TOP, "TOP", 0, "Top", "Reproject the strokes using the X-Y plane"},
      {GP_REPROJECT_VIEW,
       "VIEW",
       0,
       "View",
       "Reproject the strokes to end up on the same plane, as if drawn from the current "
       "viewpoint "
       "using 'Cursor' Stroke Placement"},
      {GP_REPROJECT_SURFACE,
       "SURFACE",
       0,
       "Surface",
       "Reproject the strokes on to the scene geometry, as if drawn using 'Surface' placement"},
      {GP_REPROJECT_CURSOR,
       "CURSOR",
       0,
       "Cursor",
       "Reproject the strokes using the orientation of 3D cursor"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Reproject Strokes";
  ot->idname = "GPENCIL_OT_reproject";
  ot->description =
      "Reproject the selected strokes from the current viewpoint as if they had been newly "
      "drawn "
      "(e.g. to fix problems from accidental 3D cursor movement or accidental viewport changes, "
      "or for matching deforming geometry)";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = gpencil_strokes_reproject_exec;
  ot->poll = gpencil_strokes_edit3d_poll;
  ot->ui = gpencil_strokes_reproject_ui;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(
      ot->srna, "type", reproject_type, GP_REPROJECT_VIEW, "Projection Type", "");

  prop = RNA_def_boolean(ot->srna,
                         "keep_original",
                         false,
                         "Keep Original",
                         "Keep original strokes and create a copy before reprojecting");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MOVIECLIP);

  RNA_def_float(ot->srna, "offset", 0.0f, 0.0f, 10.0f, "Surface Offset", "", 0.0f, 10.0f);
}

static int gpencil_recalc_geometry_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = CTX_data_active_object(C);
  if ((ob == nullptr) || (ob->type != OB_GPENCIL_LEGACY)) {
    return OPERATOR_CANCELLED;
  }

  bGPdata *gpd = (bGPdata *)ob->data;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        BKE_gpencil_stroke_geometry_update(gpd, gps);
      }
    }
  }

  /* update changed data */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  return OPERATOR_FINISHED;
}

/* -------------------------------------------------------------------- */
/** \name Stroke Perimeter from View Operator
 * \{ */

enum {
  GP_PERIMETER_VIEW = 0,
  GP_PERIMETER_FRONT = 1,
  GP_PERIMETER_SIDE = 2,
  GP_PERIMETER_TOP = 3,
  GP_PERIMETER_CAMERA = 4,
};

enum {
  GP_STROKE_USE_ACTIVE_MATERIAL = 0,
  GP_STROKE_USE_CURRENT_MATERIAL = 1,
  GP_STROKE_USE_NEW_MATERIAL = 2,
};

static int gpencil_stroke_outline_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = (bGPdata *)ob->data;
  const int subdivisions = RNA_int_get(op->ptr, "subdivisions");
  const float length = RNA_float_get(op->ptr, "length");
  const bool keep = RNA_boolean_get(op->ptr, "keep");
  const int thickness = RNA_int_get(op->ptr, "thickness");

  const int view_mode = RNA_enum_get(op->ptr, "view_mode");
  const int material_mode = RNA_enum_get(op->ptr, "material_mode");
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));

  /* sanity checks */
  if (ELEM(nullptr, gpd)) {
    return OPERATOR_CANCELLED;
  }

  bool changed = false;

  float viewmat[4][4];
  copy_m4_m4(viewmat, rv3d->viewmat);

  switch (view_mode) {
    case GP_PERIMETER_FRONT:
      unit_m4(rv3d->viewmat);
      viewmat[1][1] = 0.0f;
      viewmat[1][2] = -1.0f;

      viewmat[2][1] = 1.0f;
      viewmat[2][2] = 0.0f;

      viewmat[3][2] = -10.0f;
      break;
    case GP_PERIMETER_SIDE:
      zero_m4(viewmat);
      viewmat[0][2] = 1.0f;
      viewmat[1][0] = 1.0f;
      viewmat[2][1] = 1.0f;
      viewmat[3][3] = 1.0f;
      break;
    case GP_PERIMETER_TOP:
      unit_m4(viewmat);
      break;
    case GP_PERIMETER_CAMERA: {
      Scene *scene = CTX_data_scene(C);
      Object *cam_ob = scene->camera;
      if (cam_ob != nullptr) {
        invert_m4_m4(viewmat, cam_ob->object_to_world);
      }
      break;
    }
    default:
      break;
  }

  /* Untag strokes to be sure nothing is pending. */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        gps->flag &= ~GP_STROKE_TAG;
      }
    }
  }
  /* Create a new material. */
  int mat_idx = 0;
  if (material_mode == GP_STROKE_USE_NEW_MATERIAL) {
    Material *ma = BKE_gpencil_object_material_new(bmain, ob, "Material", nullptr);
    MaterialGPencilStyle *gp_style = ma->gp_style;

    gp_style->flag |= GP_MATERIAL_FILL_SHOW;
    mat_idx = ob->totcol - 1;
  }

  /* loop all selected strokes */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (gpl->flag & GP_LAYER_HIDE) {
      continue;
    }
    /* Prepare transform matrix. */
    float diff_mat[4][4];
    BKE_gpencil_layer_transform_matrix_get(depsgraph, ob, gpl, diff_mat);

    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == nullptr) {
          continue;
        }

        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          if ((gps->flag & GP_STROKE_SELECT) == 0) {
            continue;
          }
          if (gps->totpoints == 0) {
            continue;
          }
          if (!ED_gpencil_stroke_material_visible(ob, gps)) {
            continue;
          }

          MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);
          const bool is_stroke = ((gp_style->flag & GP_MATERIAL_STROKE_SHOW) != 0);

          if (!is_stroke) {
            continue;
          }

          /* Duplicate the stroke to apply any layer thickness change. */
          bGPDstroke *gps_duplicate = BKE_gpencil_stroke_duplicate(gps, true, false);

          /* Apply layer thickness change. */
          gps_duplicate->thickness += gpl->line_change;
          /* Apply object scale to thickness. */
          gps_duplicate->thickness *= mat4_to_scale(ob->object_to_world);
          CLAMP_MIN(gps_duplicate->thickness, 1.0f);

          /* Stroke. */
          const float ovr_thickness = keep ? thickness : 0.0f;
          bGPDstroke *gps_perimeter = BKE_gpencil_stroke_perimeter_from_view(
              viewmat, gpd, gpl, gps_duplicate, subdivisions, diff_mat, ovr_thickness);
          gps_perimeter->flag &= ~GP_STROKE_SELECT;
          /* Assign material. */
          switch (material_mode) {
            case GP_STROKE_USE_ACTIVE_MATERIAL: {
              if (ob->actcol - 1 < 0) {
                gps_perimeter->mat_nr = 0;
              }
              else {
                gps_perimeter->mat_nr = ob->actcol - 1;
              }
              break;
            }
            case GP_STROKE_USE_CURRENT_MATERIAL:
              gps_perimeter->mat_nr = gps->mat_nr;
              break;
            case GP_STROKE_USE_NEW_MATERIAL:
              gps_perimeter->mat_nr = mat_idx;
              break;
            default:
              break;
          }

          /* Sample stroke. */
          if (length > 0.0f) {
            BKE_gpencil_stroke_sample(gpd, gps_perimeter, length, false, 0);
          }
          /* Set stroke thickness. */
          gps_perimeter->thickness = thickness;

          /* Set pressure constant. */
          bGPDspoint *pt;
          for (int i = 0; i < gps_perimeter->totpoints; i++) {
            pt = &gps_perimeter->points[i];
            pt->pressure = 1.0f;
          }

          /* Add perimeter stroke to frame. */
          BLI_insertlinkafter(&gpf->strokes, gps, gps_perimeter);

          /* Tag original stroke to be removed. */
          gps->flag |= GP_STROKE_TAG;

          /* Free Temp stroke. */
          BKE_gpencil_free_stroke(gps_duplicate);
          changed = true;
        }

        /* If not multi-edit, exit loop. */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }
  /* Free old strokes. */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {
        if (gps->flag & GP_STROKE_TAG) {
          BLI_remlink(&gpf->strokes, gps);
          BKE_gpencil_free_stroke(gps);
        }
      }
    }
  }

  if (changed) {
    /* notifiers */
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_outline(wmOperatorType *ot)
{
  static const EnumPropertyItem view_mode[] = {
      {GP_PERIMETER_VIEW, "VIEW", 0, "View", ""},
      {GP_PERIMETER_FRONT, "FRONT", 0, "Front", ""},
      {GP_PERIMETER_SIDE, "SIDE", 0, "Side", ""},
      {GP_PERIMETER_TOP, "TOP", 0, "Top", ""},
      {GP_PERIMETER_CAMERA, "CAMERA", 0, "Camera", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };
  static const EnumPropertyItem material_mode[] = {
      {GP_STROKE_USE_ACTIVE_MATERIAL, "ACTIVE", 0, "Active Material", ""},
      {GP_STROKE_USE_CURRENT_MATERIAL, "KEEP", 0, "Keep Material", "Keep current stroke material"},
      {GP_STROKE_USE_NEW_MATERIAL, "NEW", 0, "New Material", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Convert Stroke to Outline";
  ot->idname = "GPENCIL_OT_stroke_outline";
  ot->description = "Convert stroke to perimeter";

  /* api callbacks */
  ot->exec = gpencil_stroke_outline_exec;
  ot->poll = gpencil_stroke_edit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "view_mode", view_mode, GP_PERIMETER_VIEW, "View", "");
  RNA_def_property_translation_context(ot->prop, BLT_I18NCONTEXT_EDITOR_VIEW3D);

  RNA_def_enum(ot->srna,
               "material_mode",
               material_mode,
               GP_STROKE_USE_ACTIVE_MATERIAL,
               "Material Mode",
               "");

  RNA_def_int(ot->srna,
              "thickness",
              1,
              1,
              1000,
              "Thickness",
              "Thickness of the stroke perimeter",
              1,
              1000);
  RNA_def_boolean(ot->srna,
                  "keep",
                  true,
                  "Keep Shape",
                  "Try to keep global shape when the stroke thickness change");

  RNA_def_int(ot->srna, "subdivisions", 3, 0, 10, "Subdivisions", "", 0, 10);

  RNA_def_float(ot->srna, "length", 0.0f, 0.0f, 100.0f, "Sample Length", "", 0.0f, 100.0f);
}

/** \} */

void GPENCIL_OT_recalc_geometry(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Recalculate internal geometry";
  ot->idname = "GPENCIL_OT_recalc_geometry";
  ot->description = "Update all internal geometry data";

  /* callbacks */
  ot->exec = gpencil_recalc_geometry_exec;
  ot->poll = gpencil_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Subdivide Operator
 * \{ */

/* helper to smooth */
static void gpencil_smooth_stroke(bContext *C, wmOperator *op)
{
  const int repeat = RNA_int_get(op->ptr, "repeat");
  float factor = RNA_float_get(op->ptr, "factor");
  const bool only_selected = RNA_boolean_get(op->ptr, "only_selected");
  const bool smooth_position = RNA_boolean_get(op->ptr, "smooth_position");
  const bool smooth_thickness = RNA_boolean_get(op->ptr, "smooth_thickness");
  const bool smooth_strength = RNA_boolean_get(op->ptr, "smooth_strength");
  const bool smooth_uv = RNA_boolean_get(op->ptr, "smooth_uv");

  if (factor == 0.0f) {
    return;
  }

  GP_EDITABLE_STROKES_BEGIN (gpstroke_iter, C, gpl, gps) {
    if (gps->flag & GP_STROKE_SELECT) {
      /* TODO use `BKE_gpencil_stroke_smooth` when the weights are better used. */
      bGPDstroke gps_old = blender::dna::shallow_copy(*gps);
      gps_old.points = (bGPDspoint *)MEM_dupallocN(gps->points);
      bool need_update = false;
      /* Here the iteration needs to be done outside the smooth functions,
       * as there are points that don't get smoothed. */
      for (int n = 0; n < repeat; n++) {
        for (int i = 0; i < gps->totpoints; i++) {
          if (only_selected && (gps->points[i].flag & GP_SPOINT_SELECT) == 0) {
            continue;
          }

          /* Perform smoothing. */
          if (smooth_position) {
            BKE_gpencil_stroke_smooth_point(&gps_old, i, factor, 1, false, false, gps);
            need_update = true;
          }
          if (smooth_strength) {
            BKE_gpencil_stroke_smooth_strength(&gps_old, i, factor, 1, gps);
          }
          if (smooth_thickness) {
            BKE_gpencil_stroke_smooth_thickness(&gps_old, i, 1.0f - factor, 1, gps);
          }
          if (smooth_uv) {
            BKE_gpencil_stroke_smooth_uv(&gps_old, i, factor, 1, gps);
            need_update = true;
          }
        }
        if (n < repeat - 1) {
          blender::dna::shallow_copy_array(gps_old.points, gps->points, gps->totpoints);
        }
      }
      MEM_freeN(gps_old.points);

      if (need_update) {
        gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
        BKE_gpencil_stroke_geometry_update(gpd_, gps);
      }
    }
  }
  GP_EDITABLE_STROKES_END(gpstroke_iter);
}

/* helper: Count how many points need to be inserted */
static int gpencil_count_subdivision_cuts(bGPDstroke *gps)
{
  bGPDspoint *pt;
  int i;
  int totnewpoints = 0;
  for (i = 0, pt = gps->points; i < gps->totpoints && pt; i++, pt++) {
    if (pt->flag & GP_SPOINT_SELECT) {
      if (i + 1 < gps->totpoints) {
        if (gps->points[i + 1].flag & GP_SPOINT_SELECT) {
          totnewpoints++;
        }
      }
    }
  }

  if ((gps->flag & GP_STROKE_CYCLIC) && (gps->points[0].flag & GP_SPOINT_SELECT) &&
      (gps->points[gps->totpoints - 1].flag & GP_SPOINT_SELECT))
  {
    totnewpoints++;
  }

  return totnewpoints;
}

static void gpencil_stroke_subdivide(bGPDstroke *gps, const int cuts)
{
  bGPDspoint *temp_points;
  int totnewpoints, oldtotpoints;
  int i2;
  /* loop as many times as cuts */
  for (int s = 0; s < cuts; s++) {
    totnewpoints = gpencil_count_subdivision_cuts(gps);
    if (totnewpoints == 0) {
      continue;
    }
    /* duplicate points in a temp area */
    temp_points = static_cast<bGPDspoint *>(MEM_dupallocN(gps->points));
    oldtotpoints = gps->totpoints;

    MDeformVert *temp_dverts = nullptr;
    MDeformVert *dvert_final = nullptr;
    MDeformVert *dvert = nullptr;
    MDeformVert *dvert_next = nullptr;
    if (gps->dvert != nullptr) {
      temp_dverts = static_cast<MDeformVert *>(MEM_dupallocN(gps->dvert));
    }

    /* resize the points arrays */
    gps->totpoints += totnewpoints;
    gps->points = static_cast<bGPDspoint *>(
        MEM_recallocN(gps->points, sizeof(*gps->points) * gps->totpoints));
    if (gps->dvert != nullptr) {
      gps->dvert = static_cast<MDeformVert *>(
          MEM_recallocN(gps->dvert, sizeof(*gps->dvert) * gps->totpoints));
    }

    /* loop and interpolate */
    i2 = 0;
    for (int i = 0; i < oldtotpoints; i++) {
      bGPDspoint *pt = &temp_points[i];
      bGPDspoint *pt_final = &gps->points[i2];

      /* copy current point */
      copy_v3_v3(&pt_final->x, &pt->x);
      pt_final->pressure = pt->pressure;
      pt_final->strength = pt->strength;
      pt_final->time = pt->time;
      pt_final->flag = pt->flag;
      copy_v4_v4(pt_final->vert_color, pt->vert_color);

      if (gps->dvert != nullptr) {
        dvert = &temp_dverts[i];
        dvert_final = &gps->dvert[i2];
        dvert_final->totweight = dvert->totweight;
        dvert_final->dw = dvert->dw;
      }
      i2++;

      /* if next point is selected add a half way point */
      if (pt->flag & GP_SPOINT_SELECT) {
        if (i + 1 < oldtotpoints) {
          if (temp_points[i + 1].flag & GP_SPOINT_SELECT) {
            pt_final = &gps->points[i2];
            if (gps->dvert != nullptr) {
              dvert_final = &gps->dvert[i2];
            }
            /* Interpolate all values */
            bGPDspoint *next = &temp_points[i + 1];
            interp_v3_v3v3(&pt_final->x, &pt->x, &next->x, 0.5f);
            pt_final->pressure = interpf(pt->pressure, next->pressure, 0.5f);
            pt_final->strength = interpf(pt->strength, next->strength, 0.5f);
            CLAMP(pt_final->strength, GPENCIL_STRENGTH_MIN, 1.0f);
            interp_v4_v4v4(pt_final->vert_color, pt->vert_color, next->vert_color, 0.5f);
            pt_final->time = interpf(pt->time, next->time, 0.5f);
            pt_final->flag |= GP_SPOINT_SELECT;

            /* interpolate weights */
            if (gps->dvert != nullptr) {
              dvert = &temp_dverts[i];
              dvert_next = &temp_dverts[i + 1];
              dvert_final = &gps->dvert[i2];

              dvert_final->totweight = dvert->totweight;
              dvert_final->dw = static_cast<MDeformWeight *>(MEM_dupallocN(dvert->dw));

              /* interpolate weight values */
              for (int d = 0; d < dvert->totweight; d++) {
                MDeformWeight *dw_a = &dvert->dw[d];
                if (dvert_next->totweight > d) {
                  MDeformWeight *dw_b = &dvert_next->dw[d];
                  MDeformWeight *dw_final = &dvert_final->dw[d];
                  dw_final->weight = interpf(dw_a->weight, dw_b->weight, 0.5f);
                }
              }
            }

            i2++;
          }
        }
      }
    }

    /* Subdivide between last and first point. */
    if (gps->flag & GP_STROKE_CYCLIC) {
      bGPDspoint *pt = &temp_points[oldtotpoints - 1];
      bGPDspoint *next = &temp_points[0];
      if ((pt->flag & GP_SPOINT_SELECT) && (next->flag & GP_SPOINT_SELECT)) {
        bGPDspoint *pt_final = &gps->points[i2];
        if (gps->dvert != nullptr) {
          dvert_final = &gps->dvert[i2];
        }
        /* Interpolate all values */
        interp_v3_v3v3(&pt_final->x, &pt->x, &next->x, 0.5f);
        pt_final->pressure = interpf(pt->pressure, next->pressure, 0.5f);
        pt_final->strength = interpf(pt->strength, next->strength, 0.5f);
        CLAMP(pt_final->strength, GPENCIL_STRENGTH_MIN, 1.0f);
        interp_v4_v4v4(pt_final->vert_color, pt->vert_color, next->vert_color, 0.5f);
        pt_final->time = interpf(pt->time, next->time, 0.5f);
        pt_final->flag |= GP_SPOINT_SELECT;

        /* interpolate weights */
        if (gps->dvert != nullptr) {
          dvert = &temp_dverts[oldtotpoints - 1];
          dvert_next = &temp_dverts[0];
          dvert_final = &gps->dvert[i2];

          dvert_final->totweight = dvert->totweight;
          dvert_final->dw = static_cast<MDeformWeight *>(MEM_dupallocN(dvert->dw));

          /* interpolate weight values */
          for (int d = 0; d < dvert->totweight; d++) {
            MDeformWeight *dw_a = &dvert->dw[d];
            if (dvert_next->totweight > d) {
              MDeformWeight *dw_b = &dvert_next->dw[d];
              MDeformWeight *dw_final = &dvert_final->dw[d];
              dw_final->weight = interpf(dw_a->weight, dw_b->weight, 0.5f);
            }
          }
        }
      }
    }

    /* free temp memory */
    MEM_SAFE_FREE(temp_points);
    MEM_SAFE_FREE(temp_dverts);
  }
}

static int gpencil_stroke_subdivide_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  const int cuts = RNA_int_get(op->ptr, "number_cuts");

  /* sanity checks */
  if (ELEM(nullptr, gpd)) {
    return OPERATOR_CANCELLED;
  }

  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));

  bool changed = false;
  if (is_curve_edit) {
    GP_EDITABLE_CURVES_BEGIN(gps_iter, C, gpl, gps, gpc)
    {
      if (gpc->flag & GP_CURVE_SELECT) {
        BKE_gpencil_editcurve_subdivide(gps, cuts);
        BKE_gpencil_editcurve_recalculate_handles(gps);
        gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
        BKE_gpencil_stroke_geometry_update(gpd, gps);
        changed = true;
      }
    }
    GP_EDITABLE_CURVES_END(gps_iter);
  }
  else {
    /* Go through each editable + selected stroke */
    GP_EDITABLE_STROKES_BEGIN (gpstroke_iter, C, gpl, gps) {
      if (gps->flag & GP_STROKE_SELECT) {
        gpencil_stroke_subdivide(gps, cuts);
        /* Calc geometry data. */
        BKE_gpencil_stroke_geometry_update(gpd, gps);
        changed = true;
      }
    }
    GP_EDITABLE_STROKES_END(gpstroke_iter);

    if (changed) {
      /* smooth stroke */
      gpencil_smooth_stroke(C, op);
    }
  }

  if (changed) {
    /* notifiers */
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

static bool gpencil_subdivide_curve_edit_poll_property(const bContext *C,
                                                       wmOperator * /*op*/,
                                                       const PropertyRNA *prop)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  if (gpd != nullptr && GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd)) {
    const char *prop_id = RNA_property_identifier(prop);
    /* Only show number_cuts in curve edit mode */
    if (!STREQ(prop_id, "number_cuts")) {
      return false;
    }
  }

  return true;
}

void GPENCIL_OT_stroke_subdivide(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Subdivide Stroke";
  ot->idname = "GPENCIL_OT_stroke_subdivide";
  ot->description =
      "Subdivide between continuous selected points of the stroke adding a point half way "
      "between "
      "them";

  /* api callbacks */
  ot->exec = gpencil_stroke_subdivide_exec;
  ot->poll = gpencil_active_layer_poll;
  ot->poll_property = gpencil_subdivide_curve_edit_poll_property;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_int(ot->srna, "number_cuts", 1, 1, 10, "Number of Cuts", "", 1, 5);
  /* avoid re-using last var because it can cause _very_ high value and annoy users */
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  /* Smooth parameters */
  RNA_def_float(ot->srna, "factor", 0.0f, 0.0f, 2.0f, "Smooth", "", 0.0f, 2.0f);
  prop = RNA_def_int(ot->srna, "repeat", 1, 1, 10, "Repeat", "", 1, 5);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  RNA_def_boolean(ot->srna,
                  "only_selected",
                  true,
                  "Selected Points",
                  "Smooth only selected points in the stroke");
  RNA_def_boolean(ot->srna, "smooth_position", true, "Position", "");
  RNA_def_boolean(ot->srna, "smooth_thickness", true, "Thickness", "");
  RNA_def_boolean(ot->srna, "smooth_strength", false, "Strength", "");
  RNA_def_boolean(ot->srna, "smooth_uv", false, "UV", "");
}

/* ** simplify stroke *** */
static int gpencil_stroke_simplify_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  float factor = RNA_float_get(op->ptr, "factor");

  /* sanity checks */
  if (ELEM(nullptr, gpd)) {
    return OPERATOR_CANCELLED;
  }

  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));

  bool changed = false;
  if (is_curve_edit) {
    BKE_report(op->reports, RPT_ERROR, "Not implemented!");
  }
  else {
    /* Go through each editable + selected stroke */
    GP_EDITABLE_STROKES_BEGIN (gpstroke_iter, C, gpl, gps) {
      if (gps->flag & GP_STROKE_SELECT) {
        /* simplify stroke using Ramer-Douglas-Peucker algorithm */
        BKE_gpencil_stroke_simplify_adaptive(gpd, gps, factor);
        changed = true;
      }
    }
    GP_EDITABLE_STROKES_END(gpstroke_iter);
  }

  if (changed) {
    /* notifiers */
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_simplify(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Simplify Stroke";
  ot->idname = "GPENCIL_OT_stroke_simplify";
  ot->description = "Simplify selected strokes, reducing number of points";

  /* api callbacks */
  ot->exec = gpencil_stroke_simplify_exec;
  ot->poll = gpencil_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_float(ot->srna, "factor", 0.0f, 0.0f, 100.0f, "Factor", "", 0.0f, 100.0f);
  /* avoid re-using last var */
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ** simplify stroke using fixed algorithm *** */
static int gpencil_stroke_simplify_fixed_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  int steps = RNA_int_get(op->ptr, "step");

  /* sanity checks */
  if (ELEM(nullptr, gpd)) {
    return OPERATOR_CANCELLED;
  }

  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));

  bool changed = false;
  if (is_curve_edit) {
    BKE_report(op->reports, RPT_ERROR, "Not implemented!");
  }
  else {
    /* Go through each editable + selected stroke */
    GP_EDITABLE_STROKES_BEGIN (gpstroke_iter, C, gpl, gps) {
      if (gps->flag & GP_STROKE_SELECT) {
        changed |= true;
        for (int i = 0; i < steps; i++) {
          BKE_gpencil_stroke_simplify_fixed(gpd, gps);
        }
      }
    }
    GP_EDITABLE_STROKES_END(gpstroke_iter);
  }

  if (changed) {
    /* notifiers */
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_simplify_fixed(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Simplify Fixed Stroke";
  ot->idname = "GPENCIL_OT_stroke_simplify_fixed";
  ot->description = "Simplify selected strokes, reducing number of points using fixed algorithm";

  /* api callbacks */
  ot->exec = gpencil_stroke_simplify_fixed_exec;
  ot->poll = gpencil_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_int(ot->srna, "step", 1, 1, 100, "Steps", "Number of simplify steps", 1, 10);

  /* avoid re-using last var */
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ** Resample stroke *** */
static int gpencil_stroke_sample_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  const float length = RNA_float_get(op->ptr, "length");
  const float sharp_threshold = RNA_float_get(op->ptr, "sharp_threshold");

  /* sanity checks */
  if (ELEM(nullptr, gpd)) {
    return OPERATOR_CANCELLED;
  }

  /* Go through each editable + selected stroke */
  GP_EDITABLE_STROKES_BEGIN (gpstroke_iter, C, gpl, gps) {
    if (gps->flag & GP_STROKE_SELECT) {
      BKE_gpencil_stroke_sample(gpd, gps, length, true, sharp_threshold);
    }
  }
  GP_EDITABLE_STROKES_END(gpstroke_iter);

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_sample(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Sample Stroke";
  ot->idname = "GPENCIL_OT_stroke_sample";
  ot->description = "Sample stroke points to predefined segment length";

  /* api callbacks */
  ot->exec = gpencil_stroke_sample_exec;
  ot->poll = gpencil_stroke_not_in_curve_edit_mode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_float(ot->srna, "length", 0.1f, 0.0f, 100.0f, "Length", "", 0.0f, 100.0f);
  prop = RNA_def_float(
      ot->srna, "sharp_threshold", 0.1f, 0.0f, M_PI, "Sharp Threshold", "", 0.0f, M_PI);
  /* avoid re-using last var */
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Trim Operator
 * \{ */

static int gpencil_stroke_trim_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  /* sanity checks */
  if (ELEM(nullptr, gpd)) {
    return OPERATOR_CANCELLED;
  }

  /* Go through each editable + selected stroke */
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {

        if (gpf == nullptr) {
          continue;
        }

        LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {

          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }

          if (gps->flag & GP_STROKE_SELECT) {
            if (is_curve_edit) {
              BKE_report(op->reports, RPT_ERROR, "Not implemented!");
            }
            else {
              BKE_gpencil_stroke_trim(gpd, gps);
            }
          }
        }
        /* If not multi-edit, exit loop. */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }
  CTX_DATA_END;

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_trim(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Trim Stroke";
  ot->idname = "GPENCIL_OT_stroke_trim";
  ot->description = "Trim selected stroke to first loop or intersection";

  /* api callbacks */
  ot->exec = gpencil_stroke_trim_exec;
  ot->poll = gpencil_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Separate Operator
 * \{ */

enum eGP_SeparateModes {
  /* Points */
  GP_SEPARATE_POINT = 0,
  /* Selected Strokes */
  GP_SEPARATE_STROKE,
  /* Current Layer */
  GP_SEPARATE_LAYER,
};

static int gpencil_stroke_separate_exec(bContext *C, wmOperator *op)
{
  Base *base_new;
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Base *base_prev = CTX_data_active_base(C);
  bGPdata *gpd_src = ED_gpencil_data_get_active(C);
  Object *ob = CTX_data_active_object(C);

  Object *ob_dst = nullptr;
  bGPdata *gpd_dst = nullptr;
  bGPDlayer *gpl_dst = nullptr;
  bGPDframe *gpf_dst = nullptr;
  bGPDspoint *pt;
  Material *ma = nullptr;
  int i, idx;

  eGP_SeparateModes mode = eGP_SeparateModes(RNA_enum_get(op->ptr, "mode"));

  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd_src));
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd_src));

  /* sanity checks */
  if (ELEM(nullptr, gpd_src)) {
    return OPERATOR_CANCELLED;
  }

  if ((mode == GP_SEPARATE_LAYER) && BLI_listbase_is_single(&gpd_src->layers)) {
    BKE_report(op->reports, RPT_ERROR, "Cannot separate an object with one layer only");
    return OPERATOR_CANCELLED;
  }

  /* Cancel if nothing selected. */
  if (ELEM(mode, GP_SEPARATE_POINT, GP_SEPARATE_STROKE)) {
    bool has_selected = false;
    CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
      if (ED_gpencil_layer_has_selected_stroke(gpl, is_multiedit)) {
        has_selected = true;
        break;
      }
    }
    CTX_DATA_END;

    if (!has_selected) {
      BKE_report(op->reports, RPT_ERROR, "Nothing selected");
      return OPERATOR_CANCELLED;
    }
  }

  /* Create a new object. */
  /* Take into account user preferences for duplicating actions. */
  const eDupli_ID_Flags dupflag = eDupli_ID_Flags(U.dupflag & USER_DUP_ACT);

  base_new = ED_object_add_duplicate(bmain, scene, view_layer, base_prev, dupflag);
  ob_dst = base_new->object;
  ob_dst->mode = OB_MODE_OBJECT;
  /* Duplication will increment #bGPdata user-count, but since we create a new grease-pencil
   * data-block for ob_dst (which gets its own user automatically),
   * we have to decrement the user-count again. */
  gpd_dst = BKE_gpencil_data_addnew(bmain, gpd_src->id.name + 2);
  id_us_min(static_cast<ID *>(ob_dst->data));
  ob_dst->data = (bGPdata *)gpd_dst;

  BKE_defgroup_copy_list(&gpd_dst->vertex_group_names, &gpd_src->vertex_group_names);
  gpd_dst->vertex_group_active_index = gpd_src->vertex_group_active_index;

  /* Loop old data-block and separate parts. */
  if (ELEM(mode, GP_SEPARATE_POINT, GP_SEPARATE_STROKE)) {
    CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
      gpl_dst = nullptr;
      bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                      gpl->actframe);

      for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
        if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {

          if (gpf == nullptr) {
            continue;
          }

          gpf_dst = nullptr;

          LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {

            /* skip strokes that are invalid for current view */
            if (ED_gpencil_stroke_can_use(C, gps) == false) {
              continue;
            }
            /* check if the color is editable */
            if (ED_gpencil_stroke_material_editable(ob, gpl, gps) == false) {
              continue;
            }
            /* Separate selected strokes. */
            if (gps->flag & GP_STROKE_SELECT) {
              /* add layer if not created before */
              if (gpl_dst == nullptr) {
                gpl_dst = BKE_gpencil_layer_addnew(gpd_dst, gpl->info, false, false);
                BKE_gpencil_layer_copy_settings(gpl, gpl_dst);
                /* Copy masks. */
                BKE_gpencil_layer_mask_copy(gpl, gpl_dst);
              }

              /* add frame if not created before */
              if (gpf_dst == nullptr) {
                gpf_dst = BKE_gpencil_layer_frame_get(gpl_dst, gpf->framenum, GP_GETFRAME_ADD_NEW);
              }

              /* add duplicate materials */

              /* XXX same material can be in multiple slots. */
              ma = BKE_gpencil_material(ob, gps->mat_nr + 1);

              idx = BKE_gpencil_object_material_ensure(bmain, ob_dst, ma);

              /* selected points mode */
              if (mode == GP_SEPARATE_POINT) {
                if (is_curve_edit) {
                  BKE_report(op->reports, RPT_ERROR, "Not implemented!");
                }
                else {
                  /* Check if all points are selected. */
                  bool all_points_selected = true;
                  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
                    if ((pt->flag & GP_SPOINT_SELECT) == 0) {
                      all_points_selected = false;
                      break;
                    }
                  }

                  /* Separate the entire stroke. */
                  if (all_points_selected) {
                    /* deselect old stroke */
                    gps->flag &= ~GP_STROKE_SELECT;
                    BKE_gpencil_stroke_select_index_reset(gps);
                    /* unlink from source frame */
                    BLI_remlink(&gpf->strokes, gps);
                    gps->prev = gps->next = nullptr;
                    /* relink to destination frame */
                    BLI_addtail(&gpf_dst->strokes, gps);
                    /* Reassign material. */
                    gps->mat_nr = idx;

                    continue;
                  }

                  /* make copy of source stroke */
                  bGPDstroke *gps_dst = BKE_gpencil_stroke_duplicate(gps, true, true);

                  /* Reassign material. */
                  gps_dst->mat_nr = idx;

                  /* link to destination frame */
                  BLI_addtail(&gpf_dst->strokes, gps_dst);

                  /* Invert selection status of all points in destination stroke */
                  for (i = 0, pt = gps_dst->points; i < gps_dst->totpoints; i++, pt++) {
                    pt->flag ^= GP_SPOINT_SELECT;
                  }

                  /* delete selected points from destination stroke */
                  BKE_gpencil_stroke_delete_tagged_points(
                      gpd_dst, gpf_dst, gps_dst, nullptr, GP_SPOINT_SELECT, false, false, 0);

                  /* delete selected points from origin stroke */
                  BKE_gpencil_stroke_delete_tagged_points(
                      gpd_src, gpf, gps, gps->next, GP_SPOINT_SELECT, false, false, 0);
                }
              }
              /* selected strokes mode */
              else if (mode == GP_SEPARATE_STROKE) {
                /* deselect old stroke */
                gps->flag &= ~GP_STROKE_SELECT;
                BKE_gpencil_stroke_select_index_reset(gps);
                /* unlink from source frame */
                BLI_remlink(&gpf->strokes, gps);
                gps->prev = gps->next = nullptr;
                /* relink to destination frame */
                BLI_addtail(&gpf_dst->strokes, gps);
                /* Reassign material. */
                gps->mat_nr = idx;
              }
            }
          }
        }

        /* If not multi-edit, exit loop. */
        if (!is_multiedit) {
          break;
        }
      }
    }
    CTX_DATA_END;
  }
  else if (mode == GP_SEPARATE_LAYER) {
    bGPDlayer *gpl = CTX_data_active_gpencil_layer(C);
    if (gpl) {
      /* try to set a new active layer in source datablock */
      if (gpl->prev) {
        BKE_gpencil_layer_active_set(gpd_src, gpl->prev);
      }
      else if (gpl->next) {
        BKE_gpencil_layer_active_set(gpd_src, gpl->next);
      }
      /* unlink from source datablock */
      BLI_remlink(&gpd_src->layers, gpl);
      gpl->prev = gpl->next = nullptr;
      /* relink to destination datablock */
      BLI_addtail(&gpd_dst->layers, gpl);

      /* add duplicate materials */
      LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }
          ma = BKE_gpencil_material(ob, gps->mat_nr + 1);
          gps->mat_nr = BKE_gpencil_object_material_ensure(bmain, ob_dst, ma);
        }
      }
    }
  }

  /* Ensure destination object has one active layer. */
  if (gpd_dst->layers.first != nullptr) {
    if (BKE_gpencil_layer_active_get(gpd_dst) == nullptr) {
      BKE_gpencil_layer_active_set(gpd_dst, static_cast<bGPDlayer *>(gpd_dst->layers.first));
    }
  }

  /* Remove unused slots. */
  int actcol = ob_dst->actcol;
  for (int slot = 1; slot <= ob_dst->totcol; slot++) {
    while (slot <= ob_dst->totcol && !BKE_object_material_slot_used(ob_dst, slot)) {
      ob_dst->actcol = slot;
      if (!BKE_object_material_slot_remove(bmain, ob_dst)) {
        break;
      }
      if (actcol >= slot) {
        actcol--;
      }
    }
  }
  ob_dst->actcol = actcol;

  /* Remove any invalid Mask relationship. */
  BKE_gpencil_layer_mask_cleanup_all_layers(gpd_dst);

  DEG_id_tag_update(&gpd_src->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  DEG_id_tag_update(&gpd_dst->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, nullptr);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_SELECTED, nullptr);
  ED_outliner_select_sync_from_object_tag(C);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_separate(wmOperatorType *ot)
{
  static const EnumPropertyItem separate_type[] = {
      {GP_SEPARATE_POINT, "POINT", 0, "Selected Points", "Separate the selected points"},
      {GP_SEPARATE_STROKE, "STROKE", 0, "Selected Strokes", "Separate the selected strokes"},
      {GP_SEPARATE_LAYER, "LAYER", 0, "Active Layer", "Separate the strokes of the current layer"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Separate Strokes";
  ot->idname = "GPENCIL_OT_stroke_separate";
  ot->description = "Separate the selected strokes or layer in a new grease pencil object";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = gpencil_stroke_separate_exec;
  ot->poll = gpencil_strokes_edit3d_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "mode", separate_type, GP_SEPARATE_POINT, "Mode", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Split Operator
 * \{ */

static int gpencil_stroke_split_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bGPDspoint *pt;
  int i;

  /* sanity checks */
  if (ELEM(nullptr, gpd)) {
    return OPERATOR_CANCELLED;
  }
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));

  /* loop strokes and split parts */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {

        if (gpf == nullptr) {
          continue;
        }

        LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {

          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }
          /* check if the color is editable */
          if (ED_gpencil_stroke_material_editable(ob, gpl, gps) == false) {
            continue;
          }
          /* Split selected strokes. */
          if (gps->flag & GP_STROKE_SELECT) {
            if (is_curve_edit) {
              BKE_report(op->reports, RPT_ERROR, "Not implemented!");
            }
            else {
              /* make copy of source stroke */
              bGPDstroke *gps_dst = BKE_gpencil_stroke_duplicate(gps, true, true);

              /* link to same frame */
              BLI_addtail(&gpf->strokes, gps_dst);

              /* invert selection status of all points in destination stroke */
              for (i = 0, pt = gps_dst->points; i < gps_dst->totpoints; i++, pt++) {
                pt->flag ^= GP_SPOINT_SELECT;
              }

              /* delete selected points from destination stroke */
              BKE_gpencil_stroke_delete_tagged_points(
                  gpd, gpf, gps_dst, nullptr, GP_SPOINT_SELECT, true, false, 0);

              /* delete selected points from origin stroke */
              BKE_gpencil_stroke_delete_tagged_points(
                  gpd, gpf, gps, gps->next, GP_SPOINT_SELECT, false, false, 0);
            }
          }
        }
        /* select again tagged points */
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          bGPDspoint *ptn = gps->points;
          for (int i2 = 0; i2 < gps->totpoints; i2++, ptn++) {
            if (ptn->flag & GP_SPOINT_TAG) {
              ptn->flag |= GP_SPOINT_SELECT;
              ptn->flag &= ~GP_SPOINT_TAG;
            }
          }
        }
      }

      /* If not multi-edit, exit loop. */
      if (!is_multiedit) {
        break;
      }
    }
  }
  CTX_DATA_END;

  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_split(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Split Strokes";
  ot->idname = "GPENCIL_OT_stroke_split";
  ot->description = "Split selected points as new stroke on same frame";

  /* callbacks */
  ot->exec = gpencil_stroke_split_exec;
  ot->poll = gpencil_strokes_edit3d_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Smooth Operator
 * \{ */

static int gpencil_stroke_smooth_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  /* sanity checks */
  if (ELEM(nullptr, gpd)) {
    return OPERATOR_CANCELLED;
  }

  gpencil_smooth_stroke(C, op);

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_smooth(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Smooth Stroke";
  ot->idname = "GPENCIL_OT_stroke_smooth";
  ot->description = "Smooth selected strokes";

  /* api callbacks */
  ot->exec = gpencil_stroke_smooth_exec;
  ot->poll = gpencil_stroke_not_in_curve_edit_mode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_int(ot->srna, "repeat", 2, 1, 1000, "Repeat", "", 1, 1000);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  RNA_def_float(ot->srna, "factor", 1.0f, 0.0f, 2.0f, "Factor", "", 0.0f, 1.0f);
  RNA_def_boolean(ot->srna,
                  "only_selected",
                  true,
                  "Selected Points",
                  "Smooth only selected points in the stroke");
  RNA_def_boolean(ot->srna, "smooth_position", true, "Position", "");
  RNA_def_boolean(ot->srna, "smooth_thickness", true, "Thickness", "");
  RNA_def_boolean(ot->srna, "smooth_strength", false, "Strength", "");
  RNA_def_boolean(ot->srna, "smooth_uv", false, "UV", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Cutter Operator
 * \{ */

/* smart stroke cutter for trimming stroke ends */
struct GP_SelectLassoUserData {
  rcti rect;
  const int (*mcoords)[2];
  int mcoords_len;
};

static bool gpencil_test_lasso(bGPDstroke *gps,
                               bGPDspoint *pt,
                               const GP_SpaceConversion *gsc,
                               const float diff_mat[4][4],
                               void *user_data)
{
  const GP_SelectLassoUserData *data = static_cast<const GP_SelectLassoUserData *>(user_data);
  bGPDspoint pt2;
  int x0, y0;
  gpencil_point_to_world_space(pt, diff_mat, &pt2);
  gpencil_point_to_xy(gsc, gps, &pt2, &x0, &y0);
  /* test if in lasso */
  return (!ELEM(V2D_IS_CLIPPED, x0, y0) && BLI_rcti_isect_pt(&data->rect, x0, y0) &&
          BLI_lasso_is_point_inside(data->mcoords, data->mcoords_len, x0, y0, INT_MAX));
}

typedef bool (*GPencilTestFn)(bGPDstroke *gps,
                              bGPDspoint *pt,
                              const GP_SpaceConversion *gsc,
                              const float diff_mat[4][4],
                              void *user_data);

static void gpencil_cutter_dissolve(bGPdata *gpd,
                                    bGPDlayer *hit_layer,
                                    bGPDstroke *hit_stroke,
                                    const bool flat_caps)
{
  bGPDspoint *pt = nullptr;
  bGPDspoint *pt1 = nullptr;
  int i;

  bGPDstroke *gpsn = hit_stroke->next;

  int totselect = 0;
  for (i = 0, pt = hit_stroke->points; i < hit_stroke->totpoints; i++, pt++) {
    if (pt->flag & GP_SPOINT_SELECT) {
      totselect++;
    }
  }

  /* if all points selected delete or only 2 points and 1 selected */
  if (((totselect == 1) && (hit_stroke->totpoints == 2)) || (hit_stroke->totpoints == totselect)) {
    BLI_remlink(&hit_layer->actframe->strokes, hit_stroke);
    BKE_gpencil_free_stroke(hit_stroke);
    hit_stroke = nullptr;
  }

  /* if very small distance delete */
  if ((hit_stroke) && (hit_stroke->totpoints == 2)) {
    pt = &hit_stroke->points[0];
    pt1 = &hit_stroke->points[1];
    if (len_v3v3(&pt->x, &pt1->x) < 0.001f) {
      BLI_remlink(&hit_layer->actframe->strokes, hit_stroke);
      BKE_gpencil_free_stroke(hit_stroke);
      hit_stroke = nullptr;
    }
  }

  if (hit_stroke) {
    /* tag and dissolve (untag new points) */
    for (i = 0, pt = hit_stroke->points; i < hit_stroke->totpoints; i++, pt++) {
      if (pt->flag & GP_SPOINT_SELECT) {
        pt->flag &= ~GP_SPOINT_SELECT;
        pt->flag |= GP_SPOINT_TAG;
      }
      else if (pt->flag & GP_SPOINT_TAG) {
        pt->flag &= ~GP_SPOINT_TAG;
      }
    }
    /* If flat caps mode check extremes. */
    if (flat_caps) {
      if (hit_stroke->points[0].flag & GP_SPOINT_TAG) {
        hit_stroke->caps[0] = GP_STROKE_CAP_FLAT;
      }

      if (hit_stroke->points[hit_stroke->totpoints - 1].flag & GP_SPOINT_TAG) {
        hit_stroke->caps[1] = GP_STROKE_CAP_FLAT;
      }
    }

    BKE_gpencil_stroke_delete_tagged_points(
        gpd, hit_layer->actframe, hit_stroke, gpsn, GP_SPOINT_TAG, false, false, 1);
  }
}

static int gpencil_cutter_lasso_select(bContext *C,
                                       wmOperator *op,
                                       GPencilTestFn is_inside_fn,
                                       void *user_data)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *obact = CTX_data_active_object(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  ScrArea *area = CTX_wm_area(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  const float scale = ts->gp_sculpt.isect_threshold;
  const bool flat_caps = RNA_boolean_get(op->ptr, "flat_caps");
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));

  bGPDspoint *pt;
  GP_SpaceConversion gsc = {nullptr};

  bool changed = false;

  /* sanity checks */
  if (area == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "No active area");
    return OPERATOR_CANCELLED;
  }

  /* init space conversion stuff */
  gpencil_point_conversion_init(C, &gsc);

  /* Deselect all strokes. */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);
    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        if (gps->flag & GP_STROKE_SELECT) {
          int i;
          for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
            pt->flag &= ~GP_SPOINT_SELECT;
          }

          gps->flag &= ~GP_STROKE_SELECT;
          BKE_gpencil_stroke_select_index_reset(gps);
        }
      }
      /* If not multi-edit, exit loop. */
      if (!is_multiedit) {
        break;
      }
    }
  }

  /* Select points */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if ((gpl->flag & GP_LAYER_LOCKED) || (gpl->flag & GP_LAYER_HIDE)) {
      continue;
    }

    float diff_mat[4][4];
    BKE_gpencil_layer_transform_matrix_get(depsgraph, obact, gpl, diff_mat);

    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);
    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == nullptr) {
          continue;
        }

        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          } /* check if the color is editable */
          if (ED_gpencil_stroke_material_editable(obact, gpl, gps) == false) {
            continue;
          }
          int tot_inside = 0;
          const int oldtot = gps->totpoints;
          for (int i = 0; i < gps->totpoints; i++) {
            pt = &gps->points[i];
            if ((pt->flag & GP_SPOINT_SELECT) || (pt->flag & GP_SPOINT_TAG)) {
              continue;
            }
            /* convert point coords to screen-space */
            const bool is_inside = is_inside_fn(gps, pt, &gsc, diff_mat, user_data);
            if (is_inside) {
              tot_inside++;
              changed = true;
              pt->flag |= GP_SPOINT_SELECT;
              gps->flag |= GP_STROKE_SELECT;
              BKE_gpencil_stroke_select_index_set(gpd, gps);
              float r_hita[3], r_hitb[3];
              if (gps->totpoints > 1) {
                ED_gpencil_select_stroke_segment(
                    gpd, gpl, gps, pt, true, true, scale, r_hita, r_hitb);
              }
              /* avoid infinite loops */
              if (gps->totpoints > oldtot) {
                break;
              }
            }
          }
          /* if mark all points inside lasso set to remove all stroke */
          if ((tot_inside == oldtot) || ((tot_inside == 1) && (oldtot == 2))) {
            for (int i = 0; i < gps->totpoints; i++) {
              pt = &gps->points[i];
              pt->flag |= GP_SPOINT_SELECT;
            }
          }
        }
        /* If not multi-edit, exit loop. */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }

  /* Dissolve selected points. */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);
    bGPDframe *gpf_act = gpl->actframe;
    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      gpl->actframe = gpf;
      LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {
        if (gps->flag & GP_STROKE_SELECT) {
          gpencil_cutter_dissolve(gpd, gpl, gps, flat_caps);
        }
      }
      /* If not multi-edit, exit loop. */
      if (!is_multiedit) {
        break;
      }
    }
    gpl->actframe = gpf_act;
  }

  /* updates */
  if (changed) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);
    WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, nullptr);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);
  }

  return OPERATOR_FINISHED;
}

static bool gpencil_cutter_poll(bContext *C)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  if (GPENCIL_PAINT_MODE(gpd)) {
    if (gpd->layers.first) {
      return true;
    }
  }

  return false;
}

static int gpencil_cutter_exec(bContext *C, wmOperator *op)
{
  ScrArea *area = CTX_wm_area(C);
  /* sanity checks */
  if (area == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "No active area");
    return OPERATOR_CANCELLED;
  }

  GP_SelectLassoUserData data{};
  data.mcoords = WM_gesture_lasso_path_to_array(C, op, &data.mcoords_len);

  /* Sanity check. */
  if (data.mcoords == nullptr) {
    return OPERATOR_PASS_THROUGH;
  }

  /* Compute boundbox of lasso (for faster testing later). */
  BLI_lasso_boundbox(&data.rect, data.mcoords, data.mcoords_len);

  gpencil_cutter_lasso_select(C, op, gpencil_test_lasso, &data);

  MEM_freeN((void *)data.mcoords);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_cutter(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Stroke Cutter";
  ot->description = "Select section and cut";
  ot->idname = "GPENCIL_OT_stroke_cutter";

  /* callbacks */
  ot->invoke = WM_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = gpencil_cutter_exec;
  ot->poll = gpencil_cutter_poll;
  ot->cancel = WM_gesture_lasso_cancel;

  /* flag */
  ot->flag = OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

  /* properties */
  WM_operator_properties_gesture_lasso(ot);

  RNA_def_boolean(ot->srna, "flat_caps", false, "Flat Caps", "");
}

bool ED_object_gpencil_exit(Main *bmain, Object *ob)
{
  bool ok = false;
  if (ob) {
    bGPdata *gpd = (bGPdata *)ob->data;

    gpd->flag &= ~(GP_DATA_STROKE_PAINTMODE | GP_DATA_STROKE_EDITMODE | GP_DATA_STROKE_SCULPTMODE |
                   GP_DATA_STROKE_WEIGHTMODE | GP_DATA_STROKE_VERTEXMODE);

    ob->restore_mode = ob->mode;
    ob->mode &= ~(OB_MODE_PAINT_GPENCIL_LEGACY | OB_MODE_EDIT_GPENCIL_LEGACY |
                  OB_MODE_SCULPT_GPENCIL_LEGACY | OB_MODE_WEIGHT_GPENCIL_LEGACY |
                  OB_MODE_VERTEX_GPENCIL_LEGACY);

    /* Inform all CoW versions that we changed the mode. */
    DEG_id_tag_update_ex(bmain, &ob->id, ID_RECALC_COPY_ON_WRITE);
    ok = true;
  }
  return ok;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Merge By Distance Operator
 * \{ */

static bool gpencil_merge_by_distance_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if ((ob == nullptr) || (ob->type != OB_GPENCIL_LEGACY)) {
    return false;
  }
  bGPdata *gpd = (bGPdata *)ob->data;
  if (gpd == nullptr) {
    return false;
  }

  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);

  return ((gpl != nullptr) && (ob->mode == OB_MODE_EDIT_GPENCIL_LEGACY));
}

static int gpencil_merge_by_distance_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = (bGPdata *)ob->data;
  const float threshold = RNA_float_get(op->ptr, "threshold");
  const bool unselected = RNA_boolean_get(op->ptr, "use_unselected");

  /* sanity checks */
  if (ELEM(nullptr, gpd)) {
    return OPERATOR_CANCELLED;
  }

  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));

  if (is_curve_edit) {
    /* TODO: merge curve points by distance */
  }
  else {
    /* Go through each editable selected stroke */
    GP_EDITABLE_STROKES_BEGIN (gpstroke_iter, C, gpl, gps) {
      if (gps->flag & GP_STROKE_SELECT) {
        BKE_gpencil_stroke_merge_distance(gpd, gpf_, gps, threshold, unselected);
      }
    }
    GP_EDITABLE_STROKES_END(gpstroke_iter);
  }

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_merge_by_distance(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Merge by Distance";
  ot->idname = "GPENCIL_OT_stroke_merge_by_distance";
  ot->description = "Merge points by distance";

  /* api callbacks */
  ot->exec = gpencil_merge_by_distance_exec;
  ot->poll = gpencil_merge_by_distance_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_float(ot->srna, "threshold", 0.001f, 0.0f, 100.0f, "Threshold", "", 0.0f, 100.0f);
  /* avoid re-using last var */
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "use_unselected",
                         false,
                         "Unselected",
                         "Use whole stroke, not only selected points");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Normalize Operator
 * \{ */

enum eGP_NormalizeMode {
  GP_NORMALIZE_THICKNESS = 0,
  GP_NORMALIZE_OPACITY,
};

static bool gpencil_stroke_normalize_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if ((ob == nullptr) || (ob->type != OB_GPENCIL_LEGACY)) {
    return false;
  }
  bGPdata *gpd = (bGPdata *)ob->data;
  if (gpd == nullptr) {
    return false;
  }

  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);

  return ((gpl != nullptr) && (ob->mode == OB_MODE_EDIT_GPENCIL_LEGACY));
}

static void gpencil_stroke_normalize_ui(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;
  uiLayout *row;

  const eGP_NormalizeMode mode = eGP_NormalizeMode(RNA_enum_get(op->ptr, "mode"));

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  row = uiLayoutRow(layout, true);
  uiItemR(row, op->ptr, "mode", UI_ITEM_NONE, nullptr, ICON_NONE);

  if (mode == GP_NORMALIZE_THICKNESS) {
    row = uiLayoutRow(layout, true);
    uiItemR(row, op->ptr, "value", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  else if (mode == GP_NORMALIZE_OPACITY) {
    row = uiLayoutRow(layout, true);
    uiItemR(row, op->ptr, "factor", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
}

static int gpencil_stroke_normalize_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  /* Sanity checks. */
  if (ELEM(nullptr, gpd)) {
    return OPERATOR_CANCELLED;
  }

  const eGP_NormalizeMode mode = eGP_NormalizeMode(RNA_enum_get(op->ptr, "mode"));
  const int value = RNA_int_get(op->ptr, "value");
  const float factor = RNA_float_get(op->ptr, "factor");

  /* Go through each editable + selected stroke. */
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {

        if (gpf == nullptr) {
          continue;
        }

        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {

          /* Skip strokes that are invalid for current view. */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }
          bool is_curve_ready = (gps->editcurve != nullptr);
          bool selected = (is_curve_edit && is_curve_ready) ?
                              (gps->editcurve->flag & GP_CURVE_SELECT) :
                              (gps->flag & GP_STROKE_SELECT);
          if (!selected) {
            continue;
          }

          float stroke_thickness_inv = 1.0f / max_ii(gps->thickness, 1);
          /* Fill opacity need to be managed before. */
          if (mode == GP_NORMALIZE_OPACITY) {
            gps->fill_opacity_fac = factor;
            CLAMP(gps->fill_opacity_fac, 0.0f, 1.0f);
          }

          /* Loop all Polyline points. */
          if (!is_curve_edit || !is_curve_ready) {
            for (int i = 0; i < gps->totpoints; i++) {
              bGPDspoint *pt = &gps->points[i];
              if (mode == GP_NORMALIZE_THICKNESS) {
                pt->pressure = max_ff(float(value) * stroke_thickness_inv, 0.0f);
              }
              else if (mode == GP_NORMALIZE_OPACITY) {
                pt->strength = factor;
                CLAMP(pt->strength, 0.0f, 1.0f);
              }
            }
          }
          else {
            /* Loop all Bezier points. */
            for (int i = 0; i < gps->editcurve->tot_curve_points; i++) {
              bGPDcurve_point *gpc_pt = &gps->editcurve->curve_points[i];
              if (mode == GP_NORMALIZE_THICKNESS) {
                gpc_pt->pressure = max_ff(float(value) * stroke_thickness_inv, 0.0f);
              }
              else if (mode == GP_NORMALIZE_OPACITY) {
                gpc_pt->strength = factor;
                CLAMP(gpc_pt->strength, 0.0f, 1.0f);
              }
            }

            gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
            BKE_gpencil_stroke_geometry_update(gpd, gps);
          }
        }
        /* If not multi-edit, exit loop. */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }
  CTX_DATA_END;

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_normalize(wmOperatorType *ot)
{
  static const EnumPropertyItem prop_gpencil_normalize_modes[] = {
      {GP_NORMALIZE_THICKNESS,
       "THICKNESS",
       0,
       "Thickness",
       "Normalizes the stroke thickness by making all points use the same thickness value"},
      {GP_NORMALIZE_OPACITY,
       "OPACITY",
       0,
       "Opacity",
       "Normalizes the stroke opacity by making all points use the same opacity value"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Normalize Stroke";
  ot->idname = "GPENCIL_OT_stroke_normalize";
  ot->description = "Normalize stroke attributes";

  /* api callbacks */
  ot->exec = gpencil_stroke_normalize_exec;
  ot->poll = gpencil_stroke_normalize_poll;
  ot->ui = gpencil_stroke_normalize_ui;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  ot->prop = RNA_def_enum(
      ot->srna, "mode", prop_gpencil_normalize_modes, 0, "Mode", "Attribute to be normalized");
  RNA_def_float(ot->srna, "factor", 1.0f, 0.0f, 1.0f, "Factor", "", 0.0f, 1.0f);
  RNA_def_int(ot->srna, "value", 10, 0, 1000, "Value", "Value", 0, 1000);
}

/** \} */
