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
 * The Original Code is Copyright (C) 2016, Blender Foundation
 * This is a new part of Blender
 * Operators for interpolating new Grease Pencil frames from existing strokes
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
#include "BLI_utildefines.h"
#include "BLI_easing.h"
#include "BLI_math.h"

#include "BLT_translation.h"

#include "DNA_color_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil.h"
#include "BKE_report.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_view2d.h"

#include "ED_gpencil.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_space_api.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "gpencil_intern.h"

/* ************************************************ */
/* Core/Shared Utilities */

/* Poll callback for interpolation operators */
static bool gpencil_view3d_poll(bContext *C)
{
  bGPdata *gpd = CTX_data_gpencil_data(C);
  bGPDlayer *gpl = CTX_data_active_gpencil_layer(C);

  /* only 3D view */
  ScrArea *sa = CTX_wm_area(C);
  if (sa && sa->spacetype != SPACE_VIEW3D) {
    return 0;
  }

  /* need data to interpolate */
  if (ELEM(NULL, gpd, gpl)) {
    return 0;
  }

  return 1;
}

/* Perform interpolation */
static void gp_interpolate_update_points(const bGPDstroke *gps_from,
                                         const bGPDstroke *gps_to,
                                         bGPDstroke *new_stroke,
                                         float factor)
{
  /* update points */
  for (int i = 0; i < new_stroke->totpoints; i++) {
    const bGPDspoint *prev = &gps_from->points[i];
    const bGPDspoint *next = &gps_to->points[i];
    bGPDspoint *pt = &new_stroke->points[i];

    /* Interpolate all values */
    interp_v3_v3v3(&pt->x, &prev->x, &next->x, factor);
    pt->pressure = interpf(prev->pressure, next->pressure, 1.0f - factor);
    pt->strength = interpf(prev->strength, next->strength, 1.0f - factor);
    CLAMP(pt->strength, GPENCIL_STRENGTH_MIN, 1.0f);

    /* GPXX interpolate dverts */
#if 0
    MDeformVert *dvert = &new_stroke->dvert[i];
    dvert->totweight = 0;
    dvert->dw = NULL;
#endif
  }
}

/* ****************** Interpolate Interactive *********************** */

/* Helper: Update all strokes interpolated */
static void gp_interpolate_update_strokes(bContext *C, tGPDinterpolate *tgpi)
{
  bGPdata *gpd = tgpi->gpd;
  tGPDinterpolate_layer *tgpil;
  const float shift = tgpi->shift;

  for (tgpil = tgpi->ilayers.first; tgpil; tgpil = tgpil->next) {
    bGPDstroke *new_stroke;
    const float factor = tgpil->factor + shift;

    for (new_stroke = tgpil->interFrame->strokes.first; new_stroke;
         new_stroke = new_stroke->next) {
      bGPDstroke *gps_from, *gps_to;
      int stroke_idx;

      if (new_stroke->totpoints == 0) {
        continue;
      }

      /* get strokes to interpolate */
      stroke_idx = BLI_findindex(&tgpil->interFrame->strokes, new_stroke);

      gps_from = BLI_findlink(&tgpil->prevFrame->strokes, stroke_idx);
      gps_to = BLI_findlink(&tgpil->nextFrame->strokes, stroke_idx);

      /* update points position */
      if ((gps_from) && (gps_to)) {
        gp_interpolate_update_points(gps_from, gps_to, new_stroke, factor);
      }
    }
  }

  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);
}

/* Helper: Verify valid strokes for interpolation */
static bool gp_interpolate_check_todo(bContext *C, bGPdata *gpd)
{
  Object *ob = CTX_data_active_object(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  eGP_Interpolate_SettingsFlag flag = ts->gp_interpolate.flag;

  /* get layers */
  for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
    /* all layers or only active */
    if (!(flag & GP_TOOLFLAG_INTERPOLATE_ALL_LAYERS) && !(gpl->flag & GP_LAYER_ACTIVE)) {
      continue;
    }
    /* only editable and visible layers are considered */
    if (!gpencil_layer_is_editable(gpl) || (gpl->actframe == NULL)) {
      continue;
    }

    /* read strokes */
    for (bGPDstroke *gps_from = gpl->actframe->strokes.first; gps_from;
         gps_from = gps_from->next) {
      bGPDstroke *gps_to;
      int fFrame;

      /* only selected */
      if ((flag & GP_TOOLFLAG_INTERPOLATE_ONLY_SELECTED) &&
          ((gps_from->flag & GP_STROKE_SELECT) == 0)) {
        continue;
      }
      /* skip strokes that are invalid for current view */
      if (ED_gpencil_stroke_can_use(C, gps_from) == false) {
        continue;
      }
      /* check if the color is editable */
      if (ED_gpencil_stroke_color_use(ob, gpl, gps_from) == false) {
        continue;
      }

      /* get final stroke to interpolate */
      fFrame = BLI_findindex(&gpl->actframe->strokes, gps_from);
      gps_to = BLI_findlink(&gpl->actframe->next->strokes, fFrame);
      if (gps_to == NULL) {
        continue;
      }

      return true;
    }
  }
  return false;
}

