/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008, Blender Foundation
 * This is a new part of Blender
 * Operators for editing Grease Pencil strokes
 */

/** \file
 * \ingroup edgpencil
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_lasso_2d.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_gpencil_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_workspace.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"
#include "WM_toolsystem.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_view2d.h"

#include "ED_gpencil.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_select_utils.h"
#include "ED_space_api.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "gpencil_intern.h"

/* ************************************************ */
/* Stroke Edit Mode Management */

/* poll callback for all stroke editing operators */
static bool gp_stroke_edit_poll(bContext *C)
{
  /* edit only supported with grease pencil objects */
  Object *ob = CTX_data_active_object(C);
  if ((ob == NULL) || (ob->type != OB_GPENCIL)) {
    return false;
  }

  /* NOTE: this is a bit slower, but is the most accurate... */
  return CTX_DATA_COUNT(C, editable_gpencil_strokes) != 0;
}

/* poll callback to verify edit mode in 3D view only */
static bool gp_strokes_edit3d_poll(bContext *C)
{
  /* edit only supported with grease pencil objects */
  Object *ob = CTX_data_active_object(C);
  if ((ob == NULL) || (ob->type != OB_GPENCIL)) {
    return false;
  }

  /* 2 Requirements:
   * - 1) Editable GP data
   * - 2) 3D View only
   */
  return (gp_stroke_edit_poll(C) && ED_operator_view3d_active(C));
}

static bool gpencil_editmode_toggle_poll(bContext *C)
{
  /* edit only supported with grease pencil objects */
  Object *ob = CTX_data_active_object(C);
  if ((ob == NULL) || (ob->type != OB_GPENCIL)) {
    return false;
  }

  /* if using gpencil object, use this gpd */
  if (ob->type == OB_GPENCIL) {
    return ob->data != NULL;
  }

  return ED_gpencil_data_get_active(C) != NULL;
}

static int gpencil_editmode_toggle_exec(bContext *C, wmOperator *op)
{
  const int back = RNA_boolean_get(op->ptr, "back");

  struct wmMsgBus *mbus = CTX_wm_message_bus(C);
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bool is_object = false;
  short mode;
  /* if using a gpencil object, use this datablock */
  Object *ob = CTX_data_active_object(C);
  if ((ob) && (ob->type == OB_GPENCIL)) {
    gpd = ob->data;
    is_object = true;
  }

  if (gpd == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No active GP data");
    return OPERATOR_CANCELLED;
  }

  /* Just toggle editmode flag... */
  gpd->flag ^= GP_DATA_STROKE_EDITMODE;
  /* recalculate parent matrix */
  if (gpd->flag & GP_DATA_STROKE_EDITMODE) {
    ED_gpencil_reset_layers_parent(depsgraph, ob, gpd);
  }
  /* set mode */
  if (gpd->flag & GP_DATA_STROKE_EDITMODE) {
    mode = OB_MODE_EDIT_GPENCIL;
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

  /* setup other modes */
  ED_gpencil_setup_modes(C, gpd, mode);
  /* set cache as dirty */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA, NULL);
  WM_event_add_notifier(C, NC_GPENCIL | ND_GPENCIL_EDITMODE, NULL);
  WM_event_add_notifier(C, NC_SCENE | ND_MODE, NULL);

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
      ot->srna, "back", 0, "Return to Previous Mode", "Return to previous mode");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/* set select mode */
static int gpencil_selectmode_toggle_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  const int mode = RNA_int_get(op->ptr, "mode");

  /* Just set mode */
  ts->gpencil_selectmode = mode;

  WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, NULL);
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
  ot->poll = gp_strokes_edit3d_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  /* properties */
  prop = RNA_def_int(ot->srna, "mode", 0, 0, 2, "Select mode", "Select mode", 0, 2);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/* Stroke Paint Mode Management */

static bool gpencil_paintmode_toggle_poll(bContext *C)
{
  /* if using gpencil object, use this gpd */
  Object *ob = CTX_data_active_object(C);
  if ((ob) && (ob->type == OB_GPENCIL)) {
    return ob->data != NULL;
  }
  return ED_gpencil_data_get_active(C) != NULL;
}

static int gpencil_paintmode_toggle_exec(bContext *C, wmOperator *op)
{
  const bool back = RNA_boolean_get(op->ptr, "back");

  struct wmMsgBus *mbus = CTX_wm_message_bus(C);
  Main *bmain = CTX_data_main(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  bool is_object = false;
  short mode;
  /* if using a gpencil object, use this datablock */
  Object *ob = CTX_data_active_object(C);
  if ((ob) && (ob->type == OB_GPENCIL)) {
    gpd = ob->data;
    is_object = true;
  }

  if (gpd == NULL) {
    return OPERATOR_CANCELLED;
  }

  /* Just toggle paintmode flag... */
  gpd->flag ^= GP_DATA_STROKE_PAINTMODE;
  /* set mode */
  if (gpd->flag & GP_DATA_STROKE_PAINTMODE) {
    mode = OB_MODE_PAINT_GPENCIL;
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

  if (mode == OB_MODE_PAINT_GPENCIL) {
    /* be sure we have brushes */
    BKE_paint_ensure(ts, (Paint **)&ts->gp_paint);
    Paint *paint = &ts->gp_paint->paint;
    /* if not exist, create a new one */
    if ((paint->brush == NULL) || (paint->brush->gpencil_settings == NULL)) {
      BKE_brush_gpencil_presets(C);
    }
    BKE_paint_toolslots_brush_validate(bmain, &ts->gp_paint->paint);
  }

  /* setup other modes */
  ED_gpencil_setup_modes(C, gpd, mode);
  /* set cache as dirty */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | ND_GPENCIL_EDITMODE, NULL);
  WM_event_add_notifier(C, NC_SCENE | ND_MODE, NULL);

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
      ot->srna, "back", 0, "Return to Previous Mode", "Return to previous mode");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/* Stroke Sculpt Mode Management */

static bool gpencil_sculptmode_toggle_poll(bContext *C)
{
  /* if using gpencil object, use this gpd */
  Object *ob = CTX_data_active_object(C);
  if ((ob) && (ob->type == OB_GPENCIL)) {
    return ob->data != NULL;
  }
  return ED_gpencil_data_get_active(C) != NULL;
}

static int gpencil_sculptmode_toggle_exec(bContext *C, wmOperator *op)
{
  const bool back = RNA_boolean_get(op->ptr, "back");

  struct wmMsgBus *mbus = CTX_wm_message_bus(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bool is_object = false;
  short mode;
  /* if using a gpencil object, use this datablock */
  Object *ob = CTX_data_active_object(C);
  if ((ob) && (ob->type == OB_GPENCIL)) {
    gpd = ob->data;
    is_object = true;
  }

  if (gpd == NULL) {
    return OPERATOR_CANCELLED;
  }

  /* Just toggle sculptmode flag... */
  gpd->flag ^= GP_DATA_STROKE_SCULPTMODE;
  /* set mode */
  if (gpd->flag & GP_DATA_STROKE_SCULPTMODE) {
    mode = OB_MODE_SCULPT_GPENCIL;
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

  /* setup other modes */
  ED_gpencil_setup_modes(C, gpd, mode);
  /* set cache as dirty */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | ND_GPENCIL_EDITMODE, NULL);
  WM_event_add_notifier(C, NC_SCENE | ND_MODE, NULL);

  if (is_object) {
    WM_msg_publish_rna_prop(mbus, &ob->id, ob, Object, mode);
  }
  if (G.background == false) {
    WM_toolsystem_update_from_context_view3d(C);
  }

  return OPERATOR_FINISHED;
}

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
      ot->srna, "back", 0, "Return to Previous Mode", "Return to previous mode");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/* Stroke Weight Paint Mode Management */

static bool gpencil_weightmode_toggle_poll(bContext *C)
{
  /* if using gpencil object, use this gpd */
  Object *ob = CTX_data_active_object(C);
  if ((ob) && (ob->type == OB_GPENCIL)) {
    return ob->data != NULL;
  }
  return ED_gpencil_data_get_active(C) != NULL;
}

static int gpencil_weightmode_toggle_exec(bContext *C, wmOperator *op)
{
  const bool back = RNA_boolean_get(op->ptr, "back");

  struct wmMsgBus *mbus = CTX_wm_message_bus(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bool is_object = false;
  short mode;
  /* if using a gpencil object, use this datablock */
  Object *ob = CTX_data_active_object(C);
  if ((ob) && (ob->type == OB_GPENCIL)) {
    gpd = ob->data;
    is_object = true;
  }

  if (gpd == NULL) {
    return OPERATOR_CANCELLED;
  }

  /* Just toggle weightmode flag... */
  gpd->flag ^= GP_DATA_STROKE_WEIGHTMODE;
  /* set mode */
  if (gpd->flag & GP_DATA_STROKE_WEIGHTMODE) {
    mode = OB_MODE_WEIGHT_GPENCIL;
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
  }

  /* setup other modes */
  ED_gpencil_setup_modes(C, gpd, mode);
  /* set cache as dirty */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | ND_GPENCIL_EDITMODE, NULL);
  WM_event_add_notifier(C, NC_SCENE | ND_MODE, NULL);

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
      ot->srna, "back", 0, "Return to Previous Mode", "Return to previous mode");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/* ************************************************ */
/* Stroke Editing Operators */

/* ************ Stroke Hide selection Toggle ************** */

static int gpencil_hideselect_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
  View3D *v3d = CTX_wm_view3d(C);
  if (v3d == NULL) {
    return OPERATOR_CANCELLED;
  }

  /* Just toggle alpha... */
  if (v3d->vertex_opacity > 0.0f) {
    v3d->vertex_opacity = 0.0f;
  }
  else {
    v3d->vertex_opacity = 1.0f;
  }

  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA, NULL);
  WM_event_add_notifier(C, NC_GPENCIL | ND_GPENCIL_EDITMODE, NULL);
  WM_event_add_notifier(C, NC_SCENE | ND_MODE, NULL);

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
  ot->poll = gp_stroke_edit_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;
}

/* ************** Duplicate Selected Strokes **************** */

/* Make copies of selected point segments in a selected stroke */
static void gp_duplicate_points(const bGPDstroke *gps,
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
    else {
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
      // printf("copying from %d to %d = %d\n", start_idx, i, len);

      /* make copies of the relevant data */
      if (len) {
        bGPDstroke *gpsd;

        /* make a stupid copy first of the entire stroke (to get the flags too) */
        gpsd = MEM_dupallocN(gps);

        /* saves original layer name */
        BLI_strncpy(gpsd->runtime.tmp_layerinfo, layername, sizeof(gpsd->runtime.tmp_layerinfo));

        /* initialize triangle memory - will be calculated on next redraw */
        gpsd->triangles = NULL;
        gpsd->flag |= GP_STROKE_RECALC_GEOMETRY;
        gpsd->tot_triangles = 0;

        /* now, make a new points array, and copy of the relevant parts */
        gpsd->points = MEM_callocN(sizeof(bGPDspoint) * len, "gps stroke points copy");
        memcpy(gpsd->points, gps->points + start_idx, sizeof(bGPDspoint) * len);
        gpsd->totpoints = len;

        if (gps->dvert != NULL) {
          gpsd->dvert = MEM_callocN(sizeof(MDeformVert) * len, "gps stroke weights copy");
          memcpy(gpsd->dvert, gps->dvert + start_idx, sizeof(MDeformVert) * len);

          /* Copy weights */
          int e = start_idx;
          for (int j = 0; j < gpsd->totpoints; j++) {
            MDeformVert *dvert_dst = &gps->dvert[e];
            MDeformVert *dvert_src = &gps->dvert[j];
            dvert_dst->dw = MEM_dupallocN(dvert_src->dw);
            e++;
          }
        }

        /* add to temp buffer */
        gpsd->next = gpsd->prev = NULL;
        BLI_addtail(new_strokes, gpsd);

        /* cleanup + reset for next */
        start_idx = -1;
      }
    }
  }
}

static int gp_duplicate_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  if (gpd == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
    return OPERATOR_CANCELLED;
  }

  if (GPENCIL_MULTIEDIT_SESSIONS_ON(gpd)) {
    BKE_report(op->reports, RPT_ERROR, "Operator not supported in multiframe edition");
    return OPERATOR_CANCELLED;
  }

  /* for each visible (and editable) layer's selected strokes,
   * copy the strokes into a temporary buffer, then append
   * once all done
   */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    ListBase new_strokes = {NULL, NULL};
    bGPDframe *gpf = gpl->actframe;
    bGPDstroke *gps;

    if (gpf == NULL) {
      continue;
    }

    /* make copies of selected strokes, and deselect these once we're done */
    for (gps = gpf->strokes.first; gps; gps = gps->next) {
      /* skip strokes that are invalid for current view */
      if (ED_gpencil_stroke_can_use(C, gps) == false) {
        continue;
      }

      if (gps->flag & GP_STROKE_SELECT) {
        if (gps->totpoints == 1) {
          /* Special Case: If there's just a single point in this stroke... */
          bGPDstroke *gpsd;

          /* make direct copies of the stroke and its points */
          gpsd = MEM_dupallocN(gps);
          BLI_strncpy(gpsd->runtime.tmp_layerinfo, gpl->info, sizeof(gpsd->runtime.tmp_layerinfo));
          gpsd->points = MEM_dupallocN(gps->points);
          if (gps->dvert != NULL) {
            gpsd->dvert = MEM_dupallocN(gps->dvert);
            BKE_gpencil_stroke_weights_duplicate(gps, gpsd);
          }

          /* triangle information - will be calculated on next redraw */
          gpsd->flag |= GP_STROKE_RECALC_GEOMETRY;
          gpsd->triangles = NULL;

          /* add to temp buffer */
          gpsd->next = gpsd->prev = NULL;
          BLI_addtail(&new_strokes, gpsd);
        }
        else {
          /* delegate to a helper, as there's too much to fit in here (for copying subsets)... */
          gp_duplicate_points(gps, &new_strokes, gpl->info);
        }

        /* deselect original stroke, or else the originals get moved too
         * (when using the copy + move macro)
         */
        gps->flag &= ~GP_STROKE_SELECT;
      }
    }

    /* add all new strokes in temp buffer to the frame (preventing double-copies) */
    BLI_movelisttolist(&gpf->strokes, &new_strokes);
    BLI_assert(new_strokes.first == NULL);
  }
  CTX_DATA_END;

  /* updates */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_duplicate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Duplicate Strokes";
  ot->idname = "GPENCIL_OT_duplicate";
  ot->description = "Duplicate the selected Grease Pencil strokes";

  /* callbacks */
  ot->exec = gp_duplicate_exec;
  ot->poll = gp_stroke_edit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ************** Extrude Selected Strokes **************** */

