/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgpencil
 */

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_lasso_2d.h"
#include "BLI_math_color.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.hh"
#include "BKE_gpencil_curve_legacy.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_material.h"
#include "BKE_report.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_view2d.hh"

#include "ED_gpencil_legacy.hh"
#include "ED_select_utils.hh"
#include "ED_view3d.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "gpencil_intern.h"

/* -------------------------------------------------------------------- */
/** \name Shared Utilities
 * \{ */

/* Convert sculpt mask mode to Select mode */
static int gpencil_select_mode_from_sculpt(eGP_Sculpt_SelectMaskFlag mode)
{
  if (mode & GP_SCULPT_MASK_SELECTMODE_POINT) {
    return GP_SELECTMODE_POINT;
  }
  if (mode & GP_SCULPT_MASK_SELECTMODE_STROKE) {
    return GP_SELECTMODE_STROKE;
  }
  if (mode & GP_SCULPT_MASK_SELECTMODE_SEGMENT) {
    return GP_SELECTMODE_SEGMENT;
  }
  return GP_SELECTMODE_POINT;
}

/* Convert vertex mask mode to Select mode */
static int gpencil_select_mode_from_vertex(eGP_Sculpt_SelectMaskFlag mode)
{
  if (mode & GP_VERTEX_MASK_SELECTMODE_POINT) {
    return GP_SELECTMODE_POINT;
  }
  if (mode & GP_VERTEX_MASK_SELECTMODE_STROKE) {
    return GP_SELECTMODE_STROKE;
  }
  if (mode & GP_VERTEX_MASK_SELECTMODE_SEGMENT) {
    return GP_SELECTMODE_SEGMENT;
  }
  return GP_SELECTMODE_POINT;
}

static bool gpencil_select_poll(bContext *C)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  if (GPENCIL_SCULPT_MODE(gpd)) {
    if (!GPENCIL_ANY_SCULPT_MASK(ts->gpencil_selectmode_sculpt)) {
      return false;
    }
  }

  if (GPENCIL_VERTEX_MODE(gpd)) {
    if (!GPENCIL_ANY_VERTEX_MASK(ts->gpencil_selectmode_vertex)) {
      return false;
    }
  }

  /* We just need some visible strokes,
   * and to be in edit-mode or other modes only to catch event. */
  if (GPENCIL_ANY_MODE(gpd)) {
    /* TODO: include a check for visible strokes? */
    if (gpd->layers.first) {
      return true;
    }
  }

  return false;
}

static bool gpencil_3d_point_to_screen_space(ARegion *region,
                                             const float diff_mat[4][4],
                                             const float co[3],
                                             int r_co[2])
{
  float parent_co[3];
  mul_v3_m4v3(parent_co, diff_mat, co);
  int screen_co[2];
  if (ED_view3d_project_int_global(
          region, parent_co, screen_co, V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_WIN) ==
      V3D_PROJ_RET_OK)
  {
    if (!ELEM(V2D_IS_CLIPPED, screen_co[0], screen_co[1])) {
      copy_v2_v2_int(r_co, screen_co);
      return true;
    }
  }
  r_co[0] = V2D_IS_CLIPPED;
  r_co[1] = V2D_IS_CLIPPED;
  return false;
}

/* helper to deselect all selected strokes/points */
static void deselect_all_selected(bContext *C)
{
  /* Set selection index to 0. */
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);
  gpd->select_last_index = 0;

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
      BKE_gpencil_stroke_select_index_reset(gps);
    }

    /* deselect curve and curve points */
    if (gps->editcurve != nullptr) {
      bGPDcurve *gpc = gps->editcurve;
      for (int j = 0; j < gpc->tot_curve_points; j++) {
        bGPDcurve_point *gpc_pt = &gpc->curve_points[j];
        BezTriple *bezt = &gpc_pt->bezt;
        gpc_pt->flag &= ~GP_CURVE_POINT_SELECT;
        BEZT_DESEL_ALL(bezt);
      }

      gpc->flag &= ~GP_CURVE_SELECT;
    }
  }
  CTX_DATA_END;
}

static void select_all_stroke_points(bGPdata *gpd, bGPDstroke *gps, bool select)
{
  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    if (select) {
      pt->flag |= GP_SPOINT_SELECT;
    }
    else {
      pt->flag &= ~GP_SPOINT_SELECT;
    }
  }

  if (select) {
    gps->flag |= GP_STROKE_SELECT;
    BKE_gpencil_stroke_select_index_set(gpd, gps);
  }
  else {
    gps->flag &= ~GP_STROKE_SELECT;
    BKE_gpencil_stroke_select_index_reset(gps);
  }
}

