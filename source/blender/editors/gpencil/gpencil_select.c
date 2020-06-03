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
 * The Original Code is Copyright (C) 2014, Blender Foundation
 * This is a new part of Blender
 */

/** \file
 * \ingroup edgpencil
 */

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_lasso_2d.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_material.h"
#include "BKE_report.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_view2d.h"

#include "ED_gpencil.h"
#include "ED_select_utils.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "gpencil_intern.h"

/* -------------------------------------------------------------------- */
/** \name Shared Utilities
 * \{ */

/* Check if mouse inside stroke. */
static bool gpencil_point_inside_stroke(bGPDstroke *gps,
                                        GP_SpaceConversion *gsc,
                                        int mouse[2],
                                        const float diff_mat[4][4])
{
  bool hit = false;
  if (gps->totpoints == 0) {
    return hit;
  }

  int(*mcoords)[2] = NULL;
  int len = gps->totpoints;
  mcoords = MEM_mallocN(sizeof(int) * 2 * len, __func__);

  /* Convert stroke to 2D array of points. */
  bGPDspoint *pt;
  int i;
  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
    bGPDspoint pt2;
    gp_point_to_parent_space(pt, diff_mat, &pt2);
    gp_point_to_xy(gsc, gps, &pt2, &mcoords[i][0], &mcoords[i][1]);
  }

  /* Compute boundbox of lasso (for faster testing later). */
  rcti rect;
  BLI_lasso_boundbox(&rect, mcoords, len);

  /* Test if point inside stroke. */
  hit = ((!ELEM(V2D_IS_CLIPPED, mouse[0], mouse[1])) &&
         BLI_rcti_isect_pt(&rect, mouse[0], mouse[1]) &&
         BLI_lasso_is_point_inside(mcoords, len, mouse[0], mouse[1], INT_MAX));

  /* Free memory. */
  MEM_SAFE_FREE(mcoords);

  return hit;
}

/* Convert sculpt mask mode to Select mode */
static int gpencil_select_mode_from_sculpt(eGP_Sculpt_SelectMaskFlag mode)
{
  if (mode & GP_SCULPT_MASK_SELECTMODE_POINT) {
    return GP_SELECTMODE_POINT;
  }
  else if (mode & GP_SCULPT_MASK_SELECTMODE_STROKE) {
    return GP_SELECTMODE_STROKE;
  }
  else if (mode & GP_SCULPT_MASK_SELECTMODE_SEGMENT) {
    return GP_SELECTMODE_SEGMENT;
  }
  else {
    return GP_SELECTMODE_POINT;
  }
}

/* Convert vertex mask mode to Select mode */
static int gpencil_select_mode_from_vertex(eGP_Sculpt_SelectMaskFlag mode)
{
  if (mode & GP_VERTEX_MASK_SELECTMODE_POINT) {
    return GP_SELECTMODE_POINT;
  }
  else if (mode & GP_VERTEX_MASK_SELECTMODE_STROKE) {
    return GP_SELECTMODE_STROKE;
  }
  else if (mode & GP_VERTEX_MASK_SELECTMODE_SEGMENT) {
    return GP_SELECTMODE_SEGMENT;
  }
  else {
    return GP_SELECTMODE_POINT;
  }
}