/* helper to copy a point to temp area */
static void copy_move_point(bGPDstroke *gps,
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

  if (gps->dvert != NULL) {
    MDeformVert *dvert = &temp_dverts[from_idx];
    MDeformVert *dvert_final = &gps->dvert[to_idx];

    dvert_final->totweight = dvert->totweight;
    /* if copy, duplicate memory, otherwise move only the pointer */
    if (copy) {
      dvert_final->dw = MEM_dupallocN(dvert->dw);
    }
    else {
      dvert_final->dw = dvert->dw;
    }
  }
}

static void gpencil_add_move_points(bGPDframe *gpf, bGPDstroke *gps)
{
  bGPDspoint *temp_points = NULL;
  MDeformVert *temp_dverts = NULL;
  bGPDspoint *pt = NULL;
  const bGPDspoint *pt_start = &gps->points[0];
  const bGPDspoint *pt_last = &gps->points[gps->totpoints - 1];
  const bool do_first = (pt_start->flag & GP_SPOINT_SELECT);
  const bool do_last = ((pt_last->flag & GP_SPOINT_SELECT) && (pt_start != pt_last));
  const bool do_stroke = (do_first || do_last);

  /* review points in the middle of stroke to create new strokes */
  for (int i = 0; i < gps->totpoints; i++) {
    /* skip first and last point */
    if ((i == 0) || (i == gps->totpoints - 1)) {
      continue;
    }

    pt = &gps->points[i];
    if (pt->flag == GP_SPOINT_SELECT) {
      /* duplicate original stroke data */
      bGPDstroke *gps_new = MEM_dupallocN(gps);
      gps_new->prev = gps_new->next = NULL;

      /* add new points array */
      gps_new->totpoints = 1;
      gps_new->points = MEM_callocN(sizeof(bGPDspoint), __func__);
      gps_new->dvert = NULL;

      if (gps->dvert != NULL) {
        gps_new->dvert = MEM_callocN(sizeof(MDeformVert), __func__);
      }

      gps->flag |= GP_STROKE_RECALC_GEOMETRY;
      gps_new->triangles = NULL;
      gps_new->tot_triangles = 0;
      BLI_insertlinkafter(&gpf->strokes, gps, gps_new);

      /* copy selected point data to new stroke */
      copy_move_point(gps_new, gps->points, gps->dvert, i, 0, true);

      /* deselect orinal point */
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
    temp_points = MEM_dupallocN(gps->points);
    oldtotpoints = gps->totpoints;
    if (gps->dvert != NULL) {
      temp_dverts = MEM_dupallocN(gps->dvert);
    }

    /* if first point, need move all one position */
    if (do_first) {
      i2 = 1;
    }

    /* resize the points arrays */
    gps->totpoints = totnewpoints;
    gps->points = MEM_recallocN(gps->points, sizeof(*gps->points) * gps->totpoints);
    if (gps->dvert != NULL) {
      gps->dvert = MEM_recallocN(gps->dvert, sizeof(*gps->dvert) * gps->totpoints);
    }

    /* move points to new position */
    for (int i = 0; i < oldtotpoints; i++) {
      copy_move_point(gps, temp_points, temp_dverts, i, i2, false);
      i2++;
    }
    gps->flag |= GP_STROKE_RECALC_GEOMETRY;

    /* If first point, add new point at the beginning. */
    if (do_first) {
      copy_move_point(gps, temp_points, temp_dverts, 0, 0, true);
      /* deselect old */
      pt = &gps->points[1];
      pt->flag &= ~GP_SPOINT_SELECT;
      /* select new */
      pt = &gps->points[0];
      pt->flag |= GP_SPOINT_SELECT;
    }

    /* if last point, add new point at the end */
    if (do_last) {
      copy_move_point(gps, temp_points, temp_dverts, oldtotpoints - 1, gps->totpoints - 1, true);

      /* deselect old */
      pt = &gps->points[gps->totpoints - 2];
      pt->flag &= ~GP_SPOINT_SELECT;
      /* select new */
      pt = &gps->points[gps->totpoints - 1];
      pt->flag |= GP_SPOINT_SELECT;
    }

    MEM_SAFE_FREE(temp_points);
    MEM_SAFE_FREE(temp_dverts);
  }

  /* if the stroke is not reused, deselect */
  if (!do_stroke) {
    gps->flag &= ~GP_STROKE_SELECT;
  }
}

static int gp_extrude_exec(bContext *C, wmOperator *op)
{
  Object *obact = CTX_data_active_object(C);
  bGPdata *gpd = (bGPdata *)obact->data;
  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);
  bGPDstroke *gps = NULL;

  if (gpd == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
    return OPERATOR_CANCELLED;
  }

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = (is_multiedit) ? gpl->frames.first : gpl->actframe;

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == NULL) {
          continue;
        }

        for (gps = gpf->strokes.first; gps; gps = gps->next) {
          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }

          if (gps->flag & GP_STROKE_SELECT) {
            gpencil_add_move_points(gpf, gps);
          }
        }
        /* if not multiedit, exit loop*/
        if (!is_multiedit) {
          break;
        }
      }
    }
  }
  CTX_DATA_END;

  /* updates */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);
  DEG_id_tag_update(&obact->id, ID_RECALC_COPY_ON_WRITE);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_extrude(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Extrude Stroke Points";
  ot->idname = "GPENCIL_OT_extrude";
  ot->description = "Extrude the selected Grease Pencil points";

  /* callbacks */
  ot->exec = gp_extrude_exec;
  ot->poll = gp_stroke_edit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************* Copy/Paste Strokes ************************* */
/* Grease Pencil stroke data copy/paste buffer:
 * - The copy operation collects all segments of selected strokes,
 *   dumping "ready to be copied" copies of the strokes into the buffer.
 * - The paste operation makes a copy of those elements, and adds them
 *   to the active layer. This effectively flattens down the strokes
 *   from several different layers into a single layer.
 */

/* list of bGPDstroke instances */
/* NOTE: is exposed within the editors/gpencil module so that other tools can use it too */
ListBase gp_strokes_copypastebuf = {NULL, NULL};

/* Hash for hanging on to all the colors used by strokes in the buffer
 *
 * This is needed to prevent dangling and unsafe pointers when pasting across data-blocks,
 * or after a color used by a stroke in the buffer gets deleted (via user action or undo).
 */
static GHash *gp_strokes_copypastebuf_colors = NULL;

static GHash *gp_strokes_copypastebuf_colors_material_to_name_create(Main *bmain)
{
  GHash *ma_to_name = BLI_ghash_ptr_new(__func__);

  for (Material *ma = bmain->materials.first; ma != NULL; ma = ma->id.next) {
    char *name = BKE_id_to_unique_string_key(&ma->id);
    BLI_ghash_insert(ma_to_name, ma, name);
  }

  return ma_to_name;
}

static void gp_strokes_copypastebuf_colors_material_to_name_free(GHash *ma_to_name)
{
  BLI_ghash_free(ma_to_name, NULL, MEM_freeN);
}

static GHash *gp_strokes_copypastebuf_colors_name_to_material_create(Main *bmain)
{
  GHash *name_to_ma = BLI_ghash_str_new(__func__);

  for (Material *ma = bmain->materials.first; ma != NULL; ma = ma->id.next) {
    char *name = BKE_id_to_unique_string_key(&ma->id);
    BLI_ghash_insert(name_to_ma, name, ma);
  }

  return name_to_ma;
}

static void gp_strokes_copypastebuf_colors_name_to_material_free(GHash *name_to_ma)
{
  BLI_ghash_free(name_to_ma, MEM_freeN, NULL);
}

/* Free copy/paste buffer data */
void ED_gpencil_strokes_copybuf_free(void)
{
  bGPDstroke *gps, *gpsn;

  /* Free the colors buffer
   * NOTE: This is done before the strokes so that the ptrs are still safe
   */
  if (gp_strokes_copypastebuf_colors) {
    BLI_ghash_free(gp_strokes_copypastebuf_colors, NULL, MEM_freeN);
    gp_strokes_copypastebuf_colors = NULL;
  }

  /* Free the stroke buffer */
  for (gps = gp_strokes_copypastebuf.first; gps; gps = gpsn) {
    gpsn = gps->next;

    if (gps->points) {
      MEM_freeN(gps->points);
    }
    if (gps->dvert) {
      BKE_gpencil_free_stroke_weights(gps);
      MEM_freeN(gps->dvert);
    }

    MEM_SAFE_FREE(gps->triangles);

    BLI_freelinkN(&gp_strokes_copypastebuf, gps);
  }

  gp_strokes_copypastebuf.first = gp_strokes_copypastebuf.last = NULL;
}

/**
 * Ensure that destination datablock has all the colors the pasted strokes need.
 * Helper function for copy-pasting strokes
 */
GHash *gp_copybuf_validate_colormap(bContext *C)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  GHash *new_colors = BLI_ghash_int_new("GPencil Paste Dst Colors");
  GHashIterator gh_iter;

  /* For each color, check if exist and add if not */
  GHash *name_to_ma = gp_strokes_copypastebuf_colors_name_to_material_create(bmain);

  GHASH_ITER (gh_iter, gp_strokes_copypastebuf_colors) {
    int *key = BLI_ghashIterator_getKey(&gh_iter);
    char *ma_name = BLI_ghashIterator_getValue(&gh_iter);
    Material *ma = BLI_ghash_lookup(name_to_ma, ma_name);

    BKE_gpencil_object_material_ensure(bmain, ob, ma);

    /* Store this mapping (for use later when pasting) */
    if (!BLI_ghash_haskey(new_colors, POINTER_FROM_INT(*key))) {
      BLI_ghash_insert(new_colors, POINTER_FROM_INT(*key), ma);
    }
  }

  gp_strokes_copypastebuf_colors_name_to_material_free(name_to_ma);

  return new_colors;
}

/* --------------------- */
/* Copy selected strokes */

static int gp_strokes_copy_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  if (gpd == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
    return OPERATOR_CANCELLED;
  }

  if (GPENCIL_MULTIEDIT_SESSIONS_ON(gpd)) {
    BKE_report(op->reports, RPT_ERROR, "Operator not supported in multiframe edition");
    return OPERATOR_CANCELLED;
  }

  /* clear the buffer first */
  ED_gpencil_strokes_copybuf_free();

  /* for each visible (and editable) layer's selected strokes,
   * copy the strokes into a temporary buffer, then append
   * once all done
   */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *gpf = gpl->actframe;
    bGPDstroke *gps;

    if (gpf == NULL) {
      continue;
    }

    /* make copies of selected strokes, and deselect these once we're done */
    for (gps = gpf->strokes.first; gps; gps = gps->next) {
      /* skip strokes that are invalid for current view */
      if (ED_gpencil_stroke_can_use(C, gps) == false) {
        continue;
      }

      if (gps->flag & GP_STROKE_SELECT) {
        if (gps->totpoints == 1) {
          /* Special Case: If there's just a single point in this stroke... */
          bGPDstroke *gpsd;

          /* make direct copies of the stroke and its points */
          gpsd = MEM_dupallocN(gps);
          /* saves original layer name */
          BLI_strncpy(gpsd->runtime.tmp_layerinfo, gpl->info, sizeof(gpsd->runtime.tmp_layerinfo));
          gpsd->points = MEM_dupallocN(gps->points);
          if (gps->dvert != NULL) {
            gpsd->dvert = MEM_dupallocN(gps->dvert);
            BKE_gpencil_stroke_weights_duplicate(gps, gpsd);
          }

          /* triangles cache - will be recalculated on next redraw */
          gpsd->flag |= GP_STROKE_RECALC_GEOMETRY;
          gpsd->tot_triangles = 0;
          gpsd->triangles = NULL;

          /* add to temp buffer */
          gpsd->next = gpsd->prev = NULL;
          BLI_addtail(&gp_strokes_copypastebuf, gpsd);
        }
        else {
          /* delegate to a helper, as there's too much to fit in here (for copying subsets)... */
          gp_duplicate_points(gps, &gp_strokes_copypastebuf, gpl->info);
        }
      }
    }
  }
  CTX_DATA_END;

  /* Build up hash of material colors used in these strokes */
  if (gp_strokes_copypastebuf.first) {
    gp_strokes_copypastebuf_colors = BLI_ghash_int_new("GPencil CopyBuf Colors");
    GHash *ma_to_name = gp_strokes_copypastebuf_colors_material_to_name_create(bmain);
    for (bGPDstroke *gps = gp_strokes_copypastebuf.first; gps; gps = gps->next) {
      if (ED_gpencil_stroke_can_use(C, gps)) {
        char **ma_name_val;
        if (!BLI_ghash_ensure_p(
                gp_strokes_copypastebuf_colors, &gps->mat_nr, (void ***)&ma_name_val)) {
          Material *ma = give_current_material(ob, gps->mat_nr + 1);
          char *ma_name = BLI_ghash_lookup(ma_to_name, ma);
          *ma_name_val = MEM_dupallocN(ma_name);
        }
      }
    }
    gp_strokes_copypastebuf_colors_material_to_name_free(ma_to_name);
  }

  /* updates (to ensure operator buttons are refreshed, when used via hotkeys) */
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA, NULL);  // XXX?

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
  ot->exec = gp_strokes_copy_exec;
  ot->poll = gp_stroke_edit_poll;

  /* flags */
  // ot->flag = OPTYPE_REGISTER;
}

/* --------------------- */
/* Paste selected strokes */

static bool gp_strokes_paste_poll(bContext *C)
{
  /* 1) Must have GP datablock to paste to
   *    - We don't need to have an active layer though, as that can easily get added
   *    - If the active layer is locked, we can't paste there,
   *      but that should prompt a warning instead.
   * 2) Copy buffer must at least have something (though it may be the wrong sort...).
   */
  return (ED_gpencil_data_get_active(C) != NULL) &&
         (!BLI_listbase_is_empty(&gp_strokes_copypastebuf));
}