static void select_all_curve_points(bGPdata *gpd, bGPDstroke *gps, bGPDcurve *gpc, bool deselect)
{
  for (int i = 0; i < gpc->tot_curve_points; i++) {
    bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
    BezTriple *bezt = &gpc_pt->bezt;
    if (deselect == false) {
      gpc_pt->flag |= GP_CURVE_POINT_SELECT;
      BEZT_SEL_ALL(bezt);
    }
    else {
      gpc_pt->flag &= ~GP_CURVE_POINT_SELECT;
      BEZT_DESEL_ALL(bezt);
    }
  }

  if (deselect == false) {
    gpc->flag |= GP_CURVE_SELECT;
    gps->flag |= GP_STROKE_SELECT;
    BKE_gpencil_stroke_select_index_set(gpd, gps);
  }
  else {
    gpc->flag &= ~GP_CURVE_SELECT;
    gps->flag &= ~GP_STROKE_SELECT;
    BKE_gpencil_stroke_select_index_reset(gps);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select All Operator
 * \{ */

static bool gpencil_select_all_poll(bContext *C)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  /* We just need some visible strokes,
   * and to be in edit-mode or other modes only to catch event. */
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
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));

  if (gpd == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
    return OPERATOR_CANCELLED;
  }

  /* If not edit/sculpt mode, the event has been caught but not processed. */
  if (GPENCIL_NONE_EDIT_MODE(gpd)) {
    return OPERATOR_CANCELLED;
  }

  /* For sculpt mode, if mask is disable, only allows deselect */
  if (GPENCIL_SCULPT_MODE(gpd)) {
    ToolSettings *ts = CTX_data_tool_settings(C);
    if (!GPENCIL_ANY_SCULPT_MASK(ts->gpencil_selectmode_sculpt) && (action != SEL_DESELECT)) {
      return OPERATOR_CANCELLED;
    }
  }

  if (is_curve_edit) {
    ED_gpencil_select_curve_toggle_all(C, action);
  }
  else {
    ED_gpencil_select_toggle_all(C, action);
  }

  /* updates */
  DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

  /* Copy-on-eval tag is needed, or else no refresh happens */
  DEG_id_tag_update(&gpd->id, ID_RECALC_SYNC_TO_EVAL);

  WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, nullptr);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);
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
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));

  if (gpd == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
    return OPERATOR_CANCELLED;
  }

  /* If not edit/sculpt mode, the event has been caught but not processed. */
  if (GPENCIL_NONE_EDIT_MODE(gpd)) {
    return OPERATOR_CANCELLED;
  }

  if (is_curve_edit) {
    GP_EDITABLE_CURVES_BEGIN(gps_iter, C, gpl, gps, gpc)
    {
      if (gpc->flag & GP_CURVE_SELECT) {
        for (int i = 0; i < gpc->tot_curve_points; i++) {
          bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
          BezTriple *bezt = &gpc_pt->bezt;
          gpc_pt->flag |= GP_CURVE_POINT_SELECT;
          BEZT_SEL_ALL(bezt);
        }
      }
    }
    GP_EDITABLE_CURVES_END(gps_iter);
  }
  else {
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
  }

  /* updates */
  DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

  /* Copy-on-eval tag is needed, or else no refresh happens */
  DEG_id_tag_update(&gpd->id, ID_RECALC_SYNC_TO_EVAL);

  WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, nullptr);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);
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
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  const bool unselect_ends = RNA_boolean_get(op->ptr, "unselect_ends");
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));

  if (gpd == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "No Grease Pencil data");
    return OPERATOR_CANCELLED;
  }

  /* If not edit/sculpt mode, the event has been caught but not processed. */
  if (GPENCIL_NONE_EDIT_MODE(gpd)) {
    return OPERATOR_CANCELLED;
  }

  bool changed = false;
  if (is_curve_edit) {
    GP_EDITABLE_CURVES_BEGIN(gps_iter, C, gpl, gps, gpc)
    {
      if ((gps->flag & GP_STROKE_SELECT) && (gps->totpoints > 1)) {
        int idx = 0;
        int start = 0;
        if (unselect_ends) {
          start = 1;
        }

        for (int i = start; i < gpc->tot_curve_points; i++) {
          bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
          if ((idx % 2) == 0) {
            gpc_pt->flag |= GP_SPOINT_SELECT;
            BEZT_SEL_ALL(&gpc_pt->bezt);
          }
          else {
            gpc_pt->flag &= ~GP_SPOINT_SELECT;
            BEZT_DESEL_ALL(&gpc_pt->bezt);
          }
          idx++;
        }

        if (unselect_ends) {
          bGPDcurve_point *gpc_pt = &gpc->curve_points[0];
          gpc_pt->flag &= ~GP_SPOINT_SELECT;
          BEZT_DESEL_ALL(&gpc_pt->bezt);

          gpc_pt = &gpc->curve_points[gpc->tot_curve_points - 1];
          gpc_pt->flag &= ~GP_SPOINT_SELECT;
          BEZT_DESEL_ALL(&gpc_pt->bezt);
        }

        BKE_gpencil_curve_sync_selection(gpd, gps);
        changed = true;
      }
    }
    GP_EDITABLE_CURVES_END(gps_iter);
  }
  else {
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

        changed = true;
      }
    }
    CTX_DATA_END;
  }

  if (changed) {
    /* updates */
    DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

    /* Copy-on-eval tag is needed, or else no refresh happens */
    DEG_id_tag_update(&gpd->id, ID_RECALC_SYNC_TO_EVAL);

    WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, nullptr);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);
  }

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
                  false,
                  "Unselect Ends",
                  "Do not select the first and last point of the stroke");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Random Operator
 * \{ */

static int gpencil_select_random_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  if ((gpd == nullptr) || GPENCIL_NONE_EDIT_MODE(gpd)) {
    return OPERATOR_CANCELLED;
  }

  const bool unselect_ends = RNA_boolean_get(op->ptr, "unselect_ends");
  const bool select = (RNA_enum_get(op->ptr, "action") == SEL_SELECT);
  const float randfac = RNA_float_get(op->ptr, "ratio");
  const int seed = WM_operator_properties_select_random_seed_increment_get(op);
  const int start = (unselect_ends) ? 1 : 0;
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));

  int selectmode;
  if (ob && ob->mode == OB_MODE_SCULPT_GPENCIL_LEGACY) {
    selectmode = gpencil_select_mode_from_sculpt(
        eGP_Sculpt_SelectMaskFlag(ts->gpencil_selectmode_sculpt));
  }
  else if (ob && ob->mode == OB_MODE_VERTEX_GPENCIL_LEGACY) {
    selectmode = gpencil_select_mode_from_vertex(
        eGP_Sculpt_SelectMaskFlag(ts->gpencil_selectmode_vertex));
  }
  else {
    selectmode = ts->gpencil_selectmode_edit;
  }

  bool changed = false;
  int seed_iter = seed;
  int stroke_idx = 0;

  if (is_curve_edit) {
    GP_EDITABLE_CURVES_BEGIN(gps_iter, C, gpl, gps, gpc)
    {
      /* Only apply to unselected strokes (if select). */
      if (select) {
        if ((gps->flag & GP_STROKE_SELECT) || (gps->totpoints == 0)) {
          continue;
        }
      }
      else {
        if (((gps->flag & GP_STROKE_SELECT) == 0) || (gps->totpoints == 0)) {
          continue;
        }
      }

      /* Different seed by stroke. */
      seed_iter += gps->totpoints + stroke_idx;
      stroke_idx++;

      if (selectmode == GP_SELECTMODE_STROKE) {
        RNG *rng = BLI_rng_new(seed_iter);
        const uint j = BLI_rng_get_uint(rng) % gps->totpoints;
        bool select_stroke = ((gps->totpoints * randfac) <= j) ? true : false;
        select_stroke ^= select;
        /* Curve function has select parameter inverted. */
        select_all_curve_points(gpd, gps, gps->editcurve, !select_stroke);
        changed = true;
        BLI_rng_free(rng);
      }
      else {
        int elem_map_len = 0;
        bGPDcurve_point **elem_map = static_cast<bGPDcurve_point **>(
            MEM_mallocN(sizeof(*elem_map) * gpc->tot_curve_points, __func__));
        bGPDcurve_point *ptc;
        for (int i = start; i < gpc->tot_curve_points; i++) {
          bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
          elem_map[elem_map_len++] = gpc_pt;
        }

        BLI_array_randomize(elem_map, sizeof(*elem_map), elem_map_len, seed_iter);
        const int count_select = elem_map_len * randfac;
        for (int i = 0; i < count_select; i++) {
          ptc = elem_map[i];
          if (select) {
            ptc->flag |= GP_SPOINT_SELECT;
            BEZT_SEL_ALL(&ptc->bezt);
          }
          else {
            ptc->flag &= ~GP_SPOINT_SELECT;
            BEZT_DESEL_ALL(&ptc->bezt);
          }
        }
        MEM_freeN(elem_map);

        /* unselect start and end points */
        if (unselect_ends) {
          bGPDcurve_point *gpc_pt = &gpc->curve_points[0];
          gpc_pt->flag &= ~GP_SPOINT_SELECT;
          BEZT_DESEL_ALL(&gpc_pt->bezt);

          gpc_pt = &gpc->curve_points[gpc->tot_curve_points - 1];
          gpc_pt->flag &= ~GP_SPOINT_SELECT;
          BEZT_DESEL_ALL(&gpc_pt->bezt);
        }

        BKE_gpencil_curve_sync_selection(gpd, gps);
      }

      changed = true;
    }
    GP_EDITABLE_CURVES_END(gps_iter);
  }
  else {
    CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
      /* Only apply to unselected strokes (if select). */
      if (select) {
        if ((gps->flag & GP_STROKE_SELECT) || (gps->totpoints == 0)) {
          continue;
        }
      }
      else {
        if (((gps->flag & GP_STROKE_SELECT) == 0) || (gps->totpoints == 0)) {
          continue;
        }
      }

      /* Different seed by stroke. */
      seed_iter += gps->totpoints + stroke_idx;
      stroke_idx++;

      if (selectmode == GP_SELECTMODE_STROKE) {
        RNG *rng = BLI_rng_new(seed_iter);
        const uint j = BLI_rng_get_uint(rng) % gps->totpoints;
        bool select_stroke = ((gps->totpoints * randfac) <= j) ? true : false;
        select_stroke ^= select;
        select_all_stroke_points(gpd, gps, select_stroke);
        changed = true;
        BLI_rng_free(rng);
      }
      else {
        int elem_map_len = 0;
        bGPDspoint **elem_map = static_cast<bGPDspoint **>(
            MEM_mallocN(sizeof(*elem_map) * gps->totpoints, __func__));
        bGPDspoint *pt;
        for (int i = start; i < gps->totpoints; i++) {
          pt = &gps->points[i];
          elem_map[elem_map_len++] = pt;
        }

        BLI_array_randomize(elem_map, sizeof(*elem_map), elem_map_len, seed_iter);
        const int count_select = elem_map_len * randfac;
        for (int i = 0; i < count_select; i++) {
          pt = elem_map[i];
          if (select) {
            pt->flag |= GP_SPOINT_SELECT;
          }
          else {
            pt->flag &= ~GP_SPOINT_SELECT;
          }
        }
        MEM_freeN(elem_map);

        /* unselect start and end points */
        if (unselect_ends) {
          pt = &gps->points[0];
          pt->flag &= ~GP_SPOINT_SELECT;

          pt = &gps->points[gps->totpoints - 1];
          pt->flag &= ~GP_SPOINT_SELECT;
        }

        BKE_gpencil_stroke_sync_selection(gpd, gps);
      }

      changed = true;
    }
    CTX_DATA_END;
  }

  if (changed) {
    /* updates */
    DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

    /* Copy-on-eval tag is needed, or else no refresh happens */
    DEG_id_tag_update(&gpd->id, ID_RECALC_SYNC_TO_EVAL);

    WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, nullptr);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_random(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Random";
  ot->idname = "GPENCIL_OT_select_random";
  ot->description = "Select random points for non selected strokes";

  /* callbacks */
  ot->exec = gpencil_select_random_exec;
  ot->poll = gpencil_select_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_select_random(ot);
  RNA_def_boolean(ot->srna,
                  "unselect_ends",
                  false,
                  "Unselect Ends",
                  "Do not select the first and last point of the stroke");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Grouped Operator
 * \{ */

enum eGP_SelectGrouped {
  /* Select strokes in the same layer */
  GP_SEL_SAME_LAYER = 0,

  /* Select strokes with the same color */
  GP_SEL_SAME_MATERIAL = 1,

  /* TODO: All with same prefix -
   * Useful for isolating all layers for a particular character for instance. */
  /* TODO: All with same appearance - color/opacity/volumetric/fills ? */
};

/* ----------------------------------- */

/* On each visible layer, check for selected strokes - if found, select all others */
static bool gpencil_select_same_layer(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));

  bool changed = false;
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *gpf = BKE_gpencil_layer_frame_get(gpl, scene->r.cfra, GP_GETFRAME_USE_PREV);
    bool found = false;

    if (gpf == nullptr) {
      continue;
    }

    /* Search for a selected stroke */
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
      if (ED_gpencil_stroke_can_use(C, gps)) {
        if (gps->flag & GP_STROKE_SELECT) {
          found = true;
          break;
        }
      }
    }

    /* Select all if found */
    if (found) {
      if (is_curve_edit) {
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          if (gps->editcurve != nullptr && ED_gpencil_stroke_can_use(C, gps)) {
            bGPDcurve *gpc = gps->editcurve;
            for (int i = 0; i < gpc->tot_curve_points; i++) {
              bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
              gpc_pt->flag |= GP_CURVE_POINT_SELECT;
              BEZT_SEL_ALL(&gpc_pt->bezt);
            }
            gpc->flag |= GP_CURVE_SELECT;
            gps->flag |= GP_STROKE_SELECT;
            BKE_gpencil_stroke_select_index_set(gpd, gps);

            changed = true;
          }
        }
      }
      else {
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          if (ED_gpencil_stroke_can_use(C, gps)) {
            bGPDspoint *pt;
            int i;

            for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
              pt->flag |= GP_SPOINT_SELECT;
            }

            gps->flag |= GP_STROKE_SELECT;
            BKE_gpencil_stroke_select_index_set(gpd, gps);

            changed = true;
          }
        }
      }
    }
  }
  CTX_DATA_END;

  return changed;
}