static bool gpencil_select_poll(bContext *C)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  if (GPENCIL_SCULPT_MODE(gpd)) {
    if (!(GPENCIL_ANY_SCULPT_MASK(ts->gpencil_selectmode_sculpt))) {
      return false;
    }
  }

  if (GPENCIL_VERTEX_MODE(gpd)) {
    if (!(GPENCIL_ANY_VERTEX_MASK(ts->gpencil_selectmode_vertex))) {
      return false;
    }
  }

  /* we just need some visible strokes, and to be in editmode or other modes only to catch event */
  if (GPENCIL_ANY_MODE(gpd)) {
    /* TODO: include a check for visible strokes? */
    if (gpd->layers.first) {
      return true;
    }
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select All Operator
 * \{ */
static bool gpencil_select_all_poll(bContext *C)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  /* we just need some visible strokes, and to be in editmode or other modes only to catch event */
  if (GPENCIL_ANY_MODE(gpd)) {
    if (gpd->layers.first) {
      return true;
    }
  }

  return false;
}

static int gpencil_select_all_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  int action = RNA_enum_get(op->ptr, "action");

  if (gpd == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
    return OPERATOR_CANCELLED;
  }

  /* if not edit/sculpt mode, the event is catched but not processed */
  if (GPENCIL_NONE_EDIT_MODE(gpd)) {
    return OPERATOR_CANCELLED;
  }

  /* For sculpt mode, if mask is disable, only allows deselect */
  if (GPENCIL_SCULPT_MODE(gpd)) {
    ToolSettings *ts = CTX_data_tool_settings(C);
    if ((!(GPENCIL_ANY_SCULPT_MASK(ts->gpencil_selectmode_sculpt))) && (action != SEL_DESELECT)) {
      return OPERATOR_CANCELLED;
    }
  }

  ED_gpencil_select_toggle_all(C, action);

  /* updates */
  DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

  /* copy on write tag is needed, or else no refresh happens */
  DEG_id_tag_update(&gpd->id, ID_RECALC_COPY_ON_WRITE);

  WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);
  return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "(De)select All Strokes";
  ot->idname = "GPENCIL_OT_select_all";
  ot->description = "Change selection of all Grease Pencil strokes currently visible";

  /* callbacks */
  ot->exec = gpencil_select_all_exec;
  ot->poll = gpencil_select_all_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_select_all(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked Operator
 * \{ */

static int gpencil_select_linked_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  if (gpd == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
    return OPERATOR_CANCELLED;
  }

  /* if not edit/sculpt mode, the event is catched but not processed */
  if (GPENCIL_NONE_EDIT_MODE(gpd)) {
    return OPERATOR_CANCELLED;
  }

  /* select all points in selected strokes */
  CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
    if (gps->flag & GP_STROKE_SELECT) {
      bGPDspoint *pt;
      int i;

      for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
        pt->flag |= GP_SPOINT_SELECT;
      }
    }
  }
  CTX_DATA_END;

  /* updates */
  DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

  /* copy on write tag is needed, or else no refresh happens */
  DEG_id_tag_update(&gpd->id, ID_RECALC_COPY_ON_WRITE);

  WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);
  return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_linked(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Linked";
  ot->idname = "GPENCIL_OT_select_linked";
  ot->description = "Select all points in same strokes as already selected points";

  /* callbacks */
  ot->exec = gpencil_select_linked_exec;
  ot->poll = gpencil_select_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Alternate Operator
 * \{ */

static int gpencil_select_alternate_exec(bContext *C, wmOperator *op)
{
  const bool unselect_ends = RNA_boolean_get(op->ptr, "unselect_ends");
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  if (gpd == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
    return OPERATOR_CANCELLED;
  }

  /* if not edit/sculpt mode, the event is catched but not processed */
  if (GPENCIL_NONE_EDIT_MODE(gpd)) {
    return OPERATOR_CANCELLED;
  }

  /* select all points in selected strokes */
  CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
    if ((gps->flag & GP_STROKE_SELECT) && (gps->totpoints > 1)) {
      bGPDspoint *pt;
      int row = 0;
      int start = 0;
      if (unselect_ends) {
        start = 1;
      }

      for (int i = start; i < gps->totpoints; i++) {
        pt = &gps->points[i];
        if ((row % 2) == 0) {
          pt->flag |= GP_SPOINT_SELECT;
        }
        else {
          pt->flag &= ~GP_SPOINT_SELECT;
        }
        row++;
      }

      /* unselect start and end points */
      if (unselect_ends) {
        pt = &gps->points[0];
        pt->flag &= ~GP_SPOINT_SELECT;

        pt = &gps->points[gps->totpoints - 1];
        pt->flag &= ~GP_SPOINT_SELECT;
      }
    }
  }
  CTX_DATA_END;

  /* updates */
  DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

  /* copy on write tag is needed, or else no refresh happens */
  DEG_id_tag_update(&gpd->id, ID_RECALC_COPY_ON_WRITE);

  WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);
  return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_alternate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Alternated";
  ot->idname = "GPENCIL_OT_select_alternate";
  ot->description = "Select alternative points in same strokes as already selected points";

  /* callbacks */
  ot->exec = gpencil_select_alternate_exec;
  ot->poll = gpencil_select_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna,
                  "unselect_ends",
                  true,
                  "Unselect Ends",
                  "Do not select the first and last point of the stroke");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Grouped Operator
 * \{ */

typedef enum eGP_SelectGrouped {
  /* Select strokes in the same layer */
  GP_SEL_SAME_LAYER = 0,

  /* Select strokes with the same color */
  GP_SEL_SAME_MATERIAL = 1,

  /* TODO: All with same prefix -
   * Useful for isolating all layers for a particular character for instance. */
  /* TODO: All with same appearance - color/opacity/volumetric/fills ? */
} eGP_SelectGrouped;

/* ----------------------------------- */

/* On each visible layer, check for selected strokes - if found, select all others */
static void gp_select_same_layer(bContext *C)
{
  Scene *scene = CTX_data_scene(C);

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *gpf = BKE_gpencil_layer_frame_get(gpl, CFRA, GP_GETFRAME_USE_PREV);
    bGPDstroke *gps;
    bool found = false;

    if (gpf == NULL) {
      continue;
    }

    /* Search for a selected stroke */
    for (gps = gpf->strokes.first; gps; gps = gps->next) {
      if (ED_gpencil_stroke_can_use(C, gps)) {
        if (gps->flag & GP_STROKE_SELECT) {
          found = true;
          break;
        }
      }
    }

    /* Select all if found */
    if (found) {
      for (gps = gpf->strokes.first; gps; gps = gps->next) {
        if (ED_gpencil_stroke_can_use(C, gps)) {
          bGPDspoint *pt;
          int i;

          for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
            pt->flag |= GP_SPOINT_SELECT;
          }

          gps->flag |= GP_STROKE_SELECT;
        }
      }
    }
  }
  CTX_DATA_END;
}

/* Select all strokes with same colors as selected ones */
static void gp_select_same_material(bContext *C)
{
  /* First, build set containing all the colors of selected strokes */
  GSet *selected_colors = BLI_gset_str_new("GP Selected Colors");

  CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
    if (gps->flag & GP_STROKE_SELECT) {
      /* add instead of insert here, otherwise the uniqueness check gets skipped,
       * and we get many duplicate entries...
       */
      BLI_gset_add(selected_colors, &gps->mat_nr);
    }
  }
  CTX_DATA_END;

  /* Second, select any visible stroke that uses these colors */
  CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
    if (BLI_gset_haskey(selected_colors, &gps->mat_nr)) {
      /* select this stroke */
      bGPDspoint *pt;
      int i;

      for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
        pt->flag |= GP_SPOINT_SELECT;
      }

      gps->flag |= GP_STROKE_SELECT;
    }
  }
  CTX_DATA_END;

  /* free memomy */
  if (selected_colors != NULL) {
    BLI_gset_free(selected_colors, NULL);
  }
}

/* ----------------------------------- */