typedef enum eGP_PasteMode {
  GP_COPY_ONLY = -1,
  GP_COPY_MERGE = 1,
} eGP_PasteMode;

static int gp_strokes_paste_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bGPDlayer *gpl = CTX_data_active_gpencil_layer(C); /* only use active for copy merge */
  Scene *scene = CTX_data_scene(C);
  int cfra_eval = CFRA;
  bGPDframe *gpf;

  eGP_PasteMode type = RNA_enum_get(op->ptr, "type");
  GHash *new_colors;

  /* check for various error conditions */
  if (gpd == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
    return OPERATOR_CANCELLED;
  }
  else if (GPENCIL_MULTIEDIT_SESSIONS_ON(gpd)) {
    BKE_report(op->reports, RPT_ERROR, "Operator not supported in multiframe edition");
    return OPERATOR_CANCELLED;
  }
  else if (BLI_listbase_is_empty(&gp_strokes_copypastebuf)) {
    BKE_report(op->reports,
               RPT_ERROR,
               "No strokes to paste, select and copy some points before trying again");
    return OPERATOR_CANCELLED;
  }
  else if (gpl == NULL) {
    /* no active layer - let's just create one */
    gpl = BKE_gpencil_layer_addnew(gpd, DATA_("GP_Layer"), true);
  }
  else if ((gpencil_layer_is_editable(gpl) == false) && (type == GP_COPY_MERGE)) {
    BKE_report(
        op->reports, RPT_ERROR, "Can not paste strokes when active layer is hidden or locked");
    return OPERATOR_CANCELLED;
  }
  else {
    /* Check that some of the strokes in the buffer can be used */
    bGPDstroke *gps;
    bool ok = false;

    for (gps = gp_strokes_copypastebuf.first; gps; gps = gps->next) {
      if (ED_gpencil_stroke_can_use(C, gps)) {
        ok = true;
        break;
      }
    }

    if (ok == false) {
      /* XXX: this check is not 100% accurate
       * (i.e. image editor is incompatible with normal 2D strokes),
       * but should be enough to give users a good idea of what's going on.
       */
      if (CTX_wm_area(C)->spacetype == SPACE_VIEW3D) {
        BKE_report(op->reports, RPT_ERROR, "Cannot paste 2D strokes in 3D View");
      }
      else {
        BKE_report(op->reports, RPT_ERROR, "Cannot paste 3D strokes in 2D editors");
      }

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
  }
  CTX_DATA_END;

  /* Ensure that all the necessary colors exist */
  new_colors = gp_copybuf_validate_colormap(C);

  /* Copy over the strokes from the buffer (and adjust the colors) */
  for (bGPDstroke *gps = gp_strokes_copypastebuf.first; gps; gps = gps->next) {
    if (ED_gpencil_stroke_can_use(C, gps)) {
      /* Need to verify if layer exists */
      if (type != GP_COPY_MERGE) {
        gpl = BLI_findstring(&gpd->layers, gps->runtime.tmp_layerinfo, offsetof(bGPDlayer, info));
        if (gpl == NULL) {
          /* no layer - use active (only if layer deleted before paste) */
          gpl = CTX_data_active_gpencil_layer(C);
        }
      }

      /* Ensure we have a frame to draw into
       * NOTE: Since this is an op which creates strokes,
       *       we are obliged to add a new frame if one
       *       doesn't exist already
       */
      gpf = BKE_gpencil_layer_getframe(gpl, cfra_eval, GP_GETFRAME_ADD_NEW);
      if (gpf) {
        /* Create new stroke */
        bGPDstroke *new_stroke = MEM_dupallocN(gps);
        new_stroke->runtime.tmp_layerinfo[0] = '\0';

        new_stroke->points = MEM_dupallocN(gps->points);
        if (gps->dvert != NULL) {
          new_stroke->dvert = MEM_dupallocN(gps->dvert);
          BKE_gpencil_stroke_weights_duplicate(gps, new_stroke);
        }
        new_stroke->flag |= GP_STROKE_RECALC_GEOMETRY;
        new_stroke->triangles = NULL;

        new_stroke->next = new_stroke->prev = NULL;
        BLI_addtail(&gpf->strokes, new_stroke);

        /* Remap material */
        Material *ma = BLI_ghash_lookup(new_colors, POINTER_FROM_INT(new_stroke->mat_nr));
        new_stroke->mat_nr = BKE_gpencil_object_material_get_index(ob, ma);
        BLI_assert(new_stroke->mat_nr >= 0); /* have to add the material first */
      }
    }
  }

  /* free temp data */
  BLI_ghash_free(new_colors, NULL, NULL);

  /* updates */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_paste(wmOperatorType *ot)
{
  static const EnumPropertyItem copy_type[] = {
      {GP_COPY_ONLY, "COPY", 0, "Copy", ""},
      {GP_COPY_MERGE, "MERGE", 0, "Merge", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Paste Strokes";
  ot->idname = "GPENCIL_OT_paste";
  ot->description = "Paste previously copied strokes or copy and merge in active layer";

  /* callbacks */
  ot->exec = gp_strokes_paste_exec;
  ot->poll = gp_strokes_paste_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_USE_EVAL_DATA;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", copy_type, 0, "Type", "");
}

/* ******************* Move To Layer ****************************** */

static int gp_move_to_layer_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(evt))
{
  uiPopupMenu *pup;
  uiLayout *layout;

  /* call the menu, which will call this operator again, hence the canceled */
  pup = UI_popup_menu_begin(C, op->type->name, ICON_NONE);
  layout = UI_popup_menu_layout(pup);
  uiItemsEnumO(layout, "GPENCIL_OT_move_to_layer", "layer");
  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

// FIXME: allow moving partial strokes
static int gp_move_to_layer_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = CTX_data_gpencil_data(C);
  Scene *scene = CTX_data_scene(C);
  int cfra_eval = CFRA;
  bGPDlayer *target_layer = NULL;
  ListBase strokes = {NULL, NULL};
  int layer_num = RNA_enum_get(op->ptr, "layer");
  const bool use_autolock = (bool)(gpd->flag & GP_DATA_AUTOLOCK_LAYERS);

  if (GPENCIL_MULTIEDIT_SESSIONS_ON(gpd)) {
    BKE_report(op->reports, RPT_ERROR, "Operator not supported in multiframe edition");
    return OPERATOR_CANCELLED;
  }

  /* if autolock enabled, disabled now */
  if (use_autolock) {
    gpd->flag &= ~GP_DATA_AUTOLOCK_LAYERS;
  }

  /* Get layer or create new one */
  if (layer_num == -1) {
    /* Create layer */
    target_layer = BKE_gpencil_layer_addnew(gpd, DATA_("GP_Layer"), true);
  }
  else {
    /* Try to get layer */
    target_layer = BLI_findlink(&gpd->layers, layer_num);

    if (target_layer == NULL) {
      /* back autolock status */
      if (use_autolock) {
        gpd->flag |= GP_DATA_AUTOLOCK_LAYERS;
      }
      BKE_reportf(op->reports, RPT_ERROR, "There is no layer number %d", layer_num);
      return OPERATOR_CANCELLED;
    }
  }

  /* Extract all strokes to move to this layer
   * NOTE: We need to do this in a two-pass system to avoid conflicts with strokes
   *       getting repeatedly moved
   */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *gpf = gpl->actframe;
    bGPDstroke *gps, *gpsn;

    /* skip if no frame with strokes, or if this is the layer we're moving strokes to */
    if ((gpl == target_layer) || (gpf == NULL)) {
      continue;
    }

    /* make copies of selected strokes, and deselect these once we're done */
    for (gps = gpf->strokes.first; gps; gps = gpsn) {
      gpsn = gps->next;

      /* skip strokes that are invalid for current view */
      if (ED_gpencil_stroke_can_use(C, gps) == false) {
        continue;
      }

      /* TODO: Don't just move entire strokes - instead, only copy the selected portions... */
      if (gps->flag & GP_STROKE_SELECT) {
        BLI_remlink(&gpf->strokes, gps);
        BLI_addtail(&strokes, gps);
      }
    }

    /* if new layer and autolock, lock old layer */
    if ((layer_num == -1) && (use_autolock)) {
      gpl->flag |= GP_LAYER_LOCKED;
    }
  }
  CTX_DATA_END;

  /* Paste them all in one go */
  if (strokes.first) {
    bGPDframe *gpf = BKE_gpencil_layer_getframe(target_layer, cfra_eval, GP_GETFRAME_ADD_NEW);

    BLI_movelisttolist(&gpf->strokes, &strokes);
    BLI_assert((strokes.first == strokes.last) && (strokes.first == NULL));
  }

  /* back autolock status */
  if (use_autolock) {
    gpd->flag |= GP_DATA_AUTOLOCK_LAYERS;
  }

  /* updates */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_move_to_layer(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Move Strokes to Layer";
  ot->idname = "GPENCIL_OT_move_to_layer";
  ot->description =
      "Move selected strokes to another layer";  // XXX: allow moving individual points too?

  /* callbacks */
  ot->invoke = gp_move_to_layer_invoke;
  ot->exec = gp_move_to_layer_exec;
  ot->poll = gp_stroke_edit_poll;  // XXX?

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* gp layer to use (dynamic enum) */
  ot->prop = RNA_def_enum(ot->srna, "layer", DummyRNA_DEFAULT_items, 0, "Grease Pencil Layer", "");
  RNA_def_enum_funcs(ot->prop, ED_gpencil_layers_with_new_enum_itemf);
}

/* ********************* Add Blank Frame *************************** */

/* Basically the same as the drawing op */
static bool UNUSED_FUNCTION(gp_blank_frame_add_poll)(bContext *C)
{
  if (ED_operator_regionactive(C)) {
    /* check if current context can support GPencil data */
    if (ED_gpencil_data_get_pointers(C, NULL) != NULL) {
      return 1;
    }
    else {
      CTX_wm_operator_poll_msg_set(C, "Failed to find Grease Pencil data to draw into");
    }
  }
  else {
    CTX_wm_operator_poll_msg_set(C, "Active region not set");
  }

  return 0;
}

static int gp_blank_frame_add_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  Scene *scene = CTX_data_scene(C);
  int cfra_eval = CFRA;

  bGPDlayer *active_gpl = BKE_gpencil_layer_getactive(gpd);

  const bool all_layers = RNA_boolean_get(op->ptr, "all_layers");

  /* Initialise datablock and an active layer if nothing exists yet */
  if (ELEM(NULL, gpd, active_gpl)) {
    /* Let's just be lazy, and call the "Add New Layer" operator,
     * which sets everything up as required. */
    WM_operator_name_call(C, "GPENCIL_OT_layer_add", WM_OP_EXEC_DEFAULT, NULL);
  }

  /* Go through each layer, adding a frame after the active one
   * and/or shunting all the others out of the way
   */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    if ((all_layers == false) && (gpl != active_gpl)) {
      continue;
    }

    /* 1) Check for an existing frame on the current frame */
    bGPDframe *gpf = BKE_gpencil_layer_find_frame(gpl, cfra_eval);
    if (gpf) {
      /* Shunt all frames after (and including) the existing one later by 1-frame */
      for (; gpf; gpf = gpf->next) {
        gpf->framenum += 1;
      }
    }

    /* 2) Now add a new frame, with nothing in it */
    gpl->actframe = BKE_gpencil_layer_getframe(gpl, cfra_eval, GP_GETFRAME_ADD_NEW);
  }
  CTX_DATA_END;

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_blank_frame_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Insert Blank Frame";
  ot->idname = "GPENCIL_OT_blank_frame_add";
  ot->description =
      "Insert a blank frame on the current frame "
      "(all subsequently existing frames, if any, are shifted right by one frame)";

  /* callbacks */
  ot->exec = gp_blank_frame_add_exec;
  ot->poll = gp_add_poll;

  /* properties */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  RNA_def_boolean(ot->srna,
                  "all_layers",
                  false,
                  "All Layers",
                  "Create blank frame in all layers, not only active");
}

/* ******************* Delete Active Frame ************************ */

static bool gp_actframe_delete_poll(bContext *C)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bGPDlayer *gpl = BKE_gpencil_layer_getactive(gpd);

  /* only if there's an active layer with an active frame */
  return (gpl && gpl->actframe);
}

/* delete active frame - wrapper around API calls */
static int gp_actframe_delete_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bGPDlayer *gpl = BKE_gpencil_layer_getactive(gpd);

  Scene *scene = CTX_data_scene(C);
  int cfra_eval = CFRA;

  bGPDframe *gpf = BKE_gpencil_layer_getframe(gpl, cfra_eval, GP_GETFRAME_USE_PREV);

  /* if there's no existing Grease-Pencil data there, add some */
  if (gpd == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No grease pencil data");
    return OPERATOR_CANCELLED;
  }
  if (ELEM(NULL, gpl, gpf)) {
    BKE_report(op->reports, RPT_ERROR, "No active frame to delete");
    return OPERATOR_CANCELLED;
  }

  /* delete it... */
  BKE_gpencil_layer_delframe(gpl, gpf);

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

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
  ot->exec = gp_actframe_delete_exec;
  ot->poll = gp_actframe_delete_poll;
}

/* **************** Delete All Active Frames ****************** */

static bool gp_actframe_delete_all_poll(bContext *C)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  /* 1) There must be grease pencil data
   * 2) Hopefully some of the layers have stuff we can use
   */
  return (gpd && gpd->layers.first);
}

static int gp_actframe_delete_all_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  Scene *scene = CTX_data_scene(C);
  int cfra_eval = CFRA;

  bool success = false;

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    /* try to get the "active" frame - but only if it actually occurs on this frame */
    bGPDframe *gpf = BKE_gpencil_layer_getframe(gpl, cfra_eval, GP_GETFRAME_USE_PREV);

    if (gpf == NULL) {
      continue;
    }

    /* delete it... */
    BKE_gpencil_layer_delframe(gpl, gpf);

    /* we successfully modified something */
    success = true;
  }
  CTX_DATA_END;

  /* updates */
  if (success) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
    return OPERATOR_FINISHED;
  }
  else {
    BKE_report(op->reports, RPT_ERROR, "No active frame(s) to delete");
    return OPERATOR_CANCELLED;
  }
}