/* Select all strokes with same colors as selected ones */
static bool gpencil_select_same_material(bContext *C)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));
  /* First, build set containing all the colors of selected strokes */
  GSet *selected_colors = BLI_gset_int_new("GP Selected Colors");

  bool changed = false;

  CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
    if (gps->flag & GP_STROKE_SELECT) {
      /* add instead of insert here, otherwise the uniqueness check gets skipped,
       * and we get many duplicate entries...
       */
      BLI_gset_add(selected_colors, POINTER_FROM_INT(gps->mat_nr));
    }
  }
  CTX_DATA_END;

  /* Second, select any visible stroke that uses these colors */
  if (is_curve_edit) {
    CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
      if (gps->editcurve != nullptr &&
          BLI_gset_haskey(selected_colors, POINTER_FROM_INT(gps->mat_nr)))
      {
        bGPDcurve *gpc = gps->editcurve;
        for (int i = 0; i < gpc->tot_curve_points; i++) {
          bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
          gpc_pt->flag |= GP_CURVE_POINT_SELECT;
          BEZT_SEL_ALL(&gpc_pt->bezt);
        }
        gpc->flag |= GP_CURVE_SELECT;
        gps->flag |= GP_STROKE_SELECT;
        BKE_gpencil_stroke_select_index_set(gpd, gps);

        changed = true;
      }
    }
    CTX_DATA_END;
  }
  else {
    CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
      if (BLI_gset_haskey(selected_colors, POINTER_FROM_INT(gps->mat_nr))) {
        /* select this stroke */
        bGPDspoint *pt;
        int i;

        for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
          pt->flag |= GP_SPOINT_SELECT;
        }

        gps->flag |= GP_STROKE_SELECT;
        BKE_gpencil_stroke_select_index_set(gpd, gps);

        changed = true;
      }
    }
    CTX_DATA_END;
  }

  /* Free memory. */
  if (selected_colors != nullptr) {
    BLI_gset_free(selected_colors, nullptr);
  }

  return changed;
}

/* ----------------------------------- */

static int gpencil_select_grouped_exec(bContext *C, wmOperator *op)
{
  eGP_SelectGrouped mode = eGP_SelectGrouped(RNA_enum_get(op->ptr, "type"));
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  /* If not edit/sculpt mode, the event has been caught but not processed. */
  if (GPENCIL_NONE_EDIT_MODE(gpd)) {
    return OPERATOR_CANCELLED;
  }

  bool changed = false;

  switch (mode) {
    case GP_SEL_SAME_LAYER:
      changed = gpencil_select_same_layer(C);
      break;
    case GP_SEL_SAME_MATERIAL:
      changed = gpencil_select_same_material(C);
      break;

    default:
      BLI_assert_msg(0, "unhandled select grouped gpencil mode");
      break;
  }

  if (changed) {
    /* updates */
    DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

    /* Copy-on-eval tag is needed, or else no refresh happens */
    DEG_id_tag_update(&gpd->id, ID_RECALC_SYNC_TO_EVAL);

    WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, nullptr);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);
  }
  return OPERATOR_FINISHED;
}

void GPENCIL_OT_select_grouped(wmOperatorType *ot)
{
  static const EnumPropertyItem prop_select_grouped_types[] = {
      {GP_SEL_SAME_LAYER, "LAYER", 0, "Layer", "Shared layers"},
      {GP_SEL_SAME_MATERIAL, "MATERIAL", 0, "Material", "Shared materials"},
      {0, nullptr, 0, nullptr, nullptr},
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
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));

  /* If not edit/sculpt mode, the event has been caught but not processed. */
  if (GPENCIL_NONE_EDIT_MODE(gpd)) {
    return OPERATOR_CANCELLED;
  }

  const bool only_selected = RNA_boolean_get(op->ptr, "only_selected_strokes");
  const bool extend = RNA_boolean_get(op->ptr, "extend");

  bool changed = false;
  CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
    /* skip stroke if we're only manipulating selected strokes */
    if (only_selected && !(gps->flag & GP_STROKE_SELECT)) {
      continue;
    }

    /* select first point */
    BLI_assert(gps->totpoints >= 1);

    if (is_curve_edit) {
      if (gps->editcurve != nullptr) {
        bGPDcurve *gpc = gps->editcurve;
        gpc->curve_points[0].flag |= GP_CURVE_POINT_SELECT;
        BEZT_SEL_ALL(&gpc->curve_points[0].bezt);
        gpc->flag |= GP_CURVE_SELECT;
        gps->flag |= GP_STROKE_SELECT;
        BKE_gpencil_stroke_select_index_set(gpd, gps);

        if ((extend == false) && (gps->totpoints > 1)) {
          for (int i = 1; i < gpc->tot_curve_points; i++) {
            bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
            gpc_pt->flag &= ~GP_CURVE_POINT_SELECT;
            BEZT_DESEL_ALL(&gpc_pt->bezt);
          }
        }
        changed = true;
      }
    }
    else {
      gps->points->flag |= GP_SPOINT_SELECT;
      gps->flag |= GP_STROKE_SELECT;
      BKE_gpencil_stroke_select_index_set(gpd, gps);

      /* deselect rest? */
      if ((extend == false) && (gps->totpoints > 1)) {
        /* start from index 1, to skip the first point that we'd just selected... */
        bGPDspoint *pt = &gps->points[1];
        int i = 1;

        for (; i < gps->totpoints; i++, pt++) {
          pt->flag &= ~GP_SPOINT_SELECT;
        }
      }
      changed = true;
    }
  }
  CTX_DATA_END;

  if (changed) {
    /* updates */
    DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

    /* Copy-on-eval tag is needed, or else no refresh happens */
    DEG_id_tag_update(&gpd->id, ID_RECALC_SYNC_TO_EVAL);

    WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, nullptr);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);
  }

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
/** \name Select Last
 * \{ */