static int gpencil_select_grouped_exec(bContext *C, wmOperator *op)
{
  eGP_SelectGrouped mode = RNA_enum_get(op->ptr, "type");
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  /* if not edit/sculpt mode, the event is catched but not processed */
  if (GPENCIL_NONE_EDIT_MODE(gpd)) {
    return OPERATOR_CANCELLED;
  }

  switch (mode) {
    case GP_SEL_SAME_LAYER:
      gp_select_same_layer(C);
      break;
    case GP_SEL_SAME_MATERIAL:
      gp_select_same_material(C);
      break;

    default:
      BLI_assert(!"unhandled select grouped gpencil mode");
      break;
  }

  /* updates */
  DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

  /* copy on write tag is needed, or else no refresh happens */
  DEG_id_tag_update(&gpd->id, ID_RECALC_COPY_ON_WRITE);

  WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);
  return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_grouped(wmOperatorType *ot)
{
  static const EnumPropertyItem prop_select_grouped_types[] = {
      {GP_SEL_SAME_LAYER, "LAYER", 0, "Layer", "Shared layers"},
      {GP_SEL_SAME_MATERIAL, "MATERIAL", 0, "Material", "Shared materials"},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Select Grouped";
  ot->idname = "GPENCIL_OT_select_grouped";
  ot->description = "Select all strokes with similar characteristics";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = gpencil_select_grouped_exec;
  ot->poll = gpencil_select_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  ot->prop = RNA_def_enum(
      ot->srna, "type", prop_select_grouped_types, GP_SEL_SAME_LAYER, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select First
 * \{ */

static int gpencil_select_first_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  /* if not edit/sculpt mode, the event is catched but not processed */
  if (GPENCIL_NONE_EDIT_MODE(gpd)) {
    return OPERATOR_CANCELLED;
  }

  const bool only_selected = RNA_boolean_get(op->ptr, "only_selected_strokes");
  const bool extend = RNA_boolean_get(op->ptr, "extend");

  CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
    /* skip stroke if we're only manipulating selected strokes */
    if (only_selected && !(gps->flag & GP_STROKE_SELECT)) {
      continue;
    }

    /* select first point */
    BLI_assert(gps->totpoints >= 1);

    gps->points->flag |= GP_SPOINT_SELECT;
    gps->flag |= GP_STROKE_SELECT;

    /* deselect rest? */
    if ((extend == false) && (gps->totpoints > 1)) {
      /* start from index 1, to skip the first point that we'd just selected... */
      bGPDspoint *pt = &gps->points[1];
      int i = 1;

      for (; i < gps->totpoints; i++, pt++) {
        pt->flag &= ~GP_SPOINT_SELECT;
      }
    }
  }
  CTX_DATA_END;

  /* updates */
  DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

  /* copy on write tag is needed, or else no refresh happens */
  DEG_id_tag_update(&gpd->id, ID_RECALC_COPY_ON_WRITE);

  WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);
  return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_first(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select First";
  ot->idname = "GPENCIL_OT_select_first";
  ot->description = "Select first point in Grease Pencil strokes";

  /* callbacks */
  ot->exec = gpencil_select_first_exec;
  ot->poll = gpencil_select_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna,
                  "only_selected_strokes",
                  false,
                  "Selected Strokes Only",
                  "Only select the first point of strokes that already have points selected");

  RNA_def_boolean(ot->srna,
                  "extend",
                  false,
                  "Extend",
                  "Extend selection instead of deselecting all other selected points");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select First
 * \{ */

static int gpencil_select_last_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  /* if not edit/sculpt mode, the event is catched but not processed */
  if (GPENCIL_NONE_EDIT_MODE(gpd)) {
    return OPERATOR_CANCELLED;
  }

  const bool only_selected = RNA_boolean_get(op->ptr, "only_selected_strokes");
  const bool extend = RNA_boolean_get(op->ptr, "extend");

  CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
    /* skip stroke if we're only manipulating selected strokes */
    if (only_selected && !(gps->flag & GP_STROKE_SELECT)) {
      continue;
    }

    /* select last point */
    BLI_assert(gps->totpoints >= 1);

    gps->points[gps->totpoints - 1].flag |= GP_SPOINT_SELECT;
    gps->flag |= GP_STROKE_SELECT;

    /* deselect rest? */
    if ((extend == false) && (gps->totpoints > 1)) {
      /* don't include the last point... */
      bGPDspoint *pt = gps->points;
      int i = 1;

      for (; i < gps->totpoints - 1; i++, pt++) {
        pt->flag &= ~GP_SPOINT_SELECT;
      }
    }
  }
  CTX_DATA_END;

  /* updates */
  DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

  /* copy on write tag is needed, or else no refresh happens */
  DEG_id_tag_update(&gpd->id, ID_RECALC_COPY_ON_WRITE);

  WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);
  return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_last(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Last";
  ot->idname = "GPENCIL_OT_select_last";
  ot->description = "Select last point in Grease Pencil strokes";

  /* callbacks */
  ot->exec = gpencil_select_last_exec;
  ot->poll = gpencil_select_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna,
                  "only_selected_strokes",
                  false,
                  "Selected Strokes Only",
                  "Only select the last point of strokes that already have points selected");

  RNA_def_boolean(ot->srna,
                  "extend",
                  false,
                  "Extend",
                  "Extend selection instead of deselecting all other selected points");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Mode Operator
 * \{ */

static int gpencil_select_more_exec(bContext *C, wmOperator *UNUSED(op))
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  /* if not edit/sculpt mode, the event is catched but not processed */
  if (GPENCIL_NONE_EDIT_MODE(gpd)) {
    return OPERATOR_CANCELLED;
  }

  CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
    if (gps->flag & GP_STROKE_SELECT) {
      bGPDspoint *pt;
      int i;
      bool prev_sel;

      /* First Pass: Go in forward order,
       * expanding selection if previous was selected (pre changes).
       * - This pass covers the "after" edges of selection islands
       */
      prev_sel = false;
      for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
        if (pt->flag & GP_SPOINT_SELECT) {
          /* selected point - just set flag for next point */
          prev_sel = true;
        }
        else {
          /* unselected point - expand selection if previous was selected... */
          if (prev_sel) {
            pt->flag |= GP_SPOINT_SELECT;
          }
          prev_sel = false;
        }
      }

      /* Second Pass: Go in reverse order, doing the same as before (except in opposite order)
       * - This pass covers the "before" edges of selection islands
       */
      prev_sel = false;
      for (pt -= 1; i > 0; i--, pt--) {
        if (pt->flag & GP_SPOINT_SELECT) {
          prev_sel = true;
        }
        else {
          /* unselected point - expand selection if previous was selected... */
          if (prev_sel) {
            pt->flag |= GP_SPOINT_SELECT;
          }
          prev_sel = false;
        }
      }
    }
  }
  CTX_DATA_END;

  /* updates */
  DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

  /* copy on write tag is needed, or else no refresh happens */
  DEG_id_tag_update(&gpd->id, ID_RECALC_COPY_ON_WRITE);

  WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);
  return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_more(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select More";
  ot->idname = "GPENCIL_OT_select_more";
  ot->description = "Grow sets of selected Grease Pencil points";

  /* callbacks */
  ot->exec = gpencil_select_more_exec;
  ot->poll = gpencil_select_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Less Operator
 * \{ */

static int gpencil_select_less_exec(bContext *C, wmOperator *UNUSED(op))
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  /* if not edit/sculpt mode, the event is catched but not processed */
  if (GPENCIL_NONE_EDIT_MODE(gpd)) {
    return OPERATOR_CANCELLED;
  }

  CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
    if (gps->flag & GP_STROKE_SELECT) {
      bGPDspoint *pt;
      int i;
      bool prev_sel;

      /* First Pass: Go in forward order, shrinking selection
       * if previous was not selected (pre changes).
       * - This pass covers the "after" edges of selection islands
       */
      prev_sel = false;
      for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
        if (pt->flag & GP_SPOINT_SELECT) {
          /* shrink if previous wasn't selected */
          if (prev_sel == false) {
            pt->flag &= ~GP_SPOINT_SELECT;
          }
          prev_sel = true;
        }
        else {
          /* mark previous as being unselected - and hence, is trigger for shrinking */
          prev_sel = false;
        }
      }

      /* Second Pass: Go in reverse order, doing the same as before (except in opposite order)
       * - This pass covers the "before" edges of selection islands
       */
      prev_sel = false;
      for (pt -= 1; i > 0; i--, pt--) {
        if (pt->flag & GP_SPOINT_SELECT) {
          /* shrink if previous wasn't selected */
          if (prev_sel == false) {
            pt->flag &= ~GP_SPOINT_SELECT;
          }
          prev_sel = true;
        }
        else {
          /* mark previous as being unselected - and hence, is trigger for shrinking */
          prev_sel = false;
        }
      }
    }
  }
  CTX_DATA_END;

  /* updates */
  DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

  /* copy on write tag is needed, or else no refresh happens */
  DEG_id_tag_update(&gpd->id, ID_RECALC_COPY_ON_WRITE);

  WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);
  return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_less(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Less";
  ot->idname = "GPENCIL_OT_select_less";
  ot->description = "Shrink sets of selected Grease Pencil points";

  /* callbacks */
  ot->exec = gpencil_select_less_exec;
  ot->poll = gpencil_select_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Circle Select Operator
 * \{ */

/**
 * Helper to check if a given stroke is within the area.
 *
 * \note Code here is adapted (i.e. copied directly)
 * from gpencil_paint.c #gp_stroke_eraser_dostroke().
 * It would be great to de-duplicate the logic here sometime, but that can wait.
 */
static bool gp_stroke_do_circle_sel(bGPdata *UNUSED(gpd),
                                    bGPDlayer *gpl,
                                    bGPDstroke *gps,
                                    GP_SpaceConversion *gsc,
                                    const int mx,
                                    const int my,
                                    const int radius,
                                    const bool select,
                                    rcti *rect,
                                    const float diff_mat[4][4],
                                    const int selectmode,
                                    const float scale)
{
  bGPDspoint *pt1 = NULL;
  bGPDspoint *pt2 = NULL;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  int i;
  bool changed = false;
  bGPDstroke *gps_active = (gps->runtime.gps_orig) ? gps->runtime.gps_orig : gps;
  bGPDspoint *pt_active = NULL;

  if (gps->totpoints == 1) {
    bGPDspoint pt_temp;
    gp_point_to_parent_space(gps->points, diff_mat, &pt_temp);
    gp_point_to_xy(gsc, gps, &pt_temp, &x0, &y0);

    /* do boundbox check first */
    if ((!ELEM(V2D_IS_CLIPPED, x0, y0)) && BLI_rcti_isect_pt(rect, x0, y0)) {
      /* only check if point is inside */
      if (((x0 - mx) * (x0 - mx) + (y0 - my) * (y0 - my)) <= radius * radius) {
        /* change selection */
        if (select) {
          gps_active->points->flag |= GP_SPOINT_SELECT;
          gps_active->flag |= GP_STROKE_SELECT;
        }
        else {
          gps_active->points->flag &= ~GP_SPOINT_SELECT;
          gps_active->flag &= ~GP_STROKE_SELECT;
        }

        return true;
      }
    }
  }
  else {
    /* Loop over the points in the stroke, checking for intersections
     * - an intersection means that we touched the stroke
     */
    bool hit = false;
    for (i = 0; (i + 1) < gps->totpoints; i++) {
      /* get points to work with */
      pt1 = gps->points + i;
      pt2 = gps->points + i + 1;
      bGPDspoint npt;
      gp_point_to_parent_space(pt1, diff_mat, &npt);
      gp_point_to_xy(gsc, gps, &npt, &x0, &y0);

      gp_point_to_parent_space(pt2, diff_mat, &npt);
      gp_point_to_xy(gsc, gps, &npt, &x1, &y1);

      /* check that point segment of the boundbox of the selection stroke */
      if (((!ELEM(V2D_IS_CLIPPED, x0, y0)) && BLI_rcti_isect_pt(rect, x0, y0)) ||
          ((!ELEM(V2D_IS_CLIPPED, x1, y1)) && BLI_rcti_isect_pt(rect, x1, y1))) {
        float mval[2] = {(float)mx, (float)my};

        /* check if point segment of stroke had anything to do with
         * eraser region  (either within stroke painted, or on its lines)
         * - this assumes that linewidth is irrelevant
         */
        if (gp_stroke_inside_circle(mval, radius, x0, y0, x1, y1)) {
          /* change selection of stroke, and then of both points
           * (as the last point otherwise wouldn't get selected
           * as we only do n-1 loops through).
           */
          hit = true;
          if (select) {
            pt_active = pt1->runtime.pt_orig;
            if (pt_active != NULL) {
              pt_active->flag |= GP_SPOINT_SELECT;
            }
            pt_active = pt2->runtime.pt_orig;
            if (pt_active != NULL) {
              pt_active->flag |= GP_SPOINT_SELECT;
            }
            changed = true;
          }
          else {
            pt_active = pt1->runtime.pt_orig;
            if (pt_active != NULL) {
              pt_active->flag &= ~GP_SPOINT_SELECT;
            }
            pt_active = pt2->runtime.pt_orig;
            if (pt_active != NULL) {
              pt_active->flag &= ~GP_SPOINT_SELECT;
            }
            changed = true;
          }
        }
      }
      /* if stroke mode, don't check more points */
      if ((hit) && (selectmode == GP_SELECTMODE_STROKE)) {
        break;
      }
    }

    /* if stroke mode expand selection */
    if ((hit) && (selectmode == GP_SELECTMODE_STROKE)) {
      for (i = 0, pt1 = gps->points; i < gps->totpoints; i++, pt1++) {
        pt_active = (pt1->runtime.pt_orig) ? pt1->runtime.pt_orig : pt1;
        if (pt_active != NULL) {
          if (select) {
            pt_active->flag |= GP_SPOINT_SELECT;
          }
          else {
            pt_active->flag &= ~GP_SPOINT_SELECT;
          }
        }
      }
    }

    /* expand selection to segment */
    pt_active = (pt1->runtime.pt_orig) ? pt1->runtime.pt_orig : pt1;
    if ((hit) && (selectmode == GP_SELECTMODE_SEGMENT) && (select) && (pt_active != NULL)) {
      float r_hita[3], r_hitb[3];
      bool hit_select = (bool)(pt1->flag & GP_SPOINT_SELECT);
      ED_gpencil_select_stroke_segment(
          gpl, gps_active, pt_active, hit_select, false, scale, r_hita, r_hitb);
    }

    /* Ensure that stroke selection is in sync with its points */
    BKE_gpencil_stroke_sync_selection(gps_active);
  }

  return changed;
}

static int gpencil_circle_select_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  Object *ob = CTX_data_active_object(C);

  int selectmode;
  if (ob && ob->mode == OB_MODE_SCULPT_GPENCIL) {
    selectmode = gpencil_select_mode_from_sculpt(ts->gpencil_selectmode_sculpt);
  }
  else if (ob && ob->mode == OB_MODE_VERTEX_GPENCIL) {
    selectmode = gpencil_select_mode_from_vertex(ts->gpencil_selectmode_vertex);
  }
  else {
    selectmode = ts->gpencil_selectmode_edit;
  }

  const float scale = ts->gp_sculpt.isect_threshold;

  /* if not edit/sculpt mode, the event is catched but not processed */
  if (GPENCIL_NONE_EDIT_MODE(gpd)) {
    return OPERATOR_CANCELLED;
  }

  ScrArea *area = CTX_wm_area(C);

  const int mx = RNA_int_get(op->ptr, "x");
  const int my = RNA_int_get(op->ptr, "y");
  const int radius = RNA_int_get(op->ptr, "radius");

  GP_SpaceConversion gsc = {NULL};
  /* for bounding rect around circle (for quicky intersection testing) */
  rcti rect = {0};

  bool changed = false;

  /* sanity checks */
  if (area == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No active area");
    return OPERATOR_CANCELLED;
  }

  const eSelectOp sel_op = ED_select_op_modal(RNA_enum_get(op->ptr, "mode"),
                                              WM_gesture_is_modal_first(op->customdata));
  const bool select = (sel_op != SEL_OP_SUB);
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    ED_gpencil_select_toggle_all(C, SEL_DESELECT);
    changed = true;
  }

  /* init space conversion stuff */
  gp_point_conversion_init(C, &gsc);

  /* rect is rectangle of selection circle */
  rect.xmin = mx - radius;
  rect.ymin = my - radius;
  rect.xmax = mx + radius;
  rect.ymax = my + radius;

  /* find visible strokes, and select if hit */
  GP_EVALUATED_STROKES_BEGIN (gpstroke_iter, C, gpl, gps) {
    changed |= gp_stroke_do_circle_sel(gpd,
                                       gpl,
                                       gps,
                                       &gsc,
                                       mx,
                                       my,
                                       radius,
                                       select,
                                       &rect,
                                       gpstroke_iter.diff_mat,
                                       selectmode,
                                       scale);
  }
  GP_EVALUATED_STROKES_END(gpstroke_iter);

  /* updates */
  if (changed) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

    /* copy on write tag is needed, or else no refresh happens */
    DEG_id_tag_update(&gpd->id, ID_RECALC_COPY_ON_WRITE);

    WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_circle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Circle Select";
  ot->description = "Select Grease Pencil strokes using brush selection";
  ot->idname = "GPENCIL_OT_select_circle";

  /* callbacks */
  ot->invoke = WM_gesture_circle_invoke;
  ot->modal = WM_gesture_circle_modal;
  ot->exec = gpencil_circle_select_exec;
  ot->poll = gpencil_select_poll;
  ot->cancel = WM_gesture_circle_cancel;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_gesture_circle(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic Select Utility
 *
 * Use for lasso & box select.
 *
 * \{ */

typedef bool (*GPencilTestFn)(bGPDstroke *gps,
                              bGPDspoint *pt,
                              const GP_SpaceConversion *gsc,
                              const float diff_mat[4][4],
                              void *user_data);

static int gpencil_generic_select_exec(
    bContext *C, wmOperator *op, GPencilTestFn is_inside_fn, rcti box, void *user_data)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  ScrArea *area = CTX_wm_area(C);

  int selectmode;
  if (ob && ob->mode == OB_MODE_SCULPT_GPENCIL) {
    selectmode = gpencil_select_mode_from_sculpt(ts->gpencil_selectmode_sculpt);
  }
  else if (ob && ob->mode == OB_MODE_VERTEX_GPENCIL) {
    selectmode = gpencil_select_mode_from_vertex(ts->gpencil_selectmode_vertex);
  }
  else {
    selectmode = ts->gpencil_selectmode_edit;
  }

  const bool strokemode = ((selectmode == GP_SELECTMODE_STROKE) &&
                           ((gpd->flag & GP_DATA_STROKE_PAINTMODE) == 0));
  const bool segmentmode = ((selectmode == GP_SELECTMODE_SEGMENT) &&
                            ((gpd->flag & GP_DATA_STROKE_PAINTMODE) == 0));

  const eSelectOp sel_op = RNA_enum_get(op->ptr, "mode");
  const float scale = ts->gp_sculpt.isect_threshold;

  GP_SpaceConversion gsc = {NULL};

  bool changed = false;

  /* sanity checks */
  if (area == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No active area");
    return OPERATOR_CANCELLED;
  }

  /* init space conversion stuff */
  gp_point_conversion_init(C, &gsc);

  /* deselect all strokes first? */
  if (SEL_OP_USE_PRE_DESELECT(sel_op) || (GPENCIL_PAINT_MODE(gpd))) {

    CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
      bGPDspoint *pt;
      int i;

      for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
        pt->flag &= ~GP_SPOINT_SELECT;
      }

      gps->flag &= ~GP_STROKE_SELECT;
    }
    CTX_DATA_END;
  }

  /* select/deselect points */
  GP_EVALUATED_STROKES_BEGIN (gpstroke_iter, C, gpl, gps) {
    bGPDstroke *gps_active = (gps->runtime.gps_orig) ? gps->runtime.gps_orig : gps;
    bool whole = false;

    bGPDspoint *pt;
    int i;
    bool hit = false;
    for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
      bGPDspoint *pt_active = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;

      /* convert point coords to screenspace */
      const bool is_inside = is_inside_fn(gps, pt, &gsc, gpstroke_iter.diff_mat, user_data);
      if (strokemode == false) {
        const bool is_select = (pt_active->flag & GP_SPOINT_SELECT) != 0;
        const int sel_op_result = ED_select_op_action_deselected(sel_op, is_select, is_inside);
        if (sel_op_result != -1) {
          SET_FLAG_FROM_TEST(pt_active->flag, sel_op_result, GP_SPOINT_SELECT);
          changed = true;
          hit = true;

          /* Expand selection to segment. */
          if (segmentmode) {
            bool hit_select = (bool)(pt_active->flag & GP_SPOINT_SELECT);
            float r_hita[3], r_hitb[3];
            ED_gpencil_select_stroke_segment(
                gpl, gps_active, pt_active, hit_select, false, scale, r_hita, r_hitb);
          }
        }
      }
      else {
        if (is_inside) {
          hit = true;
          break;
        }
      }
    }

    /* If nothing hit, check if the mouse is inside a filled stroke using the center or
     * Box or lasso area. */
    if (!hit) {
      /* Only check filled strokes. */
      MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);
      if ((gp_style->flag & GP_MATERIAL_FILL_SHOW) == 0) {
        continue;
      }
      int mval[2];
      mval[0] = (box.xmax + box.xmin) / 2;
      mval[1] = (box.ymax + box.ymin) / 2;

      whole = gpencil_point_inside_stroke(gps_active, &gsc, mval, gpstroke_iter.diff_mat);
    }

    /* if stroke mode expand selection. */
    if ((strokemode) || (whole)) {
      const bool is_select = BKE_gpencil_stroke_select_check(gps_active) || whole;
      const bool is_inside = hit || whole;
      const int sel_op_result = ED_select_op_action_deselected(sel_op, is_select, is_inside);
      if (sel_op_result != -1) {
        for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
          bGPDspoint *pt_active = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;

          if (sel_op_result) {
            pt_active->flag |= GP_SPOINT_SELECT;
          }
          else {
            pt_active->flag &= ~GP_SPOINT_SELECT;
          }
        }
        changed = true;
      }
    }

    /* Ensure that stroke selection is in sync with its points */
    BKE_gpencil_stroke_sync_selection(gps_active);
  }
  GP_EVALUATED_STROKES_END(gpstroke_iter);

  /* if paint mode,delete selected points */
  if (GPENCIL_PAINT_MODE(gpd)) {
    gp_delete_selected_point_wrap(C);
    changed = true;
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  }

  /* updates */
  if (changed) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

    /* copy on write tag is needed, or else no refresh happens */
    DEG_id_tag_update(&gpd->id, ID_RECALC_COPY_ON_WRITE);

    WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);
  }
  return OPERATOR_FINISHED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Box Select Operator
 * \{ */

struct GP_SelectBoxUserData {
  rcti rect;
};

static bool gpencil_test_box(bGPDstroke *gps,
                             bGPDspoint *pt,
                             const GP_SpaceConversion *gsc,
                             const float diff_mat[4][4],
                             void *user_data)
{
  const struct GP_SelectBoxUserData *data = user_data;
  bGPDspoint pt2;
  int x0, y0;
  gp_point_to_parent_space(pt, diff_mat, &pt2);
  gp_point_to_xy(gsc, gps, &pt2, &x0, &y0);
  return ((!ELEM(V2D_IS_CLIPPED, x0, y0)) && BLI_rcti_isect_pt(&data->rect, x0, y0));
}

static int gpencil_box_select_exec(bContext *C, wmOperator *op)
{
  struct GP_SelectBoxUserData data = {0};
  WM_operator_properties_border_to_rcti(op, &data.rect);
  rcti rect = data.rect;
  return gpencil_generic_select_exec(C, op, gpencil_test_box, rect, &data);
}

void GPENCIL_OT_select_box(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Box Select";
  ot->description = "Select Grease Pencil strokes within a rectangular region";
  ot->idname = "GPENCIL_OT_select_box";

  /* callbacks */
  ot->invoke = WM_gesture_box_invoke;
  ot->exec = gpencil_box_select_exec;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->poll = gpencil_select_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_gesture_box(ot);
  WM_operator_properties_select_operation(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lasso Select Operator
 * \{ */

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
  const struct GP_SelectLassoUserData *data = user_data;
  bGPDspoint pt2;
  int x0, y0;
  gp_point_to_parent_space(pt, diff_mat, &pt2);
  gp_point_to_xy(gsc, gps, &pt2, &x0, &y0);
  /* test if in lasso boundbox + within the lasso noose */
  return ((!ELEM(V2D_IS_CLIPPED, x0, y0)) && BLI_rcti_isect_pt(&data->rect, x0, y0) &&
          BLI_lasso_is_point_inside(data->mcoords, data->mcoords_len, x0, y0, INT_MAX));
}

static int gpencil_lasso_select_exec(bContext *C, wmOperator *op)
{
  struct GP_SelectLassoUserData data = {0};
  data.mcoords = WM_gesture_lasso_path_to_array(C, op, &data.mcoords_len);

  /* Sanity check. */
  if (data.mcoords == NULL) {
    return OPERATOR_PASS_THROUGH;
  }

  /* Compute boundbox of lasso (for faster testing later). */
  BLI_lasso_boundbox(&data.rect, data.mcoords, data.mcoords_len);

  rcti rect = data.rect;
  int ret = gpencil_generic_select_exec(C, op, gpencil_test_lasso, rect, &data);

  MEM_freeN((void *)data.mcoords);

  return ret;
}

void GPENCIL_OT_select_lasso(wmOperatorType *ot)
{
  ot->name = "Lasso Select Strokes";
  ot->description = "Select Grease Pencil strokes using lasso selection";
  ot->idname = "GPENCIL_OT_select_lasso";

  ot->invoke = WM_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = gpencil_lasso_select_exec;
  ot->poll = gpencil_select_poll;
  ot->cancel = WM_gesture_lasso_cancel;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_select_operation(ot);
  WM_operator_properties_gesture_lasso(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mouse Pick Select Operator
 * \{ */

/* helper to deselect all selected strokes/points */
static void deselect_all_selected(bContext *C)
{
  CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
    /* deselect stroke and its points if selected */
    if (gps->flag & GP_STROKE_SELECT) {
      bGPDspoint *pt;
      int i;

      /* deselect points */
      for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
        pt->flag &= ~GP_SPOINT_SELECT;
      }

      /* deselect stroke itself too */
      gps->flag &= ~GP_STROKE_SELECT;
    }
  }
  CTX_DATA_END;
}

static int gpencil_select_exec(bContext *C, wmOperator *op)
{
  ScrArea *area = CTX_wm_area(C);
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  const float scale = ts->gp_sculpt.isect_threshold;
  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);

  /* "radius" is simply a threshold (screen space) to make it easier to test with a tolerance */
  const float radius = 0.4f * U.widget_unit;
  const int radius_squared = (int)(radius * radius);

  const bool use_shift_extend = RNA_boolean_get(op->ptr, "use_shift_extend");
  bool extend = RNA_boolean_get(op->ptr, "extend") || use_shift_extend;
  bool deselect = RNA_boolean_get(op->ptr, "deselect");
  bool toggle = RNA_boolean_get(op->ptr, "toggle");
  bool whole = RNA_boolean_get(op->ptr, "entire_strokes");
  const bool deselect_all = RNA_boolean_get(op->ptr, "deselect_all") && !use_shift_extend;

  int mval[2] = {0};

  GP_SpaceConversion gsc = {NULL};

  bGPDlayer *hit_layer = NULL;
  bGPDstroke *hit_stroke = NULL;
  bGPDspoint *hit_point = NULL;
  int hit_distance = radius_squared;

  /* sanity checks */
  if (area == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No active area");
    return OPERATOR_CANCELLED;
  }

  /* if select mode is stroke, use whole stroke */
  if ((ob) && (ob->mode == OB_MODE_SCULPT_GPENCIL)) {
    whole = (bool)(gpencil_select_mode_from_sculpt(ts->gpencil_selectmode_sculpt) ==
                   GP_SELECTMODE_STROKE);
  }
  else if ((ob) && (ob->mode == OB_MODE_VERTEX_GPENCIL)) {
    whole = (bool)(gpencil_select_mode_from_vertex(ts->gpencil_selectmode_sculpt) ==
                   GP_SELECTMODE_STROKE);
  }
  else {
    whole = (bool)(ts->gpencil_selectmode_edit == GP_SELECTMODE_STROKE);
  }

  /* init space conversion stuff */
  gp_point_conversion_init(C, &gsc);

  /* get mouse location */
  RNA_int_get_array(op->ptr, "location", mval);

  /* First Pass: Find stroke point which gets hit */
  GP_EVALUATED_STROKES_BEGIN (gpstroke_iter, C, gpl, gps) {
    bGPDstroke *gps_active = (gps->runtime.gps_orig) ? gps->runtime.gps_orig : gps;
    bGPDspoint *pt;
    int i;

    /* Check boundbox to speedup. */
    float fmval[2];
    copy_v2fl_v2i(fmval, mval);
    if (!ED_gpencil_stroke_check_collision(
            &gsc, gps_active, fmval, radius, gpstroke_iter.diff_mat)) {
      continue;
    }

    /* firstly, check for hit-point */
    for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
      int xy[2];
      if ((!is_multiedit) && (pt->runtime.pt_orig == NULL)) {
        continue;
      }

      bGPDspoint pt2;
      gp_point_to_parent_space(pt, gpstroke_iter.diff_mat, &pt2);
      gp_point_to_xy(&gsc, gps, &pt2, &xy[0], &xy[1]);

      /* do boundbox check first */
      if (!ELEM(V2D_IS_CLIPPED, xy[0], xy[1])) {
        const int pt_distance = len_manhattan_v2v2_int(mval, xy);

        /* check if point is inside */
        if (pt_distance <= radius_squared) {
          /* only use this point if it is a better match than the current hit - T44685 */
          if (pt_distance < hit_distance) {
            hit_layer = gpl;
            hit_stroke = gps_active;
            hit_point = (!is_multiedit) ? pt->runtime.pt_orig : pt;
            hit_distance = pt_distance;
          }
        }
      }
    }
    if (ELEM(NULL, hit_stroke, hit_point)) {
      /* If nothing hit, check if the mouse is inside any filled stroke.
       * Only check filling materials. */
      MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);
      if ((gp_style->flag & GP_MATERIAL_FILL_SHOW) == 0) {
        continue;
      }
      bool hit_fill = gpencil_point_inside_stroke(gps, &gsc, mval, gpstroke_iter.diff_mat);
      if (hit_fill) {
        hit_stroke = gps_active;
        hit_point = &gps_active->points[0];
        /* Extend selection to all stroke. */
        whole = true;
      }
    }
  }
  GP_EVALUATED_STROKES_END(gpstroke_iter);

  /* Abort if nothing hit... */
  if (ELEM(NULL, hit_stroke, hit_point)) {

    if (deselect_all) {
      /* since left mouse select change, deselect all if click outside any hit */
      deselect_all_selected(C);

      /* copy on write tag is needed, or else no refresh happens */
      DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);
      DEG_id_tag_update(&gpd->id, ID_RECALC_COPY_ON_WRITE);
      WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);

      return OPERATOR_FINISHED;
    }

    return OPERATOR_CANCELLED;
  }

  /* adjust selection behavior - for toggle option */
  if (toggle) {
    deselect = (hit_point->flag & GP_SPOINT_SELECT) != 0;
  }

  /* If not extending selection, deselect everything else */
  if (extend == false) {
    deselect_all_selected(C);
  }

  /* Perform selection operations... */
  if (whole) {
    bGPDspoint *pt;
    int i;

    /* entire stroke's points */
    for (i = 0, pt = hit_stroke->points; i < hit_stroke->totpoints; i++, pt++) {
      if (deselect == false) {
        pt->flag |= GP_SPOINT_SELECT;
      }
      else {
        pt->flag &= ~GP_SPOINT_SELECT;
      }
    }

    /* stroke too... */
    if (deselect == false) {
      hit_stroke->flag |= GP_STROKE_SELECT;
    }
    else {
      hit_stroke->flag &= ~GP_STROKE_SELECT;
    }
  }
  else {
    /* just the point (and the stroke) */
    if (deselect == false) {
      /* we're adding selection, so selection must be true */
      hit_point->flag |= GP_SPOINT_SELECT;
      hit_stroke->flag |= GP_STROKE_SELECT;

      /* expand selection to segment */
      int selectmode;
      if (ob && ob->mode == OB_MODE_SCULPT_GPENCIL) {
        selectmode = gpencil_select_mode_from_sculpt(ts->gpencil_selectmode_sculpt);
      }
      else if (ob && ob->mode == OB_MODE_VERTEX_GPENCIL) {
        selectmode = gpencil_select_mode_from_vertex(ts->gpencil_selectmode_vertex);
      }
      else {
        selectmode = ts->gpencil_selectmode_edit;
      }

      if (selectmode == GP_SELECTMODE_SEGMENT) {
        float r_hita[3], r_hitb[3];
        bool hit_select = (bool)(hit_point->flag & GP_SPOINT_SELECT);
        ED_gpencil_select_stroke_segment(
            hit_layer, hit_stroke, hit_point, hit_select, false, scale, r_hita, r_hitb);
      }
    }
    else {
      /* deselect point */
      hit_point->flag &= ~GP_SPOINT_SELECT;

      /* ensure that stroke is selected correctly */
      BKE_gpencil_stroke_sync_selection(hit_stroke);
    }
  }

  /* updates */
  if (hit_point != NULL) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

    /* copy on write tag is needed, or else no refresh happens */
    DEG_id_tag_update(&gpd->id, ID_RECALC_COPY_ON_WRITE);

    WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);
  }

  return OPERATOR_FINISHED;
}