void GPENCIL_OT_active_frames_delete_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete All Active Frames";
  ot->idname = "GPENCIL_OT_active_frames_delete_all";
  ot->description = "Delete the active frame(s) of all editable Grease Pencil layers";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* callbacks */
  ot->exec = gp_actframe_delete_all_exec;
  ot->poll = gp_actframe_delete_all_poll;
}

/* ******************* Delete Operator ************************ */

typedef enum eGP_DeleteMode {
  /* delete selected stroke points */
  GP_DELETEOP_POINTS = 0,
  /* delete selected strokes */
  GP_DELETEOP_STROKES = 1,
  /* delete active frame */
  GP_DELETEOP_FRAME = 2,
} eGP_DeleteMode;

typedef enum eGP_DissolveMode {
  /* dissolve all selected points */
  GP_DISSOLVE_POINTS = 0,
  /* dissolve between selected points */
  GP_DISSOLVE_BETWEEN = 1,
  /* dissolve unselected points */
  GP_DISSOLVE_UNSELECT = 2,
} eGP_DissolveMode;

/* ----------------------------------- */

/* Delete selected strokes */
static int gp_delete_selected_strokes(bContext *C)
{
  bool changed = false;
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = (is_multiedit) ? gpl->frames.first : gpl->actframe;

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        bGPDstroke *gps, *gpsn;

        if (gpf == NULL) {
          continue;
        }

        /* simply delete strokes which are selected */
        for (gps = gpf->strokes.first; gps; gps = gpsn) {
          gpsn = gps->next;

          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }

          /* free stroke if selected */
          if (gps->flag & GP_STROKE_SELECT) {
            /* free stroke memory arrays, then stroke itself */
            if (gps->points) {
              MEM_freeN(gps->points);
            }
            if (gps->dvert) {
              BKE_gpencil_free_stroke_weights(gps);
              MEM_freeN(gps->dvert);
            }
            MEM_SAFE_FREE(gps->triangles);
            BLI_freelinkN(&gpf->strokes, gps);

            changed = true;
          }
        }
      }
    }
  }
  CTX_DATA_END;

  if (changed) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

/* ----------------------------------- */

/* Delete selected points but keep the stroke */
static int gp_dissolve_selected_points(bContext *C, eGP_DissolveMode mode)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);
  bool changed = false;
  int first = 0;
  int last = 0;

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = (is_multiedit) ? gpl->frames.first : gpl->actframe;

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {

        bGPDstroke *gps, *gpsn;

        if (gpf == NULL) {
          continue;
        }

        /* simply delete points from selected strokes
         * NOTE: we may still have to remove the stroke if it ends up having no points!
         */
        for (gps = gpf->strokes.first; gps; gps = gpsn) {
          gpsn = gps->next;

          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }
          /* check if the color is editable */
          if (ED_gpencil_stroke_color_use(ob, gpl, gps) == false) {
            continue;
          }

          /* the stroke must have at least one point selected for any operator */
          if (gps->flag & GP_STROKE_SELECT) {
            bGPDspoint *pt;
            MDeformVert *dvert = NULL;
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
              if (gps->points) {
                MEM_freeN(gps->points);
              }
              if (gps->dvert) {
                BKE_gpencil_free_stroke_weights(gps);
                MEM_freeN(gps->dvert);
              }
              if (gps->triangles) {
                MEM_freeN(gps->triangles);
              }
              BLI_freelinkN(&gpf->strokes, gps);
              DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
            }
            else {
              /* just copy all points to keep into a smaller buffer */
              bGPDspoint *new_points = MEM_callocN(sizeof(bGPDspoint) * tot,
                                                   "new gp stroke points copy");
              bGPDspoint *npt = new_points;

              MDeformVert *new_dvert = NULL;
              MDeformVert *ndvert = NULL;

              if (gps->dvert != NULL) {
                new_dvert = MEM_callocN(sizeof(MDeformVert) * tot, "new gp stroke weights copy");
                ndvert = new_dvert;
              }

              switch (mode) {
                case GP_DISSOLVE_POINTS:
                  (gps->dvert != NULL) ? dvert = gps->dvert : NULL;
                  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
                    if ((pt->flag & GP_SPOINT_SELECT) == 0) {
                      *npt = *pt;
                      npt++;

                      if (gps->dvert != NULL) {
                        *ndvert = *dvert;
                        ndvert->dw = MEM_dupallocN(dvert->dw);
                        ndvert++;
                        dvert++;
                      }
                    }
                  }
                  break;
                case GP_DISSOLVE_BETWEEN:
                  /* copy first segment */
                  (gps->dvert != NULL) ? dvert = gps->dvert : NULL;
                  for (i = 0, pt = gps->points; i < first; i++, pt++) {
                    *npt = *pt;
                    npt++;

                    if (gps->dvert != NULL) {
                      *ndvert = *dvert;
                      ndvert->dw = MEM_dupallocN(dvert->dw);
                      ndvert++;
                      dvert++;
                    }
                  }
                  /* copy segment (selected points) */
                  (gps->dvert != NULL) ? dvert = gps->dvert + first : NULL;
                  for (i = first, pt = gps->points + first; i < last; i++, pt++) {
                    if (pt->flag & GP_SPOINT_SELECT) {
                      *npt = *pt;
                      npt++;

                      if (gps->dvert != NULL) {
                        *ndvert = *dvert;
                        ndvert->dw = MEM_dupallocN(dvert->dw);
                        ndvert++;
                        dvert++;
                      }
                    }
                  }
                  /* copy last segment */
                  (gps->dvert != NULL) ? dvert = gps->dvert + last : NULL;
                  for (i = last, pt = gps->points + last; i < gps->totpoints; i++, pt++) {
                    *npt = *pt;
                    npt++;

                    if (gps->dvert != NULL) {
                      *ndvert = *dvert;
                      ndvert->dw = MEM_dupallocN(dvert->dw);
                      ndvert++;
                      dvert++;
                    }
                  }

                  break;
                case GP_DISSOLVE_UNSELECT:
                  /* copy any selected point */
                  (gps->dvert != NULL) ? dvert = gps->dvert : NULL;
                  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
                    if (pt->flag & GP_SPOINT_SELECT) {
                      *npt = *pt;
                      npt++;

                      if (gps->dvert != NULL) {
                        *ndvert = *dvert;
                        ndvert->dw = MEM_dupallocN(dvert->dw);
                        ndvert++;
                        dvert++;
                      }
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

              /* triangles cache needs to be recalculated */
              gps->flag |= GP_STROKE_RECALC_GEOMETRY;
              gps->tot_triangles = 0;

              /* deselect the stroke, since none of its selected points will still be selected */
              gps->flag &= ~GP_STROKE_SELECT;
              for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
                pt->flag &= ~GP_SPOINT_SELECT;
              }
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
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

/* ----------------------------------- */

/* Temp data for storing information about an "island" of points
 * that should be kept when splitting up a stroke. Used in:
 * gp_stroke_delete_tagged_points()
 */
typedef struct tGPDeleteIsland {
  int start_idx;
  int end_idx;
} tGPDeleteIsland;

static void gp_stroke_join_islands(bGPDframe *gpf, bGPDstroke *gps_first, bGPDstroke *gps_last)
{
  bGPDspoint *pt = NULL;
  bGPDspoint *pt_final = NULL;
  const int totpoints = gps_first->totpoints + gps_last->totpoints;

  /* create new stroke */
  bGPDstroke *join_stroke = MEM_dupallocN(gps_first);

  join_stroke->points = MEM_callocN(sizeof(bGPDspoint) * totpoints, __func__);
  join_stroke->totpoints = totpoints;
  join_stroke->flag &= ~GP_STROKE_CYCLIC;

  /* copy points (last before) */
  int e1 = 0;
  int e2 = 0;
  float delta = 0.0f;

  for (int i = 0; i < totpoints; i++) {
    pt_final = &join_stroke->points[i];
    if (i < gps_last->totpoints) {
      pt = &gps_last->points[e1];
      e1++;
    }
    else {
      pt = &gps_first->points[e2];
      e2++;
    }

    /* copy current point */
    copy_v3_v3(&pt_final->x, &pt->x);
    pt_final->pressure = pt->pressure;
    pt_final->strength = pt->strength;
    pt_final->time = delta;
    pt_final->flag = pt->flag;

    /* retiming with fixed time interval (we cannot determine real time) */
    delta += 0.01f;
  }

  /* Copy over vertex weight data (if available) */
  if ((gps_first->dvert != NULL) || (gps_last->dvert != NULL)) {
    join_stroke->dvert = MEM_callocN(sizeof(MDeformVert) * totpoints, __func__);
    MDeformVert *dvert_src = NULL;
    MDeformVert *dvert_dst = NULL;

    /* Copy weights (last before)*/
    e1 = 0;
    e2 = 0;
    for (int i = 0; i < totpoints; i++) {
      dvert_dst = &join_stroke->dvert[i];
      dvert_src = NULL;
      if (i < gps_last->totpoints) {
        if (gps_last->dvert) {
          dvert_src = &gps_last->dvert[e1];
          e1++;
        }
      }
      else {
        if (gps_first->dvert) {
          dvert_src = &gps_first->dvert[e2];
          e2++;
        }
      }

      if ((dvert_src) && (dvert_src->dw)) {
        dvert_dst->dw = MEM_dupallocN(dvert_src->dw);
      }
    }
  }

  /* add new stroke at head */
  BLI_addhead(&gpf->strokes, join_stroke);

  /* remove first stroke */
  BLI_remlink(&gpf->strokes, gps_first);
  BKE_gpencil_free_stroke(gps_first);

  /* remove last stroke */
  BLI_remlink(&gpf->strokes, gps_last);
  BKE_gpencil_free_stroke(gps_last);
}

/* Split the given stroke into several new strokes, partitioning
 * it based on whether the stroke points have a particular flag
 * is set (e.g. "GP_SPOINT_SELECT" in most cases, but not always)
 *
 * The algorithm used here is as follows:
 * 1) We firstly identify the number of "islands" of non-tagged points
 *    which will all end up being in new strokes.
 *    - In the most extreme case (i.e. every other vert is a 1-vert island),
 *      we have at most n / 2 islands
 *    - Once we start having larger islands than that, the number required
 *      becomes much less
 * 2) Each island gets converted to a new stroke
 * If the number of points is <= limit, the stroke is deleted
 */
void gp_stroke_delete_tagged_points(bGPDframe *gpf,
                                    bGPDstroke *gps,
                                    bGPDstroke *next_stroke,
                                    int tag_flags,
                                    bool select,
                                    int limit)
{
  tGPDeleteIsland *islands = MEM_callocN(sizeof(tGPDeleteIsland) * (gps->totpoints + 1) / 2,
                                         "gp_point_islands");
  bool in_island = false;
  int num_islands = 0;

  bGPDstroke *gps_first = NULL;
  const bool is_cyclic = (bool)(gps->flag & GP_STROKE_CYCLIC);

  /* First Pass: Identify start/end of islands */
  bGPDspoint *pt = gps->points;
  for (int i = 0; i < gps->totpoints; i++, pt++) {
    if (pt->flag & tag_flags) {
      /* selected - stop accumulating to island */
      in_island = false;
    }
    else {
      /* unselected - start of a new island? */
      int idx;

      if (in_island) {
        /* extend existing island */
        idx = num_islands - 1;
        islands[idx].end_idx = i;
      }
      else {
        /* start of new island */
        in_island = true;
        num_islands++;

        idx = num_islands - 1;
        islands[idx].start_idx = islands[idx].end_idx = i;
      }
    }
  }

  /* Watch out for special case where No islands = All points selected = Delete Stroke only */
  if (num_islands) {
    /* There are islands, so create a series of new strokes,
     * adding them before the "next" stroke. */
    int idx;
    bGPDstroke *new_stroke = NULL;

    /* Create each new stroke... */
    for (idx = 0; idx < num_islands; idx++) {
      tGPDeleteIsland *island = &islands[idx];
      new_stroke = MEM_dupallocN(gps);

      /* if cyclic and first stroke, save to join later */
      if ((is_cyclic) && (gps_first == NULL)) {
        gps_first = new_stroke;
      }

      /* initialize triangle memory  - to be calculated on next redraw */
      new_stroke->triangles = NULL;
      new_stroke->flag |= GP_STROKE_RECALC_GEOMETRY;
      new_stroke->flag &= ~GP_STROKE_CYCLIC;
      new_stroke->tot_triangles = 0;

      /* Compute new buffer size (+ 1 needed as the endpoint index is "inclusive") */
      new_stroke->totpoints = island->end_idx - island->start_idx + 1;

      /* Copy over the relevant point data */
      new_stroke->points = MEM_callocN(sizeof(bGPDspoint) * new_stroke->totpoints,
                                       "gp delete stroke fragment");
      memcpy(new_stroke->points,
             gps->points + island->start_idx,
             sizeof(bGPDspoint) * new_stroke->totpoints);

      /* Copy over vertex weight data (if available) */
      if (gps->dvert != NULL) {
        /* Copy over the relevant vertex-weight points */
        new_stroke->dvert = MEM_callocN(sizeof(MDeformVert) * new_stroke->totpoints,
                                        "gp delete stroke fragment weight");
        memcpy(new_stroke->dvert,
               gps->dvert + island->start_idx,
               sizeof(MDeformVert) * new_stroke->totpoints);

        /* Copy weights */
        int e = island->start_idx;
        for (int i = 0; i < new_stroke->totpoints; i++) {
          MDeformVert *dvert_src = &gps->dvert[e];
          MDeformVert *dvert_dst = &new_stroke->dvert[i];
          if (dvert_src->dw) {
            dvert_dst->dw = MEM_dupallocN(dvert_src->dw);
          }
          e++;
        }
      }
      /* Each island corresponds to a new stroke.
       * We must adjust the timings of these new strokes:
       *
       * Each point's timing data is a delta from stroke's inittime, so as we erase some points
       * from the start of the stroke, we have to offset this inittime and all remaining points'
       * delta values. This way we get a new stroke with exactly the same timing as if user had
       * started drawing from the first non-removed point.
       */
      {
        bGPDspoint *pts;
        float delta = gps->points[island->start_idx].time;
        int j;

        new_stroke->inittime += (double)delta;

        pts = new_stroke->points;
        for (j = 0; j < new_stroke->totpoints; j++, pts++) {
          pts->time -= delta;
          /* set flag for select again later */
          if (select == true) {
            pts->flag &= ~GP_SPOINT_SELECT;
            pts->flag |= GP_SPOINT_TAG;
          }
        }
      }

      /* Add new stroke to the frame or delete if below limit */
      if ((limit > 0) && (new_stroke->totpoints <= limit)) {
        BKE_gpencil_free_stroke(new_stroke);
      }
      else {
        if (next_stroke) {
          BLI_insertlinkbefore(&gpf->strokes, next_stroke, new_stroke);
        }
        else {
          BLI_addtail(&gpf->strokes, new_stroke);
        }
      }
    }
    /* if cyclic, need to join last stroke with first stroke */
    if ((is_cyclic) && (gps_first != NULL) && (gps_first != new_stroke)) {
      gp_stroke_join_islands(gpf, gps_first, new_stroke);
    }
  }

  /* free islands */
  MEM_freeN(islands);

  /* Delete the old stroke */
  BLI_remlink(&gpf->strokes, gps);
  BKE_gpencil_free_stroke(gps);
}

/* Split selected strokes into segments, splitting on selected points */
static int gp_delete_selected_points(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);
  bool changed = false;

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = (is_multiedit) ? gpl->frames.first : gpl->actframe;

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        bGPDstroke *gps, *gpsn;

        if (gpf == NULL) {
          continue;
        }

        /* simply delete strokes which are selected */
        for (gps = gpf->strokes.first; gps; gps = gpsn) {
          gpsn = gps->next;

          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }
          /* check if the color is editable */
          if (ED_gpencil_stroke_color_use(ob, gpl, gps) == false) {
            continue;
          }

          if (gps->flag & GP_STROKE_SELECT) {
            /* deselect old stroke, since it will be used as template for the new strokes */
            gps->flag &= ~GP_STROKE_SELECT;

            /* delete unwanted points by splitting stroke into several smaller ones */
            gp_stroke_delete_tagged_points(gpf, gps, gpsn, GP_SPOINT_SELECT, false, 0);

            changed = true;
          }
        }
      }
    }
  }
  CTX_DATA_END;

  if (changed) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