static int gpencil_select_last_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));

  /* If not edit/sculpt mode, the event has been caught but not processed. */
  if (GPENCIL_NONE_EDIT_MODE(gpd)) {
    return OPERATOR_CANCELLED;
  }

  const bool only_selected = RNA_boolean_get(op->ptr, "only_selected_strokes");
  const bool extend = RNA_boolean_get(op->ptr, "extend");

  bool changed = false;
  CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
    /* skip stroke if we're only manipulating selected strokes */
    if (only_selected && !(gps->flag & GP_STROKE_SELECT)) {
      continue;
    }

    /* select last point */
    BLI_assert(gps->totpoints >= 1);

    if (is_curve_edit) {
      if (gps->editcurve != nullptr) {
        bGPDcurve *gpc = gps->editcurve;
        gpc->curve_points[gpc->tot_curve_points - 1].flag |= GP_CURVE_POINT_SELECT;
        BEZT_SEL_ALL(&gpc->curve_points[gpc->tot_curve_points - 1].bezt);
        gpc->flag |= GP_CURVE_SELECT;
        gps->flag |= GP_STROKE_SELECT;
        BKE_gpencil_stroke_select_index_set(gpd, gps);
        if ((extend == false) && (gps->totpoints > 1)) {
          for (int i = 0; i < gpc->tot_curve_points - 1; i++) {
            bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
            gpc_pt->flag &= ~GP_CURVE_POINT_SELECT;
            BEZT_DESEL_ALL(&gpc_pt->bezt);
          }
        }
        changed = true;
      }
    }
    else {
      gps->points[gps->totpoints - 1].flag |= GP_SPOINT_SELECT;
      gps->flag |= GP_STROKE_SELECT;
      BKE_gpencil_stroke_select_index_set(gpd, gps);

      /* deselect rest? */
      if ((extend == false) && (gps->totpoints > 1)) {
        /* don't include the last point... */
        bGPDspoint *pt = gps->points;
        int i = 0;

        for (; i < gps->totpoints - 1; i++, pt++) {
          pt->flag &= ~GP_SPOINT_SELECT;
        }
      }

      changed = true;
    }
  }
  CTX_DATA_END;

  if (changed) {
    /* updates */
    DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

    /* Copy-on-eval tag is needed, or else no refresh happens */
    DEG_id_tag_update(&gpd->id, ID_RECALC_SYNC_TO_EVAL);

    WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, nullptr);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);
  }

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

static int gpencil_select_more_exec(bContext *C, wmOperator * /*op*/)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));
  /* If not edit/sculpt mode, the event has been caught but not processed. */
  if (GPENCIL_NONE_EDIT_MODE(gpd)) {
    return OPERATOR_CANCELLED;
  }

  bool changed = false;
  if (is_curve_edit) {
    GP_EDITABLE_STROKES_BEGIN (gp_iter, C, gpl, gps) {
      if (gps->editcurve != nullptr && gps->flag & GP_STROKE_SELECT) {
        bGPDcurve *editcurve = gps->editcurve;

        bool prev_sel = false;
        for (int i = 0; i < editcurve->tot_curve_points; i++) {
          bGPDcurve_point *gpc_pt = &editcurve->curve_points[i];
          BezTriple *bezt = &gpc_pt->bezt;
          if (gpc_pt->flag & GP_CURVE_POINT_SELECT) {
            /* selected point - just set flag for next point */
            prev_sel = true;
          }
          else {
            /* unselected point - expand selection if previous was selected... */
            if (prev_sel) {
              gpc_pt->flag |= GP_CURVE_POINT_SELECT;
              BEZT_SEL_ALL(bezt);
              changed = true;
            }
            prev_sel = false;
          }
        }

        prev_sel = false;
        for (int i = editcurve->tot_curve_points - 1; i >= 0; i--) {
          bGPDcurve_point *gpc_pt = &editcurve->curve_points[i];
          BezTriple *bezt = &gpc_pt->bezt;
          if (gpc_pt->flag & GP_CURVE_POINT_SELECT) {
            prev_sel = true;
          }
          else {
            /* unselected point - expand selection if previous was selected... */
            if (prev_sel) {
              gpc_pt->flag |= GP_CURVE_POINT_SELECT;
              BEZT_SEL_ALL(bezt);
              changed = true;
            }
            prev_sel = false;
          }
        }
      }
    }
    GP_EDITABLE_STROKES_END(gp_iter);
  }
  else {
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
              changed = true;
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
              changed = true;
            }
            prev_sel = false;
          }
        }
      }
    }
    CTX_DATA_END;
  }

  if (changed) {
    /* updates */
    DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

    /* Copy-on-eval tag is needed, or else no refresh happens */
    DEG_id_tag_update(&gpd->id, ID_RECALC_SYNC_TO_EVAL);

    WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, nullptr);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);
  }

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