static int gpencil_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  RNA_int_set_array(op->ptr, "location", event->mval);

  if (!RNA_struct_property_is_set(op->ptr, "use_shift_extend")) {
    RNA_boolean_set(op->ptr, "use_shift_extend", event->shift);
  }

  return gpencil_select_exec(C, op);
}

void GPENCIL_OT_select(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Select";
  ot->description = "Select Grease Pencil strokes and/or stroke points";
  ot->idname = "GPENCIL_OT_select";

  /* callbacks */
  ot->invoke = gpencil_select_invoke;
  ot->exec = gpencil_select_exec;
  ot->poll = gpencil_select_poll;

  /* flag */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_mouse_select(ot);

  prop = RNA_def_boolean(ot->srna,
                         "entire_strokes",
                         false,
                         "Entire Strokes",
                         "Select entire strokes instead of just the nearest stroke vertex");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_int_vector(ot->srna,
                            "location",
                            2,
                            NULL,
                            INT_MIN,
                            INT_MAX,
                            "Location",
                            "Mouse location",
                            INT_MIN,
                            INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN);

  prop = RNA_def_boolean(ot->srna, "use_shift_extend", false, "Extend", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/* Select by Vertex Color. */
/* Helper to create a hash of colors. */
static void gpencil_selected_hue_table(bContext *C,
                                       Object *ob,
                                       const int threshold,
                                       GHash *hue_table)
{
  const float range = pow(10, 5 - threshold);
  float hsv[3];

  /* Extract all colors. */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        if (ED_gpencil_stroke_can_use(C, gps) == false) {
          continue;
        }
        if (ED_gpencil_stroke_color_use(ob, gpl, gps) == false) {
          continue;
        }
        if ((gps->flag & GP_STROKE_SELECT) == 0) {
          continue;
        }

        /* Read all points to get all colors selected. */
        bGPDspoint *pt;
        int i;
        for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
          if (((pt->flag & GP_SPOINT_SELECT) == 0) || (pt->vert_color[3] == 0.0f)) {
            continue;
          }
          /* Round Hue value. */
          rgb_to_hsv_compat_v(pt->vert_color, hsv);
          uint key = truncf(hsv[0] * range);
          if (!BLI_ghash_haskey(hue_table, POINTER_FROM_INT(key))) {
            BLI_ghash_insert(hue_table, POINTER_FROM_INT(key), POINTER_FROM_INT(key));
          }
        }
      }
    }
  }
  CTX_DATA_END;
}