/* Helper: Create internal strokes interpolated */
static void gp_interpolate_set_points(bContext *C, tGPDinterpolate *tgpi)
{
  bGPdata *gpd = tgpi->gpd;
  bGPDlayer *active_gpl = CTX_data_active_gpencil_layer(C);
  bGPDframe *actframe = active_gpl->actframe;
  Object *ob = CTX_data_active_object(C);

  /* save initial factor for active layer to define shift limits */
  tgpi->init_factor = (float)(tgpi->cframe - actframe->framenum) /
                      (actframe->next->framenum - actframe->framenum + 1);

  /* limits are 100% below 0 and 100% over the 100% */
  tgpi->low_limit = -1.0f - tgpi->init_factor;
  tgpi->high_limit = 2.0f - tgpi->init_factor;

  /* set layers */
  for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
    tGPDinterpolate_layer *tgpil;

    /* all layers or only active */
    if (!(tgpi->flag & GP_TOOLFLAG_INTERPOLATE_ALL_LAYERS) && (gpl != active_gpl)) {
      continue;
    }
    /* only editable and visible layers are considered */
    if (!gpencil_layer_is_editable(gpl) || (gpl->actframe == NULL)) {
      continue;
    }

    /* create temp data for each layer */
    tgpil = MEM_callocN(sizeof(tGPDinterpolate_layer), "GPencil Interpolate Layer");

    tgpil->gpl = gpl;
    tgpil->prevFrame = gpl->actframe;
    tgpil->nextFrame = gpl->actframe->next;

    BLI_addtail(&tgpi->ilayers, tgpil);

    /* create a new temporary frame */
    tgpil->interFrame = MEM_callocN(sizeof(bGPDframe), "bGPDframe");
    tgpil->interFrame->framenum = tgpi->cframe;

    /* get interpolation factor by layer (usually must be equal for all layers, but not sure) */
    tgpil->factor = (float)(tgpi->cframe - tgpil->prevFrame->framenum) /
                    (tgpil->nextFrame->framenum - tgpil->prevFrame->framenum + 1);

    /* create new strokes data with interpolated points reading original stroke */
    for (bGPDstroke *gps_from = tgpil->prevFrame->strokes.first; gps_from;
         gps_from = gps_from->next) {
      bGPDstroke *gps_to;
      int fFrame;

      bGPDstroke *new_stroke = NULL;
      bool valid = true;

      /* only selected */
      if ((tgpi->flag & GP_TOOLFLAG_INTERPOLATE_ONLY_SELECTED) &&
          ((gps_from->flag & GP_STROKE_SELECT) == 0)) {
        valid = false;
      }
      /* skip strokes that are invalid for current view */
      if (ED_gpencil_stroke_can_use(C, gps_from) == false) {
        valid = false;
      }

      /* check if the color is editable */
      if (ED_gpencil_stroke_color_use(ob, tgpil->gpl, gps_from) == false) {
        valid = false;
      }

      /* get final stroke to interpolate */
      fFrame = BLI_findindex(&tgpil->prevFrame->strokes, gps_from);
      gps_to = BLI_findlink(&tgpil->nextFrame->strokes, fFrame);
      if (gps_to == NULL) {
        valid = false;
      }

      /* create new stroke */
      new_stroke = BKE_gpencil_stroke_duplicate(gps_from);

      if (valid) {
        /* if destination stroke is smaller, resize new_stroke to size of gps_to stroke */
        if (gps_from->totpoints > gps_to->totpoints) {
          new_stroke->points = MEM_recallocN(new_stroke->points,
                                             sizeof(*new_stroke->points) * gps_to->totpoints);
          if (new_stroke->dvert != NULL) {
            new_stroke->dvert = MEM_recallocN(new_stroke->dvert,
                                              sizeof(*new_stroke->dvert) * gps_to->totpoints);
          }
          new_stroke->totpoints = gps_to->totpoints;
          new_stroke->tot_triangles = 0;
          new_stroke->flag |= GP_STROKE_RECALC_GEOMETRY;
        }
        /* update points position */
        gp_interpolate_update_points(gps_from, gps_to, new_stroke, tgpil->factor);
      }
      else {
        /* need an empty stroke to keep index correct for lookup, but resize to smallest size */
        new_stroke->totpoints = 0;
        new_stroke->points = MEM_recallocN(new_stroke->points, sizeof(*new_stroke->points));
        if (new_stroke->dvert != NULL) {
          new_stroke->dvert = MEM_recallocN(new_stroke->dvert, sizeof(*new_stroke->dvert));
        }
        new_stroke->tot_triangles = 0;
        new_stroke->triangles = MEM_recallocN(new_stroke->triangles,
                                              sizeof(*new_stroke->triangles));
        new_stroke->flag |= GP_STROKE_RECALC_GEOMETRY;
      }

      /* add to strokes */
      BLI_addtail(&tgpil->interFrame->strokes, new_stroke);
    }
  }
}