static int gpencil_select_less_exec(bContext *C, wmOperator * /*op*/)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));

  /* If not edit/sculpt mode, the event has been caught but not processed. */
  if (GPENCIL_NONE_EDIT_MODE(gpd)) {
    return OPERATOR_CANCELLED;
  }

  bool changed = false;
  if (is_curve_edit) {
    GP_EDITABLE_STROKES_BEGIN (gp_iter, C, gpl, gps) {
      if (gps->editcurve != nullptr && gps->flag & GP_STROKE_SELECT) {
        bGPDcurve *editcurve = gps->editcurve;
        int i;

        bool prev_sel = false;
        for (i = 0; i < editcurve->tot_curve_points; i++) {
          bGPDcurve_point *gpc_pt = &editcurve->curve_points[i];
          BezTriple *bezt = &gpc_pt->bezt;
          if (gpc_pt->flag & GP_CURVE_POINT_SELECT) {
            /* shrink if previous wasn't selected */
            if (prev_sel == false) {
              gpc_pt->flag &= ~GP_CURVE_POINT_SELECT;
              BEZT_DESEL_ALL(bezt);
              changed = true;
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
        for (i = editcurve->tot_curve_points - 1; i > 0; i--) {
          bGPDcurve_point *gpc_pt = &editcurve->curve_points[i];
          BezTriple *bezt = &gpc_pt->bezt;
          if (gpc_pt->flag & GP_CURVE_POINT_SELECT) {
            /* shrink if previous wasn't selected */
            if (prev_sel == false) {
              gpc_pt->flag &= ~GP_CURVE_POINT_SELECT;
              BEZT_DESEL_ALL(bezt);
              changed = true;
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
    GP_EDITABLE_STROKES_END(gp_iter);
  }
  else {
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
              changed = true;
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
              changed = true;
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
  }

  if (changed) {
    /* updates */
    DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

    /* Copy-on-eval tag is needed, or else no refresh happens */
    DEG_id_tag_update(&gpd->id, ID_RECALC_SYNC_TO_EVAL);

    WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, nullptr);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);
  }

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
 * from `gpencil_paint.cc` #gpencil_stroke_eraser_dostroke().
 * It would be great to de-duplicate the logic here sometime, but that can wait.
 */
static bool gpencil_stroke_do_circle_sel(bGPdata *gpd,
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
                                         const float scale,
                                         const bool is_curve_edit)
{
  bGPDspoint *pt = nullptr;
  int x0 = 0, y0 = 0;
  int i;
  bool changed = false;
  bGPDstroke *gps_active = (gps->runtime.gps_orig) ? gps->runtime.gps_orig : gps;
  bGPDspoint *pt_active = nullptr;
  bool hit = false;

  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
    pt_active = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;

    bGPDspoint pt_temp;
    gpencil_point_to_world_space(pt, diff_mat, &pt_temp);
    gpencil_point_to_xy(gsc, gps, &pt_temp, &x0, &y0);

    /* do boundbox check first */
    if (!ELEM(V2D_IS_CLIPPED, x0, y0) && BLI_rcti_isect_pt(rect, x0, y0)) {
      /* only check if point is inside */
      if (((x0 - mx) * (x0 - mx) + (y0 - my) * (y0 - my)) <= radius * radius) {
        hit = true;

        /* change selection */
        if (select) {
          pt_active->flag |= GP_SPOINT_SELECT;
          gps_active->flag |= GP_STROKE_SELECT;
          BKE_gpencil_stroke_select_index_set(gpd, gps_active);
        }
        else {
          pt_active->flag &= ~GP_SPOINT_SELECT;
          gps_active->flag &= ~GP_STROKE_SELECT;
          BKE_gpencil_stroke_select_index_reset(gps_active);
        }
        changed = true;
        /* if stroke mode, don't check more points */
        if ((hit) && (selectmode == GP_SELECTMODE_STROKE)) {
          break;
        }

        /* Expand selection to segment. */
        if ((hit) && (selectmode == GP_SELECTMODE_SEGMENT) && (select) && (pt_active != nullptr)) {
          float r_hita[3], r_hitb[3];
          bool hit_select = bool(pt_active->flag & GP_SPOINT_SELECT);
          ED_gpencil_select_stroke_segment(
              gpd, gpl, gps_active, pt_active, hit_select, false, scale, r_hita, r_hitb);
        }
      }
    }
  }

  /* If stroke mode expand selection. */
  if ((hit) && (selectmode == GP_SELECTMODE_STROKE)) {
    for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
      pt_active = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;
      if (pt_active != nullptr) {
        if (select) {
          pt_active->flag |= GP_SPOINT_SELECT;
        }
        else {
          pt_active->flag &= ~GP_SPOINT_SELECT;
        }
      }
    }
  }

  /* If curve edit mode, generate the curve. */
  if (is_curve_edit && hit && gps_active->editcurve == nullptr) {
    BKE_gpencil_stroke_editcurve_update(gpd, gpl, gps_active);
    gps_active->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
    /* Select all curve points. */
    select_all_curve_points(gpd, gps_active, gps_active->editcurve, false);
    BKE_gpencil_stroke_geometry_update(gpd, gps_active);
    changed = true;
  }

  /* Ensure that stroke selection is in sync with its points. */
  BKE_gpencil_stroke_sync_selection(gpd, gps_active);

  return changed;
}

static bool gpencil_do_curve_circle_sel(bContext *C,
                                        bGPDstroke *gps,
                                        bGPDcurve *gpc,
                                        const int mx,
                                        const int my,
                                        const int radius,
                                        const bool select,
                                        rcti *rect,
                                        const float diff_mat[4][4],
                                        const int selectmode)
{
  ARegion *region = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);

  const bool only_selected = (v3d->overlay.handle_display == CURVE_HANDLE_SELECTED);

  bool hit = false;
  for (int i = 0; i < gpc->tot_curve_points; i++) {
    bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
    BezTriple *bezt = &gpc_pt->bezt;

    if (bezt->hide == 1) {
      continue;
    }

    const bool handles_visible = (v3d->overlay.handle_display != CURVE_HANDLE_NONE) &&
                                 (!only_selected || BEZT_ISSEL_ANY(bezt));

    /* If the handles are not visible only check control point (vec[1]). */
    int from = (!handles_visible) ? 1 : 0;
    int to = (!handles_visible) ? 2 : 3;

    for (int j = from; j < to; j++) {
      float parent_co[3];
      mul_v3_m4v3(parent_co, diff_mat, bezt->vec[j]);
      int screen_co[2];
      /* do 2d projection */
      if (ED_view3d_project_int_global(
              region, parent_co, screen_co, V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_WIN) !=
          V3D_PROJ_RET_OK)
      {
        continue;
      }

      /* view and bounding box test */
      if (ELEM(V2D_IS_CLIPPED, screen_co[0], screen_co[1]) &&
          !BLI_rcti_isect_pt(rect, screen_co[0], screen_co[1]))
      {
        continue;
      }

      /* test inside circle */
      int dist_x = screen_co[0] - mx;
      int dist_y = screen_co[1] - my;
      int dist = dist_x * dist_x + dist_y * dist_y;
      if (dist <= radius * radius) {
        hit = true;
        /* change selection */
        if (select) {
          gpc_pt->flag |= GP_CURVE_POINT_SELECT;
          BEZT_SEL_IDX(bezt, j);
        }
        else {
          BEZT_DESEL_IDX(bezt, j);
          if (!BEZT_ISSEL_ANY(bezt)) {
            gpc_pt->flag &= ~GP_CURVE_POINT_SELECT;
          }
        }
      }
    }
  }

  /* select the entire curve */
  if (hit && (selectmode == GP_SELECTMODE_STROKE)) {
    for (int i = 0; i < gpc->tot_curve_points; i++) {
      bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
      BezTriple *bezt = &gpc_pt->bezt;

      if (select) {
        gpc_pt->flag |= GP_CURVE_POINT_SELECT;
        BEZT_SEL_ALL(bezt);
      }
      else {
        gpc_pt->flag &= ~GP_CURVE_POINT_SELECT;
        BEZT_DESEL_ALL(bezt);
      }
    }
  }

  BKE_gpencil_curve_sync_selection(gpd, gps);

  return hit;
}

static int gpencil_circle_select_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  Object *ob = CTX_data_active_object(C);
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));

  int selectmode;
  if (ob && ob->mode == OB_MODE_SCULPT_GPENCIL_LEGACY) {
    selectmode = gpencil_select_mode_from_sculpt(
        eGP_Sculpt_SelectMaskFlag(ts->gpencil_selectmode_sculpt));
  }
  else if (ob && ob->mode == OB_MODE_VERTEX_GPENCIL_LEGACY) {
    selectmode = gpencil_select_mode_from_vertex(
        eGP_Sculpt_SelectMaskFlag(ts->gpencil_selectmode_vertex));
  }
  else {
    selectmode = ts->gpencil_selectmode_edit;
  }

  const float scale = ts->gp_sculpt.isect_threshold;

  /* If not edit/sculpt mode, the event has been caught but not processed. */
  if (GPENCIL_NONE_EDIT_MODE(gpd)) {
    return OPERATOR_CANCELLED;
  }

  ScrArea *area = CTX_wm_area(C);

  const int mx = RNA_int_get(op->ptr, "x");
  const int my = RNA_int_get(op->ptr, "y");
  const int radius = RNA_int_get(op->ptr, "radius");

  /* sanity checks */
  if (area == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "No active area");
    return OPERATOR_CANCELLED;
  }

  const eSelectOp sel_op = ED_select_op_modal(
      eSelectOp(RNA_enum_get(op->ptr, "mode")),
      WM_gesture_is_modal_first(static_cast<const wmGesture *>(op->customdata)));
  const bool select = (sel_op != SEL_OP_SUB);

  bool changed = false;
  /* For bounding `rect` around circle (for quickly intersection testing). */
  rcti rect = {0};
  rect.xmin = mx - radius;
  rect.ymin = my - radius;
  rect.xmax = mx + radius;
  rect.ymax = my + radius;

  if (is_curve_edit) {
    if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
      ED_gpencil_select_curve_toggle_all(C, SEL_DESELECT);
      changed = true;
    }

    GP_EDITABLE_CURVES_BEGIN(gps_iter, C, gpl, gps, gpc)
    {
      changed |= gpencil_do_curve_circle_sel(
          C, gps, gpc, mx, my, radius, select, &rect, gps_iter.diff_mat, selectmode);
    }
    GP_EDITABLE_CURVES_END(gps_iter);
  }

  if (changed == false) {
    GP_SpaceConversion gsc = {nullptr};
    /* init space conversion stuff */
    gpencil_point_conversion_init(C, &gsc);

    if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
      ED_gpencil_select_toggle_all(C, SEL_DESELECT);
      changed = true;
    }

    /* find visible strokes, and select if hit */
    GP_EVALUATED_STROKES_BEGIN (gpstroke_iter, C, gpl, gps) {
      changed |= gpencil_stroke_do_circle_sel(gpd,
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
                                              scale,
                                              is_curve_edit);
    }
    GP_EVALUATED_STROKES_END(gpstroke_iter);
  }

  /* updates */
  if (changed) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

    /* Copy-on-eval tag is needed, or else no refresh happens */
    DEG_id_tag_update(&gpd->id, ID_RECALC_SYNC_TO_EVAL);

    WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, nullptr);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);
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

struct GP_SelectUserData {
  int mx, my, radius;
  /* Bounding box rect */
  rcti rect;
  const int (*lasso_coords)[2];
  int lasso_coords_len;
};

typedef bool (*GPencilTestFn)(ARegion *region,
                              const float diff_mat[4][4],
                              const float pt[3],
                              GP_SelectUserData *user_data);

#if 0
static bool gpencil_stroke_fill_isect_rect(ARegion *region,
                                           bGPDstroke *gps,
                                           const float diff_mat[4][4],
                                           rcti rect)
{
  int min[2] = {-INT_MAX, -INT_MAX};
  int max[2] = {INT_MAX, INT_MAX};

  int(*points2d)[2] = MEM_callocN(sizeof(int[2]) * gps->totpoints, __func__);

  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    int *pt2d = points2d[i];

    int screen_co[2];
    gpencil_3d_point_to_screen_space(region, diff_mat, &pt->x, screen_co);
    DO_MINMAX2(screen_co, min, max);

    copy_v2_v2_int(pt2d, screen_co);
  }

  bool hit = false;
  /* check bounding box */
  rcti bb = {min[0], max[0], min[1], max[1]};
  if (BLI_rcti_isect(&rect, &bb, nullptr)) {
    for (int i = 0; i < gps->tot_triangles; i++) {
      bGPDtriangle *tri = &gps->triangles[i];
      int pt1[2], pt2[2], pt3[2];
      int tri_min[2] = {-INT_MAX, -INT_MAX};
      int tri_max[2] = {INT_MAX, INT_MAX};

      copy_v2_v2_int(pt1, points2d[tri->verts[0]]);
      copy_v2_v2_int(pt2, points2d[tri->verts[1]]);
      copy_v2_v2_int(pt3, points2d[tri->verts[2]]);

      DO_MINMAX2(pt1, tri_min, tri_max);
      DO_MINMAX2(pt2, tri_min, tri_max);
      DO_MINMAX2(pt3, tri_min, tri_max);

      rcti tri_bb = {tri_min[0], tri_max[0], tri_min[1], tri_max[1]};
      /* Case 1: triangle is entirely inside box selection */
      /* (XXX: Can this even happen with no point inside the box?) */
      if (BLI_rcti_inside_rcti(&tri_bb, &rect)) {
        hit = true;
        break;
      }

      /* Case 2: rectangle intersects sides of triangle */
      if (BLI_rcti_isect_segment(&rect, pt1, pt2) || BLI_rcti_isect_segment(&rect, pt2, pt3) ||
          BLI_rcti_isect_segment(&rect, pt3, pt1))
      {
        hit = true;
        break;
      }

      /* TODO: Case 3: rectangle is inside the triangle */
    }
  }

  MEM_freeN(points2d);
  return hit;
}
#endif