/* simple wrapper to external call */
int gp_delete_selected_point_wrap(bContext *C)
{
  return gp_delete_selected_points(C);
}

/* ----------------------------------- */

static int gp_delete_exec(bContext *C, wmOperator *op)
{
  eGP_DeleteMode mode = RNA_enum_get(op->ptr, "type");
  int result = OPERATOR_CANCELLED;

  switch (mode) {
    case GP_DELETEOP_STROKES: /* selected strokes */
      result = gp_delete_selected_strokes(C);
      break;

    case GP_DELETEOP_POINTS: /* selected points (breaks the stroke into segments) */
      result = gp_delete_selected_points(C);
      break;

    case GP_DELETEOP_FRAME: /* active frame */
      result = gp_actframe_delete_exec(C, op);
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
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Delete";
  ot->idname = "GPENCIL_OT_delete";
  ot->description = "Delete selected Grease Pencil strokes, vertices, or frames";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = gp_delete_exec;
  ot->poll = gp_stroke_edit_poll;

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

static int gp_dissolve_exec(bContext *C, wmOperator *op)
{
  eGP_DissolveMode mode = RNA_enum_get(op->ptr, "type");

  return gp_dissolve_selected_points(C, mode);
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
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Dissolve";
  ot->idname = "GPENCIL_OT_dissolve";
  ot->description = "Delete selected points without splitting strokes";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = gp_dissolve_exec;
  ot->poll = gp_stroke_edit_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  /* props */
  ot->prop = RNA_def_enum(ot->srna,
                          "type",
                          prop_gpencil_dissolve_types,
                          0,
                          "Type",
                          "Method used for dissolving Stroke points");
}

/* ****************** Snapping - Strokes <-> Cursor ************************ */

/* Poll callback for snap operators */
/* NOTE: For now, we only allow these in the 3D view, as other editors do not
 *       define a cursor or gridstep which can be used
 */
static bool gp_snap_poll(bContext *C)
{
  bGPdata *gpd = CTX_data_gpencil_data(C);
  ScrArea *sa = CTX_wm_area(C);

  return (gpd != NULL) && ((sa != NULL) && (sa->spacetype == SPACE_VIEW3D));
}

/* --------------------------------- */

static int gp_snap_to_grid(bContext *C, wmOperator *UNUSED(op))
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  RegionView3D *rv3d = CTX_wm_region_data(C);
  View3D *v3d = CTX_wm_view3d(C);
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  Object *obact = CTX_data_active_object(C);
  const float gridf = ED_view3d_grid_view_scale(scene, v3d, rv3d, NULL);

  for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
    /* only editable and visible layers are considered */
    if (gpencil_layer_is_editable(gpl) && (gpl->actframe != NULL)) {
      bGPDframe *gpf = gpl->actframe;
      float diff_mat[4][4];

      /* calculate difference matrix object */
      ED_gpencil_parent_location(depsgraph, obact, gpd, gpl, diff_mat);

      for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
        bGPDspoint *pt;
        int i;

        /* skip strokes that are invalid for current view */
        if (ED_gpencil_stroke_can_use(C, gps) == false) {
          continue;
        }
        /* check if the color is editable */
        if (ED_gpencil_stroke_color_use(obact, gpl, gps) == false) {
          continue;
        }

        // TODO: if entire stroke is selected, offset entire stroke by same amount?
        for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
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
            gp_apply_parent_point(depsgraph, obact, gpd, gpl, pt);
          }
        }
      }
    }
  }

  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  DEG_id_tag_update(&obact->id, ID_RECALC_COPY_ON_WRITE);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
  return OPERATOR_FINISHED;
}

void GPENCIL_OT_snap_to_grid(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Selection to Grid";
  ot->idname = "GPENCIL_OT_snap_to_grid";
  ot->description = "Snap selected points to the nearest grid points";

  /* callbacks */
  ot->exec = gp_snap_to_grid;
  ot->poll = gp_snap_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ------------------------------- */

static int gp_snap_to_cursor(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  Object *obact = CTX_data_active_object(C);

  const bool use_offset = RNA_boolean_get(op->ptr, "use_offset");
  const float *cursor_global = scene->cursor.location;

  for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
    /* only editable and visible layers are considered */
    if (gpencil_layer_is_editable(gpl) && (gpl->actframe != NULL)) {
      bGPDframe *gpf = gpl->actframe;
      float diff_mat[4][4];

      /* calculate difference matrix */
      ED_gpencil_parent_location(depsgraph, obact, gpd, gpl, diff_mat);

      for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
        bGPDspoint *pt;
        int i;

        /* skip strokes that are invalid for current view */
        if (ED_gpencil_stroke_can_use(C, gps) == false) {
          continue;
        }
        /* check if the color is editable */
        if (ED_gpencil_stroke_color_use(obact, gpl, gps) == false) {
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
        }
        else {
          /* affect each selected point */
          for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
            if (pt->flag & GP_SPOINT_SELECT) {
              copy_v3_v3(&pt->x, cursor_global);
              gp_apply_parent_point(depsgraph, obact, gpd, gpl, pt);
            }
          }
        }
      }
    }
  }

  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  DEG_id_tag_update(&obact->id, ID_RECALC_COPY_ON_WRITE);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
  return OPERATOR_FINISHED;
}

void GPENCIL_OT_snap_to_cursor(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Selection to Cursor";
  ot->idname = "GPENCIL_OT_snap_to_cursor";
  ot->description = "Snap selected points/strokes to the cursor";

  /* callbacks */
  ot->exec = gp_snap_to_cursor;
  ot->poll = gp_snap_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  ot->prop = RNA_def_boolean(ot->srna,
                             "use_offset",
                             true,
                             "With Offset",
                             "Offset the entire stroke instead of selected points only");
}

/* ------------------------------- */

static int gp_snap_cursor_to_sel(bContext *C, wmOperator *UNUSED(op))
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  Object *obact = CTX_data_active_object(C);

  float *cursor = scene->cursor.location;
  float centroid[3] = {0.0f};
  float min[3], max[3];
  size_t count = 0;

  INIT_MINMAX(min, max);

  /* calculate midpoints from selected points */
  for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
    /* only editable and visible layers are considered */
    if (gpencil_layer_is_editable(gpl) && (gpl->actframe != NULL)) {
      bGPDframe *gpf = gpl->actframe;
      float diff_mat[4][4];

      /* calculate difference matrix */
      ED_gpencil_parent_location(depsgraph, obact, gpd, gpl, diff_mat);

      for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
        bGPDspoint *pt;
        int i;

        /* skip strokes that are invalid for current view */
        if (ED_gpencil_stroke_can_use(C, gps) == false) {
          continue;
        }
        /* check if the color is editable */
        if (ED_gpencil_stroke_color_use(obact, gpl, gps) == false) {
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

            add_v3_v3(centroid, fpt);
            minmax_v3v3_v3(min, max, fpt);

            count++;
          }
        }
      }
    }
  }

  if (scene->toolsettings->transform_pivot_point == V3D_AROUND_CENTER_MEDIAN && count) {
    mul_v3_fl(centroid, 1.0f / (float)count);
    copy_v3_v3(cursor, centroid);
  }
  else {
    mid_v3_v3v3(cursor, min, max);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_snap_cursor_to_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Cursor to Selected Points";
  ot->idname = "GPENCIL_OT_snap_cursor_to_selected";
  ot->description = "Snap cursor to center of selected points";

  /* callbacks */
  ot->exec = gp_snap_cursor_to_sel;
  ot->poll = gp_snap_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************* Apply layer thickness change to strokes ************************** */

static int gp_stroke_apply_thickness_exec(bContext *C, wmOperator *UNUSED(op))
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bGPDlayer *gpl = BKE_gpencil_layer_getactive(gpd);

  /* sanity checks */
  if (ELEM(NULL, gpd, gpl, gpl->frames.first)) {
    return OPERATOR_CANCELLED;
  }

  /* loop all strokes */
  for (bGPDframe *gpf = gpl->frames.first; gpf; gpf = gpf->next) {
    for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
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
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_apply_thickness(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Apply Stroke Thickness";
  ot->idname = "GPENCIL_OT_stroke_apply_thickness";
  ot->description = "Apply the thickness change of the layer to its strokes";

  /* api callbacks */
  ot->exec = gp_stroke_apply_thickness_exec;
  ot->poll = gp_active_layer_poll;
}

/* ******************* Close Strokes ************************** */

enum {
  GP_STROKE_CYCLIC_CLOSE = 1,
  GP_STROKE_CYCLIC_OPEN = 2,
  GP_STROKE_CYCLIC_TOGGLE = 3,
};

static int gp_stroke_cyclical_set_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  Object *ob = CTX_data_active_object(C);

  const int type = RNA_enum_get(op->ptr, "type");
  const bool geometry = RNA_boolean_get(op->ptr, "geometry");
  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);
  bGPDstroke *gps = NULL;

  /* sanity checks */
  if (ELEM(NULL, gpd)) {
    return OPERATOR_CANCELLED;
  }

  /* loop all selected strokes */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = (is_multiedit) ? gpl->frames.first : gpl->actframe;

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == NULL) {
          continue;
        }

        for (gps = gpf->strokes.first; gps; gps = gps->next) {
          MaterialGPencilStyle *gp_style = BKE_material_gpencil_settings_get(ob, gps->mat_nr + 1);
          /* skip strokes that are not selected or invalid for current view */
          if (((gps->flag & GP_STROKE_SELECT) == 0) ||
              ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }
          /* skip hidden or locked colors */
          if (!gp_style || (gp_style->flag & GP_STYLE_COLOR_HIDE) ||
              (gp_style->flag & GP_STYLE_COLOR_LOCKED)) {
            continue;
          }

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

          /* Create new geometry. */
          if ((gps->flag & GP_STROKE_CYCLIC) && (geometry)) {
            BKE_gpencil_close_stroke(gps);
          }
        }

        /* if not multiedit, exit loop*/
        if (!is_multiedit) {
          break;
        }
      }
    }
  }
  CTX_DATA_END;

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

/**
 * Similar to #CURVE_OT_cyclic_toggle or #MASK_OT_cyclic_toggle, but with
 * option to force opened/closed strokes instead of just toggle behavior.
 */
void GPENCIL_OT_stroke_cyclical_set(wmOperatorType *ot)
{
  PropertyRNA *prop;

  static const EnumPropertyItem cyclic_type[] = {
      {GP_STROKE_CYCLIC_CLOSE, "CLOSE", 0, "Close all", ""},
      {GP_STROKE_CYCLIC_OPEN, "OPEN", 0, "Open all", ""},
      {GP_STROKE_CYCLIC_TOGGLE, "TOGGLE", 0, "Toggle", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Set Cyclical State";
  ot->idname = "GPENCIL_OT_stroke_cyclical_set";
  ot->description = "Close or open the selected stroke adding an edge from last to first point";

  /* api callbacks */
  ot->exec = gp_stroke_cyclical_set_exec;
  ot->poll = gp_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", cyclic_type, GP_STROKE_CYCLIC_TOGGLE, "Type", "");
  prop = RNA_def_boolean(
      ot->srna, "geometry", false, "Create Geometry", "Create new geometry for closing stroke");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ******************* Flat Stroke Caps ************************** */

enum {
  GP_STROKE_CAPS_TOGGLE_BOTH = 0,
  GP_STROKE_CAPS_TOGGLE_START = 1,
  GP_STROKE_CAPS_TOGGLE_END = 2,
  GP_STROKE_CAPS_TOGGLE_DEFAULT = 3,
};

static int gp_stroke_caps_set_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  Object *ob = CTX_data_active_object(C);

  const int type = RNA_enum_get(op->ptr, "type");

  /* sanity checks */
  if (ELEM(NULL, gpd)) {
    return OPERATOR_CANCELLED;
  }

  /* loop all selected strokes */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    if (gpl->actframe == NULL) {
      continue;
    }

    for (bGPDstroke *gps = gpl->actframe->strokes.last; gps; gps = gps->prev) {
      MaterialGPencilStyle *gp_style = BKE_material_gpencil_settings_get(ob, gps->mat_nr + 1);

      /* skip strokes that are not selected or invalid for current view */
      if (((gps->flag & GP_STROKE_SELECT) == 0) || (ED_gpencil_stroke_can_use(C, gps) == false)) {
        continue;
      }
      /* skip hidden or locked colors */
      if (!gp_style || (gp_style->flag & GP_STYLE_COLOR_HIDE) ||
          (gp_style->flag & GP_STYLE_COLOR_LOCKED)) {
        continue;
      }

      if ((type == GP_STROKE_CAPS_TOGGLE_BOTH) || (type == GP_STROKE_CAPS_TOGGLE_START)) {
        ++gps->caps[0];
        if (gps->caps[0] >= GP_STROKE_CAP_MAX) {
          gps->caps[0] = GP_STROKE_CAP_ROUND;
        }
      }
      if ((type == GP_STROKE_CAPS_TOGGLE_BOTH) || (type == GP_STROKE_CAPS_TOGGLE_END)) {
        ++gps->caps[1];
        if (gps->caps[1] >= GP_STROKE_CAP_MAX) {
          gps->caps[1] = GP_STROKE_CAP_ROUND;
        }
      }
      if (type == GP_STROKE_CAPS_TOGGLE_DEFAULT) {
        gps->caps[0] = GP_STROKE_CAP_ROUND;
        gps->caps[1] = GP_STROKE_CAP_ROUND;
      }
    }
  }
  CTX_DATA_END;

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

/**
 * Change Stroke caps mode Rounded or Flat
 */
void GPENCIL_OT_stroke_caps_set(wmOperatorType *ot)
{
  static const EnumPropertyItem toggle_type[] = {
      {GP_STROKE_CAPS_TOGGLE_BOTH, "TOGGLE", 0, "Both", ""},
      {GP_STROKE_CAPS_TOGGLE_START, "START", 0, "Start", ""},
      {GP_STROKE_CAPS_TOGGLE_END, "END", 0, "End", ""},
      {GP_STROKE_CAPS_TOGGLE_DEFAULT, "TOGGLE", 0, "Default", "Set as default rounded"},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Set Caps Mode";
  ot->idname = "GPENCIL_OT_stroke_caps_set";
  ot->description = "Change Stroke caps mode (rounded or flat)";

  /* api callbacks */
  ot->exec = gp_stroke_caps_set_exec;
  ot->poll = gp_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", toggle_type, GP_STROKE_CAPS_TOGGLE_BOTH, "Type", "");
}

/* ******************* Stroke join ************************** */

/* Helper: flip stroke */
static void gpencil_flip_stroke(bGPDstroke *gps)
{
  int end = gps->totpoints - 1;

  for (int i = 0; i < gps->totpoints / 2; i++) {
    bGPDspoint *point, *point2;
    bGPDspoint pt;

    /* save first point */
    point = &gps->points[i];
    pt.x = point->x;
    pt.y = point->y;
    pt.z = point->z;
    pt.flag = point->flag;
    pt.pressure = point->pressure;
    pt.strength = point->strength;
    pt.time = point->time;

    /* replace first point with last point */
    point2 = &gps->points[end];
    point->x = point2->x;
    point->y = point2->y;
    point->z = point2->z;
    point->flag = point2->flag;
    point->pressure = point2->pressure;
    point->strength = point2->strength;
    point->time = point2->time;

    /* replace last point with first saved before */
    point = &gps->points[end];
    point->x = pt.x;
    point->y = pt.y;
    point->z = pt.z;
    point->flag = pt.flag;
    point->pressure = pt.pressure;
    point->strength = pt.strength;
    point->time = pt.time;

    end--;
  }
}

/* Helper: copy point between strokes */
static void gpencil_stroke_copy_point(bGPDstroke *gps,
                                      bGPDspoint *point,
                                      int idx,
                                      float delta[3],
                                      float pressure,
                                      float strength,
                                      float deltatime)
{
  bGPDspoint *newpoint;

  gps->points = MEM_reallocN(gps->points, sizeof(bGPDspoint) * (gps->totpoints + 1));
  if (gps->dvert != NULL) {
    gps->dvert = MEM_reallocN(gps->dvert, sizeof(MDeformVert) * (gps->totpoints + 1));
  }
  gps->totpoints++;
  newpoint = &gps->points[gps->totpoints - 1];

  newpoint->x = point->x * delta[0];
  newpoint->y = point->y * delta[1];
  newpoint->z = point->z * delta[2];
  newpoint->flag = point->flag;
  newpoint->pressure = pressure;
  newpoint->strength = strength;
  newpoint->time = point->time + deltatime;

  if (gps->dvert != NULL) {
    MDeformVert *dvert = &gps->dvert[idx];
    MDeformVert *newdvert = &gps->dvert[gps->totpoints - 1];

    newdvert->totweight = dvert->totweight;
    newdvert->dw = MEM_dupallocN(dvert->dw);
  }
}

/* Helper: join two strokes using the shortest distance (reorder stroke if necessary ) */
static void gpencil_stroke_join_strokes(bGPDstroke *gps_a,
                                        bGPDstroke *gps_b,
                                        const bool leave_gaps)
{
  bGPDspoint point;
  bGPDspoint *pt;
  int i;
  float delta[3] = {1.0f, 1.0f, 1.0f};
  float deltatime = 0.0f;

  /* sanity checks */
  if (ELEM(NULL, gps_a, gps_b)) {
    return;
  }

  if ((gps_a->totpoints == 0) || (gps_b->totpoints == 0)) {
    return;
  }

  /* define start and end points of each stroke */
  float sa[3], sb[3], ea[3], eb[3];
  pt = &gps_a->points[0];
  copy_v3_v3(sa, &pt->x);

  pt = &gps_a->points[gps_a->totpoints - 1];
  copy_v3_v3(ea, &pt->x);

  pt = &gps_b->points[0];
  copy_v3_v3(sb, &pt->x);

  pt = &gps_b->points[gps_b->totpoints - 1];
  copy_v3_v3(eb, &pt->x);

  /* review if need flip stroke B */
  float ea_sb = len_squared_v3v3(ea, sb);
  float ea_eb = len_squared_v3v3(ea, eb);
  /* flip if distance to end point is shorter */
  if (ea_eb < ea_sb) {
    gpencil_flip_stroke(gps_b);
  }

  /* don't visibly link the first and last points? */
  if (leave_gaps) {
    /* 1st: add one tail point to start invisible area */
    point = gps_a->points[gps_a->totpoints - 1];
    deltatime = point.time;
    gpencil_stroke_copy_point(gps_a, &point, gps_a->totpoints - 1, delta, 0.0f, 0.0f, 0.0f);

    /* 2nd: add one head point to finish invisible area */
    point = gps_b->points[0];
    gpencil_stroke_copy_point(gps_a, &point, 0, delta, 0.0f, 0.0f, deltatime);
  }

  /* 3rd: add all points */
  for (i = 0, pt = gps_b->points; i < gps_b->totpoints && pt; i++, pt++) {
    /* check if still room in buffer */
    if (gps_a->totpoints <= GP_STROKE_BUFFER_MAX - 2) {
      gpencil_stroke_copy_point(gps_a, pt, i, delta, pt->pressure, pt->strength, deltatime);
    }
  }
}

static int gp_stroke_join_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bGPDlayer *activegpl = BKE_gpencil_layer_getactive(gpd);
  bGPDstroke *gps, *gpsn;
  Object *ob = CTX_data_active_object(C);

  bGPDframe *gpf_a = NULL;
  bGPDstroke *stroke_a = NULL;
  bGPDstroke *stroke_b = NULL;
  bGPDstroke *new_stroke = NULL;

  const int type = RNA_enum_get(op->ptr, "type");
  const bool leave_gaps = RNA_boolean_get(op->ptr, "leave_gaps");

  /* sanity checks */
  if (ELEM(NULL, gpd)) {
    return OPERATOR_CANCELLED;
  }

  if (activegpl->flag & GP_LAYER_LOCKED) {
    return OPERATOR_CANCELLED;
  }

  BLI_assert(ELEM(type, GP_STROKE_JOIN, GP_STROKE_JOINCOPY));

  /* read all selected strokes */
  bool first = false;
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *gpf = gpl->actframe;
    if (gpf == NULL) {
      continue;
    }

    for (gps = gpf->strokes.first; gps; gps = gpsn) {
      gpsn = gps->next;
      if (gps->flag & GP_STROKE_SELECT) {
        /* skip strokes that are invalid for current view */
        if (ED_gpencil_stroke_can_use(C, gps) == false) {
          continue;
        }
        /* check if the color is editable */
        if (ED_gpencil_stroke_color_use(ob, gpl, gps) == false) {
          continue;
        }

        /* to join strokes, cyclic must be disabled */
        gps->flag &= ~GP_STROKE_CYCLIC;

        /* saves first frame and stroke */
        if (!first) {
          first = true;
          gpf_a = gpf;
          stroke_a = gps;
        }
        else {
          stroke_b = gps;

          /* create a new stroke if was not created before (only created if something to join) */
          if (new_stroke == NULL) {
            new_stroke = MEM_dupallocN(stroke_a);
            new_stroke->points = MEM_dupallocN(stroke_a->points);
            if (stroke_a->dvert != NULL) {
              new_stroke->dvert = MEM_dupallocN(stroke_a->dvert);
              BKE_gpencil_stroke_weights_duplicate(stroke_a, new_stroke);
            }
            new_stroke->triangles = NULL;
            new_stroke->tot_triangles = 0;
            new_stroke->flag |= GP_STROKE_RECALC_GEOMETRY;

            /* if new, set current color */
            if (type == GP_STROKE_JOINCOPY) {
              new_stroke->mat_nr = stroke_a->mat_nr;
            }
          }

          /* join new_stroke and stroke B. New stroke will contain all the previous data */
          gpencil_stroke_join_strokes(new_stroke, stroke_b, leave_gaps);

          /* if join only, delete old strokes */
          if (type == GP_STROKE_JOIN) {
            if (stroke_a) {
              BLI_insertlinkbefore(&gpf_a->strokes, stroke_a, new_stroke);
              BLI_remlink(&gpf->strokes, stroke_a);
              BKE_gpencil_free_stroke(stroke_a);
              stroke_a = NULL;
            }
            if (stroke_b) {
              BLI_remlink(&gpf->strokes, stroke_b);
              BKE_gpencil_free_stroke(stroke_b);
              stroke_b = NULL;
            }
          }
        }
      }
    }
  }
  CTX_DATA_END;

  /* add new stroke if was not added before */
  if (type == GP_STROKE_JOINCOPY) {
    if (new_stroke) {
      /* Add a new frame if needed */
      if (activegpl->actframe == NULL) {
        activegpl->actframe = BKE_gpencil_frame_addnew(activegpl, gpf_a->framenum);
      }

      BLI_addtail(&activegpl->actframe->strokes, new_stroke);
    }
  }

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_join(wmOperatorType *ot)
{
  static const EnumPropertyItem join_type[] = {
      {GP_STROKE_JOIN, "JOIN", 0, "Join", ""},
      {GP_STROKE_JOINCOPY, "JOINCOPY", 0, "Join and Copy", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Join Strokes";
  ot->idname = "GPENCIL_OT_stroke_join";
  ot->description = "Join selected strokes (optionally as new stroke)";

  /* api callbacks */
  ot->exec = gp_stroke_join_exec;
  ot->poll = gp_active_layer_poll;

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

/* ******************* Stroke flip ************************** */

static int gp_stroke_flip_exec(bContext *C, wmOperator *UNUSED(op))
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  Object *ob = CTX_data_active_object(C);

  /* sanity checks */
  if (ELEM(NULL, gpd)) {
    return OPERATOR_CANCELLED;
  }

  /* read all selected strokes */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *gpf = gpl->actframe;
    if (gpf == NULL) {
      continue;
    }

    for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
      if (gps->flag & GP_STROKE_SELECT) {
        /* skip strokes that are invalid for current view */
        if (ED_gpencil_stroke_can_use(C, gps) == false) {
          continue;
        }
        /* check if the color is editable */
        if (ED_gpencil_stroke_color_use(ob, gpl, gps) == false) {
          continue;
        }

        /* flip stroke */
        gpencil_flip_stroke(gps);
      }
    }
  }
  CTX_DATA_END;

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_flip(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Flip Stroke";
  ot->idname = "GPENCIL_OT_stroke_flip";
  ot->description = "Change direction of the points of the selected strokes";

  /* api callbacks */
  ot->exec = gp_stroke_flip_exec;
  ot->poll = gp_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ***************** Reproject Strokes ********************** */

typedef enum eGP_ReprojectModes {
  /* Axis */
  GP_REPROJECT_FRONT = 0,
  GP_REPROJECT_SIDE,
  GP_REPROJECT_TOP,
  /* On same plane, parallel to viewplane */
  GP_REPROJECT_VIEW,
  /* Reprojected on to the scene geometry */
  GP_REPROJECT_SURFACE,
  /* Reprojected on 3D cursor orientation */
  GP_REPROJECT_CURSOR,
} eGP_ReprojectModes;

static int gp_strokes_reproject_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  Object *ob = CTX_data_active_object(C);
  ARegion *ar = CTX_wm_region(C);
  RegionView3D *rv3d = ar->regiondata;

  GP_SpaceConversion gsc = {NULL};
  eGP_ReprojectModes mode = RNA_enum_get(op->ptr, "type");

  float origin[3];

  /* init space conversion stuff */
  gp_point_conversion_init(C, &gsc);

  /* init autodist for geometry projection */
  if (mode == GP_REPROJECT_SURFACE) {
    view3d_region_operator_needs_opengl(CTX_wm_window(C), gsc.ar);
    ED_view3d_autodist_init(depsgraph, gsc.ar, CTX_wm_view3d(C), 0);
  }

  // TODO: For deforming geometry workflow, create new frames?

  /* Go through each editable + selected stroke, adjusting each of its points one by one... */
  GP_EDITABLE_STROKES_BEGIN (gpstroke_iter, C, gpl, gps) {
    if (gps->flag & GP_STROKE_SELECT) {
      bGPDspoint *pt;
      int i;
      /* Adjust each point */
      for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
        float xy[2];

        /* 3D to Screenspace */
        /* Note: We can't use gp_point_to_xy() here because that uses ints for the screenspace
         *       coordinates, resulting in lost precision, which in turn causes stairstepping
         *       artifacts in the final points.
         */
        bGPDspoint pt2;
        gp_point_to_parent_space(pt, gpstroke_iter.diff_mat, &pt2);
        gp_point_to_xy_fl(&gsc, gps, &pt2, &xy[0], &xy[1]);

        /* Project stroke in one axis */
        if (ELEM(mode,
                 GP_REPROJECT_FRONT,
                 GP_REPROJECT_SIDE,
                 GP_REPROJECT_TOP,
                 GP_REPROJECT_CURSOR)) {
          if (mode != GP_REPROJECT_CURSOR) {
            ED_gp_get_drawing_reference(scene, ob, gpl, ts->gpencil_v3d_align, origin);
          }
          else {
            copy_v3_v3(origin, scene->cursor.location);
          }

          int axis = 0;
          switch (mode) {
            case GP_REPROJECT_FRONT: {
              axis = 1;
              break;
            }
            case GP_REPROJECT_SIDE: {
              axis = 0;
              break;
            }
            case GP_REPROJECT_TOP: {
              axis = 2;
              break;
            }
            case GP_REPROJECT_CURSOR: {
              axis = 3;
              break;
            }
            default: {
              axis = 1;
              break;
            }
          }

          ED_gp_project_point_to_plane(scene, ob, rv3d, origin, axis, &pt2);

          copy_v3_v3(&pt->x, &pt2.x);

          /* apply parent again */
          gp_apply_parent_point(depsgraph, ob, gpd, gpl, pt);
        }
        /* Project screenspace back to 3D space (from current perspective)
         * so that all points have been treated the same way
         */
        else if (mode == GP_REPROJECT_VIEW) {
          /* Planar - All on same plane parallel to the viewplane */
          gp_point_xy_to_3d(&gsc, scene, xy, &pt->x);
        }
        else {
          /* Geometry - Snap to surfaces of visible geometry */
          /* XXX: There will be precision loss (possible stairstep artifacts)
           * from this conversion to satisfy the API's */
          const int screen_co[2] = {(int)xy[0], (int)xy[1]};

          int depth_margin = 0;  // XXX: 4 for strokes, 0 for normal
          float depth;

          /* XXX: The proper procedure computes the depths into an array,
           * to have smooth transitions when all else fails... */
          if (ED_view3d_autodist_depth(gsc.ar, screen_co, depth_margin, &depth)) {
            ED_view3d_autodist_simple(gsc.ar, screen_co, &pt->x, 0, &depth);
          }
          else {
            /* Default to planar */
            gp_point_xy_to_3d(&gsc, scene, xy, &pt->x);
          }
        }

        /* Unapply parent corrections */
        if (!ELEM(mode, GP_REPROJECT_FRONT, GP_REPROJECT_SIDE, GP_REPROJECT_TOP)) {
          mul_m4_v3(gpstroke_iter.inverse_diff_mat, &pt->x);
        }
      }
    }
  }
  GP_EDITABLE_STROKES_END(gpstroke_iter);

  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
  return OPERATOR_FINISHED;
}

void GPENCIL_OT_reproject(wmOperatorType *ot)
{
  static const EnumPropertyItem reproject_type[] = {
      {GP_REPROJECT_FRONT, "FRONT", 0, "Front", "Reproject the strokes using the X-Z plane"},
      {GP_REPROJECT_SIDE, "SIDE", 0, "Side", "Reproject the strokes using the Y-Z plane"},
      {GP_REPROJECT_TOP, "TOP", 0, "Top", "Reproject the strokes using the X-Y plane"},
      {GP_REPROJECT_VIEW,
       "VIEW",
       0,
       "View",
       "Reproject the strokes to end up on the same plane, as if drawn from the current viewpoint "
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
       "Reproject the strokes using the orienation of 3D cursor"},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Reproject Strokes";
  ot->idname = "GPENCIL_OT_reproject";
  ot->description =
      "Reproject the selected strokes from the current viewpoint as if they had been newly drawn "
      "(e.g. to fix problems from accidental 3D cursor movement or accidental viewport changes, "
      "or for matching deforming geometry)";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = gp_strokes_reproject_exec;
  ot->poll = gp_strokes_edit3d_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_USE_EVAL_DATA;

  /* properties */
  ot->prop = RNA_def_enum(
      ot->srna, "type", reproject_type, GP_REPROJECT_VIEW, "Projection Type", "");
}

/* ******************* Stroke subdivide ************************** */
/* helper to smooth */
static void gp_smooth_stroke(bContext *C, wmOperator *op)
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
      for (int r = 0; r < repeat; r++) {
        for (int i = 0; i < gps->totpoints; i++) {
          bGPDspoint *pt = &gps->points[i];
          if ((only_selected) && ((pt->flag & GP_SPOINT_SELECT) == 0)) {
            continue;
          }

          /* perform smoothing */
          if (smooth_position) {
            BKE_gpencil_smooth_stroke(gps, i, factor);
          }
          if (smooth_strength) {
            BKE_gpencil_smooth_stroke_strength(gps, i, factor);
          }
          if (smooth_thickness) {
            /* thickness need to repeat process several times */
            for (int r2 = 0; r2 < r * 10; r2++) {
              BKE_gpencil_smooth_stroke_thickness(gps, i, factor);
            }
          }
          if (smooth_uv) {
            BKE_gpencil_smooth_stroke_uv(gps, i, factor);
          }
        }
      }
    }
  }
  GP_EDITABLE_STROKES_END(gpstroke_iter);
}