/* ----------------------- */
/* Drawing Callbacks */

/* Drawing callback for modal operator in screen mode */
static void gpencil_interpolate_draw_screen(const struct bContext *C,
                                            ARegion *UNUSED(ar),
                                            void *arg)
{
  tGPDinterpolate *tgpi = (tGPDinterpolate *)arg;
  ED_gp_draw_interpolation(C, tgpi, REGION_DRAW_POST_PIXEL);
}

/* Drawing callback for modal operator in 3d mode */
static void gpencil_interpolate_draw_3d(const bContext *C, ARegion *UNUSED(ar), void *arg)
{
  tGPDinterpolate *tgpi = (tGPDinterpolate *)arg;
  ED_gp_draw_interpolation(C, tgpi, REGION_DRAW_POST_VIEW);
}

/* ----------------------- */

/* Helper: calculate shift based on position of mouse (we only use x-axis for now.
 * since this is more convenient for users to do), and store new shift value
 */
static void gpencil_mouse_update_shift(tGPDinterpolate *tgpi, wmOperator *op, const wmEvent *event)
{
  float mid = (float)(tgpi->ar->winx - tgpi->ar->winrct.xmin) / 2.0f;
  float mpos = event->x - tgpi->ar->winrct.xmin;

  if (mpos >= mid) {
    tgpi->shift = ((mpos - mid) * tgpi->high_limit) / mid;
  }
  else {
    tgpi->shift = tgpi->low_limit - ((mpos * tgpi->low_limit) / mid);
  }

  CLAMP(tgpi->shift, tgpi->low_limit, tgpi->high_limit);
  RNA_float_set(op->ptr, "shift", tgpi->shift);
}

/* Helper: Draw status message while the user is running the operator */
static void gpencil_interpolate_status_indicators(bContext *C, tGPDinterpolate *p)
{
  Scene *scene = p->scene;
  char status_str[UI_MAX_DRAW_STR];
  char msg_str[UI_MAX_DRAW_STR];

  BLI_strncpy(msg_str, TIP_("GPencil Interpolation: "), UI_MAX_DRAW_STR);

  if (hasNumInput(&p->num)) {
    char str_offs[NUM_STR_REP_LEN];

    outputNumInput(&p->num, str_offs, &scene->unit);
    BLI_snprintf(status_str, sizeof(status_str), "%s%s", msg_str, str_offs);
  }
  else {
    BLI_snprintf(status_str,
                 sizeof(status_str),
                 "%s%d %%",
                 msg_str,
                 (int)((p->init_factor + p->shift) * 100.0f));
  }

  ED_area_status_text(p->sa, status_str);
  ED_workspace_status_text(
      C, TIP_("ESC/RMB to cancel, Enter/LMB to confirm, WHEEL/MOVE to adjust factor"));
}

/* Update screen and stroke */
static void gpencil_interpolate_update(bContext *C, wmOperator *op, tGPDinterpolate *tgpi)
{
  /* update shift indicator in header */
  gpencil_interpolate_status_indicators(C, tgpi);
  /* apply... */
  tgpi->shift = RNA_float_get(op->ptr, "shift");
  /* update points position */
  gp_interpolate_update_strokes(C, tgpi);
}

/* ----------------------- */

/* Exit and free memory */
static void gpencil_interpolate_exit(bContext *C, wmOperator *op)
{
  tGPDinterpolate *tgpi = op->customdata;
  tGPDinterpolate_layer *tgpil;
  bGPdata *gpd = tgpi->gpd;

  /* don't assume that operator data exists at all */
  if (tgpi) {
    /* remove drawing handler */
    if (tgpi->draw_handle_screen) {
      ED_region_draw_cb_exit(tgpi->ar->type, tgpi->draw_handle_screen);
    }
    if (tgpi->draw_handle_3d) {
      ED_region_draw_cb_exit(tgpi->ar->type, tgpi->draw_handle_3d);
    }

    /* clear status message area */
    ED_area_status_text(tgpi->sa, NULL);
    ED_workspace_status_text(C, NULL);

    /* finally, free memory used by temp data */
    for (tgpil = tgpi->ilayers.first; tgpil; tgpil = tgpil->next) {
      BKE_gpencil_free_strokes(tgpil->interFrame);
      MEM_freeN(tgpil->interFrame);
    }

    BLI_freelistN(&tgpi->ilayers);
    MEM_freeN(tgpi);
  }
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);

  /* clear pointer */
  op->customdata = NULL;
}

/* Init new temporary interpolation data */
static bool gp_interpolate_set_init_values(bContext *C, wmOperator *op, tGPDinterpolate *tgpi)
{
  ToolSettings *ts = CTX_data_tool_settings(C);
  bGPdata *gpd = CTX_data_gpencil_data(C);

  /* set current scene and window */
  tgpi->scene = CTX_data_scene(C);
  tgpi->sa = CTX_wm_area(C);
  tgpi->ar = CTX_wm_region(C);
  tgpi->flag = ts->gp_interpolate.flag;

  /* set current frame number */
  tgpi->cframe = tgpi->scene->r.cfra;

  /* set GP datablock */
  tgpi->gpd = gpd;

  /* set interpolation weight */
  tgpi->shift = RNA_float_get(op->ptr, "shift");
  /* set layers */
  gp_interpolate_set_points(C, tgpi);

  return 1;
}

/* Allocate memory and initialize values */
static tGPDinterpolate *gp_session_init_interpolation(bContext *C, wmOperator *op)
{
  tGPDinterpolate *tgpi = MEM_callocN(sizeof(tGPDinterpolate), "GPencil Interpolate Data");

  /* define initial values */
  gp_interpolate_set_init_values(C, op, tgpi);

  /* return context data for running operator */
  return tgpi;
}

/* Init interpolation: Allocate memory and set init values */
static int gpencil_interpolate_init(bContext *C, wmOperator *op)
{
  tGPDinterpolate *tgpi;

  /* check context */
  tgpi = op->customdata = gp_session_init_interpolation(C, op);
  if (tgpi == NULL) {
    /* something wasn't set correctly in context */
    gpencil_interpolate_exit(C, op);
    return 0;
  }

  /* everything is now setup ok */
  return 1;
}

/* ----------------------- */

/* Invoke handler: Initialize the operator */
static int gpencil_interpolate_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  wmWindow *win = CTX_wm_window(C);
  bGPdata *gpd = CTX_data_gpencil_data(C);
  bGPDlayer *gpl = CTX_data_active_gpencil_layer(C);
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  int cfra_eval = (int)DEG_get_ctime(depsgraph);
  bGPDframe *actframe = gpl->actframe;
  tGPDinterpolate *tgpi = NULL;

  /* cannot interpolate if not between 2 frames */
  if (ELEM(NULL, actframe, actframe->next)) {
    BKE_report(
        op->reports,
        RPT_ERROR,
        "Cannot find a pair of grease pencil frames to interpolate between in active layer");
    return OPERATOR_CANCELLED;
  }

  /* cannot interpolate in extremes */
  if (ELEM(cfra_eval, actframe->framenum, actframe->next->framenum)) {
    BKE_report(op->reports,
               RPT_ERROR,
               "Cannot interpolate as current frame already has existing grease pencil frames");
    return OPERATOR_CANCELLED;
  }

  /* need editable strokes */
  if (!gp_interpolate_check_todo(C, gpd)) {
    BKE_report(op->reports, RPT_ERROR, "Interpolation requires some editable strokes");
    return OPERATOR_CANCELLED;
  }

  /* try to initialize context data needed */
  if (!gpencil_interpolate_init(C, op)) {
    if (op->customdata) {
      MEM_freeN(op->customdata);
    }
    return OPERATOR_CANCELLED;
  }
  else {
    tgpi = op->customdata;
  }

  /* Enable custom drawing handlers
   * It needs 2 handlers because strokes can in 3d space and screen space
   * and each handler use different coord system
   */
  tgpi->draw_handle_screen = ED_region_draw_cb_activate(
      tgpi->ar->type, gpencil_interpolate_draw_screen, tgpi, REGION_DRAW_POST_PIXEL);
  tgpi->draw_handle_3d = ED_region_draw_cb_activate(
      tgpi->ar->type, gpencil_interpolate_draw_3d, tgpi, REGION_DRAW_POST_VIEW);

  /* set cursor to indicate modal */
  WM_cursor_modal_set(win, BC_EW_SCROLLCURSOR);

  /* update shift indicator in header */
  gpencil_interpolate_status_indicators(C, tgpi);
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);

  /* add a modal handler for this operator */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

/* Modal handler: Events handling during interactive part */
static int gpencil_interpolate_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  tGPDinterpolate *tgpi = op->customdata;
  wmWindow *win = CTX_wm_window(C);
  bGPDframe *gpf_dst;
  bGPDstroke *gps_src, *gps_dst;
  tGPDinterpolate_layer *tgpil;
  const bool has_numinput = hasNumInput(&tgpi->num);

  switch (event->type) {
    case LEFTMOUSE: /* confirm */
    case RETKEY: {
      /* return to normal cursor and header status */
      ED_area_status_text(tgpi->sa, NULL);
      ED_workspace_status_text(C, NULL);
      WM_cursor_modal_restore(win);

      /* insert keyframes as required... */
      for (tgpil = tgpi->ilayers.first; tgpil; tgpil = tgpil->next) {
        gpf_dst = BKE_gpencil_layer_getframe(tgpil->gpl, tgpi->cframe, GP_GETFRAME_ADD_NEW);
        gpf_dst->key_type = BEZT_KEYTYPE_BREAKDOWN;

        /* copy strokes */
        BLI_listbase_clear(&gpf_dst->strokes);
        for (gps_src = tgpil->interFrame->strokes.first; gps_src; gps_src = gps_src->next) {
          if (gps_src->totpoints == 0) {
            continue;
          }

          /* make copy of source stroke, then adjust pointer to points too */
          gps_dst = MEM_dupallocN(gps_src);
          gps_dst->points = MEM_dupallocN(gps_src->points);
          if (gps_src->dvert != NULL) {
            gps_dst->dvert = MEM_dupallocN(gps_src->dvert);
            BKE_gpencil_stroke_weights_duplicate(gps_src, gps_dst);
          }
          gps_dst->triangles = MEM_dupallocN(gps_src->triangles);
          gps_dst->flag |= GP_STROKE_RECALC_GEOMETRY;
          BLI_addtail(&gpf_dst->strokes, gps_dst);
        }
      }

      /* clean up temp data */
      gpencil_interpolate_exit(C, op);

      /* done! */
      return OPERATOR_FINISHED;
    }

    case ESCKEY: /* cancel */
    case RIGHTMOUSE: {
      /* return to normal cursor and header status */
      ED_area_status_text(tgpi->sa, NULL);
      ED_workspace_status_text(C, NULL);
      WM_cursor_modal_restore(win);

      /* clean up temp data */
      gpencil_interpolate_exit(C, op);

      /* canceled! */
      return OPERATOR_CANCELLED;
    }

    case WHEELUPMOUSE: {
      tgpi->shift = tgpi->shift + 0.01f;
      CLAMP(tgpi->shift, tgpi->low_limit, tgpi->high_limit);
      RNA_float_set(op->ptr, "shift", tgpi->shift);

      /* update screen */
      gpencil_interpolate_update(C, op, tgpi);
      break;
    }
    case WHEELDOWNMOUSE: {
      tgpi->shift = tgpi->shift - 0.01f;
      CLAMP(tgpi->shift, tgpi->low_limit, tgpi->high_limit);
      RNA_float_set(op->ptr, "shift", tgpi->shift);

      /* update screen */
      gpencil_interpolate_update(C, op, tgpi);
      break;
    }
    case MOUSEMOVE: /* calculate new position */
    {
      /* only handle mousemove if not doing numinput */
      if (has_numinput == false) {
        /* update shift based on position of mouse */
        gpencil_mouse_update_shift(tgpi, op, event);

        /* update screen */
        gpencil_interpolate_update(C, op, tgpi);
      }
      break;
    }
    default: {
      if ((event->val == KM_PRESS) && handleNumInput(C, &tgpi->num, event)) {
        const float factor = tgpi->init_factor;
        float value;

        /* Grab shift from numeric input, and store this new value (the user see an int) */
        value = (factor + tgpi->shift) * 100.0f;
        applyNumInput(&tgpi->num, &value);
        tgpi->shift = value / 100.0f;

        /* recalculate the shift to get the right value in the frame scale */
        tgpi->shift = tgpi->shift - factor;

        CLAMP(tgpi->shift, tgpi->low_limit, tgpi->high_limit);
        RNA_float_set(op->ptr, "shift", tgpi->shift);

        /* update screen */
        gpencil_interpolate_update(C, op, tgpi);

        break;
      }
      else {
        /* unhandled event - allow to pass through */
        return OPERATOR_RUNNING_MODAL | OPERATOR_PASS_THROUGH;
      }
    }
  }

  /* still running... */
  return OPERATOR_RUNNING_MODAL;
}

/* Cancel handler */
static void gpencil_interpolate_cancel(bContext *C, wmOperator *op)
{
  /* this is just a wrapper around exit() */
  gpencil_interpolate_exit(C, op);
}

void GPENCIL_OT_interpolate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Grease Pencil Interpolation";
  ot->idname = "GPENCIL_OT_interpolate";
  ot->description = "Interpolate grease pencil strokes between frames";

  /* callbacks */
  ot->invoke = gpencil_interpolate_invoke;
  ot->modal = gpencil_interpolate_modal;
  ot->cancel = gpencil_interpolate_cancel;
  ot->poll = gpencil_view3d_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* properties */
  RNA_def_float_percentage(
      ot->srna,
      "shift",
      0.0f,
      -1.0f,
      1.0f,
      "Shift",
      "Bias factor for which frame has more influence on the interpolated strokes",
      -0.9f,
      0.9f);
}

/* ****************** Interpolate Sequence *********************** */

/* Helper: Perform easing equation calculations for GP interpolation operator */
static float gp_interpolate_seq_easing_calc(GP_Interpolate_Settings *ipo_settings, float time)
{
  const float begin = 0.0f;
  const float change = 1.0f;
  const float duration = 1.0f;

  const float back = ipo_settings->back;
  const float amplitude = ipo_settings->amplitude;
  const float period = ipo_settings->period;

  eBezTriple_Easing easing = ipo_settings->easing;
  float result = time;

  switch (ipo_settings->type) {
    case GP_IPO_BACK:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          result = BLI_easing_back_ease_in(time, begin, change, duration, back);
          break;
        case BEZT_IPO_EASE_OUT:
          result = BLI_easing_back_ease_out(time, begin, change, duration, back);
          break;
        case BEZT_IPO_EASE_IN_OUT:
          result = BLI_easing_back_ease_in_out(time, begin, change, duration, back);
          break;

        default: /* default/auto: same as ease out */
          result = BLI_easing_back_ease_out(time, begin, change, duration, back);
          break;
      }
      break;

    case GP_IPO_BOUNCE:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          result = BLI_easing_bounce_ease_in(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_OUT:
          result = BLI_easing_bounce_ease_out(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_IN_OUT:
          result = BLI_easing_bounce_ease_in_out(time, begin, change, duration);
          break;

        default: /* default/auto: same as ease out */
          result = BLI_easing_bounce_ease_out(time, begin, change, duration);
          break;
      }
      break;

    case GP_IPO_CIRC:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          result = BLI_easing_circ_ease_in(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_OUT:
          result = BLI_easing_circ_ease_out(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_IN_OUT:
          result = BLI_easing_circ_ease_in_out(time, begin, change, duration);
          break;

        default: /* default/auto: same as ease in */
          result = BLI_easing_circ_ease_in(time, begin, change, duration);
          break;
      }
      break;

    case GP_IPO_CUBIC:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          result = BLI_easing_cubic_ease_in(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_OUT:
          result = BLI_easing_cubic_ease_out(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_IN_OUT:
          result = BLI_easing_cubic_ease_in_out(time, begin, change, duration);
          break;

        default: /* default/auto: same as ease in */
          result = BLI_easing_cubic_ease_in(time, begin, change, duration);
          break;
      }
      break;

    case GP_IPO_ELASTIC:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          result = BLI_easing_elastic_ease_in(time, begin, change, duration, amplitude, period);
          break;
        case BEZT_IPO_EASE_OUT:
          result = BLI_easing_elastic_ease_out(time, begin, change, duration, amplitude, period);
          break;
        case BEZT_IPO_EASE_IN_OUT:
          result = BLI_easing_elastic_ease_in_out(
              time, begin, change, duration, amplitude, period);
          break;

        default: /* default/auto: same as ease out */
          result = BLI_easing_elastic_ease_out(time, begin, change, duration, amplitude, period);
          break;
      }
      break;

    case GP_IPO_EXPO:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          result = BLI_easing_expo_ease_in(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_OUT:
          result = BLI_easing_expo_ease_out(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_IN_OUT:
          result = BLI_easing_expo_ease_in_out(time, begin, change, duration);
          break;

        default: /* default/auto: same as ease in */
          result = BLI_easing_expo_ease_in(time, begin, change, duration);
          break;
      }
      break;

    case GP_IPO_QUAD:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          result = BLI_easing_quad_ease_in(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_OUT:
          result = BLI_easing_quad_ease_out(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_IN_OUT:
          result = BLI_easing_quad_ease_in_out(time, begin, change, duration);
          break;

        default: /* default/auto: same as ease in */
          result = BLI_easing_quad_ease_in(time, begin, change, duration);
          break;
      }
      break;

    case GP_IPO_QUART:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          result = BLI_easing_quart_ease_in(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_OUT:
          result = BLI_easing_quart_ease_out(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_IN_OUT:
          result = BLI_easing_quart_ease_in_out(time, begin, change, duration);
          break;

        default: /* default/auto: same as ease in */
          result = BLI_easing_quart_ease_in(time, begin, change, duration);
          break;
      }
      break;

    case GP_IPO_QUINT:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          result = BLI_easing_quint_ease_in(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_OUT:
          result = BLI_easing_quint_ease_out(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_IN_OUT:
          result = BLI_easing_quint_ease_in_out(time, begin, change, duration);
          break;

        default: /* default/auto: same as ease in */
          result = BLI_easing_quint_ease_in(time, begin, change, duration);
          break;
      }
      break;

    case GP_IPO_SINE:
      switch (easing) {
        case BEZT_IPO_EASE_IN:
          result = BLI_easing_sine_ease_in(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_OUT:
          result = BLI_easing_sine_ease_out(time, begin, change, duration);
          break;
        case BEZT_IPO_EASE_IN_OUT:
          result = BLI_easing_sine_ease_in_out(time, begin, change, duration);
          break;

        default: /* default/auto: same as ease in */
          result = BLI_easing_sine_ease_in(time, begin, change, duration);
          break;
      }
      break;

    default:
      printf("%s: Unknown interpolation type - %d\n", __func__, ipo_settings->type);
      break;
  }

  return result;
}

static int gpencil_interpolate_seq_exec(bContext *C, wmOperator *op)
{
  bGPdata *gpd = CTX_data_gpencil_data(C);
  bGPDlayer *active_gpl = CTX_data_active_gpencil_layer(C);
  bGPDframe *actframe = active_gpl->actframe;

  Object *ob = CTX_data_active_object(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  int cfra_eval = (int)DEG_get_ctime(depsgraph);

  GP_Interpolate_Settings *ipo_settings = &ts->gp_interpolate;
  eGP_Interpolate_SettingsFlag flag = ipo_settings->flag;

  /* cannot interpolate if not between 2 frames */
  if (ELEM(NULL, actframe, actframe->next)) {
    BKE_report(
        op->reports,
        RPT_ERROR,
        "Cannot find a pair of grease pencil frames to interpolate between in active layer");
    return OPERATOR_CANCELLED;
  }
  /* cannot interpolate in extremes */
  if (ELEM(cfra_eval, actframe->framenum, actframe->next->framenum)) {
    BKE_report(op->reports,
               RPT_ERROR,
               "Cannot interpolate as current frame already has existing grease pencil frames");
    return OPERATOR_CANCELLED;
  }

  /* loop all layer to check if need interpolation */
  for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
    bGPDframe *prevFrame, *nextFrame;
    bGPDstroke *gps_from, *gps_to;
    int cframe, fFrame;

    /* all layers or only active */
    if (((flag & GP_TOOLFLAG_INTERPOLATE_ALL_LAYERS) == 0) && (gpl != active_gpl)) {
      continue;
    }
    /* only editable and visible layers are considered */
    if (!gpencil_layer_is_editable(gpl) || (gpl->actframe == NULL)) {
      continue;
    }

    /* store extremes */
    prevFrame = gpl->actframe;
    nextFrame = gpl->actframe->next;

    /* Loop over intermediary frames and create the interpolation */
    for (cframe = prevFrame->framenum + 1; cframe < nextFrame->framenum; cframe++) {
      bGPDframe *interFrame = NULL;
      float factor;

      /* get interpolation factor */
      float framerange = nextFrame->framenum - prevFrame->framenum;
      CLAMP_MIN(framerange, 1.0f);
      factor = (float)(cframe - prevFrame->framenum) / framerange;

      if (ipo_settings->type == GP_IPO_CURVEMAP) {
        /* custom curvemap */
        if (ipo_settings->custom_ipo) {
          factor = curvemapping_evaluateF(ipo_settings->custom_ipo, 0, factor);
        }
        else {
          BKE_report(op->reports, RPT_ERROR, "Custom interpolation curve does not exist");
        }
      }
      else if (ipo_settings->type >= GP_IPO_BACK) {
        /* easing equation... */
        factor = gp_interpolate_seq_easing_calc(ipo_settings, factor);
      }

      /* create new strokes data with interpolated points reading original stroke */
      for (gps_from = prevFrame->strokes.first; gps_from; gps_from = gps_from->next) {
        bGPDstroke *new_stroke = NULL;

        /* only selected */
        if ((flag & GP_TOOLFLAG_INTERPOLATE_ONLY_SELECTED) &&
            ((gps_from->flag & GP_STROKE_SELECT) == 0)) {
          continue;
        }
        /* skip strokes that are invalid for current view */
        if (ED_gpencil_stroke_can_use(C, gps_from) == false) {
          continue;
        }
        /* check if the color is editable */
        if (ED_gpencil_stroke_color_use(ob, gpl, gps_from) == false) {
          continue;
        }

        /* get final stroke to interpolate */
        fFrame = BLI_findindex(&prevFrame->strokes, gps_from);
        gps_to = BLI_findlink(&nextFrame->strokes, fFrame);
        if (gps_to == NULL) {
          continue;
        }

        /* create a new frame if needed */
        if (interFrame == NULL) {
          interFrame = BKE_gpencil_layer_getframe(gpl, cframe, GP_GETFRAME_ADD_NEW);
          interFrame->key_type = BEZT_KEYTYPE_BREAKDOWN;
        }

        /* create new stroke */
        new_stroke = BKE_gpencil_stroke_duplicate(gps_from);

        /* if destination stroke is smaller, resize new_stroke to size of gps_to stroke */
        if (gps_from->totpoints > gps_to->totpoints) {
          /* free weights of removed points */
          if (gps_from->dvert != NULL) {
            BKE_defvert_array_free_elems(gps_from->dvert + gps_to->totpoints,
                                         gps_from->totpoints - gps_to->totpoints);
          }

          new_stroke->points = MEM_recallocN(new_stroke->points,
                                             sizeof(*new_stroke->points) * gps_to->totpoints);

          if (new_stroke->dvert != NULL) {
            new_stroke->dvert = MEM_recallocN(new_stroke->dvert,
                                              sizeof(*new_stroke->dvert) * gps_to->totpoints);
          }
          new_stroke->totpoints = gps_to->totpoints;
          new_stroke->tot_triangles = 0;
          new_stroke->flag |= GP_STROKE_RECALC_GEOMETRY;
        }

        /* update points position */
        gp_interpolate_update_points(gps_from, gps_to, new_stroke, factor);

        /* add to strokes */
        BLI_addtail(&interFrame->strokes, new_stroke);
      }
    }
  }

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_interpolate_sequence(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Interpolate Sequence";
  ot->idname = "GPENCIL_OT_interpolate_sequence";
  ot->description = "Generate 'in-betweens' to smoothly interpolate between Grease Pencil frames";

  /* api callbacks */
  ot->exec = gpencil_interpolate_seq_exec;
  ot->poll = gpencil_view3d_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************** Remove Breakdowns ************************ */

static bool gpencil_interpolate_reverse_poll(bContext *C)
{
  if (!gpencil_view3d_poll(C)) {
    return 0;
  }

  bGPDlayer *gpl = CTX_data_active_gpencil_layer(C);

  /* need to be on a breakdown frame */
  if ((gpl->actframe == NULL) || (gpl->actframe->key_type != BEZT_KEYTYPE_BREAKDOWN)) {
    CTX_wm_operator_poll_msg_set(C, "Expected current frame to be a breakdown");
    return 0;
  }

  return 1;
}

static int gpencil_interpolate_reverse_exec(bContext *C, wmOperator *UNUSED(op))
{
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  /* Go through each layer, deleting the breakdowns around the current frame,
   * but only if there is a keyframe nearby to stop at
   */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *start_key = NULL;
    bGPDframe *end_key = NULL;
    bGPDframe *gpf, *gpfn;

    /* Only continue if we're currently on a breakdown keyframe */
    if ((gpl->actframe == NULL) || (gpl->actframe->key_type != BEZT_KEYTYPE_BREAKDOWN)) {
      continue;
    }

    /* Search left for "start_key" (i.e. the first breakdown to remove) */
    gpf = gpl->actframe;
    while (gpf) {
      if (gpf->key_type == BEZT_KEYTYPE_BREAKDOWN) {
        /* A breakdown... keep going left */
        start_key = gpf;
        gpf = gpf->prev;
      }
      else {
        /* Not a breakdown (may be a key, or an extreme,
         * or something else that wasn't generated)... stop */
        break;
      }
    }

    /* Search right for "end_key" (i.e. the last breakdown to remove) */
    gpf = gpl->actframe;
    while (gpf) {
      if (gpf->key_type == BEZT_KEYTYPE_BREAKDOWN) {
        /* A breakdown... keep going right */
        end_key = gpf;
        gpf = gpf->next;
      }
      else {
        /* Not a breakdown... stop */
        break;
      }
    }

    /* Did we find anything? */
    /* NOTE: We should only proceed if there's something before/after these extents...
     * Otherwise, there's just an extent of breakdowns with no keys to interpolate between
     */
    if ((start_key && end_key) && ELEM(NULL, start_key->prev, end_key->next) == false) {
      /* Set actframe to the key before start_key, since the keys have been removed now */
      gpl->actframe = start_key->prev;

      /* Free each frame we're removing (except the last one) */
      for (gpf = start_key; gpf && gpf != end_key; gpf = gpfn) {
        gpfn = gpf->next;

        /* free strokes and their associated memory */
        BKE_gpencil_free_strokes(gpf);
        BLI_freelinkN(&gpl->frames, gpf);
      }

      /* Now free the last one... */
      BKE_gpencil_free_strokes(end_key);
      BLI_freelinkN(&gpl->frames, end_key);
    }
  }
  CTX_DATA_END;

  /* notifiers */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_interpolate_reverse(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Breakdowns";
  ot->idname = "GPENCIL_OT_interpolate_reverse";
  ot->description =
      "Remove breakdown frames generated by interpolating between two Grease Pencil frames";

  /* callbacks */
  ot->exec = gpencil_interpolate_reverse_exec;
  ot->poll = gpencil_interpolate_reverse_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* *************************************************************** */