static bool gpencil_generic_curve_select(bContext *C,
                                         Object *ob,
                                         GPencilTestFn is_inside_fn,
                                         rcti /*box*/,
                                         GP_SelectUserData *user_data,
                                         const bool strokemode,
                                         const eSelectOp sel_op)
{
  ARegion *region = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);
  const bool handle_only_selected = (v3d->overlay.handle_display == CURVE_HANDLE_SELECTED);
  const bool handle_all = (v3d->overlay.handle_display == CURVE_HANDLE_ALL);

  bool hit = false;
  bool changed = false;
  bool whole = false;

  GP_EDITABLE_CURVES_BEGIN(gps_iter, C, gpl, gps, gpc)
  {
    bool any_select = false;
    for (int i = 0; i < gpc->tot_curve_points; i++) {
      bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
      BezTriple *bezt = &gpc_pt->bezt;

      if (bezt->hide == 1) {
        continue;
      }

      const bool handles_visible = (handle_all || (handle_only_selected &&
                                                   (gpc_pt->flag & GP_CURVE_POINT_SELECT)));

      if (handles_visible) {
        for (int j = 0; j < 3; j++) {
          const bool is_select = BEZT_ISSEL_IDX(bezt, j);
          bool is_inside = is_inside_fn(region, gps_iter.diff_mat, bezt->vec[j], user_data);
          if (strokemode) {
            if (is_inside) {
              hit = true;
              any_select = true;
              break;
            }
          }
          else {
            const int sel_op_result = ED_select_op_action_deselected(sel_op, is_select, is_inside);
            if (sel_op_result != -1) {
              if (sel_op_result) {
                gpc_pt->flag |= GP_CURVE_POINT_SELECT;
                BEZT_SEL_IDX(bezt, j);
                any_select = true;
              }
              else {
                gpc_pt->flag &= ~GP_CURVE_POINT_SELECT;
                BEZT_DESEL_IDX(bezt, j);
              }
              changed = true;
              hit = true;
            }
            else {
              if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
                gpc_pt->flag &= ~GP_CURVE_POINT_SELECT;
                BEZT_DESEL_IDX(bezt, j);
              }
            }
          }
        }
      }
      /* If the handles are not visible only check ctrl point (vec[1]). */
      else {
        const bool is_select = bezt->f2;
        bool is_inside = is_inside_fn(region, gps_iter.diff_mat, bezt->vec[1], user_data);
        if (strokemode) {
          if (is_inside) {
            hit = true;
            any_select = true;
          }
        }
        else {
          const int sel_op_result = ED_select_op_action_deselected(sel_op, is_select, is_inside);
          if (sel_op_result != -1) {
            if (sel_op_result) {
              gpc_pt->flag |= GP_CURVE_POINT_SELECT;
              bezt->f2 |= SELECT;
              any_select = true;
            }
            else {
              gpc_pt->flag &= ~GP_CURVE_POINT_SELECT;
              bezt->f2 &= ~SELECT;
            }
            changed = true;
            hit = true;
          }
          else {
            if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
              gpc_pt->flag &= ~GP_CURVE_POINT_SELECT;
              bezt->f2 &= ~SELECT;
            }
          }
        }
      }
    }

/* TODO: Fix selection for filled in curves. */
#if 0
    if (!hit) {
      /* check if we selected the inside of a filled curve */
      MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);
      if ((gp_style->flag & GP_MATERIAL_FILL_SHOW) == 0) {
        continue;
      }

      whole = gpencil_stroke_fill_isect_rect(region, gps, gps_iter.diff_mat, box);
    }
#endif
    /* select the entire curve */
    if (strokemode || whole) {
      const int sel_op_result = ED_select_op_action_deselected(sel_op, any_select, hit || whole);
      if (sel_op_result != -1) {
        for (int i = 0; i < gpc->tot_curve_points; i++) {
          bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
          BezTriple *bezt = &gpc_pt->bezt;

          if (sel_op_result) {
            gpc_pt->flag |= GP_CURVE_POINT_SELECT;
            BEZT_SEL_ALL(bezt);
          }
          else {
            gpc_pt->flag &= ~GP_CURVE_POINT_SELECT;
            BEZT_DESEL_ALL(bezt);
          }
        }

        if (sel_op_result) {
          gpc->flag |= GP_CURVE_SELECT;
        }
        else {
          gpc->flag &= ~GP_CURVE_SELECT;
        }
        changed = true;
      }
    }

    BKE_gpencil_curve_sync_selection(gpd, gps);
  }
  GP_EDITABLE_CURVES_END(gps_iter);

  return changed;
}

static bool gpencil_generic_stroke_select(bContext *C,
                                          Object *ob,
                                          bGPdata *gpd,
                                          GPencilTestFn is_inside_fn,
                                          rcti box,
                                          GP_SelectUserData *user_data,
                                          const bool strokemode,
                                          const bool segmentmode,
                                          const eSelectOp sel_op,
                                          const float scale,
                                          const bool is_curve_edit)
{
  GP_SpaceConversion gsc = {nullptr};
  bool changed = false;
  /* init space conversion stuff */
  gpencil_point_conversion_init(C, &gsc);

  /* Use only object transform matrix because all layer transformations are already included
   * in the evaluated stroke. */
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob_eval = depsgraph != nullptr ? DEG_get_evaluated_object(depsgraph, ob) : ob;
  float select_mat[4][4];
  copy_m4_m4(select_mat, ob_eval->object_to_world().ptr());

  /* deselect all strokes first? */
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    deselect_all_selected(C);
    changed = true;
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

      /* Convert point coords to screen-space. Needs to use the evaluated point
       * to consider modifiers. */
      const bool is_inside = is_inside_fn(gsc.region, select_mat, &pt->x, user_data);
      if (strokemode == false) {
        const bool is_select = (pt_active->flag & GP_SPOINT_SELECT) != 0;
        const int sel_op_result = ED_select_op_action_deselected(sel_op, is_select, is_inside);
        if (sel_op_result != -1) {
          SET_FLAG_FROM_TEST(pt_active->flag, sel_op_result, GP_SPOINT_SELECT);
          changed = true;
          hit = true;

          /* Expand selection to segment. */
          if (segmentmode) {
            bool hit_select = bool(pt_active->flag & GP_SPOINT_SELECT);
            float r_hita[3], r_hitb[3];
            ED_gpencil_select_stroke_segment(
                gpd, gpl, gps_active, pt_active, hit_select, false, scale, r_hita, r_hitb);
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

      whole = ED_gpencil_stroke_point_is_inside(gps, &gsc, mval, gpstroke_iter.diff_mat);
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

    /* If curve edit mode, generate the curve. */
    if (is_curve_edit && (hit || whole) && gps_active->editcurve == nullptr) {
      BKE_gpencil_stroke_editcurve_update(gpd, gpl, gps_active);
      gps_active->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
      /* Select all curve points. */
      select_all_curve_points(gpd, gps_active, gps_active->editcurve, false);
      BKE_gpencil_stroke_geometry_update(gpd, gps_active);
      changed = true;
    }

    /* Ensure that stroke selection is in sync with its points */
    BKE_gpencil_stroke_sync_selection(gpd, gps_active);
  }
  GP_EVALUATED_STROKES_END(gpstroke_iter);

  return changed;
}

static int gpencil_generic_select_exec(bContext *C,
                                       wmOperator *op,
                                       GPencilTestFn is_inside_fn,
                                       rcti box,
                                       GP_SelectUserData *user_data)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  ScrArea *area = CTX_wm_area(C);
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));

  int selectmode;
  if (ob && ob->mode == OB_MODE_SCULPT_GPENCIL_LEGACY) {
    selectmode = gpencil_select_mode_from_sculpt(
        eGP_Sculpt_SelectMaskFlag(ts->gpencil_selectmode_sculpt));
  }
  else if (ob && ob->mode == OB_MODE_VERTEX_GPENCIL_LEGACY) {
    selectmode = gpencil_select_mode_from_vertex(
        eGP_Sculpt_SelectMaskFlag(ts->gpencil_selectmode_vertex));
  }
  else {
    selectmode = ts->gpencil_selectmode_edit;
  }

  const bool strokemode = ((selectmode == GP_SELECTMODE_STROKE) &&
                           ((gpd->flag & GP_DATA_STROKE_PAINTMODE) == 0));
  const bool segmentmode = ((selectmode == GP_SELECTMODE_SEGMENT) &&
                            ((gpd->flag & GP_DATA_STROKE_PAINTMODE) == 0));

  const eSelectOp sel_op = eSelectOp(RNA_enum_get(op->ptr, "mode"));
  const float scale = ts->gp_sculpt.isect_threshold;

  bool changed = false;

  /* sanity checks */
  if (area == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "No active area");
    return OPERATOR_CANCELLED;
  }

  if (is_curve_edit) {
    changed = gpencil_generic_curve_select(
        C, ob, is_inside_fn, box, user_data, strokemode, sel_op);
  }

  if (changed == false) {
    changed = gpencil_generic_stroke_select(C,
                                            ob,
                                            gpd,
                                            is_inside_fn,
                                            box,
                                            user_data,
                                            strokemode,
                                            segmentmode,
                                            sel_op,
                                            scale,
                                            is_curve_edit);
  }

  /* if paint mode,delete selected points */
  if (GPENCIL_PAINT_MODE(gpd)) {
    gpencil_delete_selected_point_wrap(C);
    changed = true;
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  }

  /* updates */
  if (changed) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

    /* Copy-on-eval tag is needed, or else no refresh happens */
    DEG_id_tag_update(&gpd->id, ID_RECALC_SYNC_TO_EVAL);

    WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, nullptr);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);
  }
  return OPERATOR_FINISHED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Box Select Operator
 * \{ */

static bool gpencil_test_box(ARegion *region,
                             const float diff_mat[4][4],
                             const float pt[3],
                             GP_SelectUserData *user_data)
{
  int co[2] = {0};
  if (gpencil_3d_point_to_screen_space(region, diff_mat, pt, co)) {
    return BLI_rcti_isect_pt(&user_data->rect, co[0], co[1]);
  }
  return false;
}

static int gpencil_box_select_exec(bContext *C, wmOperator *op)
{
  GP_SelectUserData data = {0};
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

static bool gpencil_test_lasso(ARegion *region,
                               const float diff_mat[4][4],
                               const float pt[3],
                               GP_SelectUserData *user_data)
{
  int co[2] = {0};
  if (gpencil_3d_point_to_screen_space(region, diff_mat, pt, co)) {
    /* test if in lasso boundbox + within the lasso noose */
    return (BLI_rcti_isect_pt(&user_data->rect, co[0], co[1]) &&
            BLI_lasso_is_point_inside(
                user_data->lasso_coords, user_data->lasso_coords_len, co[0], co[1], INT_MAX));
  }
  return false;
}

static int gpencil_lasso_select_exec(bContext *C, wmOperator *op)
{
  GP_SelectUserData data = {0};
  data.lasso_coords = WM_gesture_lasso_path_to_array(C, op, &data.lasso_coords_len);

  /* Sanity check. */
  if (data.lasso_coords == nullptr) {
    return OPERATOR_PASS_THROUGH;
  }

  /* Compute boundbox of lasso (for faster testing later). */
  BLI_lasso_boundbox(&data.rect, data.lasso_coords, data.lasso_coords_len);

  rcti rect = data.rect;
  int ret = gpencil_generic_select_exec(C, op, gpencil_test_lasso, rect, &data);

  MEM_freeN((void *)data.lasso_coords);

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
  ot->flag = OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

  /* properties */
  WM_operator_properties_select_operation(ot);
  WM_operator_properties_gesture_lasso(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mouse Pick Select Operator
 * \{ */

static void gpencil_select_curve_point(bContext *C,
                                       const int mval[2],
                                       const int radius_squared,
                                       bGPDlayer **r_gpl,
                                       bGPDstroke **r_gps,
                                       bGPDcurve **r_gpc,
                                       bGPDcurve_point **r_pt,
                                       char *handle)
{
  ARegion *region = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);
  const bool only_selected = (v3d->overlay.handle_display == CURVE_HANDLE_SELECTED);

  int hit_distance = radius_squared;

  GP_EDITABLE_CURVES_BEGIN(gps_iter, C, gpl, gps, gpc)
  {
    for (int i = 0; i < gpc->tot_curve_points; i++) {
      bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
      BezTriple *bezt = &gpc_pt->bezt;

      if (bezt->hide == 1) {
        continue;
      }

      const bool handles_visible = (v3d->overlay.handle_display != CURVE_HANDLE_NONE) &&
                                   (!only_selected || BEZT_ISSEL_ANY(bezt));

      /* If the handles are not visible only check control point (vec[1]). */
      int from = (!handles_visible) ? 1 : 0;
      int to = (!handles_visible) ? 2 : 3;

      for (int j = from; j < to; j++) {
        int screen_co[2];
        if (gpencil_3d_point_to_screen_space(region, gps_iter.diff_mat, bezt->vec[j], screen_co)) {
          const int pt_distance = len_manhattan_v2v2_int(mval, screen_co);

          if (pt_distance <= radius_squared && pt_distance < hit_distance) {
            *r_gpl = gpl;
            *r_gps = gps;
            *r_gpc = gpc;
            *r_pt = gpc_pt;
            *handle = j;
            hit_distance = pt_distance;
          }
        }
      }
    }
  }
  GP_EDITABLE_CURVES_END(gps_iter);
}

static int gpencil_select_exec(bContext *C, wmOperator *op)
{
  ScrArea *area = CTX_wm_area(C);
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  const float scale = ts->gp_sculpt.isect_threshold;
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));

  /* "radius" is simply a threshold (screen space) to make it easier to test with a tolerance */
  const float radius = 0.4f * U.widget_unit;
  const int radius_squared = int(radius * radius);

  const bool use_shift_extend = RNA_boolean_get(op->ptr, "use_shift_extend");
  bool extend = RNA_boolean_get(op->ptr, "extend") || use_shift_extend;
  bool deselect = RNA_boolean_get(op->ptr, "deselect");
  bool toggle = RNA_boolean_get(op->ptr, "toggle");
  bool whole = RNA_boolean_get(op->ptr, "entire_strokes");
  const bool deselect_all = RNA_boolean_get(op->ptr, "deselect_all") && !use_shift_extend;

  int mval[2] = {0};
  /* get mouse location */
  RNA_int_get_array(op->ptr, "location", mval);

  GP_SpaceConversion gsc = {nullptr};

  bGPDlayer *hit_layer = nullptr;
  bGPDstroke *hit_stroke = nullptr;
  bGPDspoint *hit_point = nullptr;
  bGPDcurve *hit_curve = nullptr;
  bGPDcurve_point *hit_curve_point = nullptr;
  char hit_curve_handle = 0;
  int hit_distance = radius_squared;

  /* sanity checks */
  if (area == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "No active area");
    return OPERATOR_CANCELLED;
  }

  /* if select mode is stroke, use whole stroke */
  if ((ob) && (ob->mode == OB_MODE_SCULPT_GPENCIL_LEGACY)) {
    whole |= bool(gpencil_select_mode_from_sculpt(eGP_Sculpt_SelectMaskFlag(
                      ts->gpencil_selectmode_sculpt)) == GP_SELECTMODE_STROKE);
  }
  else if ((ob) && (ob->mode == OB_MODE_VERTEX_GPENCIL_LEGACY)) {
    whole |= bool(gpencil_select_mode_from_vertex(eGP_Sculpt_SelectMaskFlag(
                      ts->gpencil_selectmode_sculpt)) == GP_SELECTMODE_STROKE);
  }
  else {
    whole |= bool(ts->gpencil_selectmode_edit == GP_SELECTMODE_STROKE);
  }

  if (is_curve_edit) {
    gpencil_select_curve_point(C,
                               mval,
                               radius_squared,
                               &hit_layer,
                               &hit_stroke,
                               &hit_curve,
                               &hit_curve_point,
                               &hit_curve_handle);
  }

  if (hit_curve == nullptr) {
    /* init space conversion stuff */
    gpencil_point_conversion_init(C, &gsc);

    /* First Pass: Find stroke point which gets hit */
    GP_EVALUATED_STROKES_BEGIN (gpstroke_iter, C, gpl, gps) {
      bGPDstroke *gps_active = (gps->runtime.gps_orig) ? gps->runtime.gps_orig : gps;
      bGPDspoint *pt;
      int i;

      /* firstly, check for hit-point */
      for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
        int xy[2];

        bGPDspoint pt2;
        gpencil_point_to_world_space(pt, gpstroke_iter.diff_mat, &pt2);
        gpencil_point_to_xy(&gsc, gps_active, &pt2, &xy[0], &xy[1]);

        /* do boundbox check first */
        if (!ELEM(V2D_IS_CLIPPED, xy[0], xy[1])) {
          const int pt_distance = len_manhattan_v2v2_int(mval, xy);

          /* check if point is inside */
          if (pt_distance <= radius_squared) {
            /* only use this point if it is a better match than the current hit - #44685 */
            if (pt_distance < hit_distance) {
              hit_layer = gpl;
              hit_stroke = gps_active;
              hit_point = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;
              hit_distance = pt_distance;
            }
          }
        }
      }
    }
    GP_EVALUATED_STROKES_END(gpstroke_iter);
  }

  /* Abort if nothing hit... */
  if (!hit_curve && !hit_curve_point && !hit_point && !hit_stroke) {

    if (deselect_all) {
      /* since left mouse select change, deselect all if click outside any hit */
      deselect_all_selected(C);

      /* Copy-on-eval tag is needed, or else no refresh happens */
      DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);
      DEG_id_tag_update(&gpd->id, ID_RECALC_SYNC_TO_EVAL);
      WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, nullptr);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);

      return OPERATOR_FINISHED;
    }

    return OPERATOR_CANCELLED;
  }

  /* select all handles if the click was on the curve but not on a handle */
  if (is_curve_edit && hit_point != nullptr) {
    whole = true;
    hit_curve = hit_stroke->editcurve;
  }

  /* adjust selection behavior - for toggle option */
  if (toggle) {
    if (hit_curve_point != nullptr) {
      BezTriple *bezt = &hit_curve_point->bezt;
      if ((bezt->f1 & SELECT) && (hit_curve_handle == 0)) {
        deselect = true;
      }
      if ((bezt->f2 & SELECT) && (hit_curve_handle == 1)) {
        deselect = true;
      }
      if ((bezt->f3 & SELECT) && (hit_curve_handle == 2)) {
        deselect = true;
      }
    }
    else {
      deselect = (hit_point->flag & GP_SPOINT_SELECT) != 0;
    }
  }

  /* If not extending selection, deselect everything else */
  if (extend == false) {
    deselect_all_selected(C);
  }

  /* Perform selection operations... */
  if (whole) {
    /* Generate editcurve if it does not exist */
    if (is_curve_edit && hit_curve == nullptr) {
      BKE_gpencil_stroke_editcurve_update(gpd, hit_layer, hit_stroke);
      hit_stroke->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
      BKE_gpencil_stroke_geometry_update(gpd, hit_stroke);
      hit_curve = hit_stroke->editcurve;
    }
    /* select all curve points */
    if (hit_curve != nullptr) {
      select_all_curve_points(gpd, hit_stroke, hit_curve, deselect);
    }
    else {
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
        BKE_gpencil_stroke_select_index_set(gpd, hit_stroke);
      }
      else {
        hit_stroke->flag &= ~GP_STROKE_SELECT;
        BKE_gpencil_stroke_select_index_reset(hit_stroke);
      }
    }
  }
  else {
    /* just the point (and the stroke) */
    if (deselect == false) {
      if (hit_curve_point != nullptr) {
        hit_curve_point->flag |= GP_CURVE_POINT_SELECT;
        BEZT_SEL_IDX(&hit_curve_point->bezt, hit_curve_handle);
        hit_curve->flag |= GP_CURVE_SELECT;
        hit_stroke->flag |= GP_STROKE_SELECT;
        BKE_gpencil_stroke_select_index_set(gpd, hit_stroke);
      }
      else {
        /* we're adding selection, so selection must be true */
        hit_point->flag |= GP_SPOINT_SELECT;
        hit_stroke->flag |= GP_STROKE_SELECT;
        BKE_gpencil_stroke_select_index_set(gpd, hit_stroke);

        /* expand selection to segment */
        int selectmode;
        if (ob && ob->mode == OB_MODE_SCULPT_GPENCIL_LEGACY) {
          selectmode = gpencil_select_mode_from_sculpt(
              eGP_Sculpt_SelectMaskFlag(ts->gpencil_selectmode_sculpt));
        }
        else if (ob && ob->mode == OB_MODE_VERTEX_GPENCIL_LEGACY) {
          selectmode = gpencil_select_mode_from_vertex(
              eGP_Sculpt_SelectMaskFlag(ts->gpencil_selectmode_vertex));
        }
        else {
          selectmode = ts->gpencil_selectmode_edit;
        }

        if (selectmode == GP_SELECTMODE_SEGMENT) {
          float r_hita[3], r_hitb[3];
          bool hit_select = bool(hit_point->flag & GP_SPOINT_SELECT);
          ED_gpencil_select_stroke_segment(
              gpd, hit_layer, hit_stroke, hit_point, hit_select, false, scale, r_hita, r_hitb);
        }
      }
    }
    else {
      if (hit_curve_point != nullptr) {
        BEZT_DESEL_IDX(&hit_curve_point->bezt, hit_curve_handle);
        if (!BEZT_ISSEL_ANY(&hit_curve_point->bezt)) {
          hit_curve_point->flag &= ~GP_CURVE_POINT_SELECT;
        }
        BKE_gpencil_curve_sync_selection(gpd, hit_stroke);
      }
      else {
        /* deselect point */
        hit_point->flag &= ~GP_SPOINT_SELECT;

        /* ensure that stroke is selected correctly */
        BKE_gpencil_stroke_sync_selection(gpd, hit_stroke);
      }
    }
  }

  /* updates */
  if (hit_curve_point != nullptr || hit_point != nullptr) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

    /* Copy-on-eval tag is needed, or else no refresh happens */
    DEG_id_tag_update(&gpd->id, ID_RECALC_SYNC_TO_EVAL);

    WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, nullptr);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);
  }

  return OPERATOR_PASS_THROUGH | OPERATOR_FINISHED;
}

static int gpencil_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  RNA_int_set_array(op->ptr, "location", event->mval);

  if (!RNA_struct_property_is_set(op->ptr, "use_shift_extend")) {
    RNA_boolean_set(op->ptr, "use_shift_extend", event->modifier & KM_SHIFT);
  }

  const int retval = gpencil_select_exec(C, op);

  return WM_operator_flag_only_pass_through_on_press(retval, event);
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
  ot->get_name = ED_select_pick_get_name;

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
                            nullptr,
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
        if (ED_gpencil_stroke_material_editable(ob, gpl, gps) == false) {
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
  if ((ob == nullptr) || (ob->type != OB_GPENCIL_LEGACY)) {
    return false;
  }
  bGPdata *gpd = (bGPdata *)ob->data;

  if (GPENCIL_VERTEX_MODE(gpd)) {
    if (!GPENCIL_ANY_VERTEX_MASK(ts->gpencil_selectmode_vertex)) {
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
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  const float threshold = RNA_int_get(op->ptr, "threshold");
  const int selectmode = gpencil_select_mode_from_vertex(
      eGP_Sculpt_SelectMaskFlag(ts->gpencil_selectmode_vertex));
  const float range = pow(10, 5 - threshold);

  bool changed = false;

  /* Create a hash table with all selected colors. */
  GHash *hue_table = BLI_ghash_int_new(__func__);
  gpencil_selected_hue_table(C, ob, threshold, hue_table);
  if (BLI_ghash_len(hue_table) == 0) {
    BKE_report(op->reports, RPT_ERROR, "Select before some Vertex to use as a filter color");
    BLI_ghash_free(hue_table, nullptr, nullptr);

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
      BKE_gpencil_stroke_select_index_set(gpd, gps);

      /* Extend stroke selection. */
      if (selectmode == GP_SELECTMODE_STROKE) {
        bGPDspoint *pt1 = nullptr;

        for (i = 0, pt1 = gps->points; i < gps->totpoints; i++, pt1++) {
          pt1->flag |= GP_SPOINT_SELECT;
        }
      }
    }
  }
  CTX_DATA_END;

  if (changed) {
    /* updates */
    DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);

    /* Copy-on-eval tag is needed, or else no refresh happens */
    DEG_id_tag_update(&gpd->id, ID_RECALC_SYNC_TO_EVAL);

    WM_event_add_notifier(C, NC_GPENCIL | NA_SELECTED, nullptr);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, nullptr);
  }

  /* Free memory. */
  if (hue_table != nullptr) {
    BLI_ghash_free(hue_table, nullptr, nullptr);
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