/* helper: Count how many points need to be inserted */
static int gp_count_subdivision_cuts(bGPDstroke *gps)
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

  return totnewpoints;
}

static int gp_stroke_subdivide_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bGPDspoint *temp_points;
  const int cuts = RNA_int_get(op->ptr, "number_cuts");

  int totnewpoints, oldtotpoints;
  int i2;

  /* sanity checks */
  if (ELEM(NULL, gpd)) {
    return OPERATOR_CANCELLED;
  }

  /* Go through each editable + selected stroke */
  GP_EDITABLE_STROKES_BEGIN (gpstroke_iter, C, gpl, gps) {
    if (gps->flag & GP_STROKE_SELECT) {
      /* loop as many times as cuts */
      for (int s = 0; s < cuts; s++) {
        totnewpoints = gp_count_subdivision_cuts(gps);
        if (totnewpoints == 0) {
          continue;
        }
        /* duplicate points in a temp area */
        temp_points = MEM_dupallocN(gps->points);
        oldtotpoints = gps->totpoints;

        MDeformVert *temp_dverts = NULL;
        MDeformVert *dvert_final = NULL;
        MDeformVert *dvert = NULL;
        MDeformVert *dvert_next = NULL;
        if (gps->dvert != NULL) {
          temp_dverts = MEM_dupallocN(gps->dvert);
        }

        /* resize the points arrays */
        gps->totpoints += totnewpoints;
        gps->points = MEM_recallocN(gps->points, sizeof(*gps->points) * gps->totpoints);
        if (gps->dvert != NULL) {
          gps->dvert = MEM_recallocN(gps->dvert, sizeof(*gps->dvert) * gps->totpoints);
        }
        gps->flag |= GP_STROKE_RECALC_GEOMETRY;

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

          if (gps->dvert != NULL) {
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
                if (gps->dvert != NULL) {
                  dvert_final = &gps->dvert[i2];
                }
                /* Interpolate all values */
                bGPDspoint *next = &temp_points[i + 1];
                interp_v3_v3v3(&pt_final->x, &pt->x, &next->x, 0.5f);
                pt_final->pressure = interpf(pt->pressure, next->pressure, 0.5f);
                pt_final->strength = interpf(pt->strength, next->strength, 0.5f);
                CLAMP(pt_final->strength, GPENCIL_STRENGTH_MIN, 1.0f);
                pt_final->time = interpf(pt->time, next->time, 0.5f);
                pt_final->flag |= GP_SPOINT_SELECT;

                /* interpolate weights */
                if (gps->dvert != NULL) {
                  dvert = &temp_dverts[i];
                  dvert_next = &temp_dverts[i + 1];
                  dvert_final = &gps->dvert[i2];

                  dvert_final->totweight = dvert->totweight;
                  dvert_final->dw = MEM_dupallocN(dvert->dw);

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
        /* free temp memory */
        MEM_SAFE_FREE(temp_points);
        MEM_SAFE_FREE(temp_dverts);
      }

      /* triangles cache needs to be recalculated */
      gps->flag |= GP_STROKE_RECALC_GEOMETRY;
      gps->tot_triangles = 0;
    }
  }
  GP_EDITABLE_STROKES_END(gpstroke_iter);

  /* smooth stroke */
  gp_smooth_stroke(C, op);

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_subdivide(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Subdivide Stroke";
  ot->idname = "GPENCIL_OT_stroke_subdivide";
  ot->description =
      "Subdivide between continuous selected points of the stroke adding a point half way between "
      "them";

  /* api callbacks */
  ot->exec = gp_stroke_subdivide_exec;
  ot->poll = gp_active_layer_poll;

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
static int gp_stroke_simplify_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  float factor = RNA_float_get(op->ptr, "factor");

  /* sanity checks */
  if (ELEM(NULL, gpd)) {
    return OPERATOR_CANCELLED;
  }

  /* Go through each editable + selected stroke */
  GP_EDITABLE_STROKES_BEGIN (gpstroke_iter, C, gpl, gps) {
    if (gps->flag & GP_STROKE_SELECT) {
      /* simplify stroke using Ramer-Douglas-Peucker algorithm */
      BKE_gpencil_simplify_stroke(gps, factor);
    }
  }
  GP_EDITABLE_STROKES_END(gpstroke_iter);

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_simplify(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Simplify Stroke";
  ot->idname = "GPENCIL_OT_stroke_simplify";
  ot->description = "Simplify selected stroked reducing number of points";

  /* api callbacks */
  ot->exec = gp_stroke_simplify_exec;
  ot->poll = gp_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_float(ot->srna, "factor", 0.0f, 0.0f, 100.0f, "Factor", "", 0.0f, 100.0f);
  /* avoid re-using last var */
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ** simplify stroke using fixed algorithm *** */
static int gp_stroke_simplify_fixed_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  int steps = RNA_int_get(op->ptr, "step");

  /* sanity checks */
  if (ELEM(NULL, gpd)) {
    return OPERATOR_CANCELLED;
  }

  /* Go through each editable + selected stroke */
  GP_EDITABLE_STROKES_BEGIN (gpstroke_iter, C, gpl, gps) {
    if (gps->flag & GP_STROKE_SELECT) {
      for (int i = 0; i < steps; i++) {
        BKE_gpencil_simplify_fixed(gps);
      }
    }
  }
  GP_EDITABLE_STROKES_END(gpstroke_iter);

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_simplify_fixed(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Simplify Fixed Stroke";
  ot->idname = "GPENCIL_OT_stroke_simplify_fixed";
  ot->description = "Simplify selected stroked reducing number of points using fixed algorithm";

  /* api callbacks */
  ot->exec = gp_stroke_simplify_fixed_exec;
  ot->poll = gp_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_int(ot->srna, "step", 1, 1, 100, "Steps", "Number of simplify steps", 1, 10);

  /* avoid re-using last var */
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ******************* Stroke trim ************************** */
static int gp_stroke_trim_exec(bContext *C, wmOperator *UNUSED(op))
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  /* sanity checks */
  if (ELEM(NULL, gpd)) {
    return OPERATOR_CANCELLED;
  }

  /* Go through each editable + selected stroke */
  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = (is_multiedit) ? gpl->frames.first : gpl->actframe;

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        bGPDstroke *gps, *gpsn;

        if (gpf == NULL) {
          continue;
        }

        for (gps = gpf->strokes.first; gps; gps = gpsn) {
          gpsn = gps->next;

          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }

          if (gps->flag & GP_STROKE_SELECT) {
            BKE_gpencil_trim_stroke(gps);
          }
        }
        /* if not multiedit, exit loop*/
        if (!is_multiedit) {
          break;
        }
      }
    }
  }
  CTX_DATA_END;

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_trim(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Trim Stroke";
  ot->idname = "GPENCIL_OT_stroke_trim";
  ot->description = "Trim selected stroke to first loop or intersection";

  /* api callbacks */
  ot->exec = gp_stroke_trim_exec;
  ot->poll = gp_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ***************** Separate Strokes ********************** */
typedef enum eGP_SeparateModes {
  /* Points */
  GP_SEPARATE_POINT = 0,
  /* Selected Strokes */
  GP_SEPARATE_STROKE,
  /* Current Layer */
  GP_SEPARATE_LAYER,
} eGP_SeparateModes;

static int gp_stroke_separate_exec(bContext *C, wmOperator *op)
{
  Base *base_new;
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Base *base_old = CTX_data_active_base(C);
  bGPdata *gpd_src = ED_gpencil_data_get_active(C);
  Object *ob = CTX_data_active_object(C);

  Object *ob_dst = NULL;
  bGPdata *gpd_dst = NULL;
  bGPDlayer *gpl_dst = NULL;
  bGPDframe *gpf_dst = NULL;
  bGPDspoint *pt;
  Material *ma = NULL;
  int i, idx;

  eGP_SeparateModes mode = RNA_enum_get(op->ptr, "mode");

  /* sanity checks */
  if (ELEM(NULL, gpd_src)) {
    return OPERATOR_CANCELLED;
  }

  if ((mode == GP_SEPARATE_LAYER) && (BLI_listbase_count(&gpd_src->layers) == 1)) {
    BKE_report(op->reports, RPT_ERROR, "Cannot separate an object with one layer only");
    return OPERATOR_CANCELLED;
  }

  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd_src);

  /* create a new object */
  base_new = ED_object_add_duplicate(bmain, scene, view_layer, base_old, 0);
  ob_dst = base_new->object;
  ob_dst->mode = OB_MODE_OBJECT;
  /* create new grease pencil datablock */
  gpd_dst = BKE_gpencil_data_addnew(bmain, gpd_src->id.name + 2);
  ob_dst->data = (bGPdata *)gpd_dst;

  /* loop old datablock and separate parts */
  if ((mode == GP_SEPARATE_POINT) || (mode == GP_SEPARATE_STROKE)) {
    CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
      gpl_dst = NULL;
      bGPDframe *init_gpf = (is_multiedit) ? gpl->frames.first : gpl->actframe;

      for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
        if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
          bGPDstroke *gps, *gpsn;

          if (gpf == NULL) {
            continue;
          }

          gpf_dst = NULL;

          for (gps = gpf->strokes.first; gps; gps = gpsn) {
            gpsn = gps->next;

            /* skip strokes that are invalid for current view */
            if (ED_gpencil_stroke_can_use(C, gps) == false) {
              continue;
            }
            /* check if the color is editable */
            if (ED_gpencil_stroke_color_use(ob, gpl, gps) == false) {
              continue;
            }
            /*  separate selected strokes */
            if (gps->flag & GP_STROKE_SELECT) {
              /* add layer if not created before */
              if (gpl_dst == NULL) {
                gpl_dst = BKE_gpencil_layer_addnew(gpd_dst, gpl->info, false);
              }

              /* add frame if not created before */
              if (gpf_dst == NULL) {
                gpf_dst = BKE_gpencil_layer_getframe(gpl_dst, gpf->framenum, GP_GETFRAME_ADD_NEW);
              }

              /* add duplicate materials */
              ma = give_current_material(
                  ob, gps->mat_nr + 1); /* XXX same material can be in multiple slots */
              idx = BKE_gpencil_object_material_ensure(bmain, ob_dst, ma);

              /* selected points mode */
              if (mode == GP_SEPARATE_POINT) {
                /* make copy of source stroke */
                bGPDstroke *gps_dst = BKE_gpencil_stroke_duplicate(gps);

                /* Reassign material. */
                gps_dst->mat_nr = idx;

                /* link to destination frame */
                BLI_addtail(&gpf_dst->strokes, gps_dst);

                /* Invert selection status of all points in destination stroke */
                for (i = 0, pt = gps_dst->points; i < gps_dst->totpoints; i++, pt++) {
                  pt->flag ^= GP_SPOINT_SELECT;
                }

                /* delete selected points from destination stroke */
                gp_stroke_delete_tagged_points(gpf_dst, gps_dst, NULL, GP_SPOINT_SELECT, false, 0);

                /* delete selected points from origin stroke */
                gp_stroke_delete_tagged_points(gpf, gps, gpsn, GP_SPOINT_SELECT, false, 0);
              }
              /* selected strokes mode */
              else if (mode == GP_SEPARATE_STROKE) {
                /* deselect old stroke */
                gps->flag &= ~GP_STROKE_SELECT;
                /* unlink from source frame */
                BLI_remlink(&gpf->strokes, gps);
                gps->prev = gps->next = NULL;
                /* relink to destination frame */
                BLI_addtail(&gpf_dst->strokes, gps);
                /* Reassign material. */
                gps->mat_nr = idx;
              }
            }
          }
        }

        /* if not multiedit, exit loop*/
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
        BKE_gpencil_layer_setactive(gpd_src, gpl->prev);
      }
      else if (gpl->next) {
        BKE_gpencil_layer_setactive(gpd_src, gpl->next);
      }
      /* unlink from source datablock */
      BLI_remlink(&gpd_src->layers, gpl);
      gpl->prev = gpl->next = NULL;
      /* relink to destination datablock */
      BLI_addtail(&gpd_dst->layers, gpl);

      /* add duplicate materials */
      for (bGPDframe *gpf = gpl->frames.first; gpf; gpf = gpf->next) {
        for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }
          ma = give_current_material(ob, gps->mat_nr + 1);
          gps->mat_nr = BKE_gpencil_object_material_ensure(bmain, ob_dst, ma);
        }
      }
    }
  }

  DEG_id_tag_update(&gpd_src->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  DEG_id_tag_update(&gpd_dst->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, NULL);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_separate(wmOperatorType *ot)
{
  static const EnumPropertyItem separate_type[] = {
      {GP_SEPARATE_POINT, "POINT", 0, "Selected Points", "Separate the selected points"},
      {GP_SEPARATE_STROKE, "STROKE", 0, "Selected Strokes", "Separate the selected strokes"},
      {GP_SEPARATE_LAYER, "LAYER", 0, "Active Layer", "Separate the strokes of the current layer"},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Separate Strokes";
  ot->idname = "GPENCIL_OT_stroke_separate";
  ot->description = "Separate the selected strokes or layer in a new grease pencil object";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = gp_stroke_separate_exec;
  ot->poll = gp_strokes_edit3d_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "mode", separate_type, GP_SEPARATE_POINT, "Mode", "");
}

/* ***************** Split Strokes ********************** */
static int gp_stroke_split_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  bGPDspoint *pt;
  int i;

  /* sanity checks */
  if (ELEM(NULL, gpd)) {
    return OPERATOR_CANCELLED;
  }
  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);

  /* loop strokes and split parts */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = (is_multiedit) ? gpl->frames.first : gpl->actframe;

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        bGPDstroke *gps, *gpsn;

        if (gpf == NULL) {
          continue;
        }

        for (gps = gpf->strokes.first; gps; gps = gpsn) {
          gpsn = gps->next;

          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }
          /* check if the color is editable */
          if (ED_gpencil_stroke_color_use(ob, gpl, gps) == false) {
            continue;
          }
          /*  split selected strokes */
          if (gps->flag & GP_STROKE_SELECT) {
            /* make copy of source stroke */
            bGPDstroke *gps_dst = BKE_gpencil_stroke_duplicate(gps);

            /* link to same frame */
            BLI_addtail(&gpf->strokes, gps_dst);

            /* invert selection status of all points in destination stroke */
            for (i = 0, pt = gps_dst->points; i < gps_dst->totpoints; i++, pt++) {
              pt->flag ^= GP_SPOINT_SELECT;
            }

            /* delete selected points from destination stroke */
            gp_stroke_delete_tagged_points(gpf, gps_dst, NULL, GP_SPOINT_SELECT, true, 0);

            /* delete selected points from origin stroke */
            gp_stroke_delete_tagged_points(gpf, gps, gpsn, GP_SPOINT_SELECT, false, 0);
          }
        }
        /* select again tagged points */
        for (gps = gpf->strokes.first; gps; gps = gps->next) {
          bGPDspoint *ptn = gps->points;
          for (int i2 = 0; i2 < gps->totpoints; i2++, ptn++) {
            if (ptn->flag & GP_SPOINT_TAG) {
              ptn->flag |= GP_SPOINT_SELECT;
              ptn->flag &= ~GP_SPOINT_TAG;
            }
          }
        }
      }

      /* if not multiedit, exit loop*/
      if (!is_multiedit) {
        break;
      }
    }
  }
  CTX_DATA_END;

  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_split(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Split Strokes";
  ot->idname = "GPENCIL_OT_stroke_split";
  ot->description = "Split selected points as new stroke on same frame";

  /* callbacks */
  ot->exec = gp_stroke_split_exec;
  ot->poll = gp_strokes_edit3d_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int gp_stroke_smooth_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  /* sanity checks */
  if (ELEM(NULL, gpd)) {
    return OPERATOR_CANCELLED;
  }

  gp_smooth_stroke(C, op);

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

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
  ot->exec = gp_stroke_smooth_exec;
  ot->poll = gp_active_layer_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_int(ot->srna, "repeat", 1, 1, 10, "Repeat", "", 1, 5);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  RNA_def_float(ot->srna, "factor", 0.5f, 0.0f, 2.0f, "Factor", "", 0.0f, 2.0f);
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

/* smart stroke cutter for trimming stroke ends */
struct GP_SelectLassoUserData {
  rcti rect;
  const int (*mcords)[2];
  int mcords_len;
};

static bool gpencil_test_lasso(bGPDstroke *gps,
                               bGPDspoint *pt,
                               const GP_SpaceConversion *gsc,
                               const float diff_mat[4][4],
                               void *user_data)
{
  const struct GP_SelectLassoUserData *data = user_data;
  bGPDspoint pt2;
  int x0, y0;
  gp_point_to_parent_space(pt, diff_mat, &pt2);
  gp_point_to_xy(gsc, gps, &pt2, &x0, &y0);
  /* test if in lasso */
  return ((!ELEM(V2D_IS_CLIPPED, x0, y0)) && BLI_rcti_isect_pt(&data->rect, x0, y0) &&
          BLI_lasso_is_point_inside(data->mcords, data->mcords_len, x0, y0, INT_MAX));
}

typedef bool (*GPencilTestFn)(bGPDstroke *gps,
                              bGPDspoint *pt,
                              const GP_SpaceConversion *gsc,
                              const float diff_mat[4][4],
                              void *user_data);

static void gpencil_cutter_dissolve(bGPDlayer *hit_layer, bGPDstroke *hit_stroke)
{
  bGPDspoint *pt = NULL;
  bGPDspoint *pt1 = NULL;
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
    hit_stroke = NULL;
  }

  /* if very small distance delete */
  if ((hit_stroke) && (hit_stroke->totpoints == 2)) {
    pt = &hit_stroke->points[0];
    pt1 = &hit_stroke->points[1];
    if (len_v3v3(&pt->x, &pt1->x) < 0.001f) {
      BLI_remlink(&hit_layer->actframe->strokes, hit_stroke);
      BKE_gpencil_free_stroke(hit_stroke);
      hit_stroke = NULL;
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
    gp_stroke_delete_tagged_points(hit_layer->actframe, hit_stroke, gpsn, GP_SPOINT_TAG, false, 1);
  }
}

static int gpencil_cutter_lasso_select(bContext *C,
                                       wmOperator *op,
                                       GPencilTestFn is_inside_fn,
                                       void *user_data)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  ScrArea *sa = CTX_wm_area(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  const float scale = ts->gp_sculpt.isect_threshold;

  bGPDspoint *pt;
  int i;
  GP_SpaceConversion gsc = {NULL};

  bool changed = false;

  /* sanity checks */
  if (sa == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No active area");
    return OPERATOR_CANCELLED;
  }

  /* init space conversion stuff */
  gp_point_conversion_init(C, &gsc);

  /* deselect all strokes first */
  CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
    for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
      pt->flag &= ~GP_SPOINT_SELECT;
    }

    gps->flag &= ~GP_STROKE_SELECT;
  }
  CTX_DATA_END;

  /* select points */
  GP_EDITABLE_STROKES_BEGIN (gpstroke_iter, C, gpl, gps) {
    int tot_inside = 0;
    const int oldtot = gps->totpoints;
    for (i = 0; i < gps->totpoints; i++) {
      pt = &gps->points[i];
      if ((pt->flag & GP_SPOINT_SELECT) || (pt->flag & GP_SPOINT_TAG)) {
        continue;
      }
      /* convert point coords to screenspace */
      const bool is_inside = is_inside_fn(gps, pt, &gsc, gpstroke_iter.diff_mat, user_data);
      if (is_inside) {
        tot_inside++;
        changed = true;
        pt->flag |= GP_SPOINT_SELECT;
        gps->flag |= GP_STROKE_SELECT;
        float r_hita[3], r_hitb[3];
        if (gps->totpoints > 1) {
          ED_gpencil_select_stroke_segment(gpl, gps, pt, true, true, scale, r_hita, r_hitb);
        }
        /* avoid infinite loops */
        if (gps->totpoints > oldtot) {
          break;
        }
      }
    }
    /* if mark all points inside lasso set to remove all stroke */
    if ((tot_inside == oldtot) || ((tot_inside == 1) && (oldtot == 2))) {
      for (i = 0; i < gps->totpoints; i++) {
        pt = &gps->points[i];
        pt->flag |= GP_SPOINT_SELECT;
      }
    }
  }
  GP_EDITABLE_STROKES_END(gpstroke_iter);

  /* dissolve selected points */
  bGPDstroke *gpsn;
  for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
    bGPDframe *gpf = gpl->actframe;
    if (gpf == NULL) {
      continue;
    }
    for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gpsn) {
      gpsn = gps->next;
      if (gps->flag & GP_STROKE_SELECT) {
        gpencil_cutter_dissolve(gpl, gps);
      }
    }
  }

  /* updates */
  if (changed) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);
    WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);
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
  ScrArea *sa = CTX_wm_area(C);
  /* sanity checks */
  if (sa == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No active area");
    return OPERATOR_CANCELLED;
  }

  struct GP_SelectLassoUserData data = {0};
  data.mcords = WM_gesture_lasso_path_to_array(C, op, &data.mcords_len);

  /* Sanity check. */
  if (data.mcords == NULL) {
    return OPERATOR_PASS_THROUGH;
  }

  /* Compute boundbox of lasso (for faster testing later). */
  BLI_lasso_boundbox(&data.rect, data.mcords, data.mcords_len);

  gpencil_cutter_lasso_select(C, op, gpencil_test_lasso, &data);

  MEM_freeN((void *)data.mcords);

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
  ot->flag = OPTYPE_UNDO | OPTYPE_USE_EVAL_DATA;

  /* properties */
  WM_operator_properties_gesture_lasso(ot);
}

bool ED_object_gpencil_exit(struct Main *bmain, Object *ob)
{
  bool ok = false;
  if (ob) {
    bGPdata *gpd = (bGPdata *)ob->data;

    gpd->flag &= ~(GP_DATA_STROKE_PAINTMODE | GP_DATA_STROKE_EDITMODE | GP_DATA_STROKE_SCULPTMODE |
                   GP_DATA_STROKE_WEIGHTMODE);

    ob->restore_mode = ob->mode;
    ob->mode &= ~(OB_MODE_PAINT_GPENCIL | OB_MODE_EDIT_GPENCIL | OB_MODE_SCULPT_GPENCIL |
                  OB_MODE_WEIGHT_GPENCIL);

    /* Inform all CoW versions that we changed the mode. */
    DEG_id_tag_update_ex(bmain, &ob->id, ID_RECALC_COPY_ON_WRITE);
    ok = true;
  }
  return ok;
}