static bool gpencil_select_vertex_color_poll(bContext *C)
{
  ToolSettings *ts = CTX_data_tool_settings(C);
  Object *ob = CTX_data_active_object(C);
  if ((ob == NULL) || (ob->type != OB_GPENCIL)) {
    return false;
  }
  bGPdata *gpd = (bGPdata *)ob->data;

  if (GPENCIL_VERTEX_MODE(gpd)) {
    if (!(GPENCIL_ANY_VERTEX_MASK(ts->gpencil_selectmode_vertex))) {
      return false;
    }

    /* Any data to use. */
    if (gpd->layers.first) {
      return true;
    }
  }

  return false;
}

static int gpencil_select_vertex_color_exec(bContext *C, wmOperator *op)
{
  ToolSettings *ts = CTX_data_tool_settings(C);
  Object *ob = CTX_data_active_object(C);

  const float threshold = RNA_int_get(op->ptr, "threshold");
  const int selectmode = gpencil_select_mode_from_vertex(ts->gpencil_selectmode_vertex);
  bGPdata *gpd = (bGPdata *)ob->data;
  const float range = pow(10, 5 - threshold);

  bool done = false;

  /* Create a hash table with all selected colors. */
  GHash *hue_table = BLI_ghash_int_new(__func__);
  gpencil_selected_hue_table(C, ob, threshold, hue_table);
  if (BLI_ghash_len(hue_table) == 0) {
    BKE_report(op->reports, RPT_ERROR, "Select before some Vertex to use as a filter color");
    BLI_ghash_free(hue_table, NULL, NULL);

    return OPERATOR_CANCELLED;
  }

  /* Select any visible stroke that uses any of these colors. */
  CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
    bGPDspoint *pt;
    int i;
    bool gps_selected = false;
    /* Check all stroke points. */
    for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
      if (pt->vert_color[3] == 0.0f) {
        continue;
      }

      /* Only check Hue to get value and saturation full ranges. */
      float hsv[3];
      /* Round Hue value. */
      rgb_to_hsv_compat_v(pt->vert_color, hsv);
      uint key = truncf(hsv[0] * range);

      if (BLI_ghash_haskey(hue_table, POINTER_FROM_INT(key))) {
        pt->flag |= GP_SPOINT_SELECT;
        gps_selected = true;
      }
    }

    if (gps_selected) {
      gps->flag |= GP_STROKE_SELECT;
      done = true;

      /* Extend stroke selection. */
      if (selectmode == GP_SELECTMODE_STROKE) {
        bGPDspoint *pt1 = NULL;

        for (i = 0, pt1 = gps->points; i < gps->totpoints; i++, pt1++) {
          pt1->flag |= GP_SPOINT_SELECT;
        }
      }
    }
  }
  CTX_DATA_END;

  if (done) {
    /* updates */
    DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

    /* copy on write tag is needed, or else no refresh happens */
    DEG_id_tag_update(&gpd->id, ID_RECALC_COPY_ON_WRITE);

    WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, NULL);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);
  }

  /* Free memory. */
  if (hue_table != NULL) {
    BLI_ghash_free(hue_table, NULL, NULL);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_vertex_color(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Select Vertex Color";
  ot->idname = "GPENCIL_OT_select_vertex_color";
  ot->description = "Select all points with similar vertex color of current selected";

  /* callbacks */
  ot->exec = gpencil_select_vertex_color_exec;
  ot->poll = gpencil_select_vertex_color_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_int(
      ot->srna,
      "threshold",
      0,
      0,
      5,
      "Threshold",
      "Tolerance of the selection. Higher values select a wider range of similar colors",
      0,
      5);
  /* avoid re-using last var */
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */
